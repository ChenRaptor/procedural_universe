NAME = cube
SRC = main.cpp
OUT = $(NAME).html

all:
	em++ main.cpp -o cube.html \
	-s USE_WEBGL2=1 \
	-s FULL_ES3=1 \
	-s ALLOW_MEMORY_GROWTH=1 \
	-s WASM=1 \
	--shell-file shell_minimal.html \
	-O2
	
clean:
	rm -f *.js *.wasm *.o

serve: all
	python3 -m http.server
