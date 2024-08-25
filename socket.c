#include "asgi.h"

int bind_to_tcp(int listen_queue, char *tcp_port) {

  printf("listening 1...\n");
  int serverfd;
  struct addrinfo hints, *res;
  int reuse = 1;

  //   // tcp_port[0] = 0;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
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

int wsgi_req_accept(int workerid, int fd, struct wsgi_request *wsgi_req) {

  printf("worker %d preparing to accept connections. (serverfd: %d)\n",
         workerid, fd);
  wsgi_req->poll.fd = accept(fd, (struct sockaddr *)&wsgi_req->c_addr,
                             (socklen_t *)&wsgi_req->c_len);

  if (wsgi_req->poll.fd < 0) {
    printf("worker %d accept(). error=%s\n", workerid,
           strerror(wsgi_req->poll.fd));
    return -1;
  }
  printf("worker %d ready to accept connections. clientfd=%d (serverfd: %d)\n",
         workerid, wsgi_req->poll.fd, fd);

  return 0;
}

int casgi_parse_response(struct pollfd *upoll, int timeout, char *buff) {
  int rlen, i;
  size_t bytes_read;

  if (!timeout)
    timeout = 1;

  printf("reading socket=%d, timeout=%ds\n", upoll->fd, timeout);
  /* first 4 byte header */
  rlen = poll(upoll, 1, timeout * 1000);
  if (rlen < 0) {
    printf("poll()\n");
    exit(1);
  } else if (rlen == 0) {
    printf("timeout. skip request\n");
    close(upoll->fd);
    return 0;
  }
  printf("%d ready events\n", rlen);

  rlen = read(upoll->fd, buff, sizeof(buff) + 1);
  if (rlen > 0 && rlen < (sizeof(buff) + 1)) {
    i = rlen;
    while (i < (sizeof(buff) + 1)) {
      rlen = poll(upoll, 1, timeout * 1000);
      if (rlen < 0) {
        printf("poll()");
        exit(1);
      } else if (rlen == 0) {
        printf("timeout waiting for header. skip request.\n");
        close(upoll->fd);
        break;
      }
      rlen = read(upoll->fd, (char *)(buff) + i, sizeof(buff) + 1 - i);
      if (rlen <= 0) {
        printf("broken header. skip request.\n");
        close(upoll->fd);
        break;
      }
      i += rlen;
    }
    if (i < (sizeof(buff) + 1)) {
      return 0;
    }
  } else if (rlen <= 0) {
    printf("invalid request header size: %d...skip\n", rlen);
    close(upoll->fd);
    return 0;
  }
  return 1;
}

int wsgi_req_recv(struct wsgi_request *wsgi_req) {
  wsgi_req->poll.events = POLLIN;
  if (!casgi_parse_response(&wsgi_req->poll, 4, wsgi_req->buffer)) {
    return -1;
  }

  printf("Received: %s\n", wsgi_req->buffer);

  // enter harakiri mode
  // wsgi_req->async_status =
  //     (*uwsgi.shared->hooks[wsgi_req->uh.modifier1])(&uwsgi, wsgi_req);
  //
  return 0;
}
