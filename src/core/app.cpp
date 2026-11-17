#include "app.h"
#include <cctype>
#include <cstdio>
#include <cstring>

namespace mk {

namespace {

enum Str {
    T_TITLE, T_PRESS, T_CONTINUE, T_NEW, T_LEVEL_PICK, T_OPTIONS, T_HISCORES, T_HELP, T_QUIT,
    T_SCHEME_3410, T_SCHEME_BW, T_SOUND_ON, T_SOUND_OFF, T_LANG, T_BACK, T_SELECT,
    T_LEVEL, T_COLLECT, T_COLLECT_KEY, T_GO, T_PAUSE, T_RESUME, T_RESTART, T_TO_MENU,
    T_DONE, T_POINTS, T_SCORE, T_RECORD, T_WIN, T_LIVES, T_NOTES,
    T_H1, T_H2, T_H3, T_H4, T_H5, T_H6, T_H7, T_H8, T_KEY_FOUND, T_EXIT_FOUND,
    T_I0, T_I1, T_I2, T_I3, T_I4, T_I5, T_I6, T_I7, T_I8, T_I9, T_PRESSING, T_WATCH, T_BTN_UP, T_BTN_A, T_BTN_B,
    T_NEXT, T_DEMO_KEYS, T_CONTROLS, T_MAP_NEXT, T_COUNT
};

const char* const UA[T_COUNT] = {
    "Munkiki's Castles", "Натисни будь-яку кнопку", "Продовжити", "Нова гра", "Рівень < %d >",
    "Налаштування", "Рекорди", "Як грати", "Вихід",
    "Екран: Nokia 3410", "Екран: чорно-білий", "Звук: увімкн.", "Звук: вимкн.", "Мова: українська",
    "B назад", "A вибрати",
    "Рівень %d", "Збери ноти: %d", "Збери %d нот або ключ", "A - грати", "Пауза", "Продовжити",
    "Почати знову (-1 життя)", "Головне меню",
    "Рівень пройдено!", "Очки: +%d", "Рахунок: %d", "Рекорд: %d", "Усі замки пройдено!", "Життя: %d", "Ноти: %d",
    "Вгору / вниз - крок вперед / назад", "Вліво / вправо - поворот", "A - штовхнути блок",
    "B - вдарити палицею", "C - роззирнутися", "SELECT - змінити екран", "START - пауза",
    "Збери всі ноти, щоб пройти рівень.", "Ключ відкриває новий замок.", "Спіраль веде в кінець замку.",
    "Збери всі ноти на рівні - і рівень пройдено.",
    "Вгору - крок уперед, вниз - крок назад, вліво і вправо - поворот.",
    "Мункікі сам перестрибує невеликі ями і вилазить на сходинку заввишки в один блок.",
    "Щоб штовхнути блок, натисни A.",
    "Є чотири особливі види блоків. Крихкі блоки розбиваються палицею - натисни B.",
    "Пружинні блоки підкидають Мункікі вище.",
    "Ковзкі блоки їдуть, доки не вдаряться в перешкоду.",
    "Блоки-ліфти самі їздять рівнем і можуть тебе підвезти.",
    "Якщо застряг, натисни START і вибери \"Почати знову\".",
    "Натисни C, щоб подивитися на світ очима Мункікі.",
    "Натискаємо: %s", "Дивись, як їде ліфт", "Вгору", "A", "B",
    "A далі", "A далі   B меню", "Керування", "Далі: рівень %d",
};

const char* const EN[T_COUNT] = {
    "Munkiki's Castles", "Press any button", "Continue", "New game", "Level < %d >",
    "Options", "High score", "Instructions", "Exit",
    "Screen: Nokia 3410", "Screen: black & white", "Sound: on", "Sound: off", "Language: English",
    "B back", "A select",
    "Level %d", "Collect notes: %d", "Collect %d notes or the key", "A - play", "Paused", "Return to game",
    "Restart level (-1 life)", "Main menu",
    "Level complete!", "Points: +%d", "Score: %d", "Best: %d", "All castles cleared!", "Lives: %d", "Notes: %d",
    "Up / down - step forward / back", "Left / right - turn", "A - push a block", "B - hit with the stick",
    "C - look around", "SELECT - change screen", "START - pause",
    "Collect all the notes to clear a level.", "The key opens a new castle.", "The spiral leads to the castle's end.",
    "Collect every note on a level to clear it.",
    "Up steps forward, down steps back, left and right turn around.",
    "Munkiki jumps over small gaps and climbs one-block steps all by himself.",
    "To push a block, press A.",
    "There are four special kinds of blocks. Breakable ones break when you hit them with the stick - press B.",
    "Bouncy blocks throw Munkiki higher.",
    "Sliding blocks keep sliding until they hit something.",
    "Travelling blocks move around the level and can give you a ride.",
    "Stuck? Press START and choose Restart level.",
    "Press C to see the world through Munkiki's eyes.",
    "Pressing: %s", "Watch the travelling block", "Up", "A", "B",
    "A next", "A next   B menu", "Controls", "Next: level %d",
};

// Instructions: text pages and the original demo levels 90-95, where a key is pressed for you.
struct Page { int text, level, key, button; };
const Page PAGES[] = {
    {T_I0, 0, -1, 0}, {T_I1, 0, -1, 0}, {T_I2, 0, -1, 0}, {0, 90, K_FWD, T_BTN_UP},
    {T_I3, 0, -1, 0}, {0, 91, K_PUSH, T_BTN_A}, {T_I4, 0, -1, 0}, {0, 92, K_HIT, T_BTN_B},
    {T_I5, 0, -1, 0}, {0, 93, K_FWD, T_BTN_UP}, {T_I6, 0, -1, 0}, {0, 94, K_PUSH, T_BTN_A},
    {T_I7, 0, -1, 0}, {0, 95, -1, 0}, {T_I8, 0, -1, 0}, {T_I9, 0, -1, 0},
};
const int PAGE_COUNT = sizeof(PAGES) / sizeof(PAGES[0]);

// Nokia-style layout on the 280x240 screen
const int HEAD_H = 34;
const int ROW_Y = 44, ROW_H = 26, ROWS = 6;
const int FOOT_Y = 206;

} // namespace

const char* App::tr(int id) const {
    if (id < 64 && !off_[id].empty()) return off_[id].c_str();
    return (save_.lang ? EN : UA)[id];
}

// Strings of the original game, by our string id -> index in the jar's language file.
static const struct { int id, nokia; } OFFICIAL[] = {
    {T_CONTINUE, 0}, {T_NEW, 1}, {T_HISCORES, 2}, {T_HELP, 3}, {T_LEVEL, 4}, {T_COLLECT, 5}, {T_COLLECT_KEY, 6},
    {T_I0, 7}, {T_I1, 8}, {T_I2, 9}, {T_I3, 10}, {T_I4, 11}, {T_I5, 12}, {T_I6, 13}, {T_I7, 14}, {T_I8, 15},
    {T_I9, 16}, {T_RESUME, 18}, {T_RESTART, 19}, {T_PAUSE, 20}, {T_OPTIONS, 22},
};

void App::buildTexts() {
    for (auto& s : off_) s.clear();
    const std::vector<std::string>& src = save_.lang ? A.text : A.textUk;
    if (src.size() < 27) return;
    // phone keys -> Lilka buttons
    static const char* const keysUA[] = {"вгору", "вліво", "вправо", "вниз", "A", "B", "START", "C"};
    static const char* const keysEN[] = {"Up", "Left", "Right", "Down", "A", "B", "START", "C"};
    const char* const* names = save_.lang ? keysEN : keysUA;
    for (auto& o : OFFICIAL) {
        const std::string& in = src[o.nokia];
        std::string out;
        for (size_t i = 0; i < in.size(); i++) {
            char c = in[i];
            if (c == '%' && i + 1 < in.size() && in[i + 1] == 'N') { out += "%d"; i++; continue; }
            if (c == '%') { out += "%%"; continue; }
            bool alone = (i == 0 || !isalnum((unsigned char)in[i - 1])) && (i + 1 >= in.size() || !isalnum((unsigned char)in[i + 1]));
            const char* keys = "2468550*#";
            int k = -1;
            if (alone) {
                if (c == '2') k = 0; else if (c == '4') k = 1; else if (c == '6') k = 2; else if (c == '8') k = 3;
                else if (c == '5') k = 4; else if (c == '0') k = 5; else if (c == '*') k = 6; else if (c == '#') k = 7;
            }
            (void)keys;
            if (k >= 0 && o.id >= T_I0 && o.id <= T_I9) out += names[k];
            else out += c;
        }
        off_[o.id] = out;
    }
}

std::string App::fmt(int id, int v) const {
    char buf[96];
    snprintf(buf, sizeof buf, tr(id), v);
    return buf;
}

void App::setScheme(int s) {
    save_.scheme = (uint8_t)(s % SCHEME_COUNT);
    R.setScheme((Scheme)save_.scheme);
}

void App::init(uint16_t* screen, const SaveData& save) {
    screen_ = screen;
    save_ = save;
    if (save_.magic != SaveData().magic) save_ = SaveData();
    view_.assign(OUT_W * OUT_H, 0);
    R.init(A, view_.data());
    setScheme(save_.scheme);
    buildTexts();
    for (int i = 0; i < 12 && i < (int)A.sounds.size(); i++) parseRingtone(A.sounds[i], melodies_[i]);
    pickLevel_ = save_.maxLevel;
    enter(TITLE);
    playSound(S_TITLE);
}

void App::persist() { saveDirty_ = true; }

void App::playSound(int id) {
    if (save_.sound && id >= 0 && id < 12 && !melodies_[id].notes.empty()) pendingSound_ = id;
}

void App::setButton(Button b, bool down) {
    if (down && !held_[b]) pressed_[b] = true;
    held_[b] = down;
    // in play, buttons go straight to the game (it latches presses itself)
    static const int map[BTN_COUNT] = {K_FWD, K_BACK, K_LEFT, K_RIGHT, K_PUSH, K_HIT, K_LOOK, K_LOOK, -1, -1};
    if (state_ == PLAY && map[b] >= 0) game.setKey(map[b], down);
}

void App::enter(State s) {
    state_ = s;
    ticks_ = 0;
    sel_ = 0;
    top_ = 0;
    dirty_ = true;
    if (s != PLAY) game.clearKeys();
}

// Word wrap counting UTF-8 code points (the fonts are monospaced).
std::vector<std::string> App::wrap(const std::string& s, int maxChars) const {
    std::vector<std::string> lines;
    std::string line, word;
    int lineLen = 0, wordLen = 0;
    auto flushWord = [&]() {
        if (wordLen == 0) return;
        if (lineLen && lineLen + 1 + wordLen > maxChars) { lines.push_back(line); line.clear(); lineLen = 0; }
        if (lineLen) { line += ' '; lineLen++; }
        line += word;
        lineLen += wordLen;
        word.clear();
        wordLen = 0;
    };
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == ' ') { flushWord(); continue; }
        word += (char)c;
        if ((c & 0xC0) != 0x80) wordLen++;
    }
    flushWord();
    if (lineLen) lines.push_back(line);
    return lines;
}

