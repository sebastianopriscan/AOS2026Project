ifneq ($(KERNELRELEASE),)

obj-m += throttleA.o

ccflags-y:= -I ${src} -I ${src}/lib

throttleA-y := mod_main.o api/api.o api/ioctl.o

else

TABLE_ADDR = $(shell sudo cat /sys/module/the_usctm/parameters/sys_call_table_address)

start:
	sudo insmod throttleA.ko the_syscall_table=$(TABLE_ADDR)
	sudo mknod /dev/throttleA-api c $$(sudo cat /sys/module/throttleA/parameters/major) 0

stop:
	-sudo rm /dev/throttleA-api
	sudo rmmod throttleA.ko

all:
	./utils/password_gen/passwordgen.sh
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	rm ./password_setup/password.c

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

bundle:
	zip bundle.zip -r ./* -x bundle.zip -x .vscode

endif