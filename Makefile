CCFLAGS=-I/usr/local/include -O2 -DNDEBUG
LDFLAGS=-L/usr/local/lib -lSDL2

all: out converter player

out:
	mkdir out

clean:
	rm -rf out converter player

out/%.o: src/%.c
	$(CC) -c -o $@ $(CCFLAGS) $<

converter: out/converter.o out/renderer.o out/tiles.o out/stream.o out/frames.o out/buffer.o out/bits.o out/blocks.o out/fastlz.o
	$(CC) -o $@ $^ $(LDFLAGS)

player: out/player.o out/renderer.o out/tiles.o out/stream.o out/frames.o out/buffer.o out/bits.o out/blocks.o out/fastlz.o
	$(CC) -o $@ $^ $(LDFLAGS)

out/converter.o: src/converter.c src/renderer.h src/stream.h src/frames.h src/tiles.h src/bits.h src/blocks.h
out/player.o: src/player.c src/renderer.h src/stream.h src/frames.h src/tiles.h src/blocks.h
out/renderer.o: src/renderer.c src/renderer.h
out/tiles.o: src/tiles.c src/tiles.h src/blocks.h
out/stream.o: src/stream.c src/stream.h src/frames.h src/tiles.h src/buffer.h src/bits.h
out/frames.o: src/frames.c src/frames.h src/tiles.h
out/buffer.o: src/buffer.c src/buffer.h
out/bits.o: src/bits.c src/bits.h
out/blocks.o: src/blocks.c src/blocks.h

out/fastlz.o: external/fastlz/fastlz.c external/fastlz/fastlz.h
	$(CC) -c -o $@ $(CCFLAGS) $<
