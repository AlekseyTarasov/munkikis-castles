// Lilka (ESP32-S3) front end.
// Looks for the original game file next to the firmware (KeiraOS passes its path in argv[0]),
// then in /sd/munkiki/ and /sd/. Accepts the .jar or a .zip that contains it.
#include <lilka.h>
#include <dirent.h>
#include <esp_system.h>
#include "../../core/app.h"

namespace {

mk::Assets* assets = nullptr;
mk::App* app = nullptr;
lilka::Canvas* canvas = nullptr;
String saveDir = "/sd/munkiki";
uint32_t lastMs = 0;

bool readFile(const String& path, std::vector<uint8_t>& out) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0 || n > 4 * 1024 * 1024) { fclose(f); return false; }
    out.resize(n);
    bool ok = fread(out.data(), 1, n, f) == (size_t)n;
    fclose(f);
    return ok;
}

bool tryLoad(const String& path, String& error) {
    std::vector<uint8_t> data;
    if (!readFile(path, data)) return false;
    std::string err;
    if (assets->loadAny(data.data(), data.size(), "en", &err)) return true;
    error = path + ": " + err.c_str();
    return false;
}

bool wantUk = true;  // first pass: prefer a build that has the official Ukrainian texts

// Try every .jar / .zip in a folder whose name mentions Munkiki (or any, if `anyName`).
bool scanDir(const String& dir, bool anyName, String& error) {
    DIR* d = opendir(dir.c_str());
    if (!d) return false;
    bool ok = false;
    while (!ok) {
        dirent* e = readdir(d);
        if (!e) break;
        String n = e->d_name;
        String l = n;
        l.toLowerCase();
        if (!(l.endsWith(".jar") || l.endsWith(".zip"))) continue;
        if (!anyName && l.indexOf("munkiki") < 0 && l.indexOf("kkm") < 0) continue;
        ok = tryLoad(dir + "/" + n, error) && (!wantUk || !assets->textUk.empty());
        if (ok) saveDir = dir;
    }
    closedir(d);
    return ok;
}

bool findGameOnce(String& error) {
    if (lilka::multiboot.getArgc() > 0 && lilka::multiboot.getArgv()) {
        String fw = lilka::multiboot.getArgv()[0];
        int slash = fw.lastIndexOf('/');
        if (slash > 0) {
            String dir = fw.substring(0, slash);
            if (scanDir(dir, false, error) || scanDir(dir, true, error)) return true;
        }
    }
    if (scanDir("/sd/munkiki", true, error)) return true;
    if (scanDir("/sd", false, error)) return true;
    return false;
}

bool findGame(String& error) {
    wantUk = true;
    if (findGameOnce(error)) return true;
    wantUk = false;
    return findGameOnce(error);
}

void showError(const String& title, const String& msg) {
    lilka::Alert alert(title, msg);
    alert.draw(&lilka::display);
    while (!alert.isFinished()) alert.update();
}

mk::SaveData loadSave() {
    mk::SaveData s;
    FILE* f = fopen((saveDir + "/munkiki.sav").c_str(), "rb");
    if (f) {
        mk::SaveData t;
        if (fread(&t, 1, sizeof t, f) == sizeof t) s = t;
        fclose(f);
    }
    return s;
}

void writeSave(const mk::SaveData& s) {
    FILE* f = fopen((saveDir + "/munkiki.sav").c_str(), "wb");
    if (!f) return;
    fwrite(&s, 1, sizeof s, f);
    fclose(f);
}

void drawTexts() {
    static const uint8_t* fonts[3] = {FONT_6x13, FONT_8x13, FONT_10x20};
    for (const auto& t : app->texts()) {
        canvas->setFont(fonts[t.size > 2 ? 2 : t.size]);
        canvas->setTextColor(t.color);
        lilka::Alignment h = t.align == 1 ? lilka::ALIGN_CENTER : (t.align == 2 ? lilka::ALIGN_END : lilka::ALIGN_START);
        canvas->drawTextAligned(t.s.c_str(), t.x, t.y, h, lilka::ALIGN_START);
    }
}

} // namespace

void setup() {
    lilka::begin();
    canvas = new lilka::Canvas(mk::SCR_W, mk::SCR_H);
    assets = new mk::Assets();
    String error;
    if (!findGame(error)) {
        showError("Munkiki's Castles",
                  "Не знайдено файл гри.\nПоклади Munkiki.jar (або .zip\nз ним) поруч із прошивкою\nабо в /sd/munkiki/\n\n" + error);
        esp_restart();
    }
    app = new mk::App(*assets);
    app->init((uint16_t*)canvas->getFramebuffer(), loadSave());
    lastMs = millis();
}

void loop() {
    lilka::State st = lilka::controller.getState();
    const lilka::ButtonState* bs[mk::BTN_COUNT] = {&st.up, &st.down, &st.left, &st.right, &st.a,
                                                     &st.b, &st.c, &st.d, &st.select, &st.start};
    for (int i = 0; i < mk::BTN_COUNT; i++) {
        if (bs[i]->justPressed) app->setButton((mk::Button)i, true);
        app->setButton((mk::Button)i, bs[i]->pressed);
    }

    uint32_t now = millis();
    uint32_t dt = now - lastMs;
    lastMs = now;
    if (app->update(dt)) {
        app->render();
        drawTexts();
        lilka::display.drawCanvas(canvas);
    }

    int s = app->takeSound();
    if (s >= 0 && app->soundOn()) {
        const mk::Melody& m = app->melody(s);
        // mk::ToneNote has the same layout as lilka::Tone (frequency, size)
        static_assert(sizeof(mk::ToneNote) == sizeof(lilka::Tone), "tone layout");
        lilka::buzzer.playMelody(reinterpret_cast<const lilka::Tone*>(m.notes.data()), m.notes.size(), m.tempo);
    }
    if (app->saveDirty()) writeSave(app->saveData());
    if (app->exitRequested()) {
        lilka::buzzer.stop();
        esp_restart();  // the launcher's bootloader returns to KeiraOS
    }
    vTaskDelay(1);
}
