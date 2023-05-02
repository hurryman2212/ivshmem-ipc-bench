#ifndef IPC_BENCH_IVSHMEM_H
#define IPC_BENCH_IVSHMEM_H

#include <stdatomic.h>
#include <stdint.h>

#define IVSHMEM_DOORBELL_MSG(ivposition, msi_index)                            \
  (uint32_t)((ivposition << 16) | (msi_index & UINT16_MAX))

struct ivshmem_reg {
  uint32_t intrmask;
  uint32_t intrstatus;
  uint32_t ivposition;
  uint32_t doorbell;
  uint32_t ivlivelist;
};
enum usernet_ivshmem_ioctl_cmd {
  IOCTL_CLEAR = 0,
  IOCTL_GETPOS = 1,

  /**
   * Do not use `2` value as ioctl() command!
   * (reserved for FIGETBSZ; see "linux/fs.h")
   */

  IOCTL_GETSIZE = 3,
  IOCTL_RING = 4,
  IOCTL_WAIT = 5
};
#define USERNET_IVSHMEM_DEVM_START 4096

typedef struct IvshmemArgs {
  int count;
  int size;

  const char *intr_dev_path;
  const char *mem_dev_path;

  int peer_id;

  int shmem_index;

  int is_reset;

  int is_nonblock;

  int is_debug;
} IvshmemArgs;
void ivshmem_parse_args(IvshmemArgs *args, int argc, char *argv[]);

void usernet_intr_wait(int fd, struct IvshmemArgs *args);
void usernet_intr_notify(int fd, struct IvshmemArgs *args);

void uio_wait(int fd, struct IvshmemArgs *args, atomic_uint *guard,
              char expect);

#endif /* IPC_BENCH_IVSHMEM_H */
