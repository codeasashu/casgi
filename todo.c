
int uwsgi_request_wsgi(struct uwsgi_server *uwsgi,
                       struct wsgi_request *wsgi_req) {

  int i;

  PyObject *zero, *wsgi_socket;

  PyObject *pydictkey, *pydictvalue;

  char *path_info;
  struct uwsgi_app *wi;

  /* Standard WSGI request */
  if (!wsgi_req->uh.pktsize) {
    uwsgi_log("Invalid WSGI request. skip.\n");
    return -1;
  }

  if (uwsgi_parse_vars(uwsgi, wsgi_req)) {
    uwsgi_log("Invalid WSGI request. skip.\n");
    return -1;
  }

  if (wsgi_req->script_name_len > 0) {
    zero = PyString_FromStringAndSize(wsgi_req->script_name,
                                      wsgi_req->script_name_len);
    if (PyDict_Contains(uwsgi->py_apps, zero)) {
      wsgi_req->app_id = PyInt_AsLong(PyDict_GetItem(uwsgi->py_apps, zero));
    } else {
      /* unavailable app for this SCRIPT_NAME */
    }
    Py_DECREF(zero);
  }

  wi = &uwsgi->wsgi_apps[wsgi_req->app_id];

  if (uwsgi->single_interpreter == 0) {
    if (!wi->interpreter) {
      internal_server_error(wsgi_req->poll.fd,
                            "wsgi application's %d interpreter not found");
      goto clear2;
    }

    // set the interpreter
    PyThreadState_Swap(wi->interpreter);
  }

  wi->requests++;
  // START: Set env vars for python handler
  wsgi_req->async_environ = wi->wsgi_environ;
  wsgi_req->async_args = wi->wsgi_args;
  Py_INCREF((PyObject *)wsgi_req->async_environ);

  for (i = 0; i < wsgi_req->var_cnt; i += 2) {
    // uwsgi_log("%.*s: %.*s\n", wsgi_req->hvec[i].iov_len,
    // wsgi_req->hvec[i].iov_base, wsgi_req->hvec[i+1].iov_len,
    // wsgi_req->hvec[i+1].iov_base);
    pydictkey = PyString_FromStringAndSize(wsgi_req->hvec[i].iov_base,
                                           wsgi_req->hvec[i].iov_len);
    pydictvalue = PyString_FromStringAndSize(wsgi_req->hvec[i + 1].iov_base,
                                             wsgi_req->hvec[i + 1].iov_len);
    PyDict_SetItem(wsgi_req->async_environ, pydictkey, pydictvalue);
    Py_DECREF(pydictkey);
    Py_DECREF(pydictvalue);
  }
  // END: Set env vars for python handler

  // set wsgi vars

  wsgi_req->async_post = fdopen(wsgi_req->poll.fd, "r");

  wsgi_socket = PyFile_FromFile(wsgi_req->async_post, "wsgi_input", "r", NULL);
  PyDict_SetItemString(wsgi_req->async_environ, "wsgi.input", wsgi_socket);
  Py_DECREF(wsgi_socket);

  zero = PyTuple_New(2);
  PyTuple_SetItem(zero, 0, PyInt_FromLong(1));
  PyTuple_SetItem(zero, 1, PyInt_FromLong(0));
  PyDict_SetItemString(wsgi_req->async_environ, "wsgi.version", zero);
  Py_DECREF(zero);

  zero = PyFile_FromFile(stderr, "wsgi_input", "w", NULL);
  PyDict_SetItemString(wsgi_req->async_environ, "wsgi.errors", zero);
  Py_DECREF(zero);

  PyDict_SetItemString(wsgi_req->async_environ, "wsgi.run_once", Py_False);

  PyDict_SetItemString(wsgi_req->async_environ, "wsgi.multithread", Py_False);
  if (uwsgi->numproc == 1) {
    PyDict_SetItemString(wsgi_req->async_environ, "wsgi.multiprocess",
                         Py_False);
  } else {
    PyDict_SetItemString(wsgi_req->async_environ, "wsgi.multiprocess", Py_True);
  }

  if (wsgi_req->scheme_len > 0) {
    zero = PyString_FromStringAndSize(wsgi_req->scheme, wsgi_req->scheme_len);
  } else if (wsgi_req->https_len > 0) {
    if (!strncasecmp(wsgi_req->https, "on", 2) || wsgi_req->https[0] == '1') {
      zero = PyString_FromString("https");
    } else {
      zero = PyString_FromString("http");
    }
  } else {
    zero = PyString_FromString("http");
  }
  PyDict_SetItemString(wsgi_req->async_environ, "wsgi.url_scheme", zero);
  Py_DECREF(zero);

  // call

  PyTuple_SetItem(wsgi_req->async_args, 0, wsgi_req->async_environ);
  wsgi_req->async_result = python_call(wi->wsgi_callable, wsgi_req->async_args);

  if (wsgi_req->async_result) {

    while (manage_python_response(uwsgi, wsgi_req) != UWSGI_OK) {
#ifdef UWSGI_ASYNC
      if (uwsgi->async > 1) {
        return UWSGI_AGAIN;
      }
#endif
    }
  }

clear:

  if (uwsgi->single_interpreter == 0) {
    // restoring main interpreter
    PyThreadState_Swap(uwsgi->main_thread);
  }

clear2:

  return UWSGI_OK;
}
