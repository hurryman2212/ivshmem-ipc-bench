#!/bin/sh
sudo rmmod uio_ivshmem
sudo modprobe -r uio
sudo modprobe usernet_ivshmem
