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
  IOCTL_GETSIZE = 1,
  IOCTL_RING = 3,
  IOCTL_WAIT = 4
};
#define USERNET_IVSHMEM_DEVM_START 4096

void intr_wait(int fd, int busy_waiting, uint16_t src_port, int debug);
void intr_notify(int fd, int busy_waiting, uint16_t dest_ivposition,
                 uint16_t dest_port, int debug);

void uio_wait(int fd, int busy_waiting, int debug);

#endif /* IPC_BENCH_IVSHMEM_H */
