// Render-speed benchmark: frames per second of the 3D view on this machine.
#include "../src/core/render.h"
#include <chrono>
#include <cstdio>
#include <vector>
using namespace mk;
int main(int argc, char** argv) {
    FILE* f = fopen(argv[1], "rb"); std::vector<uint8_t> j; fseek(f, 0, SEEK_END); j.resize(ftell(f)); fseek(f, 0, SEEK_SET);
    if (fread(j.data(), 1, j.size(), f) != j.size()) return 1;
    static Assets a; std::string e; a.loadAny(j.data(), j.size(), "en", &e);
    static Game g(a); static uint16_t px[OUT_W * OUT_H]; static Renderer r; r.init(a, px);
    for (int style = 0; style < 2; style++) {
        r.setScheme((Scheme)style);
        double worst = 0, total = 0; int n = 0;
        for (int lv : {5, 17, 23, 27, 30}) {
            g.startLevel(lv);
            for (int t = 0; t < 12; t++) g.tick();
            for (int i = 0; i < 40; i++) {
                auto t0 = std::chrono::high_resolution_clock::now();
                r.drawGame(g);
                double ms = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - t0).count();
                total += ms; n++; if (ms > worst) worst = ms;
                g.tick();
            }
        }
        printf("%s: avg %.3f ms, worst %.3f ms per frame\n", style ? "bw" : "3410", total / n, worst);
    }
}
