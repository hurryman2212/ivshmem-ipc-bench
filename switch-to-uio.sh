#!/bin/sh
modprobe -r usernet_ivshmem
modprobe uio
insmod /lib/modules/$(uname -r)/kernel/drivers/uio/uio_ivshmem.ko
