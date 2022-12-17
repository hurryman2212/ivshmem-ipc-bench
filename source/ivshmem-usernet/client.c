#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

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
                 uint16_t dest_port) {
  void *buffer = malloc(args->size);
  if (!buffer) {
    perror("malloc()");
    exit(EXIT_FAILURE);
  }

  shared_memory -= sizeof(atomic_uint);

  intr_notify(fd, busy_waiting, dest_ivposition, dest_port);

  for (; args->count > 0; --args->count) {
    intr_wait(fd, busy_waiting, src_port);

    memcpy(buffer, shared_memory + sizeof(atomic_uint), args->size);

    memset(shared_memory + sizeof(atomic_uint), '*', args->size);

    intr_notify(fd, busy_waiting, dest_ivposition, dest_port);
  }

  free(buffer);
}

int main(int argc, char *argv[]) {
  if (argc < 8) {
    fprintf(stderr,
            "usage: %s IVSHMEM_DEVPATH IVSHMEM_SIZE CLIENT_PORT "
            "SERVER_IVPOSITION SERVER_PORT COUNT SIZE\n",
            argv[0]);
    exit(EXIT_FAILURE);
  }
  const char *ivshmem_devpath = argv[1];
  size_t ivshmem_size = atoi(argv[2]);
  uint16_t client_port = atoi(argv[3]);
  uint16_t server_ivposition = atoi(argv[4]);
  uint16_t server_port = atoi(argv[5]);
  size_t count = atoi(argv[6]);
  size_t size = atoi(argv[7]);

  struct Arguments args;
  args.count = count;
  args.size = size;

  int busy_waiting = check_flag("busy", argc, argv);
  if (busy_waiting) {
    printf("busy_waiting = true");
  }

  int ivshmem_fd;
  if (busy_waiting)
    ivshmem_fd = open(ivshmem_devpath, O_RDWR | O_ASYNC);
  else
    ivshmem_fd = open(ivshmem_devpath, O_RDWR | O_ASYNC | O_NONBLOCK);
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

  communicate(ivshmem_fd, shared_memory, &args, busy_waiting, client_port,
              server_ivposition, server_port);

  cleanup(shared_memory, args.size);

  close(ivshmem_fd);

  return EXIT_SUCCESS;
}
