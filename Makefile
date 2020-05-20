LIB = libchain

OBJECTS = \
	chain.o \

override DEPS += \
	libmsp \

override SRC_ROOT = ../../src

override CFLAGS += \
	-I $(SRC_ROOT)/include/$(LIB) \

ifeq ($(LIBCHAIN_ENABLE_DIAGNOSTICS),1)
override CFLAGS += -DLIBCHAIN_ENABLE_DIAGNOSTICS
endif

include $(MAKER_ROOT)/Makefile.$(TOOLCHAIN)
