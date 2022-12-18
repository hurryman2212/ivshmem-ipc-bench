#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>

#include <sys/ioctl.h>

#include "ivshmem.h"

void intr_wait(int fd, int busy_waiting, uint16_t src_port, int debug) {
  if (busy_waiting) {
    while (ioctl(fd, IOCTL_WAIT, src_port) < 0)
      if (errno != EAGAIN)
        perror("ioctl(IOCTL_WAIT)");
      else if (debug)
        fprintf(stderr, "ioctl(IOCTL_WAIT): EAGAIN\n");
  } else {
    if (ioctl(fd, IOCTL_WAIT, src_port))
      perror("ioctl(IOCTL_WAIT)");
  }
}
void intr_notify(int fd, int busy_waiting, uint16_t dest_ivposition,
                 uint16_t dest_port, int debug) {
  if (busy_waiting) {
    while (ioctl(fd, IOCTL_RING,
                 IVSHMEM_DOORBELL_MSG(dest_ivposition, dest_port)) < 0)
      if (errno != EAGAIN)
        perror("ioctl(IOCTL_RING)");
      else if (debug)
        fprintf(stderr, "ioctl(IOCTL_RING): EAGAIN\n");
  } else {
    if (ioctl(fd, IOCTL_RING, IVSHMEM_DOORBELL_MSG(dest_ivposition, dest_port)))
      perror("ioctl(IOCTL_RING)");
  }
}

void uio_wait(int fd, int busy_waiting, int debug) {
  uint32_t dump;

  if (busy_waiting) {
    while (read(fd, &dump, sizeof(uint32_t)) != sizeof(uint32_t))
      if (errno != EAGAIN)
        perror("read()");
      else if (debug)
        fprintf(stderr, "read(): EAGAIN\n");
  } else {
    if (read(fd, &dump, sizeof(uint32_t)) != sizeof(uint32_t))
      perror("read()");
  }
}
