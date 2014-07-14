CCFLAGS=-I/usr/local/include -O2
LDFLAGS=-L/usr/local/lib -lSDL2 -lSDL2_image

all: converter player

clean:
	rm -f *.o
	rm -f converter player
