#!/bin/sh
sudo modprobe -r kvmfr
sudo modprobe -r uio_ivshmem
sudo modprobe -r usernet_ivshmem
sudo modprobe usernet_ivshmem
sudo chown $USER:$USER /dev/usernet_ivshmem* /sys/module/usernet_ivshmem/drivers/pci\:usernet_ivshmem/*/resource2_wc
