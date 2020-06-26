BIN_PATH = /usr/bin

CROSS_COMPILE = arm-linux-gnueabihf-

CC = /usr/bin/gcc

CP = /usr/bin/sudo /bin/cp
DEL = /bin/rm -f

TARGET = h264enc

SRC = main.c \
	  h264enc.c \
	  video_device.c \
	  ve.c \
	  csc.c \
	  overlay.c


CFLAGS = -Wall -O3 -I .

LIBDIR=
LDFLAGS = -lrt -lpthread
LIBS = -L/usr/local/lib

MAKEFLAGS += -rR --no-print-directory

DEP_CFLAGS = -MD -MP -MQ $@
DEFS = -DCPU_HAS_NEON

OBJ = $(addsuffix .o,$(basename $(SRC)))
DEP = $(addsuffix .d,$(basename $(SRC)))

.PHONY: clean all

all: $(TARGET)
$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) $(LIBS) -o $@ $(LDFLAGS)
	-$(CP) $(TARGET) $(BIN_PATH)

clean:
	$(DEL) $(OBJ)
	$(DEL) $(DEP)
	$(DEL) $(TARGET)

%.o: %.c
	$(CC) $(DEP_CFLAGS) $(DEFS) $(CFLAGS) -c $< -o $@ $(LDFLAGS)

%.o: %.S $(MKFILE) $(LDSCRIPT)
	$(CC) -c $< -o $@	$(LDFLAGS) 

include $(wildcard $(DEP))
