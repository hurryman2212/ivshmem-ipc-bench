#!/bin/sh
sudo rmmod uio_ivshmem
sudo modprobe -r uio
sudo modprobe usernet_ivshmem
sudo chown $USER:$USER /dev/usernet_ivshmem*
