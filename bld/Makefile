LIB = libchain

OBJECTS = \
	chain.o \

DEPS += \
	libmsp \
  libcapybara\

override SRC_ROOT = ../../src

override CFLAGS += \
	-I$(SRC_ROOT) \
	-I$(SRC_ROOT)/include/libchain \

include ../Makefile.config
