#ifndef IPC_BENCH_IVSHMEM_H
#define IPC_BENCH_IVSHMEM_H

#include <stdint.h>

/* H/W-specific */

struct ivshmem_reg {
  volatile uint32_t intrmask;
  volatile uint32_t intrstatus;
  volatile uint32_t ivposition;
  volatile uint32_t doorbell;
  volatile uint32_t ivlivelist;
};
#define IVSHMEM_DOORBELL_MSG(ivposition, msi_index)                            \
  ((uint32_t)(ivposition) << 16 | ((msi_index)&UINT16_MAX))
#define IVSHMEM_MMAP_REG_OFFSET 0
#define IVSHMEM_MMAP_MEM_OFFSET 4096

/* Driver-specific */

#define USERNET_IVSHMEM_IDENT(addr, port) IVSHMEM_DOORBELL_MSG(addr, port)
enum usernet_ivshmem_ioctl_cmds {
  /* Get total shared memory size available. */
  IOCTL_GETSIZE,
  /* Get source address number. */
  IOCTL_GETADDR,

  /**
   * Do not use `2` value as ioctl() command!
   * (reserved for FIGETBSZ; see "linux/fs.h")
   */
  IOCTL_INVAL = 2,

  /* Bind to the given source port number. */
  IOCTL_BIND,
  /* Get source identification number. */
  IOCTL_GETIDENT,
  /* Get source port number. */
  IOCTL_GETPORT,
  /* Mark this source number as a server port. */
  IOCTL_LISTEN,

  /* Connect to the given destination identification. */
  IOCTL_CONNECT,

  /* Clear all previous unconsumed interrupt. */
  IOCTL_CLEAR,

  /* Wait on the previously binded source port. */
  IOCTL_WAIT,
  /* Send signal to the connected peer. */
  IOCTL_SIGNAL,

  IOCTL_SHUTDOWN_RD,
  IOCTL_SHUTDOWN_WR,
  IOCTL_SHUTDOWN,
  IOCTL_CLOSE,
};

typedef struct IvshmemArgs {
  int count;
  int size;

  const char *intr_dev_path;
  const char *mem_dev_path;
  size_t mem_size_force;

  int peer_id;

  int shmem_index;

  int is_reset;

  int is_nonblock;

  int is_debug;
} IvshmemArgs;
void ivshmem_parse_args(IvshmemArgs *args, int argc, char *argv[]);

#define userspace_shm_notify(guard, update) ((void)(*guard = update))
void userspace_shm_wait(uint32_t *guard, const uint32_t expect);

void uio_wait(int fd, uint32_t *guard, uint32_t expect,
              struct ivshmem_reg *reg_ptr, struct IvshmemArgs *args);
void uio_notify(uint32_t *guard, uint32_t expect, struct ivshmem_reg *reg_ptr,
                struct IvshmemArgs *args);

void usernet_intr_wait(int fd, struct IvshmemArgs *args);
void usernet_intr_notify(int fd, struct IvshmemArgs *args);

#endif /* IPC_BENCH_IVSHMEM_H */
