# Makefile pour compiler le projet WebGL C++

EMSDK_PATH = ~/emsdk
EMCC = $(EMSDK_PATH)/upstream/emscripten/emcc

# Flags de compilation
CFLAGS = -O2 -s USE_WEBGL2=1 -s FULL_ES3=1 -s WASM=1
LDFLAGS = -s EXPORTED_FUNCTIONS='["_main"]' -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]'

# Cibles
all: main.js

main.js: main.cpp
	$(EMCC) main.cpp $(CFLAGS) $(LDFLAGS) -o main.js

clean:
	rm -f main.js main.wasm

serve: all
	@echo "Démarrage du serveur local sur http://localhost:8000"
	@echo "Ouvrez votre navigateur et allez à http://localhost:8000"
	python3 -m http.server 8000

setup:
	@echo "Activation de l'environnement Emscripten..."
	@echo "Exécutez : source $(EMSDK_PATH)/emsdk_env.sh"

help:
	@echo "Commandes disponibles:"
	@echo "  make        - Compile le projet"
	@echo "  make clean  - Nettoie les fichiers générés"
	@echo "  make serve  - Compile et lance un serveur local"
	@echo "  make setup  - Affiche les instructions de configuration"

.PHONY: all clean serve setup help