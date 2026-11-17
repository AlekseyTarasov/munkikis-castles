#include "game.h"
#include <cstring>

namespace mk {

static const int ONE = 65536;
static inline int cellOf(int x, int y, int z) { return x + (z << 4) + (y << 8); }

void Game::setKey(int k, bool down) {
    if (k < 0 || k >= K_COUNT) return;
    if (down && !held_[k]) latch_[k] = true;
    held_[k] = down;
}

void Game::clearKeys() {
    for (int i = 0; i < K_COUNT; i++) held_[i] = latch_[i] = false;
}

// ---------------------------------------------------------------- level setup

int Game::addBlock(int x, int y, int z, int type) {
    RenderBlock& b = blocks[blockCount];
    b.x = x; b.y = y; b.z = z; b.type = type;
    b.sx = b.sy = b.sz = 1;
    b.travel = false;
    return blockCount++;
}

// Neighbouring fixed walls are drawn as one big box: grow each wall along x, then y, then z
// while the box stays completely filled with walls.
void Game::mergeWalls() {
    for (int i = 0; i < blockCount; i++) {
        RenderBlock& b = blocks[i];
        if (b.type != B_WALL) continue;
        int size[3] = {1, 1, 1};
        auto countInside = [&](void) {
            int n = 0;
            for (int j = i + 1; j < blockCount; j++) {
                const RenderBlock& o = blocks[j];
                if (o.type == B_WALL && o.x < b.x + (size[0] << 16) && o.x >= b.x && o.y < b.y + (size[1] << 16) &&
                    o.y >= b.y && o.z < b.z + (size[2] << 16) && o.z >= b.z)
                    n++;
            }
            return n;
        };
        for (int axis = 0; axis < 3;) {
            size[axis]++;
            if (countInside() == size[0] * size[1] * size[2] - 1) continue;
            size[axis]--;
            axis++;
        }
        if (size[0] != 1 || size[1] != 1 || size[2] != 1) {
            for (int j = i + 1; j < blockCount; j++) {
                RenderBlock& o = blocks[j];
                if (o.type == B_WALL && o.x < b.x + (size[0] << 16) && o.x >= b.x && o.y < b.y + (size[1] << 16) &&
                    o.y >= b.y && o.z < b.z + (size[2] << 16) && o.z >= b.z)
                    o.type = 0;
            }
            b.sx = size[0]; b.sy = size[1]; b.sz = size[2];
        }
        // boxes are drawn centred on x/z
        b.x += (b.sx - 1) << 15;
        b.z += (b.sz - 1) << 15;
    }
}

int Game::addItem(int x, int y, int z, int type) {
    if (itemCount >= 512) itemCount = 511;  // cannot happen with real levels; stay in bounds
    RenderItem& it = items[itemCount];
    it.x = x; it.y = y; it.z = z; it.type = type;
    return itemCount++;
}

bool Game::startLevel(int number) {
    if (!A.loadLevel(number, level, lifeTaken)) return false;
    memcpy(cg_, level.grid, sizeof(cg_));
    memset(ch_, 0, sizeof(ch_));
    blockCount = itemCount = moverCount_ = 0;
    notesLeft = level.notes;
    moves = 0;
    outcome = O_NONE;

    for (int y = 0, c = 0; y < 8; y++)
        for (int z = 0; z < 16; z++)
            for (int x = 0; x < 16; x++, c++) {
                uint8_t v = cg_[c];
                if ((v & ITEM_MASK) == I_PLAYER) continue;
                if (v & BLOCK_MASK)
                    ch_[c] = (uint16_t)(1 + addBlock(x << 16, y << 16, z << 16, v & BLOCK_MASK));
                else if (v & ITEM_MASK)
                    ch_[c] = (uint16_t)((1 + addItem(x << 16, y << 16, z << 16, v & ITEM_MASK)) | 0x1000);
                if ((v & 0xC0) == TRAVELLER && (v & BLOCK_MASK)) blocks[(ch_[c] & 0x7FF) - 1].travel = true;
                if ((v & 0xC0) == TRAVELLER)
                    movers_[moverCount_++] = Mover{ch_[c], 0, 0, 0, 1, x, y, z};
            }
    mergeWalls();

    mx_ = level.startX; my_ = level.startY; mz_ = level.startZ;
    px = level.startX << 16; py = level.startY << 16; pz = level.startZ << 16;
    facing = level.facing << 16;

    // intro: Munkiki drops in from the top of the screen
    view = -1;
    heroDY = -64;
    drawHero = true;
    lookYaw = lookPitch = 0;
    starFx = fallFx = -1;
    dRot_ = dY_ = step_ = dX_ = dZ_ = 0;
    idleTicks_ = 0;
    action_ = 17;
    anim_ = 15;
    animPos_ = 0;
    animLeft_ = (int)A.anims[anim_].size();
    onTraveller_ = false;
    clearKeys();
    return true;
}

// ---------------------------------------------------------------- world helpers

int Game::takeItem(int x, int y, int z) {
    int c = cellOf(x, y, z);
    int it = cg_[c] & ITEM_MASK;
    if (it == I_PLAYER) it = 0;
    if (it) {
        if (ch_[c] & 0x1000) items[(ch_[c] & 0x7FF) - 1].type = 0;
        cg_[c] &= ~ITEM_MASK;
        ch_[c] = 0;
    }
    return it;
}

void Game::startFall(int cell, int x, int y, int z, int itemFlag) {
    if (moverCount_ >= 256 || cell < 256) return;
    Mover& m = movers_[moverCount_++];
    m = Mover{(ch_[cell] & 0x7FF) | itemFlag, 0, -16384, 0, 4, x, y, z};
    cg_[cell - 256] &= 0x80;
    cg_[cell - 256] |= cg_[cell] & 0x7F;
    ch_[cell - 256] = (uint16_t)(ch_[cell] | 0x800);
    cg_[cell] &= 0x80;
    ch_[cell] = 0;
}

void Game::dropIfLoose(int cell, int x, int y, int z) {
    int t = cg_[cell] & BLOCK_MASK;
    if (t == B_WALL || t == B_FIXED || (cg_[cell] & 0xC0) == TRAVELLER) return;
    startFall(cell, x, y, z, 0);
}

void Game::movePlayerMarker(int x, int y, int z) {
    if (mx_ == x && my_ == y && mz_ == z) return;
    int old = cellOf(mx_, my_, mz_);
    cg_[old] &= 0xC7;
    // whatever rested on Munkiki's head now falls
    if (solid(old + 256)) dropIfLoose(old + 256, x, y, z);
    mx_ = x; my_ = y; mz_ = z;
    cg_[cellOf(x, y, z)] |= I_PLAYER;
}

// 0 = standing on something; negative = falling (how far decides the animation).
int Game::supportBelow(int x, int y, int z) {
    onTraveller_ = false;
    if (y <= 0) return -111;
    int below = cellOf(x, y - 1, z);
    if (!solid(below)) {
        if (y <= 1) return -111;
        if (y == 2 && !solid(below - 256)) return -117;
        if (y > 2 && !solid(below - 256)) return -115;
        return -116;
    }
    if ((cg_[below] & 0xC0) == TRAVELLER) {
        travellerId_ = ch_[below] & 0x7FF;
        onTraveller_ = true;
    }
    return 0;
}

// What happens when walking forward: 0 blocked, 1 walk, 2 climb, 3 jump a gap,
// 4/5 big jumps from a bouncy block. `target_` receives the landing cell.
int Game::resolveStep(int x, int y, int z, int dx, int dz) {
    int P = cellOf(x, y, z);
    int D = dx + dz * 16;
    int front = P + D, far = P + 2 * D;
    if (y > 0 && (cg_[P - 256] & BLOCK_MASK) == B_BOUNCY) {
        if (solid(front) && solid(front + 256) && !solid(front + 512)) { target_ = front + 256; return 4; }
        if (solid(far) && !solid(front - 256) && !solid(far + 256)) { target_ = far; return 5; }
    }
    if (solid(front)) {
        if (!solid(front + 256) && !solid(P + 256)) { target_ = front; return 2; }
        target_ = P - 256;
        return 0;
    }
    if (y <= 0) { target_ = front - 256; return 1; }
    if (solid(far)) { target_ = front - 256; return 1; }
    while (far > 0 && !solid(far)) far -= 256;
    while (front > 0 && !solid(front)) front -= 256;
    if (far < 0 && front < 0) { target_ = -1; return 1; }
    int levelFar = far >> 8, levelFront = front >> 8;
    if (levelFront >= levelFar) {
        if (onTraveller_ && (ch_[front] & 0x7FF) == (ch_[P - 256] & 0x7FF)) { target_ = far; return 3; }
        target_ = front;
        return 1;
    }
    if (y > 1 && levelFar == y - 1 && levelFront == y - 2) { target_ = front; return 1; }
    target_ = far;
    return 3;
}

int Game::tryStep(int x, int y, int z, int dx, int dz) {
    int r = resolveStep(x, y, z, dx, dz);
    bool was = onTraveller_;
    onTraveller_ = false;
    if (target_ > 0 && (cg_[target_] & 0xC0) == TRAVELLER) {
        onTraveller_ = true;
        travellerId_ = ch_[target_] & 0x7FF;
        r = 14;  // step onto a travelling block
    }
    if (was && r != 14) {
        if (target_ > 0) target_ = ch_[target_] & 0x7FF;
        r = 15;  // step off a travelling block
    }
    return r;
}

// 1 = pushed normally, 2 = pushed a sliding block, 0 = does not move.
int Game::push(int x, int y, int z, int dx, int dz) {
    int c = cellOf(x, y, z);
    if (!solid(c)) return 0;
    int t = cg_[c] & BLOCK_MASK;
    if (t == B_FIXED || t == B_WALL) return 0;
    if (ch_[c] & 0x800) return 0;
    int D = dx + 16 * dz;
    if (solid(c + D)) return 0;
    if (cg_[c + D] & ITEM_MASK) return 0;
    if (cg_[c + 256] & ITEM_MASK) return 0;
    if (solid(c + 256) && (cg_[c + 256] & BLOCK_MASK) != B_WALL && (cg_[c + 256] & 0xC0) != TRAVELLER) return 0;
    if (moverCount_ >= 256) return 0;
    cg_[c + D] |= cg_[c] & 0x7F;
    ch_[c + D] = (uint16_t)(ch_[c] | 0x800);
    Mover& m = movers_[moverCount_++];
    m.ref = ch_[c + D] & 0x7FF;
    int r;
    if (t == B_SLIDING) { m.vx = dx * 8192; m.vz = dz * 8192; m.ticks = 8; r = 2; }
    else { m.vx = dx * 4096; m.vz = dz * 4096; m.ticks = 16; r = 1; }
    m.vy = 0;
    m.tx = x + dx; m.ty = y; m.tz = z + dz;
    return r;
}

bool Game::breakBlock(int x, int y, int z) {
    int c = cellOf(x, y, z);
    if ((cg_[c] & BLOCK_MASK) != B_BREAKABLE) return false;
    if ((ch_[c] & 0xF800) == 0x800) return false;
    int id = (ch_[c] & 0x7FF) - 1;
    ch_[c] = 0;
    cg_[c] &= ~BLOCK_MASK;
    int it = cg_[c] & ITEM_MASK;
    if (it) {
        // something was hidden inside
        ch_[c] = (uint16_t)((1 + addItem(x << 16, y << 16, z << 16, it)) | 0x1000);
    } else {
        int cc = c;
        for (int yy = y + 1; yy < 8; yy++) {
            cc += 256;
            if (solid(cc)) dropIfLoose(cc, x, yy - 1, z);
            else if (cg_[cc] & ITEM_MASK) startFall(cc, x, yy - 1, z, 0x1000);
        }
    }
    blocks[id].type = 0;
    starFx = 0;
    return true;
}

// Moving blocks: pushes, slides, falls and travelling platforms.
void Game::updateMovers() {
    for (int i = 0; i < moverCount_; i++) {
        Mover& m = movers_[i];
        int id = (m.ref & 0x7FF) - 1;
        if (m.ref & 0x1000) { items[id].x += m.vx; items[id].y += m.vy; items[id].z += m.vz; }
        else { blocks[id].x += m.vx; blocks[id].y += m.vy; blocks[id].z += m.vz; }
        if (--m.ticks != 0) continue;

        int cell = m.tx + (m.tz << 4) + (m.ty << 8);
        ch_[cell] &= ~0x800;
        bool clearSource = true;
        int sx = m.vx > 0 ? 1 : (m.vx < 0 ? -1 : 0);
        int sz = m.vz > 0 ? 1 : (m.vz < 0 ? -1 : 0);
        int sy = 0;
        if (m.vy > 0) sy = 1;
        else if (m.vy < 0) { if (m.vy == -16384) clearSource = false; sy = -1; }
        int d = sx + sz * 16 + sy * 256;
        if (clearSource && d != 0) {
            ch_[cell - d] = 0;
            cg_[cell - d] &= 0x80;
        }

        if ((cg_[cell] & 0xC0) == TRAVELLER) {
            // follow the track (cells marked 0x80)
            auto at = [&](int c) { return c >= 0 && c < 2048 ? cg_[c] : 0; };
            int nb = 0;
            if (at(cell + 1) == 0x80) nb |= 1;
            if (at(cell - 1) == 0x80) nb |= 2;
            if (at(cell + 16) == 0x80) nb |= 16;
            if (at(cell - 16) == 0x80) nb |= 32;
            if (at(cell + 256) == 0x80 || at(cell + 256) == 0xA8) nb |= 4;
            if (m.ty > 0 && at(cell - 256) == 0x80) nb |= 8;
            if (nb == 0) {
                m.vx = m.vy = m.vz = 0;
                m.ticks = 1;
                continue;
            }
            int from = 0;
            if (sx == -1) from = 1;
            if (sx == 1) from = 2;
            if (sy == -1) from = 4;
            if (sy == 1) from = 8;
            if (sz == -1) from = 16;
            if (sz == 1) from = 32;
            int axis = sy ? 12 : (sz ? 48 : 3);
            nb &= ~from;
            int dir;
            if (nb == 0) dir = from;
            else {
                dir = nb & axis;
                if (!dir) dir = nb & ~axis;
            }
            int nx = 0, ny = 0, nz = 0;
            if (dir & 1) nx = 1;
            else if (dir & 2) nx = -1;
            else if (dir & 16) nz = 1;
            else if (dir & 32) nz = -1;
            else if (dir & 4) ny = 1;
            else if (dir & 8) ny = -1;
            int nd = nx + nz * 16 + ny * 256;
            if (nd) {
                cg_[cell + nd] |= cg_[cell] & 0x7F;
                ch_[cell + nd] = (uint16_t)(ch_[cell] | 0x800);
                ch_[cell] |= 0x800;
            }
            m.vx = nx * 8192; m.vy = ny * 8192; m.vz = nz * 8192;
            m.ticks = 8;
            m.tx += nx; m.ty += ny; m.tz += nz;
        } else if (m.ty > 0 && (cg_[cell - 256] & 0x3F) == 0) {
            // nothing below: fall one cell (4 ticks)
            cg_[cell - 256] |= cg_[cell] & 0x7F;
            cg_[cell] &= 0x80;
            ch_[cell - 256] = (uint16_t)(m.ref | 0x800);
            ch_[cell] = 0;
            m.vx = 0; m.vy = -16384; m.vz = 0; m.ticks = 4;
            m.ty--;
        } else if ((cg_[cell] & BLOCK_MASK) == B_SLIDING && (sz || sx) && !solid(cell + d)) {
            // sliding blocks keep going until they hit something
            cg_[cell + d] |= cg_[cell] & 0x7F;
            ch_[cell + d] = (uint16_t)(m.ref | 0x800);
            m.ticks = 8;
            m.tx += sx; m.tz += sz;
        } else {
            for (int j = i; j < moverCount_ - 1; j++) movers_[j] = movers_[j + 1];
            moverCount_--;
            i--;
        }
    }
}

// ---------------------------------------------------------------- one tick

void Game::tick() {
    sounds = 0;
    frame = (frame + 1) & 0xFF;
    if (starFx >= 0 && ++starFx >= 20) starFx = -1;
    updateMovers();

    if (view == -1) {
        heroDY += 8;
        if (heroDY >= 0) { heroDY = 0; view = 0; }
    }
    if (view == 1) {
        if (key(K_LOOK)) {
            view = 0;
            drawHero = true;
            latch_[K_LOOK] = false;
        } else {
            if (key(K_RIGHT) && lookYaw < 45 * ONE) lookYaw += 5 * ONE;
            else if (key(K_LEFT) && lookYaw > -45 * ONE) lookYaw -= 5 * ONE;
            if (key(K_BACK) && lookPitch < 45 * ONE) lookPitch += 5 * ONE;
            else if (key(K_FWD) && lookPitch > -45 * ONE) lookPitch -= 5 * ONE;
        }
        latch_[K_PUSH] = latch_[K_HIT] = false;
    }

    int f = facing >> 16;
    int dx = (f & 1) ? 2 - f : 0;
    int dz = (f & 1) == 0 ? f - 1 : 0;
    int ix = (px + 32768) >> 16, iy = (py + 32768) >> 16, iz = (pz + 32768) >> 16;

    bool any = false;
    for (int k = 0; k < 12; k++) any |= key(k);
    if (any && (action_ & 64)) animLeft_ = 0;  // idle fidget can be interrupted

    if (animLeft_ == 0 && view == 0 && outcome == O_NONE) {
        int prev = action_;
        action_ = 0;
        dRot_ = dY_ = step_ = dX_ = dZ_ = 0;

        int it = takeItem(ix, iy, iz);
        if (it == I_NOTE) {
            if (--notesLeft == 0) { play(S_ALL_NOTES); outcome = O_NOTES; }
            else play(S_NOTE);
        } else if (it == I_LIFE) {
            lifeTaken = true;
            play(S_LIFE);
            lives++;
        } else if (it == I_KEY) {
            keysFound++;
            play(S_KEY);
            outcome = O_KEY;
        } else if (it == I_EXIT) {
            play(S_EXIT);
            outcome = O_EXIT;
        }

        movePlayerMarker(ix, iy, iz);
        action_ = supportBelow(ix, iy, iz);
        if ((action_ & ~127) == 0 && outcome == O_NONE) {
            if (key(K_LOOK)) {
                view = 1;
                drawHero = false;
                lookYaw = lookPitch = 0;
                latch_[K_LOOK] = false;
            } else if (key(K_FWD)) {
                action_ = tryStep(ix, iy, iz, dx, dz);
            } else if (key(K_RIGHT)) {
                action_ = 6;
            } else if (key(K_LEFT)) {
                action_ = 7;
            } else if (key(K_PUSH)) {
                int r = push(ix + dx, iy, iz + dz, dx, dz);
                if (r == 1) action_ = 8;
                if (r == 2) action_ = 16;
            } else if (key(K_HIT)) {
                action_ = 9;
            } else if (key(K_BACK) && !solid(cellOf(ix - dx, iy, iz - dz))) {
                action_ = onTraveller_ ? tryStep(ix, iy, iz, -dx, -dz) : 10;
            }
        }

        switch (action_) {
            case 0:
                anim_ = 0;
                facing = (facing + 32768) & 0x30000;
                if (!onTraveller_) {
                    px = (px + 32768) & ~0xFFFF;
                    py = (py + 32768) & ~0xFFFF;
                    pz = (pz + 32768) & ~0xFFFF;
                }
                break;
            case 1: moves++; anim_ = 1; step_ = ONE; break;                                 // walk
            case 2: play(S_JUMP); moves++; anim_ = 4; step_ = ONE; dY_ = ONE; break;        // climb
            case 3: play(S_JUMP); step_ = 2 * ONE; moves++; anim_ = 4; break;               // jump gap
            case 14: moves++; anim_ = 4; break;                                             // onto traveller
            case 15:                                                                         // off traveller
                play(S_JUMP);
                if (target_ > 0) {
                    dX_ = blocks[target_ - 1].x - px;
                    dZ_ = blocks[target_ - 1].z - pz;
                    dY_ = ONE + blocks[target_ - 1].y - py;
                } else {
                    step_ = 2 * ONE;
                    dY_ = -1;
                }
                if (dY_ < 0 && target_ > 0) {
                    int h = ONE + blocks[target_ - 1].y - py;
                    dY_ = h >= 0 ? ONE - (h & 0xFFFF) : (h & 0xFFFF);
                }
                moves++;
                anim_ = 4;
                break;
            case 4: play(S_JUMP); moves++; anim_ = 4; step_ = ONE; dY_ = 2 * ONE; break;    // bounce up
            case 5: play(S_JUMP); moves++; anim_ = 4; step_ = 2 * ONE; dY_ = ONE; break;    // bounce over
            case 6: anim_ = 3; dRot_ = ONE; break;                                          // turn right
            case 7: anim_ = 2; dRot_ = -ONE; break;                                         // turn left
            case 8: moves++; anim_ = 5; step_ = ONE; play(S_PUSH); break;                   // push
            case 16: moves++; anim_ = 14; step_ = ONE; play(S_PUSH); break;                 // push slider
            case 9: moves++; anim_ = 6; break;                                              // hit
            case 10: moves++; anim_ = 13; step_ = -ONE; break;                              // step back
            case -117: anim_ = 12; dY_ = -98304; break;
            case -111:
                dY_ = prev == -117 ? -ONE : -98304;
                anim_ = 16;
                play(S_DEATH);
                break;
            case -115:
                if (prev != -115) play(S_FALL);
                anim_ = 8;
                dY_ = -ONE;
                break;
            case -116:
                anim_ = prev == -115 ? 9 : 7;
                dY_ = -ONE;
                break;
        }

        if (action_ != 0) {
            idleTicks_ = 0;
            animLeft_ = (int)A.anims[anim_].size();
            if (step_ != 0) { dX_ = dx * step_; dZ_ = dz * step_; }
            dY_ /= animLeft_;
            dRot_ /= animLeft_;
            dX_ /= animLeft_;
            dZ_ /= animLeft_;
        } else if (idleTicks_++ >= 40) {
            idleTicks_ = 0;
            anim_ = 10;
            animLeft_ = (int)A.anims[anim_].size();
            action_ = 82;
        }
        if (prev != action_) animPos_ = 0;
    }

    if (onTraveller_ && (action_ == 6 || action_ == 7 || action_ == 0 || action_ == 82 || action_ == 9)) {
        const RenderBlock& b = blocks[travellerId_ - 1];
        px = b.x; py = ONE + b.y; pz = b.z;
    }

    if (animLeft_ > 0) {
        if (action_ == 14) {
            const RenderBlock& b = blocks[travellerId_ - 1];
            dX_ = b.x - px;
            dY_ = ONE + b.y - py;
            dZ_ = b.z - pz;
            if (dY_ < 0 && ((dX_ == 0 && dZ_ != 0) || (dX_ != 0 && dZ_ == 0))) dY_ = 0;
            dX_ /= animLeft_;
            dY_ /= animLeft_;
            dZ_ /= animLeft_;
        }
        if (action_ == 9 && animLeft_ == 2 && breakBlock(ix + dx, iy, iz + dz)) play(S_BREAK);
        if (action_ == -111) fallFx++;
        px += dX_;
        py += dY_;
        pz += dZ_;
        if (py < ONE) heroDY += (-dY_) >> 11;
        facing = (facing + dRot_ + 4 * ONE) & (4 * ONE - 1);
        if (--animLeft_ == 0) {
            facing = (facing + 32768) & 0x30000;
            if (action_ == -111) { outcome = O_DIED; anim_ = 0; }
        }
    }

    const auto& tab = A.anims[anim_];
    if (animPos_ >= (int)tab.size()) animPos_ = 0;
    heroFrame = tab[animPos_++];
    onBouncy = iy > 0 && ix >= 0 && ix < 16 && iz >= 0 && iz < 16 && (cg_[cellOf(ix, iy - 1, iz)] & BLOCK_MASK) == B_BOUNCY;
    if (py < ONE) py = ONE;  // the camera never goes below the first floor

    for (int k = 0; k < K_COUNT; k++)
        if (!held_[k]) latch_[k] = false;
}

} // namespace mk
