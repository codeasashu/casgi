#include "asgi.h"
#include "pylifecycle.h"

extern struct casgi_server casgi;

PyObject *method_fputs(PyObject *self, PyObject *args) {
  struct asgi_request *asgi_req = current_asgi_req(&casgi);

  char *str, *filename = NULL;
  char response[100] = "ashutosh";

  /* Parse arguments */
  if (!PyArg_ParseTuple(args, "s", &str)) {
    return NULL;
  }

  // FILE *fp = fopen(filename, "w");
  // bytes_copied = fputs(str, fp);
  printf("received cmd=%s (pid=%d), app_id=%d \n", str, getpid(),
         asgi_req->app_id);
  write(asgi_req->poll.fd, str, strlen(str));
  printf("written cmd=%s (pid=%d), app_id=%d \n", str, getpid(),
         asgi_req->app_id);
  // fclose(fp);

  return PyUnicode_FromString(response);
}

void init_paths(const char *mypath) {

  int i;
  PyObject *pysys, *pysys_dict, *pypath;

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
  if (PyList_Insert(pypath, 0, PyUnicode_FromString(".")) != 0) {
    PyErr_Print();
  }

  if (PyList_Insert(pypath, 0, PyUnicode_FromString(mypath)) != 0) {
    PyErr_Print();
  } else {
    printf("[INFO] adding %s to pythonpath.\n", mypath);
  }
}

struct casgi_app *init_casgi_app(PyObject *my_callable) {
  PyObject *wsgi_module, *wsgi_fputs, *wsgi_dict = NULL;
  int id;

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
  printf("initializing python module1.1... \n");
  PyObject *asgi_file_callable;

  Py_Initialize();

  init_paths(casgi->config->app_path);

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
  Py_Finalize();
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
  PyObject *callable = app->asgi_callable;
  PyObject *py_dict = PyDict_New();

  // Loop through each agi_pair and add it to the dictionary
  for (int i = 0; i < agi_header->env_lines; i++) {
    PyObject *py_value = PyUnicode_FromString(agi_header->env[i].value);
    PyDict_SetItemString(py_dict, agi_header->env[i].key, py_value);
    Py_DECREF(py_value);
  }

  PyObject *args = PyTuple_Pack(2, py_dict, app->asgi_fputs);
  python_call(callable, args);
  Py_DECREF(py_dict);
  Py_DECREF(args);
  return 0;
}

PyObject *python_call(PyObject *callable, PyObject *args) {

  PyObject *pyret;

  // PyObject *pValue = PyUnicode_FromString("hello");
  pyret = PyObject_Call(callable, args, NULL);
  if (PyErr_Occurred()) {
    PyErr_Print();
  }

  return pyret;
}
