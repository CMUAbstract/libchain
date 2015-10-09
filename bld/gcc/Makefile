EXEC = libchain.a

OBJECTS = \
	chain.o \

include $(MAKER_ROOT)/Makefile.gcc

SRC_ROOT = ../../src

CFLAGS += \
	-I $(SRC_ROOT)/include/libchain \

ifdef LIBCHAIN_ENABLE_DIAGNOSTICS
CFLAGS += -DLIBCHAIN_ENABLE_DIAGNOSTICS
endif

VPATH = $(SRC_ROOT)
