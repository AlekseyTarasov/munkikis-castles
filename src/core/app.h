#pragma once
// Game flow: title, menus, level intro, play, pause, victory, game over.
// Platform code feeds buttons + time and draws the frame and the text list.
#include "assets.h"
#include "game.h"
#include "mono.h"
#include "render.h"
#include "ringtone.h"
#include <cstdint>
#include <string>
#include <vector>

namespace mk {

static const int SCR_W = 280, SCR_H = 240;  // Lilka screen (landscape)
static const int BAR_Y = OUT_H;             // status bar below the 3D view

enum Button { BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_A, BTN_B, BTN_C, BTN_D, BTN_SELECT, BTN_START, BTN_COUNT };

struct TextItem {
    int x, y;          // screen pixels; y = baseline-agnostic top of the text line
    std::string s;     // UTF-8
    uint16_t color;
    uint8_t size;      // 0 small, 1 normal, 2 big
    uint8_t align;     // 0 left, 1 center, 2 right
};

struct SaveData {
    uint32_t magic = 0x4D4B4B31;  // "MKK1"
    uint8_t level = 1, maxLevel = 1;
    int8_t lives = 3;
    uint8_t keys = 0;
    uint32_t score = 0, hiscore = 0;
    uint8_t scheme = 0, sound = 1, lang = 0;  // scheme 0 = Nokia 3410 green, 1 = black & white; lang 0 = UA, 1 = EN
    uint8_t inProgress = 0;
    uint8_t deaths = 0;
    uint32_t lifeTakenMask = 0;
};

class App {
public:
    App(const Assets& a) : A(a), game(a) {}
    void init(uint16_t* screen, const SaveData& save);

    void setButton(Button b, bool down);
    // Advance by `ms` milliseconds. Returns true if the screen changed.
    bool update(uint32_t ms);
    // Draws into the screen buffer; texts to overlay are in texts().
    void render();
    const std::vector<TextItem>& texts() const { return texts_; }

    // Sound to start now (or -1), consumed on read.
    int takeSound() { int s = pendingSound_; pendingSound_ = -1; return s; }
    const Melody& melody(int id) const { return melodies_[id]; }
    bool soundOn() const { return save_.sound != 0; }
    // The player chose "Exit": the platform should return to the launcher (Lilka: reboot to KeiraOS).
    bool exitRequested() const { return exit_; }

    bool saveDirty() { bool d = saveDirty_; saveDirty_ = false; return d; }
    const SaveData& saveData() const { return save_; }

private:
    enum State { TITLE, MENU, OPTIONS, HISCORE, HELP, INSTR, MAP, INTRO, PLAY, PAUSE, VICTORY, TALLY, GAMEOVER, WIN };
    void enter(State s);
    void tick();
    void startLevel(int n, bool showIntro = true);
    void finishLevel();
    void loseLife();
    void playSound(int id);
    void text(int x, int y, const std::string& s, uint16_t c, int size = 1, int align = 0);
    void panel(int x, int y, int w, int h);
    void rect(int x, int y, int w, int h, uint16_t c);
    void copyView();
    void statusBar();
    // Nokia-style full-screen menu: dark title bar, list with a marker, scroll bar, key hints.
    void nokiaMenu(const std::string& title, const std::vector<std::string>& items, int sel, bool back);
    void header(const std::string& title);
    void footer(bool back, bool select);
    void arrow(int x, int y);
    std::vector<std::string> mainItems() const;
    void setScheme(int s);
    void buildTexts();  // official Nokia strings for the current language, adapted to Lilka buttons
    void showMap(int done, int last, int next);  // castle map for spots done..last, then start level `next`
    void startPage(int p);
    std::vector<std::string> wrap(const std::string& s, int maxChars) const;
    const char* tr(int id) const;
    std::string fmt(int id, int v) const;
    void persist();

    const Assets& A;
    Game game;
    Renderer R;
    uint16_t* screen_ = nullptr;
    std::vector<uint16_t> view_;  // 288x195 render target
    std::vector<TextItem> texts_;
    Melody melodies_[12];
    SaveData save_;
    bool saveDirty_ = false;
    int pendingSound_ = -1;

    State state_ = TITLE;
    uint32_t acc_ = 0;
    int ticks_ = 0;       // ticks in the current state
    bool held_[BTN_COUNT] = {false};
    bool pressed_[BTN_COUNT] = {false};  // edge since last tick
    int sel_ = 0;
    int top_ = 0;  // first visible menu row
    int pickLevel_ = 1;
    bool dirty_ = true;
    bool exit_ = false;

    // level progress (original rules)
    int level_ = 1;
    int score_ = 0;
    int deaths_ = 0;      // deaths on this level (reduce the time bonus)
    int gained_ = 0;      // points of the last level
    int warpTo_ = 0;
    uint32_t lifeMask_ = 0;

    // castle map
    CastleMap map_;
    int mapSpot_ = 0, mapLast_ = 0, mapNext_ = 1;
    // instructions (text pages and demo levels)
    int page_ = 0;
    std::string off_[64];  // per string id: official text, empty = use ours
};

} // namespace mk
