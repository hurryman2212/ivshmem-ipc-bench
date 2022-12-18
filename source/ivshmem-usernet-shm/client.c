#include <stdatomic.h>
#include <stdint.h>
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
void shm_wait(atomic_uint *guard) {
  while (atomic_load(guard) != 'c')
    ;
}

void shm_notify(atomic_uint *guard) { atomic_store(guard, 's'); }

void communicate(void *shared_memory, struct Arguments *args, int debug) {
  void *buffer = malloc(args->size);
  if (!buffer) {
    perror("malloc()");
    exit(EXIT_FAILURE);
  }

  atomic_uint *guard = (atomic_uint *)shared_memory;
  atomic_init(guard, 's');

  for (; args->count > 0; --args->count) {
    shm_wait(guard);

    memcpy(buffer, shared_memory + sizeof(atomic_uint), args->size);
    if (debug) {
      for (int i = 0; i < args->size; ++i) {
        if (((uint8_t *)buffer)[i] != 0x55) {
          fprintf(stderr, "Validation failed after memcpy()!\n");
          exit(EXIT_FAILURE);
        }
      }
    }

    memset(shared_memory + sizeof(atomic_uint), 0xAA, args->size);
    if (debug) {
      for (int i = 0; i < args->size; ++i) {
        if (((uint8_t *)(shared_memory + sizeof(atomic_uint)))[i] != 0xAA) {
          fprintf(stderr, "Validation failed after memset()!\n");
          exit(EXIT_FAILURE);
        }
      }
    }

    shm_notify(guard);
  }

  free(buffer);
}

int main(int argc, char *argv[]) {
  if (argc != 6) {
    fprintf(stderr, "usage: %s IVSHMEM_DEVPATH COUNT SIZE NONBLOCK DEBUG\n",
            argv[0]);
    exit(EXIT_FAILURE);
  }
  const char *ivshmem_devpath = argv[1];
  size_t count = atoi(argv[2]);
  size_t size = atoi(argv[3]);
  int busy_waiting = atoi(argv[4]);
  int debug = atoi(argv[5]);

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

  void *shared_memory = mmap(NULL, ivshmem_size, PROT_READ | PROT_WRITE,
                             MAP_SHARED, ivshmem_fd, 4096);
  if (shared_memory == MAP_FAILED) {
    perror("mmap()");
    exit(EXIT_FAILURE);
  }

  void *passed_memory =
      shared_memory + ivshmem_size - args.size - sizeof(atomic_uint);
  communicate(passed_memory, &args, debug);

  cleanup(shared_memory, ivshmem_size);

  if (close(ivshmem_fd)) {
    perror("close(ivshmem_fd)");
    exit(EXIT_FAILURE);
  }

  return EXIT_SUCCESS;
}
