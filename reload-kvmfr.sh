#!/bin/sh
modprobe -r kvmfr
modprobe -r uio_ivshmem
modprobe -r usernet_ivshmem
modprobe kvmfr
