#!/bin/sh
sudo modprobe -r uio_ivshmem
sudo modprobe -r usernet_ivshmem
sudo modprobe kvmfr
sudo chown $USER:$USER /dev/kvmfr*
