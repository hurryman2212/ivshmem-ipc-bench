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

void communicate(int fd, void *shared_memory, struct IvshmemArgs *args) {
  void *buffer = malloc(args->size);
  if (!buffer) {
    perror("malloc()");
    exit(EXIT_FAILURE);
  }

  usernet_intr_notify(fd, args);

  for (; args->count > 0; --args->count) {
    usernet_intr_wait(fd, args);

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

    usernet_intr_notify(fd, args);
  }

  free(buffer);
}

static const char IVSHMEM_INTR_DEFAULT_PATH[] = "/dev/usernet_ivshmem0";
int main(int argc, char *argv[]) {
  struct IvshmemArgs args;
  ivshmem_parse_args(&args, argc, argv);

  if (!args.intr_dev_path) {
    fprintf(stderr, "No -I option set; Use %s as the interrupt device path\n",
            IVSHMEM_INTR_DEFAULT_PATH);
    args.intr_dev_path = IVSHMEM_INTR_DEFAULT_PATH;
  }

  if (args.peer_id == -1) {
    fprintf(stderr,
            "No -A option set; Use 0 as the device peer (server) index\n");
    args.peer_id = 0;
  }

  if (args.shmem_index == -1) {
    fprintf(stderr, "No -i option set; Use 0 as the shared memory index\n");
    args.shmem_index = 0;
  }

  int ivshmem_fd;
  if (args.is_nonblock)
    ivshmem_fd = open(args.intr_dev_path, O_RDWR | O_ASYNC | O_NONBLOCK);
  else
    ivshmem_fd = open(args.intr_dev_path, O_RDWR | O_ASYNC);
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

  void *passed_memory =
      shared_memory + ivshmem_size - ((args.shmem_index + 1) * args.size);

  if (args.is_reset) {
    if (ioctl(ivshmem_fd, IOCTL_CLEAR, 0)) {
      perror("ioctl(IOCTL_CLEAR)");
      exit(EXIT_FAILURE);
    }
  }

  communicate(ivshmem_fd, passed_memory, &args);

  cleanup(shared_memory, ivshmem_size);

  if (close(ivshmem_fd)) {
    perror("close(ivshmem_fd)");
    exit(EXIT_FAILURE);
  }

  return EXIT_SUCCESS;
}
