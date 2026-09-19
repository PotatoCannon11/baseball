#include "render/mesh.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <map>

#include "render/gl.h"

namespace render {

void Mesh::upload(const std::vector<Vertex>& vertices, const std::vector<std::uint32_t>& indices) {
    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);

    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)), vertices.data(),
                 GL_STATIC_DRAW);

    glGenBuffers(1, &ebo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(std::uint32_t)),
                 indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);  // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, px)));
    glEnableVertexAttribArray(1);  // normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, nx)));
    glEnableVertexAttribArray(2);  // uv
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, u)));

    glBindVertexArray(0);
    index_count_ = static_cast<int>(indices.size());
}

void Mesh::destroy() {
    if (ebo_) glDeleteBuffers(1, &ebo_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    vao_ = vbo_ = ebo_ = 0;
    index_count_ = 0;
}

void Mesh::draw() const {
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, index_count_, GL_UNSIGNED_INT, nullptr);
}

namespace {
struct Vec3f {
    float x, y, z;
};

Vec3f normalize(Vec3f v) {
    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    return {v.x / len, v.y / len, v.z / len};
}
}  // namespace

void generate_icosphere(float radius, int subdivisions, std::vector<Vertex>* out_vertices,
                         std::vector<std::uint32_t>* out_indices) {
    std::vector<Vec3f> positions;
    std::vector<std::array<std::uint32_t, 3>> faces;

    const float t = (1.0f + std::sqrt(5.0f)) / 2.0f;
    auto add_vertex = [&](float x, float y, float z) { positions.push_back(normalize({x, y, z})); };
    add_vertex(-1, t, 0);
    add_vertex(1, t, 0);
    add_vertex(-1, -t, 0);
    add_vertex(1, -t, 0);
    add_vertex(0, -1, t);
    add_vertex(0, 1, t);
    add_vertex(0, -1, -t);
    add_vertex(0, 1, -t);
    add_vertex(t, 0, -1);
    add_vertex(t, 0, 1);
    add_vertex(-t, 0, -1);
    add_vertex(-t, 0, 1);

    faces = {{0, 11, 5}, {0, 5, 1}, {0, 1, 7},  {0, 7, 10}, {0, 10, 11}, {1, 5, 9},  {5, 11, 4}, {11, 10, 2},
              {10, 7, 6}, {7, 1, 8}, {3, 9, 4},  {3, 4, 2},  {3, 2, 6},   {3, 6, 8},  {3, 8, 9},  {4, 9, 5},
              {2, 4, 11}, {6, 2, 10}, {8, 6, 7},  {9, 8, 1}};

    std::map<std::uint64_t, std::uint32_t> midpoint_cache;
    auto get_midpoint = [&](std::uint32_t a, std::uint32_t b) -> std::uint32_t {
        const std::uint64_t key =
            a < b ? (static_cast<std::uint64_t>(a) << 32 | b) : (static_cast<std::uint64_t>(b) << 32 | a);
        const auto it = midpoint_cache.find(key);
        if (it != midpoint_cache.end()) return it->second;
        const Vec3f pa = positions[a];
        const Vec3f pb = positions[b];
        const Vec3f mid = normalize({(pa.x + pb.x) * 0.5f, (pa.y + pb.y) * 0.5f, (pa.z + pb.z) * 0.5f});
        positions.push_back(mid);
        const auto idx = static_cast<std::uint32_t>(positions.size() - 1);
        midpoint_cache[key] = idx;
        return idx;
    };

    for (int s = 0; s < subdivisions; ++s) {
        std::vector<std::array<std::uint32_t, 3>> new_faces;
        new_faces.reserve(faces.size() * 4);
        for (const auto& f : faces) {
            const std::uint32_t a = get_midpoint(f[0], f[1]);
            const std::uint32_t b = get_midpoint(f[1], f[2]);
            const std::uint32_t c = get_midpoint(f[2], f[0]);
            new_faces.push_back({f[0], a, c});
            new_faces.push_back({f[1], b, a});
            new_faces.push_back({f[2], c, b});
            new_faces.push_back({a, b, c});
        }
        faces = std::move(new_faces);
    }

    out_vertices->clear();
    out_vertices->reserve(positions.size());
    for (const Vec3f& p : positions) {
        Vertex v;
        v.px = p.x * radius;
        v.py = p.y * radius;
        v.pz = p.z * radius;
        v.nx = p.x;
        v.ny = p.y;
        v.nz = p.z;  // unit-sphere position IS the outward normal
        v.u = 0.5f + std::atan2(p.z, p.x) / (2.0f * 3.14159265f);
        v.v = 0.5f - std::asin(p.y) / 3.14159265f;
        out_vertices->push_back(v);
    }

    out_indices->clear();
    out_indices->reserve(faces.size() * 3);
    for (const auto& f : faces) {
        out_indices->push_back(f[0]);
        out_indices->push_back(f[1]);
        out_indices->push_back(f[2]);
    }
}

