#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#include "common/common.h"
#include "common/sockets.h"

void communicate(int sockfd, struct Arguments *args, int busy_waiting) {
  void *buffer = malloc(args->size);
  if (!buffer) {
    perror("malloc()");
    exit(EXIT_FAILURE);
  }

  struct Benchmarks bench;
  setup_benchmarks(&bench);

  for (int message = 0; message < args->count; ++message) {
    bench.single_start = now();

    memset(buffer, '*', args->size);

    if (send(sockfd, buffer, args->size, 0) < 0) {
      perror("send()");
      exit(EXIT_FAILURE);
    }

    if (receive(sockfd, buffer, args->size, busy_waiting) < 0) {
      perror("receive()");
      exit(EXIT_FAILURE);
    }

    benchmark(&bench);
  }

  evaluate(&bench, args);

  free(buffer);
}

int main(int argc, char *argv[]) {
  if (argc < 4) {
    fprintf(stderr, "usage: %s SERVER_PORT COUNT SIZE\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  uint16_t server_port = atoi(argv[1]);
  size_t count = atoi(argv[2]);
  size_t size = atoi(argv[3]);

  struct Arguments args;
  args.count = count;
  args.size = size;

  int busy_waiting = check_flag("busy", argc, argv);
  if (busy_waiting) {
    printf("busy_waiting = true");
  }

  int sockfd = socket(AF_INET, SOCK_STREAM, 0); // O_NONBLOCK?
  if (sockfd < 0) {
    perror("socket()");
    exit(EXIT_FAILURE);
  }

  int optval = 1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval))) {
    perror("bind()");
    exit(EXIT_FAILURE);
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

  communicate(client_fd, &args, busy_waiting);

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
