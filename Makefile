
CFLAGS += -g -Wall

ifndef PREFIX
PREFIX = /usr/local
endif

all: dhplug.so

dhplug.so: dhplug.o libgeanynotebook.a
	gcc $(CFLAGS) dhplug.o -o dhplug.so -shared \
	`pkg-config --libs geany libdevhelp-1.0 webkit-1.0` \
	-L. -lgeanynotebook

dhplug.o: dhplug.c plugincommon.h
	gcc $(CFLAGS) -c dhplug.c -fPIC \
	`pkg-config --cflags geany libdevhelp-1.0 webkit-1.0`

libgeanynotebook.a: main_notebook.o
	ar rcs libgeanynotebook.a main_notebook.o

main_notebook.o: main_notebook.c main_notebook.h plugincommon.h
	gcc $(CFLAGS) -c main_notebook.c -fPIC `pkg-config --cflags geany`

clean:
	rm -f dhplug.o main_notebook.o dhplug.so libgeanynotebook.a

install:
	install -m 0644 dhplug.so $(PREFIX)/lib/geany

uninstall:
	rm -f $(PREFIX)/lib/geany/dhplug.so
