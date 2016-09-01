LIB = libchain

OBJECTS = \
	chain.o \

DEPS += \
	libmsp \

override SRC_ROOT = ../../src

CFLAGS += \
	-I $(SRC_ROOT)/include/libchain \

ifeq ($(LIBCHAIN_ENABLE_DIAGNOSTICS),1)
CFLAGS += -DLIBCHAIN_ENABLE_DIAGNOSTICS
endif
