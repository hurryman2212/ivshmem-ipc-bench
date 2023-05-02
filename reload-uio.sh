#!/bin/sh
sudo modprobe -r uio_ivshmem
sudo modprobe -r kvmfr
sudo modprobe -r usernet_ivshmem
sudo modprobe uio_ivshmem
sudo chown $USER:$USER /dev/uio* /sys/module/uio_ivshmem/drivers/pci\:uio_ivshmem/*/resource2_wc
