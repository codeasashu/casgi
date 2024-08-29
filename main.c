#include "asgi.h"

#define check_interval 1

struct casgi_server casgi;
// struct casgi_app *app;

void load_config() {
  asgi_config *config = read_config("config.json");
  if (!config) {
    printf("Failed to load configuration.\n");
    exit(1);
  }
  casgi.config = config;
}

void warn_pipe() {
  printf("SIGPIPE: writing to a closed pipe/socket/fd !!!\n");
}

void gracefully_kill() {
  // int workerpid = casgi.workers[casgi.mywid].pid;
  printf("Gracefully killing worker %d...\n", casgi.mywid);
  casgi.workers[casgi.mywid].manage_next_request = 0;
  reload_me();
  // reload_me();
}

void reload_me() {
  printf("Reload killing worker %d...\n", casgi.mywid);
  exit(17);
}

void end_me() {
  printf("Ending killing worker %d...\n", casgi.mywid);
  exit(30);
}

void kill_them_all() {
  int i;
  printf("SIGINT/SIGQUIT received...killing workers...\n");
  for (i = 0; i < 2; i++) {
    kill(casgi.workers[i].pid, SIGKILL);
  }
  kill(getpid(), SIGKILL);
}

void grace_them_all() {
  int i;
  printf("...gracefully killing workers...\n");
  for (i = 0; i < 2; i++) {
    kill(casgi.workers[i].pid, SIGHUP);
  }
  kill(getpid(), SIGHUP);
}

void reap_them_all() {
  int i;
  printf("...brutally killing workers...\n");
  for (i = 0; i < 2; i++) {
    kill(casgi.workers[i].pid, SIGTERM);
  }
  kill(getpid(), SIGTERM);
}

