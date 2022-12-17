#include <assert.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/mman.h>

#include "common/common.h"
#include "common/ivshmem.h"

#include "ivshmem-usernet.h"

#define IVSHMEM_DEV_FILEPATH "/dev/usernet_ivshmem0"

void cleanup(void *shared_memory, size_t size) {
  if (munmap(shared_memory, size)) {
    perror("munmap()");
    exit(EXIT_FAILURE);
  }
}

void intr_wait(int fd, int busy_waiting) {
  if (busy_waiting) {
    while (ioctl(fd, IOCTL_WAIT, SERVER_PORT) < 0) {
      if (errno != EAGAIN) {
        perror("ioctl(IOCTL_WAIT)");
        exit(EXIT_FAILURE);
      }
    }
  } else {
    if (ioctl(fd, IOCTL_WAIT, SERVER_PORT)) {
      perror("ioctl(IOCTL_WAIT)");
      exit(EXIT_FAILURE);
    }
  }
}
void intr_notify(int fd) {
  if (ioctl(fd, IOCTL_RING,
            IVSHMEM_DOORBELL_MSG(CLIENT_IVPOSITION, CLIENT_PORT))) {
    perror("ioctl(IOCTL_RING)");
    exit(EXIT_FAILURE);
  }
}

void communicate(int fd, char *shared_memory, struct Arguments *args,
                 int busy_waiting) {
  void *buffer = malloc(args->size);
  if (!buffer) {
    perror("malloc()");
    exit(EXIT_FAILURE);
  }

  shared_memory -= sizeof(atomic_uint);

  if (ioctl(fd, IOCTL_CLEAR, 0)) {
    perror("ioctl(IOCTL_CLEAR)");
    exit(EXIT_FAILURE);
  }
  intr_wait(fd, busy_waiting);

  struct Benchmarks bench;
  setup_benchmarks(&bench);

  for (int message = 0; message < args->count; ++message) {
    bench.single_start = now();

    memset(shared_memory + sizeof(atomic_uint), '*', args->size);

    intr_notify(fd);
    intr_wait(fd, busy_waiting);

    memcpy(buffer, shared_memory + sizeof(atomic_uint), args->size);

    benchmark(&bench);
  }

  evaluate(&bench, args);

  free(buffer);
}

int main(int argc, char *argv[]) {
  struct Arguments args;
  parse_arguments(&args, argc, argv);
  printf("args.size = %d\n", args.size);

  int busy_waiting = check_flag("busy", argc, argv);
  int ivshmem_fd;
  if (busy_waiting)
    ivshmem_fd = open(IVSHMEM_DEV_FILEPATH, O_RDWR | O_ASYNC);
  else
    ivshmem_fd = open(IVSHMEM_DEV_FILEPATH, O_RDWR | O_ASYNC | O_NONBLOCK);
  if (ivshmem_fd < 0) {
    perror("open()");
    exit(EXIT_FAILURE);
  }

  char *shared_memory =
      (char *)mmap(NULL, IVSHMEM_DEV_FILESIZE, PROT_READ | PROT_WRITE,
                   MAP_SHARED, ivshmem_fd, 4096);
  if (shared_memory == MAP_FAILED) {
    perror("mmap()");
    exit(EXIT_FAILURE);
  }
  shared_memory += IVSHMEM_DEV_FILESIZE - args.size;

  communicate(ivshmem_fd, shared_memory, &args, busy_waiting);

  cleanup(shared_memory, args.size);

  close(ivshmem_fd);

  return EXIT_SUCCESS;
}
