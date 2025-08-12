# Compilateur Emscripten
EMCC = emcc

# Fichiers sources
SOURCES = main.cpp \
			perlin_noise.cpp \
			lookAt.cpp \
			class/IcoSphere/IcoSphere.cpp \
			color.cpp \
			class/Planet/Planet.cpp \
			class/Planet/shader.cpp \
			class/Atmosphere/Atmosphere.cpp \
			class/Atmosphere/shader.cpp OIT/oit.cpp

# Nom de la sortie
OUTPUT = triangle.js

# Options de compilation
# -s USE_WEBGL2=1 : pour WebGL2
# -s FULL_ES3=1 : pour supporter OpenGL ES 3.0 complet
# -s ALLOW_MEMORY_GROWTH : pour permettre la croissance de la mémoire à l'exécution
# -O2 : optimisation (optionnelle mais recommandée)
EMFLAGS = -s USE_WEBGL2=1 -s FULL_ES3=1 -s ALLOW_MEMORY_GROWTH -O3 -I ./class/IcoSphere -I ./class/Planet -I ./class/Atmosphere -I ./class/Shader -I ./class/FastSpatialLookup/ -I ./class/KDTree3D/ -I ./

all: $(OUTPUT)

$(OUTPUT): $(SOURCES)
	$(EMCC) $(SOURCES) -o $(OUTPUT) $(EMFLAGS)

clean:
	rm -f $(OUTPUT) triangle.wasm triangle.js.map

serve: all
	python3 -m http.server 8000 --bind 0.0.0.0


.PHONY: all clean
