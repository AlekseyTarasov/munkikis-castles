// PC harness for the whole app (menus included). Writes 280x240 screens as PPM
// and the text overlay of each frame as JSON lines (rendered later with PIL).
// usage: munkiki_app <Munkiki.jar> <outdir> "<script>" [lang]
// script tokens: <btn><ticks>; btn: U D L R A B C X(=D) S(select) T(start) _ (wait)
#include "../../core/app.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace mk;

static std::string jsonEsc(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '"' || c == '\\') o += '\\';
        o += c;
    }
    return o;
}

int main(int argc, char** argv) {
    if (argc < 4) { fprintf(stderr, "usage: %s jar outdir script\n", argv[0]); return 1; }
    FILE* f = fopen(argv[1], "rb");
    if (!f) return 1;
    std::vector<uint8_t> jar;
    fseek(f, 0, SEEK_END); jar.resize(ftell(f)); fseek(f, 0, SEEK_SET);
    if (fread(jar.data(), 1, jar.size(), f) != jar.size()) return 1;
    fclose(f);
    static Assets assets;
    std::string err;
    if (!assets.loadAny(jar.data(), jar.size(), "en", &err)) { fprintf(stderr, "%s\n", err.c_str()); return 1; }
    static App app(assets);
    static uint16_t screen[SCR_W * SCR_H];
    SaveData save;
    if (argc > 4) save.lang = (uint8_t)atoi(argv[4]);
    app.init(screen, save);

    std::string script = argv[3];
    std::string out = argv[2];
    FILE* tj = fopen((out + "/texts.jsonl").c_str(), "w");
    if (!tj) { fprintf(stderr, "cannot write to %s\n", out.c_str()); return 1; }
    int frame = 0;
    auto shot = [&]() {
        app.render();
        char p[512];
        snprintf(p, sizeof p, "%s/f%04d.ppm", out.c_str(), frame);
        FILE* o = fopen(p, "wb");
        fprintf(o, "P6\n%d %d\n255\n", SCR_W, SCR_H);
        for (int i = 0; i < SCR_W * SCR_H; i++) {
            uint16_t c = screen[i];
            uint8_t rgb[3] = {(uint8_t)(((c >> 11) & 31) * 255 / 31), (uint8_t)(((c >> 5) & 63) * 255 / 63),
                              (uint8_t)((c & 31) * 255 / 31)};
            fwrite(rgb, 1, 3, o);
        }
        fclose(o);
        fprintf(tj, "[");
        bool first = true;
        for (auto& t : app.texts()) {
            fprintf(tj, "%s[%d,%d,\"%s\",%d,%d,%d]", first ? "" : ",", t.x, t.y, jsonEsc(t.s).c_str(), t.color, t.size, t.align);
            first = false;
        }
        fprintf(tj, "]\n");
        frame++;
    };
    for (size_t p = 0; p < script.size();) {
        while (p < script.size() && script[p] == ' ') p++;
        if (p >= script.size()) break;
        char c = script[p++];
        int n = 0;
        while (p < script.size() && isdigit((unsigned char)script[p])) n = n * 10 + (script[p++] - '0');
        if (!n) n = 1;
        int b = -1;
        switch (c) {
            case 'U': b = BTN_UP; break; case 'D': b = BTN_DOWN; break; case 'L': b = BTN_LEFT; break;
            case 'R': b = BTN_RIGHT; break; case 'A': b = BTN_A; break; case 'B': b = BTN_B; break;
            case 'C': b = BTN_C; break; case 'X': b = BTN_D; break; case 'S': b = BTN_SELECT; break;
            case 'T': b = BTN_START; break;
        }
        for (int i = 0; i < n; i++) {
            if (b >= 0 && i == 0) app.setButton((Button)b, true);
            app.update(100);
            if (b >= 0 && i == n - 1) app.setButton((Button)b, false);
            int s = app.takeSound();
            if (s >= 0) printf("frame %d sound %d (%u ms)\n", frame, s, app.melody(s).durationMs());
            shot();
        }
    }
    fclose(tj);
    printf("%d frames\n", frame);
    return 0;
}
