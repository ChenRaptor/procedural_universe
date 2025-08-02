# Makefile simple pour Cube WebGL C++

# Variables
SOURCE = main.cpp
OUTPUT = cube
EMSCRIPTEN_PATH = $(HOME)/emsdk

# Flags de compilation
EMFLAGS = -s USE_WEBGL2=1 \
          -s FULL_ES2=1 \
          -s EXPORTED_FUNCTIONS='["_main","_toggleRotation","_changeColor","_resetView"]' \
          -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' \
          -s ALLOW_MEMORY_GROWTH=1 \
          -O2

# Activation automatique d'Emscripten
ACTIVATE_EMSCRIPTEN = source $(EMSCRIPTEN_PATH)/emsdk_env.sh &&

# Cibles
all: $(OUTPUT).html

$(OUTPUT).html: $(SOURCE)
	@echo "🔨 Compilation de $(SOURCE)..."
	$(ACTIVATE_EMSCRIPTEN) emcc $(SOURCE) -o $(OUTPUT).html $(EMFLAGS)
	@echo "✅ Compilation terminée: $(OUTPUT).html"

clean:
	rm -f $(OUTPUT).html $(OUTPUT).js $(OUTPUT).wasm

serve: $(OUTPUT).html
	@echo "🌐 Serveur sur http://localhost:8000/$(OUTPUT).html"
	python3 -m http.server 8000 || python -m SimpleHTTPServer 8000

.PHONY: all clean serve