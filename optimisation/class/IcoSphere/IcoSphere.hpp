#ifndef ICOSPHERE_H
#define ICOSPHERE_H

#include <vector>
#include <unordered_map>
#include <cstdint>
#include "color.h"
#include <cmath>

struct Vec3 {
    float x, y, z;
    Vec3 operator+(const Vec3& b) const;
    Vec3 operator-(const Vec3& b) const;
    Vec3 operator*(float f) const;
    Vec3 normalize() const;

    float length() const {
        return sqrt(x * x + y * y + z * z);
    }
	float lengthSquared() const {
    	return x * x + y * y + z * z;
	}

};

class IcoSphere {
	public:
		IcoSphere();
		~IcoSphere();
		void generate(int subdivisions);
		std::vector<Vec3>			vertices;
		std::vector<unsigned int>	indices;
		unsigned int				subdivisions;
	private:
		std::unordered_map<uint64_t, unsigned int> middlePointCache_;
		unsigned int getMiddlePoint(unsigned int p1, unsigned int p2);
		void initIcosahedron();
		void subdivideTriangles();
};

#endif // ICOSPHERE_H