#include "asgi.h"

extern struct casgi_server casgi;
volatile int terminate_thread;

struct asgi_request *current_asgi_req(struct casgi_server *casgi) {

  struct asgi_request *asgi_req = casgi->wsgi_req;
  return asgi_req;
}

int bind_to_tcp(int listen_queue, char *tcp_port) {

  printf("listening 1...\n");
  int serverfd;
  struct addrinfo hints, *res;
  int reuse = 1;

  //   // tcp_port[0] = 0;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  if (getaddrinfo(NULL, tcp_port, &hints, &res)) {
    printf("[ERROR] getaddrinfo()");
    exit(1);
  }

  serverfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (serverfd < 0) {
    printf("socket()");
    exit(1);
  }

  if (setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&reuse,
                 sizeof(int)) < 0) {
    printf("setsockopt()");
    exit(1);
  }

  set_nonblocking(serverfd);

  if (bind(serverfd, res->ai_addr, res->ai_addrlen) != 0) {
    printf("bind()");
    exit(1);
  }

  if (listen(serverfd, listen_queue) != 0) {
    printf("listen()");
    exit(1);
  }
  printf("master accepting connections on serverfd=%d\n", serverfd);
  return serverfd;
}

int wsgi_req_accept(int fd, struct asgi_request *wsgi_req) {
  printf("worker %d preparing to accept connections. (serverfd: %d)\n",
         casgi.mywid, fd);
  wsgi_req->poll.fd = accept(fd, (struct sockaddr *)&wsgi_req->c_addr,
                             (socklen_t *)&wsgi_req->c_len);

  if (wsgi_req->poll.fd < 0) {
    printf("worker %d accept(). error=%s\n", casgi.mywid,
           strerror(wsgi_req->poll.fd));
    return -1;
  }
  printf("worker %d ready to accept connections. clientfd=%d (serverfd: %d)\n",
         casgi.mywid, wsgi_req->poll.fd, fd);

  return 0;
}

int casgi_parse_response(struct pollfd *upoll, int timeout, char *buff) {
  int rlen, rlen2, total_bytes_read = 0;

  if (!timeout)
    timeout = 1;

  while (1) {
    int fullpkt = 0;
    rlen = poll(upoll, 1, timeout * 1000);
    if (rlen < 0) {
      printf("poll()\n");
      exit(1);
    } else if (rlen == 0) {
      printf("Poll timeout. No more data.\n");
      break;
    }
    printf("%d ready events\n", rlen);
    while (total_bytes_read < 1024) {
      printf("gona read\n");
      rlen2 = read(upoll->fd, buff + total_bytes_read, AGI_READ_CHUNK);
      if (rlen2 <= 0) {
        if (rlen2 < 0) {
          printf("read() error\n");
          free(buff);
        }
        break;
      }
      total_bytes_read += rlen2;
      if (total_bytes_read >= 2 && buff[total_bytes_read - 1] == '\n' &&
          buff[total_bytes_read - 2] == '\n') {
        fullpkt = 1;
        break;
      }
    }
    if (fullpkt == 1) {
      break;
    }
  }

  buff[total_bytes_read] = '\0';
  printf("Total bytes read: %d\n", total_bytes_read);
  return total_bytes_read;
}

int get_asgi_response(int fd, char *buff) {
  int rlen, total_bytes_read = 0;
  while (total_bytes_read < 1024) {
    printf("(worker %d) reading %d bytesfrom socket... \n", casgi.mywid,
           AGI_READ_CHUNK);
    rlen = read(fd, buff + total_bytes_read, AGI_READ_CHUNK);
    if (rlen <= 0) {
      if (rlen < 0) {
        printf("read() error\n");
        return -1;
      }
      return -1;
    }
    total_bytes_read += rlen;
    if (total_bytes_read >= 2 && buff[total_bytes_read - 1] == '\n' &&
        buff[total_bytes_read - 2] == '\n') {
      break;
    }
  }

  buff[total_bytes_read] = '\0';
  printf("(worker %d) total bytes read: %d\n", casgi.mywid, total_bytes_read);
  return total_bytes_read;
}

int get_asgi_line(int fd, char *buff) {
  int rlen, total_bytes_read = 0;
  while (total_bytes_read < 1024) {
    printf("(worker %d) [agi] reading %d bytes from fd=%d.. \n", casgi.mywid,
           AGI_READ_CHUNK, fd);
    rlen = read(fd, buff + total_bytes_read, AGI_READ_CHUNK);
    if (rlen <= 0) {
      if (rlen < 0) {
        perror("[agi] read() error\n");
        return -1;
      }
      return -1;
    }
    total_bytes_read += rlen;
    if (total_bytes_read >= 1 && buff[total_bytes_read - 1] == '\n') {
      break;
    }
  }

  buff[total_bytes_read] = '\0';
  printf("(worker %d) [agi] total bytes read: %d\n", casgi.mywid,
         total_bytes_read);
  return total_bytes_read;
}

