#include "render/Shader.h"

#include <cstdio>
#include <string>
#include <utility>

namespace {

GLuint compile(GLenum type, std::string_view src) {
    const GLuint shader = glCreateShader(type);
    const char* data = src.data();
    const GLint length = static_cast<GLint>(src.size());
    glShaderSource(shader, 1, &data, &length);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::string log(static_cast<std::size_t>(logLength > 1 ? logLength : 1), '\0');
        glGetShaderInfoLog(shader, logLength, nullptr, log.data());
        std::fprintf(stderr, "%s shader error:\n%s\n", type == GL_VERTEX_SHADER ? "Vertex" : "Fragment", log.c_str());
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

}  // namespace

Shader::~Shader() {
    if (program_) glDeleteProgram(program_);
}

Shader::Shader(Shader&& other) noexcept : program_(std::exchange(other.program_, 0)) {}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        if (program_) glDeleteProgram(program_);
        program_ = std::exchange(other.program_, 0);
    }
    return *this;
}

bool Shader::build(std::string_view vertexSrc, std::string_view fragmentSrc) {
    const GLuint vs = compile(GL_VERTEX_SHADER, vertexSrc);
    const GLuint fs = compile(GL_FRAGMENT_SHADER, fragmentSrc);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }

    const GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint logLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        std::string log(static_cast<std::size_t>(logLength > 1 ? logLength : 1), '\0');
        glGetProgramInfoLog(program, logLength, nullptr, log.data());
        std::fprintf(stderr, "Shader link error:\n%s\n", log.c_str());
        glDeleteProgram(program);
        return false;
    }

    if (program_) glDeleteProgram(program_);
    program_ = program;
    return true;
}
