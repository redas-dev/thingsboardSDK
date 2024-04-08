TARGET = libthingsboard.so

SRC = $(wildcard *.c)
OBJS = $(patsubst %.c, %.o, $(SRC))
rootdir = $(realpath .)

.PHONY: all clean install uninstall

all: thingsboard.o $(TARGET)

thingsboard.o: thingsboard.c
	gcc -c -Wall -Werror -I$(rootdir)/includes/ -fPIC $(SRC)

$(TARGET): thingsboard.o
	gcc -shared -o $(TARGET) $(OBJS)

install:
	cp $(TARGET) /usr/lib/$(TARGET)
	cp ./includes/thingsboard.h /usr/include/thingsboard.h

uninstall:
	rm -f /usr/lib/$(TARGET)
	rm -f /usr/include/thingsboard.h

clean:
	rm -f $(OBJS) $(TARGET)