ifneq ($(KERNELRELEASE),)

obj-m += throttleA.o

ccflags-y:= -I ${src} -I ${src}/lib -std=gnu11

throttleA-y := mod_main.o api/api.o api/ioctl.o hash_table/hash_table.o preempt_kprobe/preempt_kprobe.o probing/probing.o
throttleA-y += throttler_status/throttler_status.o timers/timers.o

else

start:
	sudo insmod throttleA.ko 
	sudo mknod /dev/throttleA-api c $$(sudo cat /sys/module/throttleA/parameters/major) 0

stop:
	-sudo rm /dev/throttleA-api
	sudo rmmod throttleA.ko

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

bundle:
	tar -czf bundle.tgz --exclude=bundle.tgz --exclude=.vscode ./*

endif