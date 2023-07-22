#ifndef IPC_BENCH_IVSHMEM_H
#define IPC_BENCH_IVSHMEM_H

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
  /* Get this `ivposition` value */
  IOCTL_GETPOS,
  /* Get this IVSHMEM device memory size */
  IOCTL_GETSIZE,

  /**
   * Do not use `2` value as ioctl() command!
   * (reserved for FIGETBSZ; see "linux/fs.h")
   */
  IOCTL_CLEAR = 3,

  /* Bind to the given source port number */
  IOCTL_BIND,
  /* Wait on the previously binded source port */
  IOCTL_WAIT,
  /* Send signal on the given interrupt line */
  IOCTL_SIGNAL,
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

void uio_wait(int fd, uint32_t *guard, uint32_t expect,
              struct ivshmem_reg *reg_ptr, struct IvshmemArgs *args);
void uio_notify(uint32_t *guard, uint32_t expect, struct ivshmem_reg *reg_ptr,
                struct IvshmemArgs *args);

#define userspace_shm_notify(guard, update) ((void)(*guard = update))
void userspace_shm_wait(uint32_t *guard, const uint32_t expect);

#endif /* IPC_BENCH_IVSHMEM_H */
