#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "common/common.h"
#include "common/ivshmem.h"
#include "common/sockets.h"

void communicate(int sockfd, void *shared_memory, struct SocketArgs *args,
                 struct sockaddr_in *server_addr) {
  socklen_t sock_len = sizeof(*server_addr);

  void *buffer = malloc(args->size);
  if (!buffer) {
    perror("malloc()");
    exit(EXIT_FAILURE);
  }

  int ret;
  uint8_t dummy_message = 0x00;
  for (; args->count > 0; --args->count) {
    do
      if ((ret = recvfrom(sockfd, &dummy_message, sizeof(dummy_message),
                          args->wait_all ? MSG_WAITALL : 0,
                          (struct sockaddr *)server_addr, &sock_len)) < 0)
        if ((!args->is_nonblock || (errno != EAGAIN)) && (errno != EINTR)) {
          perror("recvfrom()");
          exit(EXIT_FAILURE);
        }
    while (ret <= 0);
    memcpy(buffer, shared_memory, args->size);
    if (args->is_debug) {
      for (int i = 0; i < args->size; ++i) {
        if (((uint8_t *)buffer)[i] != 0x55) {
          fprintf(stderr, "Validation failed after memcpy()!\n");
          exit(EXIT_FAILURE);
        }
      }
    }

    memset(shared_memory, 0xAA, args->size);
    if (args->is_debug) {
      for (int i = 0; i < args->size; ++i) {
        if (((uint8_t *)shared_memory)[i] != 0xAA) {
          fprintf(stderr, "Validation failed after memset()!\n");
          exit(EXIT_FAILURE);
        }
      }
    }
    do
      if ((ret = sendto(sockfd, &dummy_message, sizeof(dummy_message), 0,
                        (const struct sockaddr *)server_addr, sock_len)) < 0)
        if ((!args->is_nonblock || (errno != EAGAIN)) && (errno != EINTR)) {
          perror("sendto()");
          exit(EXIT_FAILURE);
        }
    while (ret <= 0);
  }

  free(buffer);
}

int main(int argc, char *argv[]) {
  struct SocketArgs args;
  socket_parse_args(&args, argc, argv);

  if (args.shmem_index == -1) {
    fprintf(stderr, "No -i option set; Use 0 as the shared memory index\n");
    args.shmem_index = 0;
  }

  void *passed_memory;
  if (args.shmem_backend) {
    /* For compatibiliy, assume it is IVSHMEM backend */
    int ivshmem_memfd = open(args.shmem_backend, O_RDWR | O_ASYNC | O_NONBLOCK);
    if (ivshmem_memfd < 0) {
      perror("open(ivshmem_memfd)");
      exit(EXIT_FAILURE);
    }

    struct stat st;
    off_t ivshmem_mmap_offset = 0;
    if (stat(args.shmem_backend, &st)) {
      perror("stat()");
      exit(EXIT_FAILURE);
    }
    size_t ivshmem_size = st.st_size;
    if (!ivshmem_size) {
      /* Try usernet_ivshmem's way */
      ivshmem_size = ioctl(ivshmem_memfd, IOCTL_GETSIZE, 0);
      if (ivshmem_size < 0) {
        perror("ioctl(IOCTL_GETSIZE)");
        exit(EXIT_FAILURE);
      }
      ivshmem_mmap_offset = USERNET_IVSHMEM_DEVM_START;
    }
    fprintf(stderr, "ivshmem_size == %lu\n", ivshmem_size);
    void *shared_memory = mmap(NULL, ivshmem_size, PROT_READ | PROT_WRITE,
                               MAP_SHARED, ivshmem_memfd, ivshmem_mmap_offset);
    if (shared_memory == MAP_FAILED) {
      perror("mmap()");
      exit(EXIT_FAILURE);
    }

    passed_memory =
        shared_memory + ivshmem_size - ((args.shmem_index + 1) * args.size);
  } else {
    fprintf(stderr,
            "No shared memory backend specified; Create anonymous one\n");
    key_t segment_key = ftok("shmem", args.shmem_index);

    int segment_id = shmget(segment_key, args.size, IPC_CREAT | 0666);
    if (segment_id < 0) {
      throw("Could not get segment");
    }

    passed_memory = (char *)shmat(segment_id, NULL, 0);
    if (passed_memory == MAP_FAILED) {
      throw("Could not attach segment");
    }
  }

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

  communicate(sockfd, passed_memory, &args, &server_addr);

  if (close(sockfd)) {
    perror("close()");
    exit(EXIT_FAILURE);
  }

  if (!args.shmem_backend) {
    shmdt(passed_memory);
  }

  return EXIT_SUCCESS;
}
