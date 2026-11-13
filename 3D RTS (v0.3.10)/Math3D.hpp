#pragma once

#include <cmath>

constexpr float PI = 3.14159265358979323846f;

inline float toRadians(float degrees) {
    return degrees * (PI / 180.0f);
}

inline float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

struct Vec2 {
    float x, y;
    Vec2(float x = 0.0f, float y = 0.0f) : x(x), y(y) {}
    Vec2 operator+(const Vec2& o) const { return Vec2(x + o.x, y + o.y); }
    Vec2 operator-(const Vec2& o) const { return Vec2(x - o.x, y - o.y); }
    float length() const { return std::sqrt(x * x + y * y); }
};

struct Vec3 {
    float x, y, z;

    Vec3(float x = 0.0f, float y = 0.0f, float z = 0.0f) : x(x), y(y), z(z) {}

    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }

    float length() const {
        return std::sqrt(x * x + y * y + z * z);
    }

    float lengthXZ() const {
        return std::sqrt(x * x + z * z);
    }

    Vec3 normalized() const {
        float len = length();
        if (len < 1e-6f) return Vec3(0.0f, 0.0f, 0.0f);
        return Vec3(x / len, y / len, z / len);
    }

    static float dot(const Vec3& a, const Vec3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    static Vec3 cross(const Vec3& a, const Vec3& b) {
        return Vec3(
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        );
    }
};

struct Vec4 {
    float x, y, z, w;
    Vec4(float x = 0.0f, float y = 0.0f, float z = 0.0f, float w = 1.0f)
        : x(x), y(y), z(z), w(w) {}
};

struct Mat4 {
    float m[16];

    Mat4() {
        for (int i = 0; i < 16; ++i) m[i] = 0.0f;
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }

    static Mat4 perspective(float fovRadians, float aspect, float nearZ, float farZ) {
        Mat4 res;
        for (int i = 0; i < 16; ++i) res.m[i] = 0.0f;
        float tanHalfFov = std::tan(fovRadians / 2.0f);
        res.m[0]  = 1.0f / (aspect * tanHalfFov);
        res.m[5]  = 1.0f / tanHalfFov;
        res.m[10] = -(farZ + nearZ) / (farZ - nearZ);
        res.m[11] = -1.0f;
        res.m[14] = -(2.0f * farZ * nearZ) / (farZ - nearZ);
        return res;
    }

    static Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
        Vec3 f = (target - eye).normalized();
        Vec3 s = Vec3::cross(f, up).normalized();
        Vec3 u = Vec3::cross(s, f);

        Mat4 res;
        res.m[0]  = s.x; res.m[4]  = s.y; res.m[8]  = s.z; res.m[12] = -Vec3::dot(s, eye);
        res.m[1]  = u.x; res.m[5]  = u.y; res.m[9]  = u.z; res.m[13] = -Vec3::dot(u, eye);
        res.m[2]  = -f.x; res.m[6] = -f.y; res.m[10] = -f.z; res.m[14] = Vec3::dot(f, eye);
        res.m[3]  = 0.0f; res.m[7] = 0.0f; res.m[11] = 0.0f; res.m[15] = 1.0f;
        return res;
    }

    static Mat4 translate(const Vec3& v) {
        Mat4 res;
        res.m[12] = v.x;
        res.m[13] = v.y;
        res.m[14] = v.z;
        return res;
    }

    static Mat4 rotateY(float angleRad) {
        Mat4 res;
        float c = std::cos(angleRad);
        float s = std::sin(angleRad);
        res.m[0]  = c;
        res.m[2]  = -s;
        res.m[8]  = s;
        res.m[10] = c;
        return res;
    }

    static Mat4 scale(const Vec3& s) {
        Mat4 res;
        res.m[0]  = s.x;
        res.m[5]  = s.y;
        res.m[10] = s.z;
        return res;
    }

    Mat4 operator*(const Mat4& b) const {
        Mat4 res;
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                res.m[col * 4 + row] =
                    m[0 * 4 + row] * b.m[col * 4 + 0] +
                    m[1 * 4 + row] * b.m[col * 4 + 1] +
                    m[2 * 4 + row] * b.m[col * 4 + 2] +
                    m[3 * 4 + row] * b.m[col * 4 + 3];
            }
        }
        return res;
    }

    Vec4 operator*(const Vec4& v) const {
        return Vec4(
            m[0] * v.x + m[4] * v.y + m[8]  * v.z + m[12] * v.w,
            m[1] * v.x + m[5] * v.y + m[9]  * v.z + m[13] * v.w,
            m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14] * v.w,
            m[3] * v.x + m[7] * v.y + m[11] * v.z + m[15] * v.w
        );
    }

    Mat4 inverse() const {
        float inv[16];
        inv[0] = m[5]  * m[10] * m[15] - m[5]  * m[11] * m[14] - m[9]  * m[6]  * m[15] +
                 m[9]  * m[7]  * m[14] + m[13] * m[6]  * m[11] - m[13] * m[7]  * m[10];
        inv[4] = -m[4]  * m[10] * m[15] + m[4]  * m[11] * m[14] + m[8]  * m[6]  * m[15] -
                  m[8]  * m[7]  * m[14] - m[12] * m[6]  * m[11] + m[12] * m[7]  * m[10];
        inv[8] = m[4]  * m[9] * m[15] - m[4]  * m[11] * m[13] - m[8]  * m[5] * m[15] +
                 m[8]  * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
        inv[12] = -m[4]  * m[9] * m[14] + m[4]  * m[10] * m[13] + m[8]  * m[5] * m[14] -
                   m[8]  * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];

        inv[1] = -m[1]  * m[10] * m[15] + m[1]  * m[11] * m[14] + m[9]  * m[2] * m[15] -
                  m[9]  * m[3]  * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
        inv[5] = m[0]  * m[10] * m[15] - m[0]  * m[11] * m[14] - m[8]  * m[2] * m[15] +
                 m[8]  * m[3]  * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
        inv[9] = -m[0]  * m[9] * m[15] + m[0]  * m[11] * m[13] + m[8]  * m[1] * m[15] -
                  m[8]  * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
        inv[13] = m[0]  * m[9] * m[14] - m[0]  * m[10] * m[13] - m[8]  * m[1] * m[14] +
                  m[8]  * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];

        inv[2] = m[1]  * m[6] * m[15] - m[1]  * m[7] * m[14] - m[5]  * m[2] * m[15] +
                 m[5]  * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
        inv[6] = -m[0]  * m[6] * m[15] + m[0]  * m[7] * m[14] + m[4]  * m[2] * m[15] -
                  m[4]  * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
        inv[10] = m[0]  * m[5] * m[15] - m[0]  * m[7] * m[13] - m[4]  * m[1] * m[15] +
                  m[4]  * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
        inv[14] = -m[0]  * m[5] * m[14] + m[0]  * m[6] * m[13] + m[4]  * m[1] * m[14] -
                   m[4]  * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];

        inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] -
                  m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
        inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] +
                 m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
        inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] -
                   m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
        inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] +
                  m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

        float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
        Mat4 res;
        if (std::abs(det) < 1e-8f) return res;
        det = 1.0f / det;
        for (int i = 0; i < 16; ++i) res.m[i] = inv[i] * det;
        return res;
    }
};