#include <errno.h>
#include <stdatomic.h>
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

void shm_wait(atomic_uint *guard) {
  while (atomic_load(guard) != 's')
    ;
}
void shm_notify(atomic_uint *guard) { atomic_store(guard, 'c'); }

void communicate(int fd, void *shared_memory, struct IvshmemArgs *args) {
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

  fprintf(stderr, "reg_ptr->ivposition == %d\n", reg_ptr->ivposition);

  atomic_uint *guard = (atomic_uint *)shared_memory;
  atomic_init(guard, 'c');

  uio_wait(fd, args, guard, 's', reg_ptr);

  struct Benchmarks bench;
  setup_benchmarks(&bench);

  for (int message = 0; message < args->count; ++message) {
    bench.single_start = now();

    /* Write */

    memset(shared_memory + sizeof(atomic_uint), 0x55, args->size);
    if (args->is_debug) {
      for (int i = 0; i < args->size; ++i) {
        if (((uint8_t *)(shared_memory + sizeof(atomic_uint)))[i] != 0x55) {
          fprintf(stderr, "Validation failed after memset()!\n");
          exit(EXIT_FAILURE);
        }
      }
    }

    /* Post memory atomic first */
    shm_notify(guard);
    /* Then, send interrupt */
    reg_ptr->doorbell = IVSHMEM_DOORBELL_MSG(args->peer_id, 0);

    /* Write END */

    /* Read */

    uio_wait(fd, args, guard, 's', reg_ptr);

    memcpy(buffer, shared_memory + sizeof(atomic_uint), args->size);
    if (args->is_debug) {
      for (int i = 0; i < args->size; ++i) {
        if (((uint8_t *)buffer)[i] != 0xAA) {
          fprintf(stderr, "Validation failed after memcpy()!\n");
          exit(EXIT_FAILURE);
        }
      }
    }

    /* Read END */

    benchmark(&bench);
  }

  struct Arguments tmp_arg;
  tmp_arg.count = args->count;
  tmp_arg.size = args->size;
  evaluate(&bench, &tmp_arg);

  if (munmap(reg_ptr, 256)) {
    perror("munmap()");
    exit(EXIT_FAILURE);
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
    fprintf(stderr,
            "No -A option set; Use 1 as the device peer (client) index\n");
    args.peer_id = 1;
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

  struct stat st;
  if (stat(args.mem_dev_path, &st)) {
    perror("stat()");
    exit(EXIT_FAILURE);
  }
  size_t ivshmem_size = st.st_size;
  fprintf(stderr, "ivshmem_size == %lu\n", ivshmem_size);

  void *shared_memory = mmap(NULL, ivshmem_size, PROT_READ | PROT_WRITE,
                             MAP_SHARED, ivshmem_uiofd, 4096);
  if (shared_memory == MAP_FAILED) {
    perror("mmap()");
    exit(EXIT_FAILURE);
  }

  void *passed_memory =
      shared_memory + ivshmem_size -
      ((args.shmem_index + 1) * (args.size + sizeof(atomic_uint)));

  if (args.is_reset) {
    if (!args.is_nonblock) {
      int flags = fcntl(ivshmem_uiofd, F_GETFL, 0);
      if (flags == -1) {
        perror("fcntl(F_GETFL)");
        exit(EXIT_FAILURE);
      }
      if (fcntl(ivshmem_uiofd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl(F_SETFL)");
        exit(EXIT_FAILURE);
      }
    }
    uint32_t dump;
    if (read(ivshmem_uiofd, &dump, sizeof(dump)) != 4) {
      if (errno != EAGAIN) {
        perror("uio_wait()");
        exit(EXIT_FAILURE);
      }
    }
    if (!args.is_nonblock) {
      int flags = fcntl(ivshmem_uiofd, F_GETFL, 0);
      if (flags == -1) {
        perror("fcntl(F_GETFL)");
        exit(EXIT_FAILURE);
      }
      if (fcntl(ivshmem_uiofd, F_SETFL, flags & ~O_NONBLOCK) == -1) {
        perror("fcntl(F_SETFL)");
        exit(EXIT_FAILURE);
      }
    }
  }

  communicate(ivshmem_uiofd, passed_memory, &args);

  cleanup(shared_memory, ivshmem_size);

  if (close(ivshmem_uiofd)) {
    perror("close(ivshmem_uiofd)");
    exit(EXIT_FAILURE);
  }

  return EXIT_SUCCESS;
}
