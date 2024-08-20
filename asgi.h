#define PY_SSIZE_T_CLEAN

#define CASGI_VERSION "0.0.1-dev"

#include <stdio.h>
#include "cJSON.h"
#include <Python.h>

typedef struct asgi_config {
  char app_path[256];
} asgi_config;

struct casgi_server {
  char *python_path[64];
  int python_path_cnt;
  int has_threads;
  struct asgi_config *config;
};
