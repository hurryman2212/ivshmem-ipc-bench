#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#include "common/common.h"
#include "common/sockets.h"

void communicate(int sockfd, struct Arguments *args, int busy_waiting,
                 int debug) {
  void *buffer = malloc(args->size);
  if (!buffer) {
    perror("malloc()");
    exit(EXIT_FAILURE);
  }

  struct Benchmarks bench;
  setup_benchmarks(&bench);

  for (int message = 0; message < args->count; ++message) {
    bench.single_start = now();

    memset(buffer, 0x55, args->size);
    if (debug) {
      for (int i = 0; i < args->size; ++i) {
        if (((uint8_t *)buffer)[i] != 0x55) {
          fprintf(stderr, "Validation failed after memset()!\n");
          exit(EXIT_FAILURE);
        }
      }
    }

    if (send(sockfd, buffer, args->size, 0) < 0) {
      perror("send()");
      exit(EXIT_FAILURE);
    }

    if (receive(sockfd, buffer, args->size, busy_waiting) < 0) {
      perror("receive()");
      exit(EXIT_FAILURE);
    }
    if (debug) {
      for (int i = 0; i < args->size; ++i) {
        if (((uint8_t *)buffer)[i] != 0xAA) {
          fprintf(stderr, "Validation failed after receive()!\n");
          exit(EXIT_FAILURE);
        }
      }
    }

    benchmark(&bench);
  }

  evaluate(&bench, args);

  free(buffer);
}

int main(int argc, char *argv[]) {
  if (argc != 8) {
    fprintf(stderr,
            "usage: %s SERVER_PORT RCVBUF_SIZE SNDBUF_SIZE COUNT SIZE NONBLOCK "
            "DEBUG\n",
            argv[0]);
    exit(EXIT_FAILURE);
  }
  uint16_t server_port = atoi(argv[1]);
  size_t rcvbuf_size = atoi(argv[2]);
  size_t sndbuf_size = atoi(argv[3]);
  size_t count = atoi(argv[4]);
  size_t size = atoi(argv[5]);
  int busy_waiting = atoi(argv[6]);
  int debug = atoi(argv[7]);

  struct Arguments args;
  args.count = count;
  args.size = size;

  int sockfd = socket(AF_INET, SOCK_STREAM, 0); // O_NONBLOCK?
  if (sockfd < 0) {
    perror("socket()");
    exit(EXIT_FAILURE);
  }

  int optval;
  socklen_t optlen = sizeof(optval);

  optval = 1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &optval, optlen)) {
    perror("bind()");
    exit(EXIT_FAILURE);
  }

  if (getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &optval, &optlen)) {
    perror("getsockopt(SO_RCVBUF)");
    exit(EXIT_FAILURE);
  }
  fprintf(stderr, "(default) SO_RCVBUF == %d\n", optval);
  if (getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &optval, &optlen)) {
    perror("getsockopt(SO_SNDBUF)");
    exit(EXIT_FAILURE);
  }
  fprintf(stderr, "(default) SO_SNDBUF == %d\n", optval);

  optval = rcvbuf_size;
  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &optval, optlen)) {
    perror("setsockopt(SO_RCVBUF)");
    exit(EXIT_FAILURE);
  }
  if (getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &optval, &optlen)) {
    perror("getsockopt(SO_RCVBUF)");
    exit(EXIT_FAILURE);
  }
  fprintf(stderr, "SO_RCVBUF = %d\n", optval);
  optval = sndbuf_size;
  if (setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &optval, optlen)) {
    perror("setsockopt(SO_SNDBUF)");
    exit(EXIT_FAILURE);
  }
  if (getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &optval, &optlen)) {
    perror("getsockopt(SO_SNDBUF)");
    exit(EXIT_FAILURE);
  }
  fprintf(stderr, "SO_SNDBUF = %d\n", optval);

  if (busy_waiting) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
      perror("fcntl(F_GETFL)");
      exit(EXIT_FAILURE);
    }
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
      perror("fcntl(F_SETFL)");
      exit(EXIT_FAILURE);
    }
  }

  struct sockaddr_in server_addr = {0};
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(server_port);

  if ((bind(sockfd, (const struct sockaddr *)&server_addr,
            sizeof(server_addr))) != 0) {
    perror("bind()");
    exit(EXIT_FAILURE);
  }

  if ((listen(sockfd, 5)) != 0) {
    perror("listen()");
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in client_addr = {0};
  socklen_t socklen = sizeof(client_addr);
  int client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &socklen);
  if (client_fd < 0) {
    perror("accept()");
    exit(EXIT_FAILURE);
  }

  communicate(client_fd, &args, busy_waiting, debug);

  if (close(client_fd)) {
    perror("close()");
    exit(EXIT_FAILURE);
  }
  if (close(sockfd)) {
    perror("close()");
    exit(EXIT_FAILURE);
  }

  return EXIT_SUCCESS;
}
