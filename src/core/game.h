#pragma once
// Game rules. Positions use 16.16 fixed point in block units, like the original,
// so block puzzles behave exactly the same.
#include "assets.h"
#include <cstdint>

namespace mk {

enum Key {
    K_HIT = 0,      // phone key 0: hit with the stick
    K_FWD = 2,      // 2: forward
    K_LEFT = 4,     // 4: turn left
    K_PUSH = 5,     // 5: push
    K_RIGHT = 6,    // 6: turn right
    K_BACK = 8,     // 8: step back
    K_LOOK = 10,    // #: look around
    K_RESTART = 11, // *: restart level / game menu
    K_SOFT = 12,    // right soft key
    K_COUNT = 13
};

enum Sound { S_NOTE = 0, S_KEY = 1, S_EXIT = 2, S_BREAK = 3, S_PUSH = 4, S_JUMP = 5, S_FALL = 7,
             S_ALL_NOTES = 8, S_LIFE = 9, S_TITLE = 10, S_DEATH = 11 };

enum Outcome { O_NONE = 0, O_DIED = 1, O_NOTES = 2, O_KEY = 16, O_EXIT = 24 };

struct RenderBlock { int x, y, z, type, sx, sy, sz; bool travel; };
struct RenderItem { int x, y, z, type; };

class Game {
public:
    explicit Game(const Assets& a) : A(a) {}

    bool startLevel(int number);  // returns false if the level does not exist
    void tick();                   // one game tick (original: 100 ms)

    // Input. Like the phone version, a key press is latched until it is released
    // (so a tap shorter than one tick still counts) or consumed by the game.
    void setKey(int k, bool down);
    void clearKeys();
    bool anyKey() const { for (int k = 0; k < K_COUNT; k++) if (latch_[k]) return true; return false; }
    void setHeroAnim(int a) { anim_ = a; }  // used for the victory dance

    // --- state read by the renderer
    const Assets& A;
    Level level;
    RenderBlock blocks[2048];
    int blockCount = 0;
    RenderItem items[512];
    int itemCount = 0;
    int px = 0, py = 0, pz = 0;  // player position (16.16 blocks)
    int facing = 0;              // 16.16 quarter turns, 0..4
    int view = 0;                // -1 dropping in, 0 normal, 1 look-around (first person)
    int lookYaw = 0, lookPitch = 0;  // 16.16 degrees
    int heroFrame = 0;           // sprite frame code (bit6 = symmetric half, bit7 = mirrored)
    int heroDY = 0;              // sprite vertical offset (pixels at 96x65)
    bool drawHero = true;
    bool onBouncy = false;
    int frame = 0;               // animation counter for spinning items / bouncing blocks
    int starFx = -1;             // block-broken stars effect (0..19)
    int fallFx = -1;             // falling-to-death effect
    int notesLeft = 0;
    int moves = 0;
    Outcome outcome = O_NONE;
    bool lifeTaken = false;      // the level's extra life was already collected

    // Sound requests from the last tick (bit mask of Sound ids).
    uint32_t sounds = 0;
    int lives = 3;
    int keysFound = 0;

private:
    bool key(int k) const { return latch_[k]; }
    void play(int s) { sounds |= 1u << s; }
    bool solid(int cell) const { return cell >= 0 && cell < 2048 && (cg_[cell] & BLOCK_MASK) != 0; }
    int addBlock(int x, int y, int z, int type);
    void mergeWalls();
    int addItem(int x, int y, int z, int type);
    void updateMovers();
    int takeItem(int x, int y, int z);
    void movePlayerMarker(int x, int y, int z);
    int supportBelow(int x, int y, int z);
    int resolveStep(int x, int y, int z, int dx, int dz);
    int tryStep(int x, int y, int z, int dx, int dz);
    int push(int x, int y, int z, int dx, int dz);
    bool breakBlock(int x, int y, int z);
    void startFall(int cell, int x, int y, int z, int itemFlag);
    void dropIfLoose(int cell, int x, int y, int z);

    struct Mover { int ref, vx, vy, vz, ticks, tx, ty, tz; };
    Mover movers_[256];
    int moverCount_ = 0;

    uint8_t cg_[2048];   // cell contents (see Cell)
    uint16_t ch_[2048];  // render object id + 1 | 0x800 moving | 0x1000 item

    bool held_[K_COUNT] = {false};
    bool latch_[K_COUNT] = {false};  // set on press, kept while held, cleared on release or when consumed

    // player marker cell
    int mx_ = 0, my_ = 0, mz_ = 0;
    // animation / movement
    int action_ = 0, anim_ = 0, animPos_ = 0, animLeft_ = 0;
    int dRot_ = 0, dY_ = 0, step_ = 0, dX_ = 0, dZ_ = 0;
    int idleTicks_ = 0;
    bool onTraveller_ = false;
    int travellerId_ = 0;
    int target_ = 0;
};

} // namespace mk
