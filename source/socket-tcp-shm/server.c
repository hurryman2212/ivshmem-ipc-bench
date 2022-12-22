#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "common/common.h"

void communicate(int sockfd, void *shared_memory, struct Arguments *args,
                 int busy_waiting, int debug) {
  void *buffer = malloc(args->size);
  if (!buffer) {
    perror("malloc()");
    exit(EXIT_FAILURE);
  }

  struct Benchmarks bench;
  setup_benchmarks(&bench);

  int ret;
  uint8_t dummy_message = 0x00;
  for (int message = 0; message < args->count; ++message) {
    bench.single_start = now();

    memset(shared_memory, 0x55, args->size);
    if (debug) {
      for (int i = 0; i < args->size; ++i) {
        if (((uint8_t *)shared_memory)[i] != 0x55) {
          fprintf(stderr, "Validation failed after memset()!\n");
          exit(EXIT_FAILURE);
        }
      }
    }
    do {
      ret = send(sockfd, &dummy_message, sizeof(dummy_message), 0);
      if (ret < 0) {
        if ((busy_waiting && (errno == EAGAIN)) || (errno == EINTR))
          continue;
        else {
          perror("send()");
          exit(EXIT_FAILURE);
        }
      }
    } while (ret != sizeof(dummy_message));

    do {
      ret = recv(sockfd, &dummy_message, sizeof(dummy_message), 0);
      if (ret < 0) {
        if ((busy_waiting && (errno == EAGAIN)) || (errno == EINTR))
          continue;
        else {
          perror("recv()");
          exit(EXIT_FAILURE);
        }
      }
    } while (ret != sizeof(dummy_message));
    memcpy(buffer, shared_memory, args->size);
    if (debug) {
      for (int i = 0; i < args->size; ++i) {
        if (((uint8_t *)buffer)[i] != 0xAA) {
          fprintf(stderr, "Validation failed after memcpy()!\n");
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
  if (argc != 10) {
    fprintf(stderr,
            "usage: %s IVSHMEM_MEMPATH IVSHMEM_MEM_OFFSET SERVER_PORT "
            "RCVBUF_SIZE SNDBUF_SIZE COUNT SIZE NONBLOCK DEBUG\n",
            argv[0]);
    exit(EXIT_FAILURE);
  }
  const char *ivshmem_mempath = argv[1];
  size_t ivshmem_mem_offset = atoi(argv[2]);
  uint16_t server_port = atoi(argv[3]);
  size_t rcvbuf_size = atoi(argv[4]);
  size_t sndbuf_size = atoi(argv[5]);
  size_t count = atoi(argv[6]);
  size_t size = atoi(argv[7]);
  int busy_waiting = atoi(argv[9]);
  int debug = atoi(argv[9]);

  struct Arguments args;
  args.count = count;
  args.size = size;

  int ivshmem_memfd = open(ivshmem_mempath, O_RDWR | O_ASYNC | O_NONBLOCK);
  if (ivshmem_memfd < 0) {
    perror("open(ivshmem_memfd)");
    exit(EXIT_FAILURE);
  }

  struct stat st;
  if (stat(ivshmem_mempath, &st)) {
    perror("stat()");
    exit(EXIT_FAILURE);
  }
  size_t ivshmem_size = st.st_size;
  fprintf(stderr, "ivshmem_size == %lu\n", ivshmem_size);

  void *shared_memory = mmap(NULL, ivshmem_size, PROT_READ | PROT_WRITE,
                             MAP_SHARED, ivshmem_memfd, ivshmem_mem_offset);
  if (shared_memory == MAP_FAILED) {
    perror("mmap()");
    exit(EXIT_FAILURE);
  }
  void *passed_memory = shared_memory + ivshmem_size - args.size;

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
  int client_fd;
  do {
    client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &socklen);
    if (client_fd < 0) {
      if (((busy_waiting && (errno == EAGAIN)) || (errno == EINTR)))
        continue;
      else {
        perror("accept()");
        exit(EXIT_FAILURE);
      }
    }
  } while (client_fd < 0);

  communicate(client_fd, passed_memory, &args, busy_waiting, debug);

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
