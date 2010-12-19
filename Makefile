
CFLAGS += -g -Wall

ifndef PREFIX
PREFIX = /usr/local
endif

all: dhplug.so

dhplug.so: dhplug.o
	gcc $(CFLAGS) dhplug.o -o dhplug.so -shared \
	`pkg-config --libs geany libdevhelp-1.0 webkit-1.0`

dhplug.o: dhplug.c
	gcc $(CFLAGS) -c dhplug.c -fPIC \
	`pkg-config --cflags geany libdevhelp-1.0 webkit-1.0`

clean:
	rm -f dhplug.o dhplug.so

install:
	install -m 0644 dhplug.so $(PREFIX)/lib/geany

uninstall:
	rm -f $(PREFIX)/lib/geany/dhplug.so
