CCFLAGS=-I/usr/local/include -O2
LDFLAGS=-L/usr/local/lib -lSDL2 -lSDL2_image

all: out converter player

out:
	mkdir out

clean:
	rm -rf out converter player

converter: out/converter.o out/fastlz.o
	$(CC) -o $@ $(LDFLAGS) $^

player: out/player.o out/fastlz.o
	$(CC) -o $@ $(LDFLAGS) $^

out/converter.o: src/converter.c src/video.h
	$(CC) -c -o $@ $(CCFLAGS) $<

out/player.o: src/player.c src/video.h
	$(CC) -c -o $@ $(CCFLAGS) $<

out/fastlz.o: external/fastlz/fastlz.c
	$(CC) -c -o $@ $(CCFLAGS) $<
