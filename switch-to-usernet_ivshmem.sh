#!/bin/sh
rmmod uio_ivshmem
modprobe -r uio
modprobe usernet_ivshmem
