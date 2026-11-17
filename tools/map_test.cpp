// Renders the castle map for a few game states (PPM files in outdir).
#include "../src/core/render.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace mk;
int main(int argc, char** argv) {
    FILE* f = fopen(argv[1], "rb"); std::vector<uint8_t> j; fseek(f, 0, SEEK_END); j.resize(ftell(f)); fseek(f, 0, SEEK_SET);
    if (fread(j.data(), 1, j.size(), f) != j.size()) return 1;
    static Assets a; std::string e; a.loadAny(j.data(), j.size(), "en", &e);
    printf("sheet %dx%d\n", a.mapScreen.w, a.mapScreen.h);
    static uint16_t px[OUT_W * OUT_H]; static Renderer r; r.init(a, px);
    if (argc > 4) {  // animation: map_test jar outdir done keys
        CastleMap m; m.begin(a.mapScreen, atoi(argv[3]), atoi(argv[4]));
        for (int l = 1; l <= 45; l++) {
            m.step(l);
            r.drawMono(m.screen());
            char p[256]; snprintf(p, sizeof p, "%s/f%04d.ppm", argv[2], l);
            FILE* o = fopen(p, "wb"); fprintf(o, "P6\n%d %d\n255\n", OUT_W, OUT_H);
            for (int i = 0; i < OUT_W * OUT_H; i++) { uint16_t c = px[i]; uint8_t rgb[3] = {(uint8_t)(((c >> 11) & 31) * 8), (uint8_t)(((c >> 5) & 63) * 4), (uint8_t)((c & 31) * 8)}; fwrite(rgb, 1, 3, o); }
            fclose(o);
        }
        return 0;
    }
    struct { int done, keys, l; } cases[] = {{0, 0, 4}, {0, 0, 18}, {7, 0, 18}, {13, 1, 18}, {24, 0, 18}, {25, 2, 22}, {25, 2, 30}, {26, 3, 37}, {28, 5, 40}};
    int n = 0;
    for (auto& c : cases) {
        CastleMap m; m.begin(a.mapScreen, c.done, c.keys);
        for (int l = 1; l <= c.l; l++) m.step(l);
        r.drawMono(m.screen());
        char p[256]; snprintf(p, sizeof p, "%s/m%02d.ppm", argv[2], n++);
        FILE* o = fopen(p, "wb"); fprintf(o, "P6\n%d %d\n255\n", OUT_W, OUT_H);
        for (int i = 0; i < OUT_W * OUT_H; i++) { uint16_t c = px[i]; uint8_t rgb[3] = {(uint8_t)(((c >> 11) & 31) * 8), (uint8_t)(((c >> 5) & 63) * 4), (uint8_t)((c & 31) * 8)}; fwrite(rgb, 1, 3, o); }
        fclose(o);
    }
}