void App::showMap(int done, int last, int next) {
    mapSpot_ = done;
    mapLast_ = last;
    mapNext_ = next;
    map_.begin(A.mapScreen, done, game.keysFound);
    enter(MAP);
}

void App::startPage(int p) {
    if (p >= PAGE_COUNT) { enter(HELP); return; }
    if (p < 0) p = 0;
    enter(INSTR);
    page_ = p;
    if (PAGES[p].level) {
        game.lifeTaken = false;
        game.startLevel(PAGES[p].level);
    }
}

std::vector<std::string> App::mainItems() const {
    std::vector<std::string> v;
    if (save_.inProgress) v.push_back(tr(T_CONTINUE));
    v.push_back(tr(T_NEW));
    v.push_back(fmt(T_LEVEL_PICK, pickLevel_));
    v.push_back(tr(T_OPTIONS));
    v.push_back(tr(T_HISCORES));
    v.push_back(tr(T_HELP));
    v.push_back(tr(T_QUIT));
    return v;
}

void App::startLevel(int n, bool showIntro) {
    level_ = n;
    deaths_ = 0;
    game.lifeTaken = (lifeMask_ >> (n & 31)) & 1;
    if (!game.startLevel(n)) {
        level_ = 1;
        game.startLevel(1);
    }
    save_.level = (uint8_t)level_;
    if (level_ > save_.maxLevel && level_ <= 30) save_.maxLevel = (uint8_t)level_;
    save_.inProgress = 1;
    save_.lives = (int8_t)game.lives;
    save_.score = score_;
    save_.keys = (uint8_t)game.keysFound;
    persist();
    enter(showIntro ? INTRO : PLAY);
}

