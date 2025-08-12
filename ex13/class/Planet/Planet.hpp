#ifndef PLANET_H
#define PLANET_H

#include <vector>
#include <unordered_map>
#include <map>
#include <cstdint>
#include "color.h"
#include "IcoSphere.hpp"
#include "Atmosphere.hpp"

struct PlanetConfig {
    unsigned int subdivisions;
    float radius;
    float lvlSea;

    unsigned int continentOctaves;
    float continentPersistence;
    float continentNoiseScale;

	unsigned int bigMountainOctaves;
	float bigMountainPersistence;
	float bigMountainNoiseScale;

    unsigned int mountainOctaves;
    float mountainPersistence;
    float mountainNoiseScale;

    float heightAmplitude;

    unsigned int biomeOctaves;
    float biomePersistence;
    float biomeNoiseScale;

	unsigned int max_subdivisions;

	std::vector<ColorPoint> biomePalette; // Palette de couleurs pour les biomes
	std::vector<ColorPoint> mountainPalette;
	std::vector<ColorPoint> bigMountainPalette;
	std::vector<ColorPoint> oceanPalette;
	std::vector<ColorPoint> desertPalette;
	std::vector<ColorPoint> forestPalette;
	std::vector<ColorPoint> tundraPalette;
	std::vector<ColorPoint> snowPalette;
	std::vector<ColorPoint> mountainColors; // Palette de couleurs pour les montagnes

	bool debug;

	PlanetConfig() :
		subdivisions(9), radius(1.0f), lvlSea(0.995f),
		continentOctaves(3), continentPersistence(0.5f), continentNoiseScale(0.8f),
		mountainOctaves(8), mountainPersistence(0.9f), mountainNoiseScale(2.0f),
		bigMountainOctaves(8), bigMountainPersistence(0.7f), bigMountainNoiseScale(4.0f),
		heightAmplitude(0.05f),
		oceanPalette({
			{-0.2f, HEX(0x000030)},
			{-0.1f, HEX(0x000041)},
			{-0.005f, HEX(0x35698C)},
			{0.0f, HEX(0x40E0D0)}
		}),
		desertPalette({
			{0.0f, HEX(0xC2B280)},
			{0.5f, HEX(0xEEDC82)},
			{1.0f, HEX(0xFFE4B5)}
		}),
		forestPalette({
			{-1.0f, HEX(0x05400A)},
			{0.0f, HEX(0x527048)},
			{1.0f, HEX(0x7CFC00)},
		}),
		tundraPalette({
			{0.0f, HEX(0x9FA8A3)},
			{1.0f, HEX(0xDCE3E1)}
		}),
		mountainPalette({
			{0.0f, HEX(0x555555)},
			{1.0f, HEX(0xDDDCDC)}
		}),
		snowPalette({
			{0.0f, HEX(0xEEEEEE)},
			{1.0f, HEX(0xFFFFFF)}
		}),
		biomePalette({
			{0.0f, HEX(0x000000)},  // Noir
			{1.0f, HEX(0xf0ddc5)}   // Rouge
		}),
		bigMountainPalette({
			{0.0f, HEX(0XFFFFFF)},  // Vert moyen
			{0.3f, HEX(0x999999)},  // Gris
			{0.7f, HEX(0xCCCCCC)},  // Gris clair
			{1.0f, HEX(0xFFFFFF)}   // Blanc
		}),
		mountainColors({
			{0.0f, HEX(0x000000)},
			{0.01f, HEX(0x222222)},
			{0.05f, HEX(0x333333)},
			{0.09f, HEX(0x666666)},
			{0.1f, HEX(0x777777)},
			{0.9f, HEX(0x8c8c9c)},
		}),
		biomeOctaves(3), biomePersistence(0.6f), biomeNoiseScale(60.f),
		debug(true),  // Par défaut, pas de debug
		max_subdivisions(9)
	{}
};

class Planet {
	public:
		Planet(const PlanetConfig& config);
		~Planet();
		void generate(unsigned int subdivision);
		void prepare_render();
		void render();
		const std::vector<float>& getVertices() const { return _sphereVertices; }
		const std::vector<unsigned int>& getIndices() const { return _sphereIndices; }
		void setShowEquator(bool show) { isShowedEquator_ = show; }
		void generateAtmosphere();
		Planet& generateAllLODs();
		void setLODSelected(unsigned int lod);
		Atmosphere* getAtmosphere() const { return _atmosphere; }
	private:
		void computeVertices(const Vec3& v, size_t i);
		std::map<int, IcoSphere*> lodLevels;
		
		unsigned int _max_subdivisions; // Nombre maximum de subdivisions pour l'icosphère
		unsigned int subdivisions_;
		float radius_;
		float lvlSea_;

		unsigned int continentOctaves_;
		float continentPersistence_;
		float continentNoiseScale_;

		unsigned int bigMountainOctaves_;
		float bigMountainPersistence_;
		float bigMountainNoiseScale_;

		unsigned int mountainOctaves_;
		float mountainPersistence_;
		float mountainNoiseScale_;

		float heightAmplitude_; // Amplitude de la hauteur des montagnes

		unsigned int biomeOctaves_;
		float biomePersistence_;
		float biomeNoiseScale_;

		std::array<const std::vector<ColorPoint>*, 6> biomePalettes_;

		std::vector<ColorPoint> biomePalette_;
		std::vector<ColorPoint> mountainPalette_;
		std::vector<ColorPoint> bigMoutainPalette_;
		std::vector<ColorPoint> oceanPalette_;
		std::vector<ColorPoint> desertPalette_;
		std::vector<ColorPoint> forestPalette_;
		std::vector<ColorPoint> tundraPalette_;
		std::vector<ColorPoint> snowPalette_;
		std::vector<ColorPoint> mountainColors_;

		bool isShowedEquator_ = false; // Indique si l'équateur doit être affiché

		std::vector<float> _sphereVertices;
		std::vector<unsigned int> _sphereIndices;

		IcoSphere* _wireframeSphere;
		Atmosphere* _atmosphere;

		GLuint _vao = 0;
		GLuint _vbo = 0;
		GLuint _ebo = 0;
		bool _buffersInitialized = false;
		bool _debug;

		unsigned int 			_LODSelected;
		IcoSphere*				_LODMaxsolid = nullptr;
		std::vector<glm::vec3>	_LODMaxVertices;
		std::vector<Color>		_LODMaxColors;
		std::vector<Sphere> 	_LODLevels;

		std::array<Color, 256UL> _LUTocean;
};

extern const char* fragmentShaderPlanet;
extern const char* vertexShaderPlanet;

#endif