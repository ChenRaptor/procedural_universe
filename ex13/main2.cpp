#include "Planet.hpp"
int main()
{
	PlanetConfig cfg;
	Planet *planet = &(new Planet(cfg))->generateAllLODs();
}


//g++ -std=c++17 -Wall -Wextra \
//    main2.cpp \
//    class/Planet/Planet.cpp \
//    class/IcoSphere/IcoSphere.cpp \
//    class/Atmosphere/Atmosphere.cpp \
//    perlin_noise.cpp \
//    color.cpp \
//    -I ./class/Planet \
//    -I ./class/IcoSphere \
//    -I ./class/Atmosphere \
//    -I ./class/KDTree3D/ \
//    -I ./ \
//    -o main2

// cc main2.cpp class/Planet/Planet.cpp color.cpp class/IcoSphere/IcoSphere.cpp class/Atmosphere/Atmosphere.cpp -I ./class/Planet -I ./class/IcoSphere -I ./class/Atmosphere -I ./class/KDTree3D/ -I ./