void App::loseLife() {
    if (game.lifeTaken) lifeMask_ |= 1u << (level_ & 31);
    save_.lifeTakenMask = lifeMask_;
    if (game.lives > 0) {
        game.lives--;
        int d = deaths_ + 1;
        startLevel(level_);
        deaths_ = d;
    } else {
        if ((uint32_t)score_ > save_.hiscore) save_.hiscore = score_;
        save_.inProgress = 0;
        persist();
        enter(GAMEOVER);
    }
}

// Points for a cleared level, as in the original.
void App::finishLevel() {
    int bonusKey = game.outcome == O_KEY ? 500 : 0;
    int bonusExit = game.outcome == O_EXIT ? (5 - level_ % 5) * 400 : 0;
    int v;
    if (bonusKey + bonusExit == 0 && game.level.par < game.moves)
        v = 190 * game.level.par / (2 * game.moves - game.level.par) + 10;
    else
        v = 200;
    for (int i = 0; i < deaths_; i++) v = v * 3 >> 2;
    if (level_ < 26) v >>= 1;
    int last = level_ == 30 ? 400 : 0;
    gained_ = v + bonusKey + bonusExit + last;
    // the spiral skips the rest of the castle: +100 for each skipped level
    warpTo_ = level_ + 1;
    if (game.outcome == O_EXIT)
        while (warpTo_ % 5 != 0) { gained_ += 100; warpTo_++; }
    score_ += gained_;
    if ((uint32_t)score_ > save_.hiscore) save_.hiscore = score_;
    enter(TALLY);
}

