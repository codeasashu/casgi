#include "asgi.h"
#include "pylifecycle.h"
// #include <cstdlib>

extern struct casgi_server casgi;

PyObject *get_casgi_pydict(char *module) {

  PyObject *wsgi_module, *wsgi_dict;

  wsgi_module = PyImport_ImportModule(module);
  if (!wsgi_module) {
    PyErr_Print();
    return NULL;
  }

  wsgi_dict = PyModule_GetDict(wsgi_module);
  if (!wsgi_dict) {
    PyErr_Print();
    return NULL;
  }

  return wsgi_dict;
}

void set_dyn_pyhome(struct casgi_server *casgi) {

  char app_path[255] = "/Users/ashutosh/code/office/engine-v4/";
  char venv_version[30];
  PyObject *site_module;

  PyObject *pysys_dict = get_casgi_pydict("sys");

  PyObject *pypath = PyDict_GetItemString(pysys_dict, "path");
  if (!pypath) {
    PyErr_Print();
    exit(1);
  }

  if (PyList_Insert(pypath, 0, PyUnicode_FromString(app_path))) {
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
  char *buff;
  buff = malloc(casgi.buffer_size);

  struct asgi_request *asgi_req = current_asgi_req(&casgi);

  char *str;
  char response[100] = "ashutosh";

  /* Parse arguments */
  if (!PyArg_ParseTuple(args, "s", &str)) {
    return NULL;
  }

  printf("received cmd=%s (pid=%d), app_id=%d \n", str, getpid(),
         asgi_req->app_id);
  write(asgi_req->poll.fd, str, strlen(str));
  printf("written cmd=%s (pid=%d), app_id=%d \n", str, getpid(),
         asgi_req->app_id);

  int bytes_read = casgi_get_response_line(&asgi_req->poll, buff);
  printf("buffer response read: %s, bytes=%d \n", buff, bytes_read);
  return PyUnicode_FromString(buff);
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

  Py_Initialize();
  app->interpreter = Py_NewInterpreter();
  if (!app->interpreter) {
    printf("unable to initialize the new interpreter\n");
    exit(1);
  }
  PyThreadState_Swap(app->interpreter);
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
  printf("initializing python module (app=%d)... \n", casgi->mywid);
  PyObject *asgi_file_callable;

  Py_Initialize();

  // init_paths(casgi->config->app_path);
  set_dyn_pyhome(casgi);

  printf("initializing python module: %s\n", casgi->config->module);
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

  // Loop through each agi_pair and add it to the dictionary
  for (int i = 0; i < agi_header->env_lines; i++) {
    PyObject *py_value = PyUnicode_FromString(agi_header->env[i].value);
    PyDict_SetItemString(py_dict, agi_header->env[i].key, py_value);
    Py_DECREF(py_value);
  }

  // setenv("PYTHONPATH",
  //        "/Users/ashutosh/code/office/engine-v4/venv/lib/"
  //        "python3.12/site-packages",
  //        1);
  // sprintf(python_cmd, "import sys; sys.path.insert(0, '%s')",
  //         casgi.config->app_path);
  // PyRun_SimpleString(python_cmd);
  PyObject *args = PyTuple_Pack(2, py_dict, app->asgi_fputs);
  python_call(callable, args);
  Py_DECREF(py_dict);
  Py_DECREF(args);
  return 0;
}

PyObject *python_call(PyObject *callable, PyObject *args) {

  PyObject *pyret;

  PyRun_SimpleString("import sys; print('Updated sys.path:', sys.path)");
  // PyRun_SimpleString("import sys; print('Updated2 sys.path:', sys.path)");
  // PyObject *pValue = PyUnicode_FromString("hello");
  pyret = PyObject_Call(callable, args, NULL);

  if (pyret) {
    Py_DECREF(pyret);
  }

  if (PyErr_Occurred()) {
    PyErr_Print();
  }

  return pyret;
}
