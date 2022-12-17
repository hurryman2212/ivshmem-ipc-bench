#ifndef IPC_BENCH_IVSHMEM_H
#define IPC_BENCH_IVSHMEM_H

#include <stdint.h>

struct ivshmem_reg {
  uint32_t intr_mask;   // useless
  uint32_t intr_stat;   // useless
  uint32_t iv_position; // for reading dest. intr. ID of this VM
  uint32_t doorbell;    // for writing intr. msg.
  uint32_t IVLiveList;  // useless (?)
};
#define IVSHMEM_DOORBELL_MSG(ivposition, msi_index)                            \
  (uint32_t)((ivposition << 16) | (msi_index & UINT16_MAX))

enum usernet_ivshmem_ioctl_cmd {
  IOCTL_CLEAR = 0,
  IOCTL_RING = 1,
  IOCTL_WAIT = 3
};

void intr_wait(int fd, int busy_waiting, uint16_t src_port);
void intr_notify(int fd, int busy_waiting, uint16_t dest_ivposition,
                 uint16_t dest_port);

#endif /* IPC_BENCH_IVSHMEM_H */
