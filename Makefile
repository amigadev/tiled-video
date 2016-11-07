CCFLAGS=-I/usr/local/include -O2
LDFLAGS=-L/usr/local/lib -lSDL2 -lSDL2_image

all: out converter

out:
	mkdir out

clean:
	rm -rf out converter player

converter: out/converter.o out/renderer.o out/tiles.o out/stream.o out/frames.o
	$(CC) -o $@ $(LDFLAGS) $^

player: out/player.o out/fastlz.o
	$(CC) -o $@ $(LDFLAGS) $^

out/converter.o: src/converter.c src/renderer.h src/stream.h src/frames.h src/tiles.h
	$(CC) -c -o $@ $(CCFLAGS) $<

out/renderer.o: src/renderer.c src/renderer.h
	$(CC) -c -o $@ $(CCFLAGS) $<

out/tiles.o: src/tiles.c src/tiles.h
	$(CC) -c -o $@ $(CCFLAGS) $<

out/stream.o: src/stream.c src/stream.h src/frames.h src/tiles.h
	$(CC) -c -o $@ $(CCFLAGS) $<

out/frames.o: src/frames.c src/frames.h src/tiles.h
	$(CC) -c -o $@ $(CCFLAGS) $<

