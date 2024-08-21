#include "asgi.h"

struct casgi_server casgi;

int main(int argc, char *argv[]) {
  pid_t pid;

  memset(&casgi, 0, sizeof(struct casgi_server));
  // 4 workers
  casgi.workers = malloc(sizeof(struct casgi_worker) * 4);
  if (!casgi.workers) {
    perror("Failed to allocate memory for workers");
    exit(1);
  }
  memset(casgi.workers, 0, sizeof(struct casgi_worker) * 4);

  asgi_config *config = read_config("config.json");
  if (!config) {
    printf("Failed to load configuration.\n");
    exit(1);
  }
  casgi.config = config;

  // master is always the first worker
  casgi.workers[0].pid = getpid();
  // casgi.workers[0].app = uwsgi_wsgi_file_config(&casgi, 0);

  // PyObject *pValue = PyTuple_New(0);
  // PyObject *pValue = PyUnicode_FromString("hello");
  // python_call_asgi(casgi.workers[0].app->asgi_callable);
  //
  // printf("initializing master... \n");
  //
  pid = fork();
  if (pid == 0) {
    casgi.workers[1].pid = pid;
    casgi.workers[1].id = 1;
    casgi.workers[1].app = uwsgi_wsgi_file_config(&casgi, 1);
  } else if (pid < 1) {
    casgi_error("fork()");
    exit(1);
  } else {
    printf("spawned uWSGI worker 1 (pid: %d)\n", pid);
  }
  free(casgi.workers);
  //
  // Initialize the Python interpreter
}
