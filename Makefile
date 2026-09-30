ifneq ($(KERNELRELEASE),)

obj-m += throttleA.o

ccflags-y:= -I ${src} -I ${src}/lib -std=gnu11

throttleA-y := mod_main.o api/api.o api/ioctl.o hash_table/hash_table.o hash_table/tree.o preempt_kprobe/preempt_kprobe.o probing/probing.o
throttleA-y += throttler_status/throttler_status.o timers/timers.o oracles/oracles.o stats/stats.o syscalls/syscalls.o utils/strings.o

else

PARAMS_DIR := /sys/module/throttleA/parameters

# ioctl chardev, minor 0
API_NODE   := /dev/throttleA-api
# dump chardev, minor 0: monitored uids, minor 1: monitored paths
UIDS_NODE  := /dev/throttleA-uids
PATHS_NODE := /dev/throttleA-paths

start:
	sudo insmod throttleA.ko 
	sudo mknod $(API_NODE) c $$(sudo cat $(PARAMS_DIR)/ioctl_major) 0
	sudo mknod $(UIDS_NODE) c $$(sudo cat $(PARAMS_DIR)/dump_major) 0
	sudo mknod $(PATHS_NODE) c $$(sudo cat $(PARAMS_DIR)/dump_major) 1

stop:
	-sudo rm -f $(API_NODE) $(UIDS_NODE) $(PATHS_NODE)
	sudo rmmod throttleA.ko

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

bundle:
	tar -czf bundle.tgz --exclude=bundle.tgz --exclude=.vscode --exclude=throttleA.ko ./*

endif