#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/mman.h>

#include "common/common.h"
#include "common/ivshmem.h"

void cleanup(void *shared_memory, size_t size) {
  if (munmap(shared_memory, size)) {
    perror("munmap()");
    exit(EXIT_FAILURE);
  }
}

void communicate(int fd, void *shared_memory, struct Arguments *args,
                 int busy_waiting, uint16_t src_port, uint16_t dest_ivposition,
                 uint16_t dest_port, int debug) {
  void *buffer = malloc(args->size);
  if (!buffer) {
    perror("malloc()");
    exit(EXIT_FAILURE);
  }

  intr_notify(fd, busy_waiting, dest_ivposition, dest_port, debug);

  for (; args->count > 0; --args->count) {
    intr_wait(fd, busy_waiting, src_port, debug);

    memcpy(buffer, shared_memory, args->size);
    if (debug) {
      for (int i = 0; i < args->size; ++i) {
        if (((uint8_t *)buffer)[i] != 0x55) {
          fprintf(stderr, "Validation failed after memcpy()!\n");
          exit(EXIT_FAILURE);
        }
      }
    }

    memset(shared_memory, 0xAA, args->size);
    if (debug) {
      for (int i = 0; i < args->size; ++i) {
        if (((uint8_t *)shared_memory)[i] != 0xAA) {
          fprintf(stderr, "Validation failed after memset()!\n");
          exit(EXIT_FAILURE);
        }
      }
    }

    intr_notify(fd, busy_waiting, dest_ivposition, dest_port, debug);
  }

  if (ioctl(fd, IOCTL_CLEAR, 0)) {
    perror("ioctl(IOCTL_CLEAR)");
    exit(EXIT_FAILURE);
  }

  free(buffer);
}

int main(int argc, char *argv[]) {
  if (argc != 9) {
    fprintf(stderr,
            "usage: %s IVSHMEM_DEVPATH CLIENT_PORT SERVER_IVPOSITION "
            "SERVER_PORT COUNT SIZE NONBLOCK DEBUG\n",
            argv[0]);
    exit(EXIT_FAILURE);
  }
  const char *ivshmem_devpath = argv[1];
  uint16_t client_port = atoi(argv[2]);
  uint16_t server_ivposition = atoi(argv[3]);
  uint16_t server_port = atoi(argv[4]);
  size_t count = atoi(argv[5]);
  size_t size = atoi(argv[6]);
  int busy_waiting = atoi(argv[7]);
  int debug = atoi(argv[8]);

  struct Arguments args;
  args.count = count;
  args.size = size;
  int ivshmem_fd;

  if (busy_waiting)
    ivshmem_fd = open(ivshmem_devpath, O_RDWR | O_ASYNC | O_NONBLOCK);
  else
    ivshmem_fd = open(ivshmem_devpath, O_RDWR | O_ASYNC);
  if (ivshmem_fd < 0) {
    perror("open()");
    exit(EXIT_FAILURE);
  }

  size_t ivshmem_size = ioctl(ivshmem_fd, IOCTL_GETSIZE, 0);
  if (ivshmem_size < 0) {
    perror("ioctl(IOCTL_GETSIZE)");
    exit(EXIT_FAILURE);
  }
  fprintf(stderr, "ivshmem_size == %lu\n", ivshmem_size);

  void *shared_memory =
      mmap(NULL, ivshmem_size, PROT_READ | PROT_WRITE, MAP_SHARED, ivshmem_fd,
           USERNET_IVSHMEM_DEVM_START);
  if (shared_memory == MAP_FAILED) {
    perror("mmap()");
    exit(EXIT_FAILURE);
  }

  void *passed_memory = shared_memory + ivshmem_size - args.size;
  communicate(ivshmem_fd, passed_memory, &args, busy_waiting, client_port,
              server_ivposition, server_port, debug);

  cleanup(shared_memory, ivshmem_size);

  if (close(ivshmem_fd)) {
    perror("close(ivshmem_fd)");
    exit(EXIT_FAILURE);
  }

  return EXIT_SUCCESS;
}
