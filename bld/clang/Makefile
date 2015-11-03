EXEC = libchain.a.bc

OBJECTS = \
	chain.o \

include $(MAKER_ROOT)/Makefile.clang

SRC_DIR = ../../src

CFLAGS += \
	-I $(LIBMSP_ROOT)/src/include \
	-I $(SRC_DIR)/include/libchain \

VPATH = $(SRC_DIR)
