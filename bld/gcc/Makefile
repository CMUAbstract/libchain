EXEC = libchain.a

OBJECTS = \
	chain.o \

include $(MAKER_ROOT)/Makefile.gcc

SRC_ROOT = ../../src

CFLAGS += \
	-I $(LIBMSP_ROOT)/src/include \
	-I $(SRC_ROOT)/include/libchain \

ifeq ($(LIBCHAIN_ENABLE_DIAGNOSTICS),1)
CFLAGS += -DLIBCHAIN_ENABLE_DIAGNOSTICS
endif

VPATH = $(SRC_ROOT)
