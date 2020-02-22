CFLAGS += -Wall -O3

.PHONY: all clean

all: recfg

recfg: src/*.c src/*.h
	$(CC) $(CFLAGS) -o $@ -DRECFG_IO src/*.c

clean:
	rm -f recfg
