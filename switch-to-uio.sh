#!/bin/sh
sudo modprobe -r usernet_ivshmem
sudo modprobe uio
sudo insmod /lib/modules/$(uname -r)/kernel/drivers/uio/uio_ivshmem.ko
