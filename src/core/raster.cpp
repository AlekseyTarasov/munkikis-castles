#include "raster.h"
#include <cmath>
#include <cstring>
#include <utility>

namespace mk {

Mat4 Mat4::identity() {
    Mat4 r;
    memset(r.m, 0, sizeof(r.m));
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1;
    return r;
}

Mat4 Mat4::operator*(const Mat4& b) const {
    Mat4 r;
    for (int c = 0; c < 4; c++)
        for (int rI = 0; rI < 4; rI++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += m[k * 4 + rI] * b.m[c * 4 + k];
            r.m[c * 4 + rI] = s;
        }
    return r;
}

void Raster::init(int w, int h, uint16_t* color, float* depth) {
    w_ = w; h_ = h; color_ = color; depth_ = depth;
    proj_ = mv_ = Mat4::identity();
}

void Raster::frustum(float l, float r, float b, float t, float n, float f) {
    Mat4 p;
    memset(p.m, 0, sizeof(p.m));
    p.m[0] = 2 * n / (r - l);
    p.m[5] = 2 * n / (t - b);
    p.m[8] = (r + l) / (r - l);
    p.m[9] = (t + b) / (t - b);
    p.m[10] = -(f + n) / (f - n);
    p.m[11] = -1;
    p.m[14] = -2 * f * n / (f - n);
    proj_ = p;
}

void Raster::loadIdentity() { mv_ = Mat4::identity(); }
void Raster::push() { if (sp_ < 8) stack_[sp_++] = mv_; }
void Raster::pop() { if (sp_ > 0) mv_ = stack_[--sp_]; }

void Raster::translate(float x, float y, float z) {
    Mat4 t = Mat4::identity();
    t.m[12] = x; t.m[13] = y; t.m[14] = z;
    mv_ = mv_ * t;
}

void Raster::scale(float x, float y, float z) {
    Mat4 s = Mat4::identity();
    s.m[0] = x; s.m[5] = y; s.m[10] = z;
    mv_ = mv_ * s;
}

void Raster::rotate(float deg, float x, float y, float z) {
    float len = sqrtf(x * x + y * y + z * z);
    if (len == 0) return;
    x /= len; y /= len; z /= len;
    float a = deg * 3.14159265f / 180.f, c = cosf(a), s = sinf(a), C = 1 - c;
    Mat4 r = Mat4::identity();
    r.m[0] = x * x * C + c;     r.m[4] = x * y * C - z * s; r.m[8] = x * z * C + y * s;
    r.m[1] = y * x * C + z * s; r.m[5] = y * y * C + c;     r.m[9] = y * z * C - x * s;
    r.m[2] = x * z * C - y * s; r.m[6] = y * z * C + x * s; r.m[10] = z * z * C + c;
    mv_ = mv_ * r;
}

void Raster::clear(uint16_t rgb) {
    int n = w_ * h_;
    for (int i = 0; i < n; i++) {
        color_[i] = rgb;
        depth_[i] = 1e30f;
    }
}

void Raster::drawArrays(const int8_t* vtx, int nVtx) {
    uint8_t idx[48];
    if (nVtx > 48) nVtx = 48;
    for (int i = 0; i < nVtx; i++) idx[i] = (uint8_t)i;
    drawElements(vtx, nVtx, idx, nVtx);
}

void Raster::drawElements(const int8_t* vtx, int nVtx, const uint8_t* idx, int nIdx, const int8_t* uv) {
    V tv[24];
    if (nVtx > 24) nVtx = 24;
    Mat4 m = proj_ * mv_;
    for (int i = 0; i < nVtx; i++) {
        float x = vtx[i * 3], y = vtx[i * 3 + 1], z = vtx[i * 3 + 2];
        V& o = tv[i];
        o.x = m.m[0] * x + m.m[4] * y + m.m[8] * z + m.m[12];
        o.y = m.m[1] * x + m.m[5] * y + m.m[9] * z + m.m[13];
        o.z = m.m[2] * x + m.m[6] * y + m.m[10] * z + m.m[14];
        o.w = m.m[3] * x + m.m[7] * y + m.m[11] * z + m.m[15];
        o.ex = mv_.m[0] * x + mv_.m[4] * y + mv_.m[8] * z + mv_.m[12];
        o.ey = mv_.m[1] * x + mv_.m[5] * y + mv_.m[9] * z + mv_.m[13];
        o.ez = mv_.m[2] * x + mv_.m[6] * y + mv_.m[10] * z + mv_.m[14];
        o.u = uv ? uv[i * 2] : 0;
        o.v = uv ? uv[i * 2 + 1] : 0;
    }
    for (int i = 0; i + 2 < nIdx; i += 3) {
        if (idx[i] >= nVtx || idx[i + 1] >= nVtx || idx[i + 2] >= nVtx) continue;
        tri(tv[idx[i]], tv[idx[i + 1]], tv[idx[i + 2]]);
    }
}

// Clip against the near plane (z >= -w), then rasterize the 1-2 resulting triangles.
void Raster::tri(const V& a, const V& b, const V& c) {
    drawColor_ = color565_;
    if (shading_ && color565_ != 0) {
        float ux = b.ex - a.ex, uy = b.ey - a.ey, uz = b.ez - a.ez;
        float vx = c.ex - a.ex, vy = c.ey - a.ey, vz = c.ez - a.ez;
        float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
        float len = sqrtf(nx * nx + ny * ny + nz * nz);
        if (len > 0) {
            // light from above, slightly left and in front of the camera
            float d = (nx * -0.35f + ny * 0.85f + nz * 0.40f) / len;
            float k = 0.62f + 0.38f * (d > 0 ? d : 0);
            int r = (color565_ >> 11) & 31, g = (color565_ >> 5) & 63, bl = color565_ & 31;
            drawColor_ = (uint16_t)(((int)(r * k) << 11) | ((int)(g * k) << 5) | (int)(bl * k));
        }
    }
    const V* in[3] = {&a, &b, &c};
    float d[3];
    int inside = 0;
    for (int i = 0; i < 3; i++) {
        d[i] = in[i]->z + in[i]->w;
        if (d[i] >= 0) inside++;
    }
    if (inside == 0) return;
    if (inside == 3) { rasterize(a, b, c); return; }
    V out[4];
    int n = 0;
    for (int i = 0; i < 3; i++) {
        int j = (i + 1) % 3;
        const V& p = *in[i];
        const V& q = *in[j];
        if (d[i] >= 0) out[n++] = p;
        if ((d[i] >= 0) != (d[j] >= 0)) {
            float t = d[i] / (d[i] - d[j]);
            V r;
            r.x = p.x + (q.x - p.x) * t; r.y = p.y + (q.y - p.y) * t;
            r.z = p.z + (q.z - p.z) * t; r.w = p.w + (q.w - p.w) * t;
            r.u = p.u + (q.u - p.u) * t; r.v = p.v + (q.v - p.v) * t;
            r.ex = r.ey = r.ez = 0;
            out[n++] = r;
        }
    }
    if (n >= 3) rasterize(out[0], out[1], out[2]);
    if (n == 4) rasterize(out[0], out[2], out[3]);
}

void Raster::rasterize(const V& A, const V& B, const V& C) {
    struct S { float x, y, z, iw, u, v; };
    S s[3];
    const V* vs[3] = {&A, &B, &C};
    for (int i = 0; i < 3; i++) {
        const V& v = *vs[i];
        if (v.w <= 1e-6f) return;
        float iw = 1.f / v.w;
        s[i].x = (v.x * iw * 0.5f + 0.5f) * w_;
        s[i].y = (0.5f - v.y * iw * 0.5f) * h_;
        s[i].z = v.z * iw;
        s[i].iw = iw;
        s[i].u = v.u * iw;
        s[i].v = v.v * iw;
    }
    // Area in screen space (y down): front faces (CCW in GL) have negative area here.
    float area = (s[1].x - s[0].x) * (s[2].y - s[0].y) - (s[2].x - s[0].x) * (s[1].y - s[0].y);
    if (area == 0) return;
    if (cull_ && area > 0) return;
    if (area > 0) std::swap(s[1], s[2]), area = -area;

    float minx = fminf(s[0].x, fminf(s[1].x, s[2].x)), maxx = fmaxf(s[0].x, fmaxf(s[1].x, s[2].x));
    float miny = fminf(s[0].y, fminf(s[1].y, s[2].y)), maxy = fmaxf(s[0].y, fmaxf(s[1].y, s[2].y));
    int x0 = (int)fmaxf(0, floorf(minx)), x1 = (int)fminf(w_ - 1, ceilf(maxx));
    int y0 = (int)fmaxf(0, floorf(miny)), y1 = (int)fminf(h_ - 1, ceilf(maxy));
    if (x0 > x1 || y0 > y1) return;

    float inv = 1.f / area;
    // Edge function E_i(x,y) for the edge opposite vertex i.
    auto E = [&](int a, int b, float px, float py) {
        return (s[b].x - s[a].x) * (py - s[a].y) - (s[b].y - s[a].y) * (px - s[a].x);
    };
    float dxs[3];
    const int ea[3] = {1, 2, 0}, eb[3] = {2, 0, 1};
    for (int i = 0; i < 3; i++) {
        dxs[i] = -(s[eb[i]].y - s[ea[i]].y);
    }
    const bool textured = tex_ != nullptr;
    for (int y = y0; y <= y1; y++) {
        float py = y + 0.5f, px = x0 + 0.5f;
        float e0 = E(1, 2, px, py), e1 = E(2, 0, px, py), e2 = E(0, 1, px, py);
        uint16_t* cp = color_ + y * w_;
        float* dp = depth_ + y * w_;
        for (int x = x0; x <= x1; x++, e0 += dxs[0], e1 += dxs[1], e2 += dxs[2]) {
            if (e0 > 0 || e1 > 0 || e2 > 0) continue;  // outside (area negative => inside is <= 0)
            float b0 = e0 * inv, b1 = e1 * inv, b2 = e2 * inv;
            float z = b0 * s[0].z + b1 * s[1].z + b2 * s[2].z;
            if (z >= dp[x]) continue;
            dp[x] = z;
            if (!textured) {
                cp[x] = drawColor_;
            } else {
                float iw = b0 * s[0].iw + b1 * s[1].iw + b2 * s[2].iw;
                float u = (b0 * s[0].u + b1 * s[1].u + b2 * s[2].u) / iw;
                float v = (b0 * s[0].v + b1 * s[1].v + b2 * s[2].v) / iw;
                int tu = (int)(u * texW_), tv = (int)(v * texH_);
                tu = tu < 0 ? 0 : (tu >= texW_ ? texW_ - 1 : tu);
                tv = tv < 0 ? 0 : (tv >= texH_ ? texH_ - 1 : tv);
                cp[x] = tex_[tv * texW_ + tu] ? texLight_ : texDark_;
            }
        }
    }
}

} // namespace mk
