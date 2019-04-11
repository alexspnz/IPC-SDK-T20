export SDK_PATH?=/home/yh/share/workspace/Ingenic-SDK/Ingenic-SDK-T10T20-3.12.0-20180320

CROSS  := mips-linux-uclibc-gnu-

export CFLAGS
export IMP_PATH?=$(SDK_PATH)
export DRV_ROOT?=$(SDK_PATH)/opensource/drivers
export CC = $(CROSS)gcc
export CPP = $(CROSS)g++
export CXX = $(CROSS)g++
export LINK = $(CROSS)gcc
export STRIP = $(CROSS)strip
export AR = $(CROSS)ar
