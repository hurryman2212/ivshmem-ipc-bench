#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include "common/common.h"
#include "common/sockets.h"

void communicate(int sockfd, struct SocketArgs *args,
                 struct sockaddr_in *server_addr) {
  socklen_t sock_len = sizeof(*server_addr);

  void *buffer = malloc(args->size);
  if (!buffer) {
    perror("malloc()");
    exit(EXIT_FAILURE);
  }

  for (; args->count > 0; --args->count) {

    size_t this_size = args->size;
    int ret;
    do {
      ret = recvfrom(sockfd, (char *)(buffer + (args->size - this_size)),
                     this_size, args->wait_all ? MSG_WAITALL : 0,
                     (struct sockaddr *)server_addr, &sock_len);
      if (ret < 0)
        if ((!args->is_nonblock || (errno != EAGAIN)) && (errno != EINTR)) {
          perror("recvfrom()");
          exit(EXIT_FAILURE);
        }
      this_size -= ret;
    } while (this_size);
    if (args->is_debug) {
      for (int i = 0; i < args->size; ++i) {
        if (((uint8_t *)buffer)[i] != 0x55) {
          fprintf(stderr, "Validation failed after recv()!\n");
          exit(EXIT_FAILURE);
        }
      }
    }

    memset(buffer, 0xAA, (unsigned)args->size);
    if (args->is_debug) {
      for (int i = 0; i < args->size; ++i) {
        if (((uint8_t *)buffer)[i] != 0xAA) {
          fprintf(stderr, "Validation failed after memset()!\n");
          exit(EXIT_FAILURE);
        }
      }
    }
    this_size = args->size;
    do {
      ret =
          sendto(sockfd, (const char *)(buffer + (args->size - this_size)),
                 this_size, 0, (const struct sockaddr *)server_addr, sock_len);
      if (ret < 0)
        if ((!args->is_nonblock || (errno != EAGAIN)) && (errno != EINTR)) {
          perror("sendto()");
          exit(EXIT_FAILURE);
        }
      this_size -= ret;
    } while (this_size);
  }

  free(buffer);
}

int main(int argc, char *argv[]) {
  struct SocketArgs args;
  socket_parse_args(&args, argc, argv);

  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
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

  /* Handshake */
  char tmp_buf = 's';
  struct sockaddr_in server_addr = {0};
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(args.server_addr);
  server_addr.sin_port = htons(args.server_port);
  socklen_t sock_len = sizeof(server_addr);
  while (sendto(sockfd, (const char *)&tmp_buf, 1, 0,
                (const struct sockaddr *)&server_addr, sock_len) != 1) {
    if ((args.is_nonblock && (errno == EAGAIN)) || (errno == EINTR))
      continue;
    else {
      perror("sendto() for Handshake");
      exit(EXIT_FAILURE);
    }
  }
  while (recvfrom(sockfd, (char *)&tmp_buf, 1, 0,
                  (struct sockaddr *)&server_addr, &sock_len) != 1) {
    if ((args.is_nonblock && (errno == EAGAIN)) || (errno == EINTR))
      continue;
    else {
      perror("recvfrom() for Handshake");
      exit(EXIT_FAILURE);
    }
  }
  if (tmp_buf != 'c') {
    fprintf(stderr, "Handshake failed!\n");
    exit(EXIT_FAILURE);
  }
  fprintf(stderr, "Handshake is done!\n");

  communicate(sockfd, &args, &server_addr);

  if (close(sockfd)) {
    perror("close()");
    exit(EXIT_FAILURE);
  }

  return EXIT_SUCCESS;
}
