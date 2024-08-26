#include "asgi.h"

#define AGI_READ_CHUNK 256

extern struct casgi_server casgi;

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

int wsgi_req_accept(int fd, struct wsgi_request *wsgi_req) {
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

int casgi_parse_response(struct pollfd *upoll, int timeout, char **buff) {
  int rlen, i, total_bytes_read = 0;
  size_t bytes_read, buffer_size = AGI_READ_CHUNK;

  *buff = (char *)malloc(buffer_size);
  if (*buff == NULL) {
    printf("Memory allocation error\n");
    return -1;
  }

  if (!timeout)
    timeout = 1;

  while (1) {
    rlen = poll(upoll, 1, timeout * 1000);
    if (rlen < 0) {
      printf("poll()\n");
      exit(1);
    } else if (rlen == 0) {
      printf("Poll timeout. No more data.\n");
      break;
    }
    printf("%d ready events\n", rlen);
    rlen = read(upoll->fd, *buff + total_bytes_read, AGI_READ_CHUNK);
    if (rlen <= 0) {
      if (rlen < 0) {
        printf("read() error\n");
      }
      break;
    }

    total_bytes_read += rlen;
    if (total_bytes_read + AGI_READ_CHUNK > buffer_size) {
      buffer_size *= 2;
      char *new_buff = (char *)realloc(*buff, buffer_size);
      if (new_buff == NULL) {
        printf("Memory reallocation error\n");
        free(*buff);
        return -1;
      }
      *buff = new_buff;
    }

    // AGI send blank line (\n\n) to indicate end of request data
    if (total_bytes_read >= 2 && (*buff)[total_bytes_read - 1] == '\n' &&
        (*buff)[total_bytes_read - 2] == '\n') {
      break;
    }
  }

  (*buff)[total_bytes_read] = '\0';
  printf("Total bytes read: %d\n", total_bytes_read);
  return total_bytes_read; // Return the total number of bytes read
}

int wsgi_req_recv(struct wsgi_request *wsgi_req) {
  printf("received request in worker: %d\n", casgi.mywid);
  wsgi_req->poll.events = POLLIN;
  if (!casgi_parse_response(&wsgi_req->poll, 4, &wsgi_req->buffer)) {
    return -1;
  }

  printf("Received: %s\n", wsgi_req->buffer);
  python_call_asgi(casgi.workers[casgi.mywid].app->asgi_callable);
  // enter harakiri mode
  // wsgi_req->async_status =
  //     (*uwsgi.shared->hooks[wsgi_req->uh.modifier1])(&uwsgi, wsgi_req);
  //
  return 0;
}