bool App::update(uint32_t ms) {
    acc_ += ms;
    bool changed = dirty_;
    while (acc_ >= 100) {
        acc_ -= 100;
        tick();
        changed = true;
    }
    return changed;
}

void App::tick() {
    bool any = false;
    for (int i = 0; i < BTN_COUNT; i++) any |= pressed_[i];
    auto took = [&](Button b) { return pressed_[b]; };
    auto moveSel = [&](int n) {
        if (took(BTN_UP)) sel_ = (sel_ + n - 1) % n;
        if (took(BTN_DOWN)) sel_ = (sel_ + 1) % n;
        if (sel_ < top_) top_ = sel_;
        if (sel_ >= top_ + ROWS) top_ = sel_ - ROWS + 1;
        if (took(BTN_UP) || took(BTN_DOWN)) dirty_ = true;
    };
    ticks_++;

    switch (state_) {
        case TITLE:
            if (any && ticks_ > 3) enter(MENU);
            break;

        case MENU: {
            int n = (int)mainItems().size();
            moveSel(n);
            int item = save_.inProgress ? sel_ : sel_ + 1;  // 0 continue, 1 new, 2 level, 3 options, 4 scores, 5 help, 6 exit
            if (item == 2) {
                int maxL = save_.maxLevel < 1 ? 1 : save_.maxLevel;
                if (took(BTN_LEFT) && pickLevel_ > 1) { pickLevel_--; dirty_ = true; }
                if (took(BTN_RIGHT) && pickLevel_ < maxL) { pickLevel_++; dirty_ = true; }
            }
            if (took(BTN_A) || took(BTN_START)) {
                switch (item) {
                    case 0:
                        score_ = save_.score;
                        game.lives = save_.lives;
                        game.keysFound = save_.keys;
                        lifeMask_ = save_.lifeTakenMask;
                        startLevel(save_.level);
                        break;
                    case 1:
                    case 2:
                        score_ = 0;
                        game.lives = 3;
                        game.keysFound = pickLevel_ > 25 && item == 2 ? pickLevel_ - 25 : 0;
                        lifeMask_ = 0;
                        {
                            int n = item == 1 ? 1 : pickLevel_;
                            showMap(n - 1, n - 1, n);
                        }
                        break;
                    case 3: enter(OPTIONS); break;
                    case 4: enter(HISCORE); break;
                    case 5: startPage(0); break;
                    case 6: exit_ = true; break;
                }
            }
            break;
        }

        case OPTIONS:
            moveSel(3);
            if (took(BTN_B)) {
                enter(MENU);
                sel_ = save_.inProgress ? 3 : 2;
                break;
            }
            if (took(BTN_A) || took(BTN_LEFT) || took(BTN_RIGHT)) {
                if (sel_ == 0) setScheme(save_.scheme + 1);
                else if (sel_ == 1) save_.sound ^= 1;
                else { save_.lang ^= 1; buildTexts(); }
                persist();
                dirty_ = true;
            }
            break;

        case HISCORE:
        case HELP:
            if (state_ == HELP && took(BTN_LEFT)) { startPage(PAGE_COUNT - 1); break; }
            if ((took(BTN_B) || took(BTN_A) || took(BTN_START)) && ticks_ > 2) {
                int back = state_ == HISCORE ? 3 : 5;
                enter(MENU);
                sel_ = save_.inProgress ? back + 1 : back;
            }
            break;

        case INSTR: {
            const Page& pg = PAGES[page_];
            int back = save_.inProgress ? 6 : 5;
            if (took(BTN_B)) { enter(MENU); sel_ = back; top_ = sel_ >= ROWS ? sel_ - ROWS + 1 : 0; break; }
            if (!pg.level) {
                if (took(BTN_A) || took(BTN_RIGHT) || took(BTN_START)) startPage(page_ + 1);
                else if (took(BTN_LEFT) && page_ > 0) startPage(page_ - 1);
                break;
            }
            // demo level: the player only watches, the key is pressed for him
            if (took(BTN_A) || ticks_ > 50) { startPage(page_ + 1); break; }
            if (took(BTN_LEFT) && page_ > 0) { startPage(page_ - 1); break; }
            game.clearKeys();
            bool press = ticks_ == 15 && pg.key >= 0;
            if (press) game.setKey(pg.key, true);
            game.tick();
            if (press) game.setKey(pg.key, false);
            for (int s = 0; s < 12; s++)
                if (game.sounds & (1u << s)) playSound(s);
            if (game.outcome == O_DIED) startPage(page_);
            break;
        }

        case MAP:
            map_.step(ticks_);
            if (ticks_ >= 19 && mapSpot_ < mapLast_) {
                // the spiral warp walks through every skipped level
                mapSpot_++;
                map_.begin(A.mapScreen, mapSpot_, game.keysFound);
                ticks_ = 0;
            } else if ((any && ticks_ > 3) || ticks_ > 50) {
                startLevel(mapNext_);
            }
            break;

        case INTRO:
            if (any && ticks_ > 2) enter(PLAY);
            break;

        case PLAY:
            if (took(BTN_START)) { enter(PAUSE); break; }
            if (took(BTN_SELECT)) {
                setScheme(save_.scheme + 1);
                persist();
            }
            game.tick();
            for (int s = 0; s < 12; s++)
                if (game.sounds & (1u << s)) playSound(s);
            if (game.outcome == O_DIED) loseLife();
            else if (game.outcome != O_NONE) enter(VICTORY);
            break;

        case PAUSE:
            moveSel(3);
            if (took(BTN_B) || took(BTN_START)) { enter(PLAY); break; }
            if (took(BTN_A)) {
                if (sel_ == 0) enter(PLAY);
                else if (sel_ == 1) loseLife();
                else enter(MENU);
            }
            break;

        case VICTORY:
            game.setHeroAnim(11);
            game.tick();
            if (ticks_ > 38) finishLevel();
            break;

        case TALLY:
            if ((any && ticks_ > 5) || ticks_ > 60) {
                int next = warpTo_;
                if (level_ >= 25 + game.keysFound || next > 30) {
                    if ((uint32_t)score_ > save_.hiscore) save_.hiscore = score_;
                    save_.inProgress = 0;
                    persist();
                    playSound(S_TITLE);
                    enter(WIN);
                } else {
                    showMap(level_, next - 1, next);
                }
            }
            break;

        case GAMEOVER:
        case WIN:
            if (any && ticks_ > 10) enter(MENU);
            break;
    }
    for (int i = 0; i < BTN_COUNT; i++) pressed_[i] = false;
}