int send_asgi_line(int fd, char *buff) {
  int rlen, total_bytes_sent = 0;
  while (total_bytes_sent < strlen(buff)) {
    printf("(worker %d) [agi] writing %lu bytes to fd=%d.. \n", casgi.mywid,
           strlen(buff), fd);
    rlen = write(fd, buff, strlen(buff));
    if (rlen <= 0) {
      if (rlen < 0) {
        perror("[agi] write() error\n");
        return -1;
      }
      return -1;
    }
    total_bytes_sent += rlen;
  }

  buff[total_bytes_sent] = '\0';
  printf("(worker %d) [agi] total bytes sent: %d\n", casgi.mywid,
         total_bytes_sent);
  return total_bytes_sent;
}

int wsgi_req_recv(struct asgi_request *wsgi_req) {
  printf("received request in worker: %d\n", casgi.mywid);
  wsgi_req->poll.events = POLLIN;
  wsgi_req->app_id = casgi.mywid;
  wsgi_req->buffer = malloc(casgi.buffer_size);
  if (!wsgi_req->buffer) {
    printf("malloc() wsgi_req\n");
    exit(1);
  }
  // memset(wsgi_req->buffer, 0, casgi.buffer_size);
  if (!casgi_parse_response(&wsgi_req->poll, 4, wsgi_req->buffer)) {
    return -1;
  }

  struct agi_header agi_header;
  // memset(&agi_header, 0, sizeof(struct agi_header));
  agi_header.env = malloc(sizeof(struct agi_pair) * 30);
  if (!agi_header.env) {
    printf("malloc() agi_header\n");
    exit(1);
  }
  parse_agi_data(wsgi_req->buffer, &agi_header);
  // free(wsgi_req->buffer);
  // printf("parsed AGI data. total lines=%d. first item: key=%s, value=%s\n",
  //        agi_header.env_lines, agi_header.env[0].key,
  //        agi_header.env[0].value);
  // python_call_asgi(casgi.workers[casgi.mywid].app->asgi_callable,
  // &agi_header);
  python_request_handler(casgi.workers[casgi.mywid].app, &agi_header);
  free(wsgi_req->buffer);
  return 0;
}

void *handle_request_thread(void *arg) {
  worker_data_t *data = (worker_data_t *)arg;

  int fd = data->client_fd;
  printf("(worker %d) handing request\n", casgi.mywid);
  while (terminate_thread == 0) {

    char *buffer = malloc(casgi.buffer_size);
    if (!buffer) {
      printf("malloc() wsgi_req\n");
      exit(1);
    }
    // memset(wsgi_req->buffer, 0, casgi.buffer_size);
    if (!get_asgi_response(fd, buffer)) {
      free(buffer);
      // return -1;
    }
    printf("buff=%s\n", buffer);
    if (strstr(buffer, "HANGUP") != NULL) {
      printf("Hanging up the call.\n");
      break;
    }
    printf("(worker %d) got response: %s\n", casgi.mywid, buffer);

    struct agi_header agi_header;
    // memset(&agi_header, 0, sizeof(struct agi_header));
    agi_header.env = malloc(sizeof(struct agi_pair) * 30);
    if (!agi_header.env) {
      printf("malloc() agi_header\n");
      free(buffer);
      exit(1);
    }
    parse_agi_data(buffer, &agi_header);
    python_request_handler_v2(&agi_header);
    free(buffer);
    printf("handing next call \n");
    sleep(1);
  }
  // return 0;
}

int handle_request_main(int fd) {
  printf("(worker %d) handing request\n", casgi.mywid);
  char *buffer = malloc(casgi.buffer_size);
  if (!buffer) {
    printf("malloc() wsgi_req\n");
    exit(1);
  }
  // memset(wsgi_req->buffer, 0, casgi.buffer_size);
  if (!get_asgi_response(fd, buffer)) {
    free(buffer);
    return -1;
  }
  printf("(worker %d) got response: %s\n", casgi.mywid, buffer);

  struct agi_header agi_header;
  // memset(&agi_header, 0, sizeof(struct agi_header));
  agi_header.env = malloc(sizeof(struct agi_pair) * 30);
  if (!agi_header.env) {
    printf("malloc() agi_header\n");
    free(buffer);
    exit(1);
  }
  parse_agi_data(buffer, &agi_header);
  python_request_handler(casgi.workers[casgi.mywid].app, &agi_header);
  free(buffer);
  return 0;
}

int handle_request(int fd) {
  worker_data_t *data = malloc(sizeof(worker_data_t));
  data->client_fd = fd;

  terminate_thread = 0;
  pthread_t worker_thread;
  pthread_create(&worker_thread, NULL, handle_request_thread, (void *)data);

  // Detach the thread to automatically free resources when done
  pthread_detach(worker_thread);
  return 0;
}
