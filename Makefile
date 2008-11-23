all: wacomrotate

wacomrotate: wacomrotate.c
	gcc -Os -Wall -lX11 -lXrandr wacomrotate.c -o wacomrotate

clean:
	rm -f wacomrotate
