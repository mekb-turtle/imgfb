.PHONY: all
all: clean compile
clean:
	rm -fv -- imgfb
compile: imgfb
install:
	install -Dm755 imgfb /usr/bin/imgfb
uninstall:
	rm -fv -- /usr/bin/imgfb