// ------------------------------------------------------------------ drawing

void App::text(int x, int y, const std::string& s, uint16_t c, int size, int align) {
    texts_.push_back(TextItem{x, y, s, c, (uint8_t)size, (uint8_t)align});
}

void App::rect(int x, int y, int w, int h, uint16_t c) {
    for (int j = y < 0 ? 0 : y; j < y + h && j < SCR_H; j++)
        for (int i = x < 0 ? 0 : x; i < x + w && i < SCR_W; i++) screen_[j * SCR_W + i] = c;
}

// A framed box, like Nokia's notes and dialogs.
void App::panel(int x, int y, int w, int h) {
    rect(x, y, w, h, R.ink());
    rect(x + 3, y + 3, w - 6, h - 6, R.paper());
    rect(x + 5, y + 5, w - 10, h - 10, R.ink());
    rect(x + 6, y + 6, w - 12, h - 12, R.paper());
}

// Selection marker: a solid triangle pointing right.
void App::arrow(int x, int y) {
    for (int i = 0; i < 8; i++) rect(x + i, y + i, 1, 16 - 2 * i, R.ink());
}

void App::header(const std::string& title) {
    rect(0, 0, SCR_W, SCR_H, R.paper());
    rect(0, 0, SCR_W, HEAD_H, R.ink());
    text(10, 7, title, R.paper(), 2, 0);
}

