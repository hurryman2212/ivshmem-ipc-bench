#!/bin/sh
sudo modprobe -r uio_ivshmem
sudo modprobe -r kvmfr
sudo modprobe usernet_ivshmem
sudo chown $USER:$USER /dev/usernet_ivshmem*
