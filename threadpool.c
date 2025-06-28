#include "asgi.h"
#include <pthread.h>

typedef struct {
  pthread_t *threads;
  pthread_mutex_t mu;
  pthread_cond_t not_empty;
} threadpool;

int init(threadpool *tp, size_t num_threads, struct asgi_request *wsgi_req) {
  int rv = pthread_mutex_init(&tp->mu, NULL);
  if (rv != 0) {
    printf("error");
    return -1;
  }
  rv = pthread_cond_init(&tp->not_empty, NULL);
  if (rv != 0) {
    printf("error");
    return -1;
  }

  for (size_t i = 0; i < num_threads; ++i) {
    int rv = pthread_create(&tp->threads[i], NULL, NULL, tp);
  }
  return 0;
}
