
#define CASGI_VERSION "0.0.1-dev"

#include <stdio.h>
#define PY_SSIZE_T_CLEAN
#include <Python.h>

struct uwsgi_server {
  char *python_path[64];
  int python_path_cnt;
  int has_threads;
};