void App::footer(bool back, bool select) {
    rect(0, FOOT_Y, SCR_W, 2, R.ink());
    if (back) text(10, FOOT_Y + 10, tr(T_BACK), R.ink(), 2, 0);
    if (select) text(SCR_W - 10, FOOT_Y + 10, tr(T_SELECT), R.ink(), 2, 2);
}

void App::nokiaMenu(const std::string& title, const std::vector<std::string>& items, int sel, bool back) {
    header(title);
    int n = (int)items.size();
    for (int r = 0; r < ROWS && top_ + r < n; r++) {
        int i = top_ + r, y = ROW_Y + r * ROW_H;
        if (i == sel) arrow(10, y + 2);
        text(28, y, items[i], R.ink(), 2, 0);
    }
    // scroll bar on the right, as on the phone
    if (n > ROWS) {
        int x = SCR_W - 8, y0 = ROW_Y, h = ROWS * ROW_H - 6;
        rect(x + 2, y0, 2, h, R.ink());
        int th = h * ROWS / n, ty = y0 + (h - th) * sel / (n - 1);
        rect(x, ty, 6, th, R.ink());
    }
    footer(back, true);
}

// 288-wide render target -> 280-wide screen (crop 4 px on each side)
void App::copyView() {
    for (int y = 0; y < OUT_H; y++)
        memcpy(screen_ + y * SCR_W, view_.data() + y * OUT_W + (OUT_W - SCR_W) / 2, SCR_W * 2);
}

