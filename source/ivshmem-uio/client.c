#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>

#include "common/common.h"
#include "common/ivshmem.h"

void cleanup(void *shared_memory, size_t size) {
  if (munmap(shared_memory, size)) {
    perror("munmap()");
    exit(EXIT_FAILURE);
  }
}

void communicate(int fd, void *shared_memory, struct IvshmemArgs *args,
                 int busy_waiting, uint16_t dest_ivposition, int debug) {
  void *buffer = malloc(args->size);
  if (!buffer) {
    perror("malloc()");
    exit(EXIT_FAILURE);
  }

  struct ivshmem_reg *reg_ptr =
      mmap(NULL, 256, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (reg_ptr == MAP_FAILED) {
    perror("mmap()");
    exit(EXIT_FAILURE);
  }

  reg_ptr->doorbell = IVSHMEM_DOORBELL_MSG(dest_ivposition, 0);

  for (; args->count > 0; --args->count) {
    uio_wait(fd, busy_waiting, debug);

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

    reg_ptr->doorbell = IVSHMEM_DOORBELL_MSG(dest_ivposition, 0);
  }

  if (!busy_waiting) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
      perror("fcntl(F_GETFL)");
      exit(EXIT_FAILURE);
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
      perror("fcntl(F_SETFL)");
      exit(EXIT_FAILURE);
    }
  }

  uint32_t dump;
  if (read(fd, &dump, sizeof(dump)) != 4) {
    if (errno != EAGAIN) {
      perror("uio_wait()");
      exit(EXIT_FAILURE);
    }
  }

  if (!busy_waiting) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
      perror("fcntl(F_GETFL)");
      exit(EXIT_FAILURE);
    }
    if (fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) == -1) {
      perror("fcntl(F_SETFL)");
      exit(EXIT_FAILURE);
    }
  }

  free(buffer);
}

static const char IVSHMEM_INTR_DEFAULT_PATH[] = "/dev/uio0";
static const char IVSHMEM_MEM_DEFAULT_PATH[] =
    "/sys/class/uio/uio0/device/resource2_wc";
int main(int argc, char *argv[]) {
  struct IvshmemArgs args;
  ivshmem_parse_args(&args, argc, argv);

  if (!args.intr_dev_path) {
    fprintf(stderr, "No -I option set; Use %s as the interrupt device path\n",
            IVSHMEM_INTR_DEFAULT_PATH);
    args.intr_dev_path = IVSHMEM_INTR_DEFAULT_PATH;
  }
  if (!args.mem_dev_path) {
    fprintf(stderr, "No -M option set; Use %s as the memory device path\n",
            IVSHMEM_MEM_DEFAULT_PATH);
    args.mem_dev_path = IVSHMEM_MEM_DEFAULT_PATH;
  }

  if (args.peer_id == -1) {
    fprintf(stderr, "No -A option set; Use 0 as the memory slot index\n");
    args.peer_id = 0;
  }

  if (args.shmem_index == -1) {
    fprintf(stderr, "No -i option set; Use 0 as the shared memory index\n");
    args.shmem_index = 0;
  }

  int ivshmem_uiofd;
  if (args.is_nonblock)
    ivshmem_uiofd = open(args.intr_dev_path, O_RDWR | O_ASYNC | O_NONBLOCK);
  else
    ivshmem_uiofd = open(args.intr_dev_path, O_RDWR | O_ASYNC);
  if (ivshmem_uiofd < 0) {
    perror("open(ivshmem_uiofd)");
    exit(EXIT_FAILURE);
  }

  int ivshmem_memfd = open(args.mem_dev_path, O_RDWR | O_ASYNC | O_NONBLOCK);
  if (ivshmem_memfd < 0) {
    perror("open(ivshmem_memfd)");
    exit(EXIT_FAILURE);
  }

  struct stat st;
  if (stat(args.mem_dev_path, &st)) {
    perror("stat()");
    exit(EXIT_FAILURE);
  }
  size_t ivshmem_size = st.st_size;
  fprintf(stderr, "ivshmem_size == %lu\n", ivshmem_size);

  void *shared_memory = mmap(NULL, ivshmem_size, PROT_READ | PROT_WRITE,
                             MAP_SHARED, ivshmem_memfd, 0);
  if (shared_memory == MAP_FAILED) {
    perror("mmap()");
    exit(EXIT_FAILURE);
  }

  void *passed_memory =
      shared_memory + ivshmem_size - ((args.shmem_index + 1) * args.size);
  communicate(ivshmem_uiofd, passed_memory, &args, args.is_nonblock,
              args.server_port, args.is_debug);

  cleanup(shared_memory, ivshmem_size);

  if (close(ivshmem_memfd)) {
    perror("close(ivshmem_memfd)");
    exit(EXIT_FAILURE);
  }
  if (close(ivshmem_uiofd)) {
    perror("close(ivshmem_uiofd)");
    exit(EXIT_FAILURE);
  }

  return EXIT_SUCCESS;
}
