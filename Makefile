ifneq ($(KERNELRELEASE),)
	obj-m := user.o
	user-y := main.o source.o
	user-y += tunnel/tunnel.o
	user-y += llc_encap/llc_encap.o
else
	PWD := $(shell pwd)
	KDIR := /lib/modules/$(shell uname -r)/build
default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
clean:
	find . -name "*.o" -delete
	rm -rf *.o .*.cmd *.ko *.mod.c .tmp_versions *.symvers *.order *~
endif

# CONFIG_DEBUG_INFO=y

