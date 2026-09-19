#include "render/shader.h"

#include <cstdio>

#include "render/gl.h"

namespace render {

namespace {

unsigned int compile_stage(GLenum stage, const char* src) {
    const unsigned int shader = glCreateShader(stage);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        GLsizei len = 0;
        glGetShaderInfoLog(shader, sizeof(log), &len, log);
        std::fprintf(stderr, "render::Shader: %s stage failed to compile:\n%.*s\n",
                     stage == GL_VERTEX_SHADER ? "vertex" : "fragment", static_cast<int>(len), log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

}  // namespace

bool Shader::compile(const char* vertex_src, const char* fragment_src) {
    const unsigned int vs = compile_stage(GL_VERTEX_SHADER, vertex_src);
    if (vs == 0) return false;
    const unsigned int fs = compile_stage(GL_FRAGMENT_SHADER, fragment_src);
    if (fs == 0) {
        glDeleteShader(vs);
        return false;
    }

    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);

    GLint ok = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        GLsizei len = 0;
        glGetProgramInfoLog(program_, sizeof(log), &len, log);
        std::fprintf(stderr, "render::Shader: link failed:\n%.*s\n", static_cast<int>(len), log);
        glDeleteShader(vs);
        glDeleteShader(fs);
        glDeleteProgram(program_);
        program_ = 0;
        return false;
    }

    // Shader objects aren't needed once linked into the program.
    glDetachShader(program_, vs);
    glDetachShader(program_, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return true;
}

void Shader::destroy() {
    if (program_) {
        glDeleteProgram(program_);
        program_ = 0;
    }
}

void Shader::use() const { glUseProgram(program_); }

void Shader::set_mat4(const char* name, const Mat4& m) const {
    const GLint loc = glGetUniformLocation(program_, name);
    glUniformMatrix4fv(loc, 1, GL_FALSE, m.m);
}

void Shader::set_vec3(const char* name, float x, float y, float z) const {
    const GLint loc = glGetUniformLocation(program_, name);
    glUniform3f(loc, x, y, z);
}

void Shader::set_float(const char* name, float v) const {
    const GLint loc = glGetUniformLocation(program_, name);
    glUniform1f(loc, v);
}

void Shader::set_int(const char* name, int v) const {
    const GLint loc = glGetUniformLocation(program_, name);
    glUniform1i(loc, v);
}

}  // namespace render
