#ifndef ICOSPHERE_H
#define ICOSPHERE_H

#include <vector>
#include <unordered_map>
#include <cstdint>
#include "color.h"

struct Vec3 {
    float x, y, z;
    Vec3 operator+(const Vec3& b) const;
    Vec3 operator-(const Vec3& b) const;
    Vec3 operator*(float f) const;
    Vec3 normalize() const;
};

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

	std::vector<ColorPoint> biomePalette; // Palette de couleurs pour les biomes
	std::vector<ColorPoint> mountainPalette;
	std::vector<ColorPoint> bigMountainPalette;
	std::vector<ColorPoint> oceanPalette;
	std::vector<ColorPoint> desertPalette;
	std::vector<ColorPoint> forestPalette;
	std::vector<ColorPoint> tundraPalette;
	std::vector<ColorPoint> snowPalette;
	std::vector<ColorPoint> mountainColors; // Palette de couleurs pour les montagnes

	PlanetConfig()
		: subdivisions(9), radius(1.0f), lvlSea(0.995f),
		  continentOctaves(3), continentPersistence(0.5f), continentNoiseScale(0.8f),
		  mountainOctaves(8), mountainPersistence(0.9f), mountainNoiseScale(2.0f),
		  bigMountainOctaves(8), bigMountainPersistence(0.7f), bigMountainNoiseScale(4.0f),
		  heightAmplitude(0.05f),
          oceanPalette({
				{-0.2f, HEX(0x000030)},
              {-0.1f, HEX(0x000041)},
			  {-0.005f, HEX(0x35698C)},
              {0.0f, HEX(0x40E0D0)}
			//  {0.0f, HEX(0xFF0000)},
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
			//{-1.0f, HEX(0x000000)},  // Gris foncé
			//{-0.2f, HEX(0x000000)},  // Gris
			//{0.0f, HEX(0x000000)},   // Gris
			{0.0f, HEX(0x000000)},
			{0.01f, HEX(0x222222)},
			{0.05f, HEX(0x333333)},
			{0.09f, HEX(0x666666)},
			{0.1f, HEX(0x777777)},
			//{0.15f, HEX(0x8c8c9c)},   // Gris
			//{0.11f, HEX(0x8c8c9c)},  // Gris
			//{0.2f, HEX(0x8c8c9c)},  // Gris
			{0.9f, HEX(0x8c8c9c)},
			//{1.0f, HEX(0xFFFFFF)}   // Blanc
		  }),
		  biomeOctaves(3), biomePersistence(0.6f), biomeNoiseScale(60.f)
		{}
};


class IcoSphere {
	public:
		IcoSphere(int subdivisions, float radius, float heightAmplitude, float lvlSea, float mountainNoiseScale);
		IcoSphere(const PlanetConfig& config);
		~IcoSphere();

		void generate();

		const std::vector<float>& getVertices() const { return sphereVertices_; }
		const std::vector<unsigned int>& getIndices() const { return sphereIndices_; }
		void setShowEquator(bool show) { isShowedEquator_ = show; }

	private:
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

		std::vector<ColorPoint> biomePalette_;
		std::vector<ColorPoint> mountainPalette_;
		std::vector<ColorPoint> bigMoutainPalette_;
		std::vector<ColorPoint> oceanPalette_;
		std::vector<ColorPoint> desertPalette_;
		std::vector<ColorPoint> forestPalette_;
		std::vector<ColorPoint> tundraPalette_;
		std::vector<ColorPoint> snowPalette_;
		std::vector<ColorPoint> mountainColors_; // Palette de couleurs pour les montagnes

		bool isShowedEquator_ = false; // Indique si l'équateur doit être affiché

		// Icosahedron vertices and indices

		std::vector<Vec3> vertices_;
		std::vector<unsigned int> indices_;

		std::unordered_map<uint64_t, unsigned int> middlePointCache_;

		std::vector<float> sphereVertices_;       // position(3) + couleur(3) + normale(3)
		std::vector<unsigned int> sphereIndices_;

		unsigned int getMiddlePoint(unsigned int p1, unsigned int p2);
		void initIcosahedron();
		void subdivideTriangles();
};

#endif // ICOSPHERE_H