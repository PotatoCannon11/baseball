#pragma once

#include "render/mat4.h"

// Thin wrapper over a compiled+linked GL program. Small shaders only, per
// spec ("Simple readable 3D... Small shaders").
namespace render {

class Shader {
public:
    // Returns false (with the compiler/linker log on stderr) on failure.
    bool compile(const char* vertex_src, const char* fragment_src);
    void destroy();

    void use() const;

    void set_mat4(const char* name, const Mat4& m) const;
    void set_vec3(const char* name, float x, float y, float z) const;
    void set_float(const char* name, float v) const;
    void set_int(const char* name, int v) const;

    unsigned int id() const { return program_; }

private:
    unsigned int program_ = 0;
};

}  // namespace render
