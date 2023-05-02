#!/bin/sh
sudo modprobe -r kvmfr
sudo modprobe -r uio_ivshmem
sudo modprobe -r usernet_ivshmem
sudo modprobe kvmfr
sudo chown $USER:$USER /dev/kvmfr* /sys/module/kvmfr/drivers/pci\:kvmfr/*/resource2_wc