void generate_lathe(const LatheProfilePoint* profile, int profile_count, int radial_segments,
                     std::vector<Vertex>* out_vertices, std::vector<std::uint32_t>* out_indices) {
    out_vertices->clear();
    out_indices->clear();
    if (profile_count < 2 || radial_segments < 3) return;

    constexpr float kTwoPi = 6.28318530717958647692f;

    for (int i = 0; i < profile_count; ++i) {
        const float y = profile[i].y;
        const float r = profile[i].radius;

        float dr_dy;
        if (i == 0) {
            dr_dy = (profile[1].radius - profile[0].radius) / (profile[1].y - profile[0].y);
        } else if (i == profile_count - 1) {
            dr_dy = (profile[i].radius - profile[i - 1].radius) / (profile[i].y - profile[i - 1].y);
        } else {
            dr_dy = (profile[i + 1].radius - profile[i - 1].radius) / (profile[i + 1].y - profile[i - 1].y);
        }

        for (int j = 0; j < radial_segments; ++j) {
            const float theta = kTwoPi * static_cast<float>(j) / static_cast<float>(radial_segments);
            const float cx = std::cos(theta), cz = std::sin(theta);
            Vertex v;
            v.px = r * cx;
            v.py = y;
            v.pz = r * cz;
            const Vec3f n = normalize({cx, -dr_dy, cz});
            v.nx = n.x;
            v.ny = n.y;
            v.nz = n.z;
            v.u = static_cast<float>(j) / static_cast<float>(radial_segments);
            v.v = static_cast<float>(i) / static_cast<float>(profile_count - 1);
            out_vertices->push_back(v);
        }
    }

    for (int i = 0; i + 1 < profile_count; ++i) {
        for (int j = 0; j < radial_segments; ++j) {
            const int j_next = (j + 1) % radial_segments;
            const auto a = static_cast<std::uint32_t>(i * radial_segments + j);
            const auto b = static_cast<std::uint32_t>(i * radial_segments + j_next);
            const auto c = static_cast<std::uint32_t>((i + 1) * radial_segments + j);
            const auto d = static_cast<std::uint32_t>((i + 1) * radial_segments + j_next);
            out_indices->push_back(a);
            out_indices->push_back(c);
            out_indices->push_back(b);
            out_indices->push_back(b);
            out_indices->push_back(c);
            out_indices->push_back(d);
        }
    }

    // End caps: a fan from a center vertex, so the bat isn't open-ended.
    {
        const auto bottom_center = static_cast<std::uint32_t>(out_vertices->size());
        Vertex cv{};
        cv.px = 0;
        cv.py = profile[0].y;
        cv.pz = 0;
        cv.ny = -1;
        out_vertices->push_back(cv);
        for (int j = 0; j < radial_segments; ++j) {
            const int j_next = (j + 1) % radial_segments;
            out_indices->push_back(bottom_center);
            out_indices->push_back(static_cast<std::uint32_t>(j));
            out_indices->push_back(static_cast<std::uint32_t>(j_next));
        }

        const auto top_ring_start = static_cast<std::uint32_t>((profile_count - 1) * radial_segments);
        const auto top_center = static_cast<std::uint32_t>(out_vertices->size());
        Vertex tv{};
        tv.px = 0;
        tv.py = profile[profile_count - 1].y;
        tv.pz = 0;
        tv.ny = 1;
        out_vertices->push_back(tv);
        for (int j = 0; j < radial_segments; ++j) {
            const int j_next = (j + 1) % radial_segments;
            out_indices->push_back(top_center);
            out_indices->push_back(top_ring_start + static_cast<std::uint32_t>(j_next));
            out_indices->push_back(top_ring_start + static_cast<std::uint32_t>(j));
        }
    }
}

void generate_ground_plane(float half_size, std::vector<Vertex>* out_vertices,
                            std::vector<std::uint32_t>* out_indices) {
    out_vertices->assign({
        Vertex{-half_size, 0, -half_size, 0, 1, 0, 0, 0},
        Vertex{half_size, 0, -half_size, 0, 1, 0, 1, 0},
        Vertex{half_size, 0, half_size, 0, 1, 0, 1, 1},
        Vertex{-half_size, 0, half_size, 0, 1, 0, 0, 1},
    });
    out_indices->assign({0, 2, 1, 0, 3, 2});
}

}  // namespace render
