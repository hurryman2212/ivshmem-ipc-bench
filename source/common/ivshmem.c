#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <x86gprintrin.h>

#include "common/common.h"
#include "common/ivshmem.h"

void userspace_shm_wait(uint32_t *guard, const uint32_t expect) {
  while (*guard != expect)
    __pause(); // Optimization for spin loop
}

void usernet_intr_wait(int fd, struct IvshmemArgs *args) {
  do
    if (!ioctl(fd, IOCTL_WAIT, args->shmem_index))
      return;
  while (likely(((errno == EAGAIN) && args->is_nonblock) || (errno == EINTR)));
  perror("ioctl(IOCTL_WAIT)");
  exit(EXIT_FAILURE);
}
void usernet_intr_notify(int fd, struct IvshmemArgs *args) {
  do
    if (!ioctl(fd, IOCTL_RING,
               IVSHMEM_DOORBELL_MSG(args->peer_id, args->shmem_index)))
      return;
  while (likely(((errno == EAGAIN) && args->is_nonblock) || (errno == EINTR)));
  perror("ioctl(IOCTL_RING)");
  exit(EXIT_FAILURE);
}

void uio_wait(int fd, uint32_t *guard, uint32_t expect,
              struct ivshmem_reg *reg_ptr, struct IvshmemArgs *args) {
  int ret;
  uint32_t dump;
  do
    /* Be careful, It might return not sizeof(uint32_t) even if successful! */
    if ((ret = read(fd, &dump, sizeof(uint32_t)) > 0)) {
      if (*guard == expect)
        return;
      else // This interrupt is not for me... Ring me again.
        reg_ptr->doorbell = IVSHMEM_DOORBELL_MSG(reg_ptr->ivposition, 0);
    }
  /* There can be still EAGAIN happening even if it is blocking mode! */
  while (likely((errno == 0) || (errno == EAGAIN) || (errno == EINTR)));
  perror("read()");
  exit(EXIT_FAILURE);
}
void uio_notify(uint32_t *guard, uint32_t expect, struct ivshmem_reg *reg_ptr,
                struct IvshmemArgs *args) {
  /* Post the memory update first */
  userspace_shm_notify(guard, expect);
  /* Then, send interrupt */
  reg_ptr->doorbell = IVSHMEM_DOORBELL_MSG(args->peer_id, 0);
}

static void ivshmem_usage(const char *progname) {
  printf("Usage: %s [OPTION]...\n"
         "  -b <block_size> (default is %d)\n"
         "  -c <count> (default is %d)\n"
         "  -I <intr_dev_path>\n"
         "  -M <mem_dev_path>\n"
         "  -A <peer_address>\n"
         "  -i <shmem_index> (default is 0)\n"
         "  -R: Reset previous interrupts (default is `false`)"
         "  -N: Non-block mode (default is `false`)\n"
         "  -D: Debug mode (default is `false`)\n",
         progname, DEFAULT_MESSAGE_COUNT, DEFAULT_MESSAGE_SIZE);
}
void ivshmem_parse_args(IvshmemArgs *args, int argc, char *argv[]) {
  int c;

  args->count = DEFAULT_MESSAGE_COUNT;
  args->size = DEFAULT_MESSAGE_SIZE;

  args->intr_dev_path = NULL;
  args->mem_dev_path = NULL;

  args->peer_id = -1;

  args->shmem_index = 0;

  args->is_reset = 0;

  args->is_nonblock = 0;

  args->is_debug = 0;

  while ((c = getopt(argc, argv, "hRNDb:c:I:M:A:i:")) != -1) {
    switch (c) {
    case 'b': /* Block size */
      args->size = atoi(optarg);
      break;
    case 'c': /* Count */
      args->count = atoi(optarg);
      break;

    case 'I': /* Interrupt device path */
      args->intr_dev_path = optarg;
      break;
    case 'M': /* Memory device path */
      args->mem_dev_path = optarg;
      break;

    case 'A': /* Peer's device ID */
      args->peer_id = atoi(optarg);
      break;

    case 'i': /* Memory index */
      args->shmem_index = atoi(optarg);
      break;

    case 'R': /* Reset previous interrupts */
      args->is_reset = 1;
      break;

    case 'N': /* Non-blocking mode */
      args->is_nonblock = 1;
      break;

    case 'D': /* Debug mode */
      args->is_debug = 1;
      break;

    case 'h': /* help */
    default:
      ivshmem_usage(argv[0]);
      exit(1);
      break;
    }
  }
}
