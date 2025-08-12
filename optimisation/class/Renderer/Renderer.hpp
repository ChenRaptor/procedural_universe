#ifndef RENDERER_H
#define RENDERER_H

class Renderer {
	public:
		Renderer();
		~Renderer();
	private:
		float model[16];
		float view[16];
		float projection[16];
}


#endif