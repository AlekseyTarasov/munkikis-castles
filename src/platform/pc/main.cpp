// Headless PC harness: plays a scripted key sequence and writes frames as PPM.
// usage: munkiki_pc <Munkiki.jar> <outdir> [--level N] [--style 3410|bw] [--script "F1 _5 R1 ..."] [--every N]
// script tokens: <key><ticks>, key: F fwd, B back, L left, R right, P push, H hit, K look, _ nothing
#include "../../core/assets.h"
#include "../../core/game.h"
#include "../../core/render.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace mk;

static bool readFile(const char* path, std::vector<uint8_t>& out) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    out.resize(n);
    bool ok = fread(out.data(), 1, n, f) == (size_t)n;
    fclose(f);
    return ok;
}

static void writePPM(const char* path, const uint16_t* px) {
    FILE* f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", OUT_W, OUT_H);
    for (int i = 0; i < OUT_W * OUT_H; i++) {
        uint16_t c = px[i];
        uint8_t rgb[3] = {(uint8_t)(((c >> 11) & 31) * 255 / 31), (uint8_t)(((c >> 5) & 63) * 255 / 63),
                          (uint8_t)((c & 31) * 255 / 31)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s Munkiki.jar outdir [--level N] [--style 3410|bw] [--script S] [--every N]\n", argv[0]);
        return 1;
    }
    int levelNo = 1, every = 1;
    Scheme style = SCHEME_3410;
    std::string script = "_20";
    for (int i = 3; i + 1 < argc; i += 2) {
        if (!strcmp(argv[i], "--level")) levelNo = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--style")) style = !strcmp(argv[i + 1], "bw") ? SCHEME_BW : SCHEME_3410;
        else if (!strcmp(argv[i], "--script")) script = argv[i + 1];
        else if (!strcmp(argv[i], "--every")) every = atoi(argv[i + 1]);
    }
    std::vector<uint8_t> jar;
    if (!readFile(argv[1], jar)) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
    static Assets assets;
    std::string err;
    if (!assets.loadAny(jar.data(), jar.size(), "en", &err)) { fprintf(stderr, "load failed: %s\n", err.c_str()); return 1; }
    printf("loaded: %zu levels, %zu anims, %zu strings\n", assets.levelNumbers.size(), assets.anims.size(), assets.text.size());

    static Game game(assets);
    if (!game.startLevel(levelNo)) { fprintf(stderr, "no level %d\n", levelNo); return 1; }
    static uint16_t frame[OUT_W * OUT_H];
    static Renderer R;
    R.init(assets, frame);
    R.setScheme(style);

    // parse script
    struct Step { int key; int ticks; };
    std::vector<Step> steps;
    for (size_t p = 0; p < script.size();) {
        while (p < script.size() && script[p] == ' ') p++;
        if (p >= script.size()) break;
        char c = script[p++];
        int n = 0;
        while (p < script.size() && isdigit((unsigned char)script[p])) n = n * 10 + (script[p++] - '0');
        if (!n) n = 1;
        int k = -1;
        switch (c) {
            case 'F': k = K_FWD; break; case 'B': k = K_BACK; break; case 'L': k = K_LEFT; break;
            case 'R': k = K_RIGHT; break; case 'P': k = K_PUSH; break; case 'H': k = K_HIT; break;
            case 'K': k = K_LOOK; break; default: k = -1;
        }
        steps.push_back({k, n});
    }
    int t = 0, shot = 0;
    for (auto& s : steps)
        for (int i = 0; i < s.ticks; i++) {
            if (s.key >= 0) game.setKey(s.key, i < s.ticks);  // hold for the whole step
            game.tick();
            if (s.key >= 0 && i == s.ticks - 1) game.setKey(s.key, false);
            if (t % every == 0) {
                R.drawGame(game);
                char path[512];
                snprintf(path, sizeof path, "%s/f%04d.ppm", argv[2], shot++);
                writePPM(path, frame);
            }
            printf("t=%3d key=%2d pos=(%.2f,%.2f,%.2f) face=%.2f notes=%d moves=%d outcome=%d snd=%x\n", t, s.key,
                   game.px / 65536.0, game.py / 65536.0, game.pz / 65536.0, game.facing / 65536.0, game.notesLeft,
                   game.moves, game.outcome, game.sounds);
            t++;
        }
    return 0;
}
