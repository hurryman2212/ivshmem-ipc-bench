#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#include <sys/ioctl.h>
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

__attribute__((hot, flatten)) void communicate(void *shared_memory,
                                               struct IvshmemArgs *args) {
  void *buffer = malloc(args->size);
  if (!buffer) {
    perror("malloc()");
    exit(EXIT_FAILURE);
  }

  uint32_t *guard = (uint32_t *)shared_memory;
  userspace_shm_notify(guard, 'c');

  userspace_shm_wait(guard, 's');

  struct Benchmarks bench;
  setup_benchmarks(&bench);

  for (int message = 0; message < args->count; ++message) {
    bench.single_start = now();

    /* STC */
    memset(shared_memory + sizeof(*guard), STC_BITS_10101010, args->size);
    if (args->is_debug)
      debug_validate(shared_memory + sizeof(*guard), args->size,
                     STC_BITS_10101010);
    userspace_shm_notify(guard, 'c');

    /* CTS */
    userspace_shm_wait(guard, 's');
    memcpy(buffer, shared_memory + sizeof(*guard), args->size);
    if (args->is_debug)
      debug_validate(buffer, args->size, CTS_BITS_01010101);

    benchmark(&bench);
  }

  struct Arguments tmp_arg;
  tmp_arg.count = args->count;
  tmp_arg.size = args->size;
  evaluate(&bench, &tmp_arg);

  free(buffer);
}

static const char IVSHMEM_MEM_DEFAULT_PATH[] = "/dev/usernet_ivshmem0";
int main(int argc, char *argv[]) {
  struct IvshmemArgs args;
  ivshmem_parse_args(&args, argc, argv);

  if (!args.mem_dev_path) {
    fprintf(stderr, "No -M option set; Use %s as the memory device path\n",
            IVSHMEM_MEM_DEFAULT_PATH);
    args.mem_dev_path = IVSHMEM_MEM_DEFAULT_PATH;
  }

  if (args.shmem_index == -1) {
    fprintf(stderr, "No -i option set; Use 0 as the shared memory index\n");
    args.shmem_index = 0;
  }

  int ivshmem_fd;
  if (args.is_nonblock) {
    fprintf(stderr, "args.is_nonblock == 1\n");
    ivshmem_fd = open(args.mem_dev_path, O_RDWR | O_ASYNC | O_NONBLOCK);
  } else
    ivshmem_fd = open(args.mem_dev_path, O_RDWR | O_ASYNC);
  if (ivshmem_fd < 0) {
    perror("open()");
    exit(EXIT_FAILURE);
  }

  struct stat st;
  off_t ivshmem_mmap_offset = 0;
  if (stat(args.mem_dev_path, &st)) {
    perror("stat()");
    exit(EXIT_FAILURE);
  }
  size_t ivshmem_size = st.st_size;
  if (!ivshmem_size) {
    /* Try usernet_ivshmem's way */
    if (ioctl(ivshmem_fd, IOCTL_GETSIZE, &ivshmem_size) < 0) {
      perror("ioctl(IOCTL_GETSIZE)");
      exit(EXIT_FAILURE);
    }
    ivshmem_mmap_offset = USERNET_IVSHMEM_DEVM_START;
  }
  fprintf(stderr, "ivshmem_size == %lu\n", ivshmem_size);
  void *shared_memory = mmap(NULL, ivshmem_size, PROT_READ | PROT_WRITE,
                             MAP_SHARED, ivshmem_fd, ivshmem_mmap_offset);
  if (shared_memory == MAP_FAILED) {
    perror("mmap()");
    exit(EXIT_FAILURE);
  }

  void *passed_memory =
      shared_memory + ivshmem_size -
      ((args.shmem_index + 1) * (args.size + sizeof(uint32_t)));
  memset(passed_memory, 0, args.size + sizeof(uint32_t));

  communicate(passed_memory, &args);

  cleanup(shared_memory, ivshmem_size);

  if (close(ivshmem_fd)) {
    perror("close(ivshmem_fd)");
    exit(EXIT_FAILURE);
  }

  return EXIT_SUCCESS;
}
