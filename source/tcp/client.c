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

  for (; args->count > 0; --args->count) {
    if (receive(sockfd, buffer, args->size, busy_waiting) < 0) {
      perror("receive()");
      exit(EXIT_FAILURE);
    }

    memset(buffer, '*', args->size);

    if (send(sockfd, buffer, args->size, 0) < 0) {
      perror("send()");
      exit(EXIT_FAILURE);
    }
  }

  free(buffer);
}

int main(int argc, char *argv[]) {
  if (argc < 5) {
    fprintf(stderr, "usage: %s SERVER_IP SERVER_PORT COUNT SIZE\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  const char *server_ip = argv[1];
  uint16_t server_port = atoi(argv[2]);
  size_t count = atoi(argv[3]);
  size_t size = atoi(argv[4]);

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

  struct sockaddr_in server_addr = {0};
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(server_ip);
  server_addr.sin_port = htons(server_port);
  if (connect(sockfd, (const struct sockaddr *)&server_addr,
              sizeof(server_addr)) < 0) {
    perror("connect()");
    exit(EXIT_FAILURE);
  }

  communicate(sockfd, &args, busy_waiting);

  if (close(sockfd)) {
    perror("close()");
    exit(EXIT_FAILURE);
  }

  return EXIT_SUCCESS;
}
