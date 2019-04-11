CROSS_COMPILE ?= mips-linux-gnu-

ISVP_ENV_KERNEL_DIR = ../../../opensource/kernel

KDIR := ${ISVP_ENV_KERNEL_DIR}
MODULE_NAME := aw8733

all: modules

.PHONY: modules clean

$(MODULE_NAME)-objs := aw8733_ctl.o
obj-m := $(MODULE_NAME).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@
	
install:
	cp -av *.ko $(DRIVERS)	
clean:
	@rm -rf *.o *~ .depend .*.cmd  *.mod.c .tmp_versions *.ko *.symvers modules.order

