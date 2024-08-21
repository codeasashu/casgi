#define PY_SSIZE_T_CLEAN

#define casgi_error(x)                                                         \
  printf("%s: %s [%s line %d]\n", x, strerror(errno), __FILE__, __LINE__);
#define CASGI_VERSION "0.0.1-dev"

#include "cJSON.h"
#include <Python.h>
#include <stdio.h>

typedef struct asgi_config {
  char app_path[256];
  char module[256];
} asgi_config;

struct casgi_server {
  pid_t pid;
  struct asgi_config *config;
  struct casgi_worker *workers;
};

// Each worker is a python interpreter, running in fork mode
struct casgi_worker {
  int id;
  pid_t pid;
  struct casgi_app *app;
  uint64_t status;
  uint64_t requests;
  uint64_t failed_requests;
};

struct casgi_app {
  PyThreadState *interpreter;
  PyObject *asgi_callable;
  PyObject *pymain_dict;
  int requests;
};

asgi_config *read_config(const char *filename);

void init_paths(const char *mypath);

struct casgi_app *uwsgi_wsgi_file_config(struct casgi_server *casgi,
                                         int workerid);

PyObject *python_call(PyObject *callable, PyObject *args);
int python_call_asgi(PyObject *callable);
