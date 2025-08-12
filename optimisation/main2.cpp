#include "Planet.hpp"
int main()
{
	PlanetConfig cfg;
	cfg.subdivisions = 9;
	cfg.max_subdivisions = 7;
    cfg.lvlSea = 0.989;
    cfg.biomeNoiseScale = 5.f;
	Planet *planet = (new Planet(cfg));
	planet->generateAllLODs();


	
}


//g++ -std=c++17 -Wall -Wextra \
//    main2.cpp \
//    class/Planet/Planet.cpp \
//    class/IcoSphere/IcoSphere.cpp \
//    class/Atmosphere/Atmosphere.cpp \
//    class/FBMNoise/FBMNoise.cpp \
//    perlin_noise.cpp \
//    color.cpp \
//    -I ./class/Planet \
//    -I ./class/IcoSphere \
//    -I ./class/Atmosphere \
//    -I ./class/KDTree3D/ \
//    -I ./class/FBMNoise \
//    -I ./ \
//    -o main2

// cc main2.cpp class/Planet/Planet.cpp color.cpp class/IcoSphere/IcoSphere.cpp class/Atmosphere/Atmosphere.cpp -I ./class/Planet -I ./class/IcoSphere -I ./class/Atmosphere -I ./class/KDTree3D/ -I ./