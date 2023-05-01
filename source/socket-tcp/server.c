#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#include "common/common.h"
#include "common/sockets.h"

void communicate(int sockfd, struct SocketArgs *args, int busy_waiting,
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
    size_t this_size = args->size;
    int ret;
    do {
      ret = send(sockfd, buffer + (args->size - this_size), this_size, 0);
      if (ret < 0) {
        if ((busy_waiting && (errno == EAGAIN)) || (errno == EINTR))
          continue;
        else {
          perror("send()");
          exit(EXIT_FAILURE);
        }
      }
      this_size -= ret;
    } while (this_size);

    this_size = args->size;
    do {
      ret = recv(sockfd, buffer + (args->size - this_size), this_size, 0);
      if (ret < 0) {
        if ((busy_waiting && (errno == EAGAIN)) || (errno == EINTR))
          continue;
        else {
          perror("recv()");
          exit(EXIT_FAILURE);
        }
      }
      this_size -= ret;
    } while (this_size);
    if (debug) {
      for (int i = 0; i < args->size; ++i) {
        if (((uint8_t *)buffer)[i] != 0xAA) {
          fprintf(stderr, "Validation failed after recv()!\n");
          exit(EXIT_FAILURE);
        }
      }
    }

    benchmark(&bench);
  }

  struct Arguments tmp_arg;
  tmp_arg.count = args->count;
  tmp_arg.size = args->size;
  evaluate(&bench, &tmp_arg);

  free(buffer);
}

int main(int argc, char *argv[]) {
  struct SocketArgs args;
  socket_parse_args(&args, argc, argv);

  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
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

  if (args.rcvbuf_size != -1) {
    optval = args.rcvbuf_size;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &optval, optlen)) {
      perror("setsockopt(SO_RCVBUF)");
      exit(EXIT_FAILURE);
    }
    if (getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &optval, &optlen)) {
      perror("getsockopt(SO_RCVBUF)");
      exit(EXIT_FAILURE);
    }
    fprintf(stderr, "SO_RCVBUF = %d\n", optval);
  }
  if (args.sndbuf_size != -1) {
    optval = args.sndbuf_size;
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &optval, optlen)) {
      perror("setsockopt(SO_SNDBUF)");
      exit(EXIT_FAILURE);
    }
    if (getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &optval, &optlen)) {
      perror("getsockopt(SO_SNDBUF)");
      exit(EXIT_FAILURE);
    }
    fprintf(stderr, "SO_SNDBUF = %d\n", optval);
  }

  if (args.is_nonblock) {
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
  server_addr.sin_port = htons(args.server_port);

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
  int client_fd;
  do {
    client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &socklen);
    if (client_fd < 0) {
      if (((args.is_nonblock && (errno == EAGAIN)) || (errno == EINTR)))
        continue;
      else {
        perror("accept()");
        exit(EXIT_FAILURE);
      }
    }
  } while (client_fd < 0);

  communicate(client_fd, &args, args.is_nonblock, args.is_debug);

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
