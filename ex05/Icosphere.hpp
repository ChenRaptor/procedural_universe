#ifndef ICOSPHERE_H
#define ICOSPHERE_H

#include <vector>
#include <unordered_map>
#include <cstdint>

struct Vec3 {
    float x, y, z;
    Vec3 operator+(const Vec3& b) const;
    Vec3 operator-(const Vec3& b) const;
    Vec3 operator*(float f) const;
    Vec3 normalize() const;
};

struct IcoSphereConfig {
    unsigned int subdivisions;
    float radius;
    float lvlSea;

    unsigned int continentOctaves;
    float continentPersistence;
    float continentNoiseScale;

    unsigned int mountainOctaves;
    float mountainPersistence;
    float mountainNoiseScale;

    float heightAmplitude;

    unsigned int biomeOctaves;
    float biomePersistence;
    float biomeNoiseScale;

    IcoSphereConfig()
        : subdivisions(9), radius(1.0f), lvlSea(0.989f),
          continentOctaves(5), continentPersistence(0.5f), continentNoiseScale(1.0f),
          mountainOctaves(5), mountainPersistence(0.5f), mountainNoiseScale(2.0f),
          heightAmplitude(0.02f),
          biomeOctaves(5), biomePersistence(0.5f), biomeNoiseScale(1.0f)
        {}
};


class IcoSphere {
public:
    IcoSphere(int subdivisions, float radius, float heightAmplitude, float lvlSea, float mountainNoiseScale);
    IcoSphere(const IcoSphereConfig& config);
    ~IcoSphere();

    void generate();

    const std::vector<float>& getVertices() const { return sphereVertices_; }
    const std::vector<unsigned int>& getIndices() const { return sphereIndices_; }

private:
    unsigned int subdivisions_;
	float radius_;
	float lvlSea_;

	unsigned int continentOctaves_;
	float continentPersistence_;
	float continentNoiseScale_;

	unsigned int mountainOctaves_;
	float mountainPersistence_;
	float mountainNoiseScale_;

	float heightAmplitude_; // Amplitude de la hauteur des montagnes

	unsigned int biomeOctaves_;
	float biomePersistence_;
	float biomeNoiseScale_;

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