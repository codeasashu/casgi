#include "asgi.h"
#include "pylifecycle.h"
// #include <cstdlib>

extern struct casgi_server casgi;

PyObject *get_casgi_pydict(char *module) {

  PyObject *wsgi_module, *wsgi_dict;

  printf("(worker %d) importing module %s ... \n", casgi.mywid, module);
  wsgi_module = PyImport_ImportModule(module);
  if (!wsgi_module) {
    printf("(worker %d) unable to import module %s ... \n", casgi.mywid, module);
    PyErr_Print();
    return NULL;
  }

  printf("(worker %d) getting module dict %s ... \n", casgi.mywid, module);
  wsgi_dict = PyModule_GetDict(wsgi_module);
  if (!wsgi_dict) {
    PyErr_Print();
    return NULL;
  }

  return wsgi_dict;
}

void set_dyn_pyhome(struct casgi_server *casgi) {
  printf("setting virtualenv... (app=%d)... \n", casgi->mywid);
  char venv_version[30];
  PyObject *site_module;

  PyObject *pysys_dict = get_casgi_pydict("sys");

  printf("finding paths... (app=%d)... \n", casgi->mywid);
  PyObject *pypath = PyDict_GetItemString(pysys_dict, "path");
  if (!pypath) {
    PyErr_Print();
    exit(1);
  }

  printf("inserting app_path paths... (app=%d)... \n", casgi->mywid);
  if (PyList_Insert(pypath, 0, PyUnicode_FromString(casgi->config->app_path))) {
    PyErr_Print();
  }
  // simulate a pythonhome directive
  if ((char *)casgi->config->pyhome != NULL) {

    PyObject *venv_path = PyUnicode_FromString(casgi->config->pyhome);

    printf("setting dynamic virtualenv to %s\n", casgi->config->pyhome);

    PyDict_SetItemString(pysys_dict, "prefix", venv_path);
    PyDict_SetItemString(pysys_dict, "exec_prefix", venv_path);

    bzero(venv_version, 30);
    if (snprintf(venv_version, 30, "/lib/python%d.%d", 3, 12) == -1) {
      printf("unable to set dynamic virtualenv to %s\n", casgi->config->pyhome);
      return;
    }

    // check here
    PyUnicode_Concat(venv_path, PyUnicode_FromString(venv_version));

    if (PyList_Insert(pypath, 1, venv_path)) {
      PyErr_Print();
    }

    site_module = PyImport_ImportModule("site");
    if (site_module) {
      PyImport_ReloadModule(site_module);
    }
  }
  PyDict_SetItemString(pysys_dict, "path", pypath);
}

PyObject *method_fputs(PyObject *self, PyObject *args) {
  struct epoll_event event;
  int n;
  int epoll_fd = casgi.epollfd;
  char *buff = malloc(casgi.buffer_size);
  if (!buff) {
    printf("malloc() method_fputs\n");
    exit(1);
  }

  struct asgi_request *asgi_req = current_asgi_req(&casgi);

  char *str;

  /* Parse arguments */
  if (!PyArg_ParseTuple(args, "s", &str)) {
    PyErr_SetString(PyExc_RuntimeError, "Unable to parse args");
    return NULL;
  }

  printf("(worker %d) received cmd=%s, fd=%d \n", casgi.mywid, str, asgi_req->epoll_fd);
  event.events = EPOLLOUT;
  // event.data.fd = asgi_req->epoll_fd;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, asgi_req->epoll_fd, &event) == -1) {
    perror("Failed to add client_fd to epoll");
    PyErr_SetString(PyExc_RuntimeError, "Error in epoll_wait for reading");
    return NULL;
  }
  n = epoll_wait(epoll_fd, &event, 1, -1);
  if (n == -1) {
    perror("epoll_wait()");
    PyErr_SetString(PyExc_RuntimeError, "Error in epoll_wait for writing");
    return NULL;
  }
  if (event.events & EPOLLOUT) {
      int written = send_asgi_line(asgi_req->epoll_fd, str);
      if(written <= 0){
        printf("(worker %d) write error\n", casgi.mywid);
        exit(1);
      }
      printf("(worker %d) written bytes=%d, fd=%d ", casgi.mywid, written, asgi_req->epoll_fd);
  }
  printf("reading asterisk response...\n");
  //int written = write(asgi_req->epoll_fd, str, strlen(str));
  event.events = EPOLLIN;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, asgi_req->epoll_fd, &event) == -1) {
        perror("Failed to modify client_fd to EPOLLIN");
        return NULL;
  }
  printf("waiting for socket to be available...\n");
  n = epoll_wait(epoll_fd, &event, 1, -1);
  if (n == -1) {
    perror("epoll_wait()");
    printf("epoll_wait() error during read\n");
    PyErr_SetString(PyExc_RuntimeError, "Error in epoll_wait for reading");
    return NULL;
  }
  if (event.events & EPOLLIN) {
      int bytes_read = get_asgi_line(asgi_req->epoll_fd, buff);
      printf("(worker %d) [agi] buff=%s\n", casgi.mywid, buff);
      char *ast_resp = strndup(buff, bytes_read);
      free(buff);
      ast_resp[bytes_read] = '\0';
      return PyUnicode_FromString(ast_resp);
  }
  return PyUnicode_FromString("");
}

