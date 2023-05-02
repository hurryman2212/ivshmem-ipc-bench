#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "common/arguments.h"
#include "common/ivshmem.h"

void intr_wait(int fd, int busy_waiting, uint16_t src_port, int debug) {
  do
    if (!ioctl(fd, IOCTL_WAIT, src_port))
      return;
  while ((busy_waiting && (errno == EAGAIN)) || (errno == EINTR));
  if (errno) {
    perror("ioctl(IOCTL_WAIT)");
    exit(EXIT_FAILURE);
  }
}
void intr_notify(int fd, int busy_waiting, uint16_t dest_ivposition,
                 uint16_t dest_port, int debug) {

  do
    if (!ioctl(fd, IOCTL_RING,
               IVSHMEM_DOORBELL_MSG(dest_ivposition, dest_port)))
      return;
  while ((busy_waiting && (errno == EAGAIN)) || (errno == EINTR));
  if (errno) {
    perror("ioctl(IOCTL_RING)");
    exit(EXIT_FAILURE);
  }
}

void uio_wait(int fd, int busy_waiting, int debug) {
  uint32_t dump;
  do
    if (read(fd, &dump, sizeof(uint32_t)) == sizeof(uint32_t))
      return;
  while ((busy_waiting && (errno == EAGAIN)) || (errno == EINTR));
  if (errno) {
    perror("read()");
    exit(EXIT_FAILURE);
  }
}

static void ivshmem_usage(const char *progname) {
  printf("Usage: %s [OPTION]...\n"
         "  -b <block_size>\n"
         "  -c <count>\n"
         "  -I <intr_dev_path>\n"
         "  -M <mem_dev_path>\n"
         "  -A <peer_address> (For UIO, this refers memory slot of which "
         "offset is `block_size * peer_address`)\n"
         "  -S <server_port> (default is %d)\n"
         "  -C <client_port> (default is %d)\n"
         "  -N: Non-block mode (default is `false`)\n"
         "  -D: Debug mode (default is `false`)\n",
         progname, IVSHMEM_DEFAULT_SERVER_PORT, IVSHMEM_DEFAULT_CLIENT_PORT);
}
void ivshmem_parse_args(IvshmemArgs *args, int argc, char *argv[]) {
  int c;

  args->count = DEFAULT_MESSAGE_COUNT;
  args->size = DEFAULT_MESSAGE_SIZE;

  args->server_port = IVSHMEM_DEFAULT_SERVER_PORT;
  args->client_port = IVSHMEM_DEFAULT_CLIENT_PORT;

  args->intr_dev_path = NULL;
  args->mem_dev_path = NULL;

  args->peer_id = -1;

  args->is_nonblock = 0;

  args->is_debug = 0;

  while ((c = getopt(argc, argv, "hND:b:c:I:M:A:S:C:")) != -1) {
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
    case 'S': /* Server's port # */
      args->server_port = atoi(optarg);
      break;
    case 'C': /* Client's port # */
      args->client_port = atoi(optarg);
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
