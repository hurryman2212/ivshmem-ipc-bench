#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/ioctl.h>

#include "ivshmem.h"

void intr_wait(int fd, int busy_waiting, uint16_t src_port) {
  if (busy_waiting) {
    while (ioctl(fd, IOCTL_WAIT, src_port) < 0)
      if (errno != EAGAIN)
        perror("ioctl(IOCTL_WAIT)");
  } else {
    if (ioctl(fd, IOCTL_WAIT, src_port))
      perror("ioctl(IOCTL_WAIT)");
  }
}
void intr_notify(int fd, int busy_waiting, uint16_t dest_ivposition,
                 uint16_t dest_port) {
  if (busy_waiting) {
    while (ioctl(fd, IOCTL_RING,
                 IVSHMEM_DOORBELL_MSG(dest_ivposition, dest_port)) < 0)
      if (errno != EAGAIN)
        perror("ioctl(IOCTL_RING)");
  } else {
    if (ioctl(fd, IOCTL_RING, IVSHMEM_DOORBELL_MSG(dest_ivposition, dest_port)))
      perror("ioctl(IOCTL_RING)");
  }
}
