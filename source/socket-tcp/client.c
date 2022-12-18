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

  for (; args->count > 0; --args->count) {
    if (receive(sockfd, buffer, args->size, busy_waiting) < 0) {
      perror("receive()");
      exit(EXIT_FAILURE);
    }
    if (debug) {
      for (int i = 0; i < args->size; ++i) {
        if (((uint8_t *)buffer)[i] != 0x55) {
          fprintf(stderr, "Validation failed after receive()!\n");
          exit(EXIT_FAILURE);
        }
      }
    }

    memset(buffer, 0xAA, (unsigned)args->size);
    if (debug) {
      for (int i = 0; i < args->size; ++i) {
        if (((uint8_t *)buffer)[i] != 0xAA) {
          fprintf(stderr, "Validation failed after memset()!\n");
          exit(EXIT_FAILURE);
        }
      }
    }

    if (send(sockfd, buffer, args->size, 0) < 0) {
      perror("send()");
      exit(EXIT_FAILURE);
    }
  }

  free(buffer);
}

int main(int argc, char *argv[]) {
  if (argc != 9) {
    fprintf(stderr,
            "usage: %s SERVER_IP SERVER_PORT RCVBUF_SIZE SNDBUF_SIZE COUNT "
            "SIZE NONBLOCK DEBUG\n",
            argv[0]);
    exit(EXIT_FAILURE);
  }
  const char *server_ip = argv[1];
  uint16_t server_port = atoi(argv[2]);
  size_t rcvbuf_size = atoi(argv[3]);
  size_t sndbuf_size = atoi(argv[4]);
  size_t count = atoi(argv[5]);
  size_t size = atoi(argv[6]);
  int busy_waiting = atoi(argv[7]);
  int debug = atoi(argv[8]);

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
  server_addr.sin_addr.s_addr = inet_addr(server_ip);
  server_addr.sin_port = htons(server_port);
  if (connect(sockfd, (const struct sockaddr *)&server_addr,
              sizeof(server_addr)) < 0) {
    perror("connect()");
    exit(EXIT_FAILURE);
  }

  communicate(sockfd, &args, busy_waiting, debug);

  if (close(sockfd)) {
    perror("close()");
    exit(EXIT_FAILURE);
  }

  return EXIT_SUCCESS;
}
