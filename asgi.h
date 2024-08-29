#define PY_SSIZE_T_CLEAN
#define AGI_READ_CHUNK 256

#define casgi_error(x)                                                         \
  printf("%s: %s [%s line %d]\n", x, strerror(errno), __FILE__, __LINE__);
#define CASGI_VERSION "0.0.1-dev"

#include "agi.h"
#include "cJSON.h"
#include "netdb.h"
#include <Python.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>

typedef struct asgi_config {
  char app_path[256];
  char module[256];
  char *socket_name;
  char pyhome[256];
} asgi_config;

struct casgi_server {
  pid_t pid;
  char *pyhome;
  int serverfd;
  int epollfd;
  int mywid;
  int buffer_size;
  struct asgi_config *config;
  struct casgi_worker *workers;
  struct asgi_request *wsgi_requests;
  struct asgi_request *wsgi_req;
};

// Each worker is a python interpreter, running in fork mode
struct casgi_worker {
  int id;
  pid_t pid;
  int manage_next_request;
  struct casgi_app *app;
  uint64_t status;
  uint64_t requests;
  uint64_t failed_requests;
};

struct casgi_app {
  PyThreadState *interpreter;
  PyObject *asgi_callable;
  PyObject *asgi_fputs;
  PyObject *pymain_dict;
  int requests;
};

struct asgi_request {
  int app_id;

  struct pollfd poll;
  int epoll_fd;

  // Client info
  struct sockaddr_in c_addr;
  int c_len;

  struct timeval start_of_request;
  struct timeval end_of_request;

  char *buffer;
};

struct asgi_request *current_asgi_req(struct casgi_server *);

typedef struct {
    int client_fd;
} worker_data_t;

asgi_config *read_config(const char *filename);

void init_paths(const char *mypath);

struct casgi_app *uwsgi_wsgi_file_config(struct casgi_server *casgi,
                                         int workerid);

PyObject *python_call(PyObject *callable, PyObject *args);

int set_nonblocking(int);
void warn_pipe(void);
void goodbye_cruel_world(void);
void gracefully_kill(void);
void reap_them_all(void);
void kill_them_all(void);
void grace_them_all(void);
void reload_me(void);
void end_me(void);
int bind_to_tcp(int, char *);
int wsgi_req_accept(int, struct asgi_request *);
int wsgi_req_recv(struct asgi_request *);
int python_call_asgi(PyObject *, struct agi_header *);
int python_request_handler(struct casgi_app *, struct agi_header *);
int python_request_handler_v2(struct agi_header *);
int casgi_get_response_line(struct pollfd *, char *);
int handle_request(int);
int get_asgi_line(int fd, char *buff);
int send_asgi_line(int fd, char *buff);
PyObject *method_fputs(PyObject *, PyObject *);
