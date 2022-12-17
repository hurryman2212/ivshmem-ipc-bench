#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#include <sys/mman.h>

#include "common/common.h"

#include "ivshmem-usernet-shm.h"

#define IVSHMEM_DEV_FILEPATH "/dev/usernet_ivshmem0"

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

void communicate(char *shared_memory, struct Arguments *args) {
  void *buffer = malloc(args->size);
  if (!buffer) {
    perror("malloc()");
    exit(EXIT_FAILURE);
  }

  shared_memory -= sizeof(atomic_uint);
  atomic_uint *guard = (atomic_uint *)shared_memory;
  atomic_init(guard, 's');

  for (; args->count > 0; --args->count) {
    shm_wait(guard);
    // Read
    memcpy(buffer, shared_memory + sizeof(atomic_uint), args->size);

    // Write back
    memset(shared_memory + sizeof(atomic_uint), '*', args->size);

    shm_notify(guard);
  }

  free(buffer);
}

int main(int argc, char *argv[]) {
  struct Arguments args;
  parse_arguments(&args, argc, argv);
  printf("args.size = %d\n", args.size);

  int ivshmem_fd = open(IVSHMEM_DEV_FILEPATH, O_RDWR | O_ASYNC | O_NONBLOCK);
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

  communicate(shared_memory, &args);

  cleanup(shared_memory, args.size);

  close(ivshmem_fd);

  return EXIT_SUCCESS;
}
