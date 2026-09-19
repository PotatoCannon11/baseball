#pragma once

#include <cstdint>
#include <vector>

// Packed vertex format shared by every generated mesh: position, normal,
// uv. 32 bytes, matches "packed formats" from the spec. Meshes are
// generated once at startup into a scratch std::vector, uploaded to a GL
// buffer, and the scratch vector is then destroyed ("free scratch after
// upload") -- this happens once per mesh type, never in the frame loop,
// so the heap use here doesn't violate the sim/frame zero-allocation
// rule (that rule is about steady-state per-frame cost, not one-time
// startup asset generation).
namespace render {

struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
};
static_assert(sizeof(Vertex) == 32, "Vertex layout changed -- check packing assumptions");

class Mesh {
public:
    void upload(const std::vector<Vertex>& vertices, const std::vector<std::uint32_t>& indices);
    void destroy();
    void draw() const;

private:
    unsigned int vao_ = 0, vbo_ = 0, ebo_ = 0;
    int index_count_ = 0;
};

// Subdivision level 2-3 gives a visually round ball at typical camera
// distances without an excessive triangle count; radius in meters.
void generate_icosphere(float radius, int subdivisions, std::vector<Vertex>* out_vertices,
                         std::vector<std::uint32_t>* out_indices);

// One radial profile point: distance along the bat's long axis (y, from
// knob=0 to barrel tip), and the bat's radius at that point. The lathe
// sweeps this profile around the axis. Render mesh and (later, milestone
// 4) collision chain come from the SAME profile data, per spec.
struct LatheProfilePoint {
    float y;
    float radius;
};
void generate_lathe(const LatheProfilePoint* profile, int profile_count, int radial_segments,
                     std::vector<Vertex>* out_vertices, std::vector<std::uint32_t>* out_indices);

// A flat square field, `half_size` meters from center to edge.
void generate_ground_plane(float half_size, std::vector<Vertex>* out_vertices,
                            std::vector<std::uint32_t>* out_indices);

}  // namespace render
