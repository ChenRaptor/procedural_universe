#ifndef ATMOSPHERE_H
#define ATMOSPHERE_H

#include "IcoSphere.hpp"
#include <GLES3/gl3.h>
#include "glm/glm/glm.hpp"

struct Sphere {
	std::vector<float> sphereVertices;
	std::vector<unsigned int> sphereIndices;
};

class Atmosphere {
	public:
		Atmosphere(unsigned int max_subdivisions, float radius, Color atmosphereColor);
		~Atmosphere();
		void generate(unsigned int subdivision);
		void generateAllLODs();
		void prepare_render();
		void setLODSelected(unsigned int lod);
		void render();
	private:
		unsigned int				_max_subdivisions;
		float						_radius;
		Color						_atmosphereColor;
		std::vector<float>			_sphereVertices;
		std::vector<unsigned int>	_sphereIndices;
		GLuint _vao = 0;
		GLuint _vbo = 0;
		GLuint _ebo = 0;
		bool _buffersInitialized = false;


		unsigned int 			_LODSelected;
		IcoSphere*				_LODMaxsolid = nullptr;
		std::vector<glm::vec3>	_LODMaxVertices;
		std::vector<Color>		_LODMaxColors;
		std::vector<Sphere> 	_LODLevels;

};

extern const char* vertexShaderAtmosphere;
extern const char* fragmentShaderAtmosphere;



//#ifndef ATMOSPHERE_H
//#define ATMOSPHERE_H

//#include "IcoSphere.hpp"
//#include <GLES3/gl3.h>
//#include <vector>
//#include <memory>

//class KDTree3D;  // déclaration anticipée

//class Atmosphere {
//public:
//    Atmosphere(unsigned int maxSubdivisions, float radius, Color atmosphereColor);
//    ~Atmosphere();

//    void generate();  // pour backward compat
//    void generate(unsigned int targetSubdivision);  // génération LOD avec k-d tree et Perlin optimisé
//    void prepare_render();
//    void render();

//private:
//    unsigned int                _maxSubdivisions;
//    float                       _radius;
//    Color                       _atmosphereColor;
//    IcoSphere*                  _wireframeSphere;
//    std::vector<float>          _sphereVertices;
//    std::vector<unsigned int>   _sphereIndices;
//    GLuint                      _vao = 0;
//    GLuint                      _vbo = 0;
//    GLuint                      _ebo = 0;
//    bool                        _buffersInitialized = false;

//    std::vector<float>          _perlinValuesMaxSubdivision;
//    std::unique_ptr<KDTree3D>   _kdTreeMaxSubdivision;
//};

//extern const char* vertexShaderAtmosphere;
//extern const char* fragmentShaderAtmosphere;

//#endif

#endif