void App::statusBar() {
    rect(0, BAR_Y, SCR_W, SCR_H - BAR_Y, R.paper());
    rect(0, BAR_Y, SCR_W, 2, R.ink());
    text(8, BAR_Y + 7, fmt(T_LEVEL, level_), R.ink(), 1, 0);
    text(SCR_W / 2, BAR_Y + 7, fmt(T_NOTES, game.notesLeft), R.ink(), 1, 1);
    text(SCR_W - 8, BAR_Y + 7, fmt(T_LIVES, game.lives), R.ink(), 1, 2);
    char buf[32];
    snprintf(buf, sizeof buf, "%d", score_);
    text(SCR_W / 2, BAR_Y + 26, buf, R.ink(), 0, 1);
}

void App::render() {
    texts_.clear();
    dirty_ = false;
    switch (state_) {
        case TITLE: {
            R.drawPicture(A.title);
            copyView();
            rect(0, BAR_Y, SCR_W, SCR_H - BAR_Y, R.paper());
            rect(0, BAR_Y, SCR_W, 2, R.ink());
            if ((ticks_ / 5) & 1 || ticks_ < 5) text(SCR_W / 2, BAR_Y + 14, tr(T_PRESS), R.ink(), 1, 1);
            break;
        }
        case MENU:
            nokiaMenu(tr(T_TITLE), mainItems(), sel_, false);
            break;
        case OPTIONS:
            nokiaMenu(tr(T_OPTIONS),
                      {tr(save_.scheme ? T_SCHEME_BW : T_SCHEME_3410), tr(save_.sound ? T_SOUND_ON : T_SOUND_OFF),
                       tr(T_LANG)},
                      sel_, true);
            break;
        case HISCORE: {
            header(tr(T_HISCORES));
            text(SCR_W / 2, 100, fmt(T_RECORD, (int)save_.hiscore), R.ink(), 2, 1);
            footer(true, false);
            break;
        }
        case INSTR: {
            const Page& pg = PAGES[page_];
            char num[16];
            snprintf(num, sizeof num, "%d/%d", page_ + 1, PAGE_COUNT + 1);
            if (!pg.level) {
                header(tr(T_HELP));
                text(SCR_W - 10, 7, num, R.paper(), 2, 2);
                auto lines = wrap(tr(pg.text), 26);
                if (lines.size() <= 6) {
                    for (size_t i = 0; i < lines.size(); i++) text(10, 46 + (int)i * 26, lines[i], R.ink(), 2, 0);
                } else {
                    lines = wrap(tr(pg.text), 32);
                    for (size_t i = 0; i < lines.size() && i < 9; i++) text(10, 44 + (int)i * 18, lines[i], R.ink(), 1, 0);
                }
                footer(true, false);
                text(SCR_W - 10, FOOT_Y + 10, tr(T_NEXT), R.ink(), 2, 2);
            } else {
                R.drawGame(game);
                copyView();
                bool flash = pg.key >= 0 && ticks_ >= 14 && ticks_ < 26;
                uint16_t bg = flash ? R.ink() : R.paper(), fg = flash ? R.paper() : R.ink();
                rect(0, BAR_Y, SCR_W, SCR_H - BAR_Y, bg);
                rect(0, BAR_Y, SCR_W, 2, R.ink());
                char cap[96];
                if (pg.key >= 0) snprintf(cap, sizeof cap, tr(T_PRESSING), tr(pg.button));
                else snprintf(cap, sizeof cap, "%s", tr(T_WATCH));
                text(SCR_W / 2, BAR_Y + 7, cap, fg, 1, 1);
                text(SCR_W / 2, BAR_Y + 26, tr(T_DEMO_KEYS), fg, 0, 1);
                text(SCR_W - 6, BAR_Y + 26, num, fg, 0, 2);
            }
            break;
        }
        case MAP: {
            R.drawMono(map_.screen());
            copyView();
            rect(0, BAR_Y, SCR_W, SCR_H - BAR_Y, R.paper());
            rect(0, BAR_Y, SCR_W, 2, R.ink());
            text(SCR_W / 2, BAR_Y + 7, fmt(T_MAP_NEXT, mapNext_), R.ink(), 1, 1);
            text(SCR_W / 2, BAR_Y + 26, tr(T_NEXT), R.ink(), 0, 1);
            break;
        }
        case HELP: {
            header(tr(T_CONTROLS));
            {
                char num[16];
                snprintf(num, sizeof num, "%d/%d", PAGE_COUNT + 1, PAGE_COUNT + 1);
                text(SCR_W - 10, 7, num, R.paper(), 2, 2);
            }
            int ids[] = {T_H1, T_H2, T_H3, T_H4, T_H5, T_H6, T_H7, T_H8, T_KEY_FOUND, T_EXIT_FOUND};
            for (int i = 0; i < 10; i++) text(10, 40 + i * 16, tr(ids[i]), R.ink(), i < 7 ? 1 : 0, 0);
            footer(true, false);
            break;
        }
        case INTRO: {
            R.drawGame(game);
            copyView();
            statusBar();
            bool keyLevel = level_ % 5 == 0 && level_ < 25;
            auto lines = wrap(fmt(keyLevel ? T_COLLECT_KEY : T_COLLECT, game.level.notes), 27);
            if (lines.size() > 4) lines.resize(4);
            int h = 70 + (int)lines.size() * 17;
            panel(14, 30, SCR_W - 28, h);
            text(SCR_W / 2, 42, fmt(T_LEVEL, level_), R.ink(), 2, 1);
            for (size_t i = 0; i < lines.size(); i++) text(SCR_W / 2, 70 + (int)i * 17, lines[i], R.ink(), 1, 1);
            text(SCR_W / 2, 30 + h - 26, tr(T_GO), R.ink(), 0, 1);
            break;
        }
        case PLAY:
        case VICTORY:
            R.drawGame(game);
            copyView();
            statusBar();
            break;
        case PAUSE:
            nokiaMenu(tr(T_PAUSE), {tr(T_RESUME), tr(T_RESTART), tr(T_TO_MENU)}, sel_, true);
            break;
        case TALLY: {
            R.drawGame(game);
            copyView();
            statusBar();
            panel(20, 36, SCR_W - 40, 120);
            text(SCR_W / 2, 50, tr(T_DONE), R.ink(), 2, 1);
            if (game.outcome == O_KEY) text(SCR_W / 2, 80, tr(T_KEY_FOUND), R.ink(), 0, 1);
            if (game.outcome == O_EXIT) text(SCR_W / 2, 80, tr(T_EXIT_FOUND), R.ink(), 0, 1);
            text(SCR_W / 2, 100, fmt(T_POINTS, gained_), R.ink(), 1, 1);
            text(SCR_W / 2, 122, fmt(T_SCORE, score_), R.ink(), 1, 1);
            break;
        }
        case GAMEOVER:
        case WIN: {
            R.drawPicture(state_ == WIN ? A.title : A.gameOver);
            copyView();
            rect(0, BAR_Y, SCR_W, SCR_H - BAR_Y, R.paper());
            rect(0, BAR_Y, SCR_W, 2, R.ink());
            if (state_ == WIN) {
                panel(30, 66, SCR_W - 60, 44);
                text(SCR_W / 2, 80, tr(T_WIN), R.ink(), 1, 1);
            }
            text(SCR_W / 2, BAR_Y + 7, fmt(T_SCORE, score_), R.ink(), 1, 1);
            text(SCR_W / 2, BAR_Y + 26, fmt(T_RECORD, (int)save_.hiscore), R.ink(), 0, 1);
            break;
        }
    }
}

} // namespace mk
