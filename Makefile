TARGET = eboot
OBJS = src/example.o src/magpiedrv_cmd.o src/magpiedrv_init.o src/magpiedrv_msif_cmd.o src/magpiedrv_msif_init.o src/magpiedrv_msif_pkt.o src/magpiedrv_sema.o src/magpiedrv.o

INCDIR = src
CFLAGS = -G0 -Wall -Wextra -O0
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS) -c

PSP_FW_VERSION = 660

BUILD_PRX = 1

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = magpiedrv example

LIBDIR = src/LibPspExploit
LIBS = -lpspexploit -lpsprtc
LDFLAGS = -L.

PSPSDK = $(shell psp-config --pspsdk-path)

include $(PSPSDK)/lib/build.mak
