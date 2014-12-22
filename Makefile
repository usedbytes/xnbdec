
TARGET := xnbdec
SRC := $(TARGET).c xnb_object.c xnb_obj_sound_effect.c
OBJS = $(patsubst %.c,%.o,$(SRC))

CFLAGS = -Wall -g --std=c99

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o $(TARGET)

.PHONY: clean all
