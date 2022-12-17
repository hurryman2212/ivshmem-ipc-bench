#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#include <sys/mman.h>

#include "common/common.h"

void cleanup(void *shared_memory, size_t size) {
  if (munmap(shared_memory, size)) {
    perror("munmap()");
    exit(EXIT_FAILURE);
  }
}
void shm_wait(atomic_uint *guard) {
  while (atomic_load(guard) != 's')
    ; // busy waiting
}

void shm_notify(atomic_uint *guard) { atomic_store(guard, 'c'); }

void communicate(void *shared_memory, struct Arguments *args) {
  void *buffer = malloc(args->size);
  if (!buffer) {
    perror("malloc()");
    exit(EXIT_FAILURE);
  }

  shared_memory -= sizeof(atomic_uint);
  atomic_uint *guard = (atomic_uint *)shared_memory;
  shm_wait(guard);

  struct Benchmarks bench;
  setup_benchmarks(&bench);

  for (int message = 0; message < args->count; ++message) {
    bench.single_start = now();

    memset(shared_memory + sizeof(atomic_uint), '*', args->size);

    shm_notify(guard);
    shm_wait(guard);

    memcpy(buffer, shared_memory + sizeof(atomic_uint), args->size);

    benchmark(&bench);
  }

  evaluate(&bench, args);

  free(buffer);
}

int main(int argc, char *argv[]) {
  if (argc < 5) {
    fprintf(stderr, "usage: %s IVSHMEM_DEVPATH IVSHMEM_SIZE COUNT SIZE\n",
            argv[0]);
    exit(EXIT_FAILURE);
  }
  const char *ivshmem_devpath = argv[1];
  size_t ivshmem_size = atoi(argv[2]);
  size_t count = atoi(argv[3]);
  size_t size = atoi(argv[4]);

  struct Arguments args;
  args.count = count;
  args.size = size;

  int ivshmem_fd = open(ivshmem_devpath, O_RDWR | O_ASYNC | O_NONBLOCK);
  if (ivshmem_fd < 0) {
    perror("open()");
    exit(EXIT_FAILURE);
  }

  void *shared_memory = mmap(NULL, ivshmem_size, PROT_READ | PROT_WRITE,
                             MAP_SHARED, ivshmem_fd, 4096);
  if (shared_memory == MAP_FAILED) {
    perror("mmap()");
    exit(EXIT_FAILURE);
  }
  shared_memory += ivshmem_size - args.size;
  memset(shared_memory - sizeof(atomic_uint), 0,
         args.size + sizeof(atomic_uint));

  communicate(shared_memory, &args);

  cleanup(shared_memory, args.size);

  close(ivshmem_fd);

  return EXIT_SUCCESS;
}