void init_paths(const char *mypath) {
  PyObject *pysys, *pysys_dict, *pypath;
  char venv_version[30];

  /* add cwd to pythonpath */
  pysys = PyImport_ImportModule("sys");
  if (!pysys) {
    PyErr_Print();
    exit(1);
  }
  pysys_dict = PyModule_GetDict(pysys);
  pypath = PyDict_GetItemString(pysys_dict, "path");
  if (!pypath) {
    PyErr_Print();
    exit(1);
  }
  // if (PyList_Insert(pypath, 0, PyUnicode_FromString(".")) != 0) {
  //   PyErr_Print();
  // }

  if ((char *)casgi.config->pyhome != NULL) {

    PyObject *venv_path = PyUnicode_FromString(casgi.config->pyhome);

    printf("setting dynamic virtualenv to %s\n", casgi.config->pyhome);

    // PyDict_SetItemString(pysys_dict, "prefix", venv_path);
    // PyDict_SetItemString(pysys_dict, "exec_prefix", venv_path);

    bzero(venv_version, 30);
    if (snprintf(venv_version, 30, "/lib/python%d.%d", 3, 12) == -1) {
      printf("unable to set dynamic virtualenv to %s\n", casgi.config->pyhome);
      return;
    }

    // check here
    PyUnicode_Concat(venv_path, PyUnicode_FromString(venv_version));

    if (PyList_Insert(pypath, 0, venv_path)) {
      PyErr_Print();
    }

    // site_module = PyImport_ImportModule("site");
    // if (site_module) {
    //   PyImport_ReloadModule(site_module);
    // }
  }

  if (PyList_Insert(pypath, 0, PyUnicode_FromString(mypath)) != 0) {
    PyErr_Print();
  } else {
    printf("[INFO] adding %s to pythonpath.\n", mypath);
  }

  PyDict_SetItemString(pysys_dict, "path", pypath);
  // venv/lib/python3.12/site-packages
}

struct casgi_app *init_casgi_app(PyObject *my_callable) {
  PyObject *wsgi_fputs;
  int id = 0;

  struct casgi_app *app;

  if (my_callable == NULL) {
    printf("invalid application. skip.\n");
    return NULL;
  }

  app = malloc(sizeof(struct casgi_app));
  memset(app, 0, sizeof(struct casgi_app));

  // Py_Initialize();
  // app->interpreter = Py_NewInterpreter();
  // if (!app->interpreter) {
  //  printf("unable to initialize the new interpreter\n");
  //  exit(1);
  // }
  // PyThreadState_Swap(app->interpreter);
  static PyMethodDef FputsMethods[] = {
      {"fputs", method_fputs, METH_VARARGS, ""}, {NULL, NULL, 0, NULL}};

  wsgi_fputs = PyCFunction_New(FputsMethods, NULL);

  // init_uwsgi_vars();
  printf("interpreter for app %d initialized.\n", id);

  app->asgi_callable = my_callable;
  app->asgi_fputs = wsgi_fputs;
  Py_INCREF(my_callable);
  return app;
}

struct casgi_app *uwsgi_wsgi_file_config(struct casgi_server *casgi,
                                         int workerid) {
  struct casgi_app *app = malloc(sizeof(struct casgi_app));
  memset(app, 0, sizeof(struct casgi_app));
  PyObject *asgi_file_callable;

  Py_Initialize();

  set_dyn_pyhome(casgi);
  init_paths(casgi->config->app_path);

  // Load the module object
  PyObject *pName = PyUnicode_DecodeFSDefault(casgi->config->module);
  PyObject *pModule = PyImport_Import(pName);
  Py_DECREF(pName);
  if (!pModule) {
    PyErr_Print();
    exit(1);
  }
  // Get the class from the module
  asgi_file_callable = PyObject_GetAttrString(pModule, "application");
  if (!asgi_file_callable) {
    PyErr_Print();
    printf("unable to find \"application\" callable in wsgi file %s (%s)\n",
           casgi->config->app_path, casgi->config->module);
    exit(1);
  }
  Py_DECREF(pModule);

  return init_casgi_app(asgi_file_callable);
  // Finalize the Python interpreter
  // Py_Finalize();
}


