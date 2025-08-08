#ifndef ATMOSPHERE_H
#define ATMOSPHERE_H

#include "IcoSphere.hpp"
#include <GLES3/gl3.h>

class Atmosphere {
	public:
		Atmosphere(unsigned int subdivisions, float radius, Color atmosphereColor);
		~Atmosphere();
		void generate();
		void prepare_render();
		void render();
	private:
		unsigned int				_subdivisions;
		float						_radius;
		Color						_atmosphereColor;
		IcoSphere*					_wireframeSphere;
		std::vector<float>			_sphereVertices;
		std::vector<unsigned int>	_sphereIndices;
		GLuint _vao = 0;
		GLuint _vbo = 0;
		GLuint _ebo = 0;
		bool _buffersInitialized = false;

};

extern const char* vertexShaderAtmosphere;
extern const char* fragmentShaderAtmosphere;

#endif