#pragma once

#include <glad/gl.h>

#include <string_view>

// A linked GLSL program. Move-only; deletes the GL program on destruction.
class Shader {
public:
    Shader() = default;
    ~Shader();

    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    // Compiles and links; prints the GLSL log and returns false on failure.
    bool build(std::string_view vertexSrc, std::string_view fragmentSrc);

    void use() const { glUseProgram(program_); }
    GLint uniform(const char* name) const { return glGetUniformLocation(program_, name); }
    bool valid() const { return program_ != 0; }

private:
    GLuint program_ = 0;
};