PyObject *init_py() {
  PyObject *asgi_file_callable;

  if (!Py_IsInitialized()) {
    printf("python not initialised. initing... (app=%d)... \n", casgi.mywid);
    Py_Initialize();
  }
  printf("initialized python module (app=%d)... \n", casgi.mywid);
  PyGILState_STATE gstate = PyGILState_Ensure();
  printf("get GIL (app=%d)... \n", casgi.mywid);

  set_dyn_pyhome(&casgi);
  printf("(worker %d) init paths %s... \n", casgi.mywid, casgi.config->app_path);
  init_paths(casgi.config->app_path);

  printf("(worker %d) importing module %s... \n", casgi.mywid, casgi.config->module);
  // Load the module object
  PyObject *pName = PyUnicode_DecodeFSDefault(casgi.config->module);
  PyObject *pModule = PyImport_ImportModule(casgi.config->module);
  printf("(worker %d) imported module %s !! \n", casgi.mywid, casgi.config->module);
  Py_DECREF(pName);
  if (!pModule) {
    PyErr_Print();
    exit(1);
     PyGILState_Release(gstate);
  }
  printf("(worker %d) finding callable 'application'... \n", casgi.mywid);
  // Get the class from the module
  asgi_file_callable = PyObject_GetAttrString(pModule, "application");
  if (!asgi_file_callable) {
    PyErr_Print();
    printf("unable to find \"application\" callable in wsgi file %s (%s)\n",
           casgi.config->app_path, casgi.config->module);
    exit(1);
  }
  printf("(worker %d) found callable 'application'... \n", casgi.mywid);
  Py_DECREF(pModule);

  // PyGILState_Release(gstate);
  return asgi_file_callable;
}

int python_call_asgi(PyObject *callable, struct agi_header *agi_header) {
  PyObject *py_dict = PyDict_New();

  // Loop through each agi_pair and add it to the dictionary
  for (int i = 0; i < agi_header->env_lines; i++) {
    PyObject *py_value = PyUnicode_FromString(agi_header->env[i].value);
    PyDict_SetItemString(py_dict, agi_header->env[i].key, py_value);
    Py_DECREF(py_value);
  }

  PyObject *args = PyTuple_Pack(1, py_dict);
  python_call(callable, args);
  Py_DECREF(py_dict);
  Py_DECREF(args);
  return 0;
}

int python_request_handler(struct casgi_app *app,
                           struct agi_header *agi_header) {
  char python_cmd[300];
  PyObject *callable = app->asgi_callable;
  set_dyn_pyhome(&casgi);
  PyObject *py_dict = PyDict_New();

  printf("going to python\n");
  // Loop through each agi_pair and add it to the dictionary
  for (int i = 0; i < agi_header->env_lines; i++) {
    PyObject *py_value = PyUnicode_FromString(agi_header->env[i].value);
    PyDict_SetItemString(py_dict, agi_header->env[i].key, py_value);
    Py_DECREF(py_value);
  }

  printf("really going to python, tuplepack\n");
  PyObject *args = PyTuple_Pack(2, py_dict, app->asgi_fputs);
  printf("before python_call \n");
  python_call(callable, args);
  printf("after python_call \n");
  Py_DECREF(py_dict);
  Py_DECREF(args);
  return 0;
}


int python_request_handler_v2(struct agi_header *agi_header) {
  PyObject *callable, *asgi_fputs;

  callable = init_py();
  static PyMethodDef FputsMethods[] = {
      {"fputs", method_fputs, METH_VARARGS, ""}, {NULL, NULL, 0, NULL}};
  asgi_fputs = PyCFunction_New(FputsMethods, NULL);

  char python_cmd[300];
  // set_dyn_pyhome(&casgi);
  PyObject *py_dict = PyDict_New();

  printf("going to python\n");
  // Loop through each agi_pair and add it to the dictionary
  for (int i = 0; i < agi_header->env_lines; i++) {
    PyObject *py_value = PyUnicode_FromString(agi_header->env[i].value);
    PyDict_SetItemString(py_dict, agi_header->env[i].key, py_value);
    Py_DECREF(py_value);
  }

  printf("really going to python, tuplepack\n");
  PyObject *args = PyTuple_Pack(2, py_dict, asgi_fputs);
  printf("before python_call \n");
  python_call(callable, args);
  printf("after python_call \n");
  Py_DECREF(py_dict);
  Py_DECREF(args);
  return 0;
}

PyObject *python_call(PyObject *callable, PyObject *args) {

  PyObject *pyret;

  pyret = PyObject_Call(callable, args, NULL);

  if (pyret) {
    Py_DECREF(pyret);
  }

  if (PyErr_Occurred()) {
    PyErr_Print();
  }

  return pyret;
}
