#!/bin/sh
sudo modprobe -r kvmfr
sudo modprobe -r usernet_ivshmem
sudo modprobe uio_ivshmem
sudo chown $USER:$USER /dev/uio* /sys/class/uio/uio*/device/resource2_wc 
