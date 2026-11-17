// Random-input stress test over every level (run with -fsanitize=address,undefined).
#include "../src/core/app.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace mk;
int main(int argc, char** argv) {
    FILE* f = fopen(argv[1], "rb"); std::vector<uint8_t> j; fseek(f, 0, SEEK_END); j.resize(ftell(f)); fseek(f, 0, SEEK_SET);
    if (fread(j.data(), 1, j.size(), f) != j.size()) return 1;
    static Assets a; std::string e;
    if (!a.loadAny(j.data(), j.size(), "en", &e)) { puts(e.c_str()); return 1; }
    static Game g(a); static uint16_t px[OUT_W * OUT_H]; static Renderer r; r.init(a, px);
    srand(argc > 2 ? atoi(argv[2]) : 1);
    int deaths = 0, wins = 0, ticks = 0;
    static const int keys[] = {K_FWD, K_FWD, K_FWD, K_LEFT, K_RIGHT, K_PUSH, K_PUSH, K_HIT, K_BACK, K_LOOK};
    for (int lv : a.levelNumbers) {
        for (int run = 0; run < 6; run++) {
            g.lives = 3;
            g.startLevel(lv);
            for (int t = 0; t < 1500 && g.outcome == O_NONE; t++, ticks++) {
                if (rand() % 3 == 0) { int k = keys[rand() % 10]; g.setKey(k, true); g.tick(); g.setKey(k, false); }
                else g.tick();
                if (t % 7 == 0) { r.setScheme(t % 14 ? SCHEME_3410 : SCHEME_BW); r.drawGame(g); }
            }
            if (g.outcome == O_DIED) deaths++; else if (g.outcome != O_NONE) wins++;
        }
    }
    // the whole app with random buttons
    static App app(a); static uint16_t scr[SCR_W * SCR_H]; app.init(scr, SaveData());
    for (int t = 0; t < 20000; t++) {
        int b = rand() % BTN_COUNT;
        if (rand() % 4 == 0) app.setButton((Button)b, true);
        if (app.update(100)) app.render();
        app.setButton((Button)b, false);
        app.takeSound();
    }
    printf("ok: %d ticks, %d deaths, %d random clears\n", ticks, deaths, wins);
}
