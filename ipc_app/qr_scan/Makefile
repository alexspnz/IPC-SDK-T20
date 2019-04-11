CC=mips-linux-gnu-gcc
STRIP=mips-linux-gnu-strip

INGENIC_INC = $(SDK_DIR)/include    
QCAM_INC = $(MIDDLEWARE)/include
YSX_INC = $(MIDDLEWARE)/_install/include

C_FLAGS +=-muclibc
C_FLAGS += -I $(INGENIC_INC)
C_FLAGS += -I $(QCAM_INC)
C_FLAGS += -I $(YSX_INC)
    
INGENIC_LIB = $(MIDDLEWARE)/lib
YSX_INC = $(MIDDLEWARE)/_install/lib

LDFLAGS :=
LDFLAGS += -L $(INGENIC_LIB)
LDFLAGS += -L $(YSX_INC)
LDFLAGS += -lpthread -lstdc++ -limp  -lalog  -lrt  -lysx_vid -lysx_sys  -lsysutils -lz

INSTALL_DIR=$(APP_INSTALL)/bin

SRC += $(YSX_INC)/libysx_sys.a
SRC += qr_code.c
SRC += ./libzbar.a

target ?=qr_scan

all: $(target)

qr_scan:$(SRC)
	$(CC) $^ $(C_FLAGS) $(LDFLAGS)  -o $@  
	$(STRIP) $@



install:
	if [ ! -d $(INSTALL_DIR) ] ;then \
		mkdir -p $(INSTALL_DIR);\
	fi
	install -c $(target) $(INSTALL_DIR)

clean:
	rm -fr *.o $(target)

.PHONY:clean	

