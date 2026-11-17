#pragma once
// Tiny software 3D rasterizer covering what the game needs:
// matrix stack, perspective frustum, back-face culling, near-plane clipping,
// depth buffer, flat-colored and textured triangles. Output is RGB565.
#include <cstdint>

namespace mk {

struct Mat4 {
    float m[16];  // column-major, like OpenGL
    static Mat4 identity();
    Mat4 operator*(const Mat4& b) const;
};

class Raster {
public:
    void init(int w, int h, uint16_t* color, float* depth);
    int width() const { return w_; }
    int height() const { return h_; }
    uint16_t* pixels() { return color_; }

    void frustum(float l, float r, float b, float t, float n, float f);
    void loadIdentity();
    void push();
    void pop();
    void translate(float x, float y, float z);
    void rotate(float deg, float x, float y, float z);
    void scale(float x, float y, float z);

    void clear(uint16_t rgb);
    void setColor(uint16_t rgb) { color565_ = rgb; }
    // Luminance texture (0/255, up to 32x32) and the two colors it maps to.
    void bindTexture(const uint8_t* lum, int w, int h, uint16_t dark, uint16_t light) {
        tex_ = lum; texW_ = w; texH_ = h; texDark_ = dark; texLight_ = light;
    }
    void unbindTexture() { tex_ = nullptr; }
    void setCulling(bool on) { cull_ = on; }
    // Optional simple face shading (not in the original): darkens faces turned away from the light.
    void setShading(bool on) { shading_ = on; }

    // Signed-byte vertices (x,y,z), unsigned-byte indices, optional byte UVs (u,v in 0..1).
    void drawElements(const int8_t* vtx, int nVtx, const uint8_t* idx, int nIdx, const int8_t* uv = nullptr);
    void drawArrays(const int8_t* vtx, int nVtx);

private:
    struct V {
        float x, y, z, w;  // clip space
        float u, v;
        float ex, ey, ez;  // eye space (for shading)
    };
    void tri(const V& a, const V& b, const V& c);
    void rasterize(const V& a, const V& b, const V& c);

    int w_ = 0, h_ = 0;
    uint16_t* color_ = nullptr;
    float* depth_ = nullptr;
    Mat4 proj_, mv_;
    Mat4 stack_[8];
    int sp_ = 0;
    uint16_t color565_ = 0;
    const uint8_t* tex_ = nullptr;
    int texW_ = 32, texH_ = 32;
    uint16_t texDark_ = 0, texLight_ = 0xFFFF;
    bool cull_ = true;
    bool shading_ = false;
    uint16_t drawColor_ = 0;  // color565_ after shading for the current triangle
};

inline uint16_t rgb565(int r, int g, int b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

} // namespace mk
