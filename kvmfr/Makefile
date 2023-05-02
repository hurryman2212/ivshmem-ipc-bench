obj-m += kvmfr.o
USER  := $(shell whoami)
KVER ?= $(shell uname -r)
KDIR ?= /lib/modules/$(KVER)/build

all:
	make -C $(KDIR) M=$(shell pwd) modules

clean:
	make -C $(KDIR) M=$(shell pwd) clean

test:
	gcc test.c -Wall -Werror -g -Og -o test	

load: all
	grep -q '^kvmfr' /proc/modules && sudo rmmod kvmfr || true
	sudo insmod ./kvmfr.ko
	sudo chown $(USER):$(USER) /dev/kvmfr0

install:
	cp kvmfr.ko /lib/modules/$(shell uname -r)/kernel/drivers/char/
	depmod
uninstall:
	rm -f /lib/modules/$(shell uname -r)/kernel/drivers/char/kvmfr.ko
	depmod

.PHONY: test