int main(int argc, char *argv[]) {
  int epoll_fd;
  struct epoll_event ev, events[10];
  pid_t pid, masterpid;
  char *tcp_port;

  masterpid = getpid();
  memset(&casgi, 0, sizeof(struct casgi_server));
  casgi.pid = masterpid;
  casgi.buffer_size = 4096;

  // 4 workers
  casgi.workers = malloc(sizeof(struct casgi_worker) * 2);
  if (!casgi.workers) {
    perror("Failed to allocate memory for workers");
    exit(1);
  }
  memset(casgi.workers, 0, sizeof(struct casgi_worker) * 2);

  casgi.wsgi_requests = malloc(sizeof(struct asgi_request) * 1);
  if (casgi.wsgi_requests == NULL) {
    printf("unable to allocate memory for requests.\n");
    exit(1);
  }
  memset(casgi.wsgi_requests, 0, sizeof(struct asgi_request) * 1);
  // by default set wsgi_req to the first slot
  casgi.wsgi_req = casgi.wsgi_requests;
  load_config();

  printf("master (pid: %d) ready. booting workers...\n", masterpid);
  if (casgi.config->socket_name == NULL) {
    tcp_port = "9125";
    casgi.serverfd = bind_to_tcp(64, tcp_port);
  } else {
    tcp_port = strchr(casgi.config->socket_name, ':');
    if (tcp_port == NULL) {
      tcp_port = "9125";
      printf("[WARN] socket_name not provided. defaulting to %s \n", tcp_port);
    } else {
      tcp_port += 1;
    }
    printf("tcp port got here: %s\n", tcp_port);
    casgi.serverfd = bind_to_tcp(64, tcp_port);
  }
  printf("\t master listening on tcp port=%s\n", tcp_port);

  epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) {
      perror("epoll_create1");
      close(casgi.serverfd);
      exit(EXIT_FAILURE);
  }

  ev.events = EPOLLIN;
  ev.data.fd = casgi.serverfd;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, casgi.serverfd, &ev) == -1) {
      perror("epoll_ctl: listener_fd");
      close(casgi.serverfd);
      exit(1);
  }

  casgi.epollfd = epoll_fd;

  if (!Py_IsInitialized()) {
        Py_Initialize();

        // Initialize thread support (required for multi-threading with Python)
        PyEval_InitThreads();  // This will acquire the GIL initially
        PyEval_SaveThread();   // Release the GIL and allow other threads to run
  }

  for (int i = 0; i < 1; i++) {
    pid = fork();
    if (pid == 0) {
      casgi.workers[i].pid = getpid();
      casgi.workers[i].id = i;
      casgi.workers[i].manage_next_request = 1;
      // casgi.workers[i].app = uwsgi_wsgi_file_config(&casgi, i);
      casgi.mywid = i;
      printf("worker%d (pid: %d) booted\n", i, casgi.workers[i].pid);
      break;
    } else if (pid < 1) {
      casgi_error("fork()");
      exit(1);
    } else {
      printf("spawned uWSGI worker %d (pid: %d)\n", i, pid);
    }
  }

  if (getpid() == masterpid) {
    printf("running in master\n");
    signal(SIGHUP, (void *)&grace_them_all);
    signal(SIGTERM, (void *)&reap_them_all);
    signal(SIGINT, (void *)&kill_them_all);
    signal(SIGQUIT, (void *)&kill_them_all);
    /* used only to avoid human-errors */
    // signal(SIGUSR1, (void *)&stats);
    // master
    for (;;) {
      printf("inside master. sleeping every %d seconds \n", check_interval);
      sleep(check_interval);
    }
    // } else {
    // inside worker
    // printf("inside worker %d\n", casgi.mywid);
    // python_call_asgi(casgi.workers[casgi.mywid].app->asgi_callable);
  }

  // worker signal handlers
  signal(SIGHUP, (void *)&gracefully_kill);
  /* close the process (useful for master INT) */
  signal(SIGINT, (void *)&end_me);
  /* brutally reload */
  signal(SIGTERM, (void *)&reload_me);
  signal(SIGPIPE, (void *)&warn_pipe);

  // if (wsgi_req_accept(casgi.serverfd, casgi.wsgi_req)) {
  //   printf("accepting()");
  // }
  while (casgi.workers[casgi.mywid].manage_next_request) {
    // wsgi_req_setup(uwsgi.wsgi_req, 0);
        printf("(worker %d): before epoll_wait on fd=%d\n", casgi.mywid, casgi.epollfd);
        int n = epoll_wait(casgi.epollfd, events, 10, -1);
        if (n == -1) {
            perror("epoll_wait");
            close(casgi.serverfd);
            exit(1);
        }
        printf("(worker %d): after epoll_wait on fd=%d\n", casgi.mywid, casgi.epollfd);

        for (int i = 0; i < n; i++) {
	    if (events[i].data.fd == casgi.serverfd) {
                printf("(worker %d): event matched on fd=%d\n", casgi.mywid, casgi.serverfd);
		int client_fd = accept(casgi.serverfd, NULL, NULL);
		if (client_fd == -1) {
                    perror("accept");
                    printf("(worker %d) accept\n", casgi.mywid);
                } else {
		    set_nonblocking(client_fd);
                    ev.events = EPOLLIN | EPOLLET; // Use Edge-Triggered (ET) mode for non-blocking
                    ev.data.fd = client_fd;
                    if (epoll_ctl(casgi.epollfd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
                        perror("epoll_ctl: client_fd");
                        close(client_fd);
                    }
                    printf("(worker %d) accepted new client on fd=%d\n", casgi.mywid, client_fd);
		    casgi.wsgi_req->epoll_fd = client_fd;
		    handle_request(client_fd);
		}
             } else {
		printf("(worker %d) handing the client. clientfd=%d\n", casgi.mywid, events[i].data.fd);
		// handle_request(casgi.wsgi_req->epoll_fd);
	    }
	}
        printf("(worker %d) moving to next while loop\n", casgi.mywid);
        sleep(1);

    // if (wsgi_req_accept(casgi.serverfd, casgi.wsgi_req)) {
    //   sleep(1);
    //   continue;
    // }

    // if (wsgi_req_recv(casgi.wsgi_req)) {
    //   sleep(1);
    //   continue;
    // }
    // uwsgi_close_request(&uwsgi, uwsgi.wsgi_req);
  }

  free(casgi.workers);
  Py_Finalize();
  //
  // Initialize the Python interpreter
  return 0;
}
