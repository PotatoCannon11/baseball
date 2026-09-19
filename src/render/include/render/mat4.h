#pragma once

#include <cmath>
#include <cstring>

// Plain float 4x4 matrix, column-major (GL convention: m[col*4 + row]),
// so it can be handed to glUniformMatrix4fv(..., GL_FALSE, ...) directly.
// Render data is float throughout, per spec ("Physics state in double,
// render data in float") -- this never touches sim::Vec3/Quat (double);
// conversion happens once at the sim/render boundary.
namespace render {

struct Mat4 {
    float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};  // identity

    static Mat4 identity() { return Mat4{}; }

    static Mat4 translation(float x, float y, float z) {
        Mat4 r;
        r.m[12] = x;
        r.m[13] = y;
        r.m[14] = z;
        return r;
    }

    static Mat4 scale(float sx, float sy, float sz) {
        Mat4 r;
        r.m[0] = sx;
        r.m[5] = sy;
        r.m[10] = sz;
        return r;
    }

    // Column-major multiply: returns a*b (applies b first, then a).
    static Mat4 multiply(const Mat4& a, const Mat4& b) {
        Mat4 r;
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k) sum += a.m[k * 4 + row] * b.m[col * 4 + k];
                r.m[col * 4 + row] = sum;
            }
        }
        return r;
    }

    static Mat4 from_quat_and_pos(float qw, float qx, float qy, float qz, float px, float py, float pz) {
        Mat4 r;
        const float xx = qx * qx, yy = qy * qy, zz = qz * qz;
        const float xy = qx * qy, xz = qx * qz, yz = qy * qz;
        const float wx = qw * qx, wy = qw * qy, wz = qw * qz;
        r.m[0] = 1 - 2 * (yy + zz);
        r.m[1] = 2 * (xy + wz);
        r.m[2] = 2 * (xz - wy);
        r.m[3] = 0;
        r.m[4] = 2 * (xy - wz);
        r.m[5] = 1 - 2 * (xx + zz);
        r.m[6] = 2 * (yz + wx);
        r.m[7] = 0;
        r.m[8] = 2 * (xz + wy);
        r.m[9] = 2 * (yz - wx);
        r.m[10] = 1 - 2 * (xx + yy);
        r.m[11] = 0;
        r.m[12] = px;
        r.m[13] = py;
        r.m[14] = pz;
        r.m[15] = 1;
        return r;
    }

    static Mat4 perspective(float fovy_radians, float aspect, float z_near, float z_far) {
        Mat4 r{};
        std::memset(r.m, 0, sizeof(r.m));
        const float f = 1.0f / std::tan(fovy_radians * 0.5f);
        r.m[0] = f / aspect;
        r.m[5] = f;
        r.m[10] = (z_far + z_near) / (z_near - z_far);
        r.m[11] = -1.0f;
        r.m[14] = (2.0f * z_far * z_near) / (z_near - z_far);
        return r;
    }

    static Mat4 orthographic(float left, float right, float bottom, float top, float z_near, float z_far) {
        Mat4 r{};
        std::memset(r.m, 0, sizeof(r.m));
        r.m[0] = 2.0f / (right - left);
        r.m[5] = 2.0f / (top - bottom);
        r.m[10] = -2.0f / (z_far - z_near);
        r.m[12] = -(right + left) / (right - left);
        r.m[13] = -(top + bottom) / (top - bottom);
        r.m[14] = -(z_far + z_near) / (z_far - z_near);
        r.m[15] = 1.0f;
        return r;
    }

    static Mat4 look_at(float eye_x, float eye_y, float eye_z, float center_x, float center_y, float center_z,
                         float up_x, float up_y, float up_z) {
        float fx = center_x - eye_x, fy = center_y - eye_y, fz = center_z - eye_z;
        float flen = std::sqrt(fx * fx + fy * fy + fz * fz);
        fx /= flen;
        fy /= flen;
        fz /= flen;

        // s = f x up, normalized
        float sx = fy * up_z - fz * up_y;
        float sy = fz * up_x - fx * up_z;
        float sz = fx * up_y - fy * up_x;
        float slen = std::sqrt(sx * sx + sy * sy + sz * sz);
        sx /= slen;
        sy /= slen;
        sz /= slen;

        // u = s x f
        const float ux = sy * fz - sz * fy;
        const float uy = sz * fx - sx * fz;
        const float uz = sx * fy - sy * fx;

        Mat4 r;
        r.m[0] = sx;
        r.m[1] = ux;
        r.m[2] = -fx;
        r.m[3] = 0;
        r.m[4] = sy;
        r.m[5] = uy;
        r.m[6] = -fy;
        r.m[7] = 0;
        r.m[8] = sz;
        r.m[9] = uz;
        r.m[10] = -fz;
        r.m[11] = 0;
        r.m[12] = -(sx * eye_x + sy * eye_y + sz * eye_z);
        r.m[13] = -(ux * eye_x + uy * eye_y + uz * eye_z);
        r.m[14] = fx * eye_x + fy * eye_y + fz * eye_z;
        r.m[15] = 1;
        return r;
    }
};

}  // namespace render
