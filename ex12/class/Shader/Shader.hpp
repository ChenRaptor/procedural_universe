#ifndef SHADER_H
#define SHADER_H

#include <GLES3/gl3.h>
#include <string>
#include <iostream>

class Shader {
public:
    GLuint ID;

    // Constructeur : compile vertex & fragment shaders puis link le programme
    Shader(const char* vertexSrc, const char* fragmentSrc) {
        GLuint vert = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vert, 1, &vertexSrc, nullptr);
        glCompileShader(vert);
        checkCompileErrors(vert, "VERTEX");

        GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(frag, 1, &fragmentSrc, nullptr);
        glCompileShader(frag);
        checkCompileErrors(frag, "FRAGMENT");

        ID = glCreateProgram();
        glAttachShader(ID, vert);
        glAttachShader(ID, frag);
        glLinkProgram(ID);
        checkCompileErrors(ID, "PROGRAM");

        glDeleteShader(vert);
        glDeleteShader(frag);
    }

    // Activer ce shader
    void use() const {
        glUseProgram(ID);
    }

    // (Optionnel) fonctions pour définir les uniforms...

private:
    void checkCompileErrors(GLuint shader, const std::string& type) {
        GLint success;
        GLchar infoLog[1024];
        if (type != "PROGRAM") {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
                std::cerr << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n"
                          << infoLog << "\n -- --------------------------------------------------- -- \n";
            }
        } else {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success) {
                glGetProgramInfoLog(shader, 1024, nullptr, infoLog);
                std::cerr << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n"
                          << infoLog << "\n -- --------------------------------------------------- -- \n";
            }
        }
    }
};

#endif
