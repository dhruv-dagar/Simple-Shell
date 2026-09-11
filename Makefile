CC := gcc
CFLAGS := -Wall -Wextra -O2
TARGET := simple-shell

.PHONY: all clean

all: $(TARGET)

$(TARGET): shell.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET) *.o
