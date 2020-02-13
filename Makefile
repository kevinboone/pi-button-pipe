PROG=pi-button-pipe
VERSION=0.0.2

all: $(PROG)

$(PROG): main.c
	gcc -DVERSION=\"$(VERSION)\" -s -Wall -O3 -o $(PROG) main.c

clean:
	rm -f *.o $(PROG)

install: $(PROG)
	cp -p $(PROG) /usr/bin


