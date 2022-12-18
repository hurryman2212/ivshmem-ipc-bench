#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <sys/ioctl.h>

#include "ivshmem.h"

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
