#include "track.hpp"

void rollSlot(Slot& s, Rng& rng) {
  int a = rng.range(-7, 7), b = rng.range(-7, 7);     // triangular around 0
  int d = (a + b) / 2; if (d < -7) d = -7; if (d > 7) d = 7;        // C++ truncation toward zero, by definition (pinned by golden)
  s.degree = (int8_t)d;
  s.gateRoll = rng.u100(); s.noteRoll = rng.u100(); s.noteAmt = rng.u100();
  s.octRoll = rng.u100();  s.octAmt = rng.u100();   s.weightRoll = rng.u100();
  s.slopVal = rng.u100();  s.tieVal = rng.u100();
}

void generatePattern(Slot out[MAX_STEPS], int seed) {
  Rng rng((uint32_t)seed);
  for (int i = 0; i < MAX_STEPS; ++i) rollSlot(out[i], rng);
}

static int iabs(int v) { return v < 0 ? -v : v; }
static int isign(int v) { return v < 0 ? -1 : 1; }

NoteOut renderSlot(const Slot& s, const TrackParams& p, const Globals& g) {
  NoteOut o;
  o.plays = s.gateRoll < p.chance;
  int deg = s.degree + p.trans;
  if (p.note != 0 && s.noteRoll < iabs(p.note))
    deg += isign(p.note) * (int)(s.noteAmt / 99.0 * iabs(p.note) / 100.0 * 12 + 0.5);
  int oct = 0;
  if (p.octave != 0 && s.octRoll < iabs(p.octave))
    oct = isign(p.octave) * (int)(s.octAmt / 99.0 * iabs(p.octave) / 100.0 * 5 + 0.5);
  if (s.weightRoll < g.weight) deg = snapConsonant(g.scale, deg);
  int st = 48 + 12 * (oct + p.octa) + degreeToSemitone(g.scale, g.root, deg);
  o.semitone = foldSemitone(st);
  o.gateLenPct = p.gateLen;
  o.slopPct = (int)(p.slop * s.slopVal / 99.0 + 0.5);
  o.tie = s.tieVal < p.tie;
  return o;
}

int Walk::origin(int dirn, int length) {
  if (dirn == D_REVERSE) return length - 1;
  if (dirn == D_DIVERGE) return (length - 1) / 2;
  return 0;
}

void Walk::reset(int dirn, int length, int seed, int passIdx) {
  pos = origin(dirn, length); sign = 1; k = 0; tideOffset = 0; prev = -1; permPos = 0;
  pocketPos = 0; pocketLaps = 0;
  gravityP = 5 + seed % 26;
  // the first advance() must return the origin, so park one step "before" it
  if (dirn == D_FORWARD || dirn == D_TIDE || dirn == D_GRAVITY) pos = length - 1;
  if (dirn == D_REVERSE) pos = 0;
  if (dirn == D_PENDULUM || dirn == D_PONG) { pos = -1; sign = 1; }
  newPass(dirn, length, seed, passIdx);
  if (dirn == D_TIDE) tideOffset = 0; // newPass bumped it; first pass is unshifted
}

void Walk::newPass(int dirn, int length, int seed, int passIdx) {
  if (dirn == D_TIDE) tideOffset = (tideOffset + 1) % length;
  if (dirn == D_SHUFFLE) {
    Rng r = derive((uint32_t)seed, (uint32_t)passIdx, 0x5u);
    for (int i = 0; i < length; ++i) perm[i] = i;
    for (int i = length - 1; i > 0; --i) { int j = r.range(0, i); int t = perm[i]; perm[i] = perm[j]; perm[j] = t; }
    permPos = 0;
  }
}

int Walk::advance(int dirn, int length, Rng& seeded, Rng& live) {
  int L = length; int out = 0;
  switch (dirn) {
    case D_FORWARD: out = posmod(pos + 1, L); break;
    case D_REVERSE: out = posmod(pos - 1, L); break;
    case D_PENDULUM: // ends played twice: 0 1 2 3 3 2 1 0 0 1 ...
      if (L == 1) { out = 0; break; }
      { int n = pos + sign; if (n >= L) { sign = -1; n = L - 1; } else if (n < 0) { sign = 1; n = 0; } out = n; }
      break;
    case D_PONG:     // ends played once: 0 1 2 3 2 1 0 1 ...
      if (L == 1) { out = 0; break; }
      { int n = pos + sign; if (n >= L) { sign = -1; n = L - 2; } else if (n < 0) { sign = 1; n = 1; } out = n; }
      break;
    case D_DRUNK: { int r = live.u100(); int d = r < 50 ? 1 : (r < 75 ? 0 : -1); out = posmod(pos + d, L); } break;
    case D_RANDOM: out = live.range(0, L - 1); break;
    case D_TIDE:   out = posmod(pos + 1, L); break;           // offset applied by caller via tideOffset
    case D_SHUFFLE: out = perm[permPos]; permPos = (permPos + 1) % L; break;
    case D_POOLS:
      if (pocketLaps <= 0 || L <= 2) {
        pocketSize = L < 4 ? L : live.range(3, 4);
        pocketStart = live.range(0, L - 1); pocketLaps = live.range(2, 4); pocketPos = 0;
      }
      out = posmod(pocketStart + pocketPos, L);
      if (++pocketPos >= pocketSize) { pocketPos = 0; --pocketLaps; }
      break;
    case D_STRIDE: { // 0,2,1,3,2,4,3,5... cycle 2L
      int i = k % (2 * L); int base = i / 2; out = posmod(base + (i % 2 == 0 ? 0 : 2), L); ++k; } break;
    case D_GRAVITY: out = (seeded.u100() < gravityP) ? 0 : posmod(pos + 1, L); break;
    case D_CONVERGE: { int i = k % L; out = (i % 2 == 0) ? i / 2 : L - 1 - i / 2; ++k; } break;
    case D_DIVERGE: { int i = k % L; int mid = (L - 1) / 2; out = (i % 2 == 0) ? mid - i / 2 : mid + (i + 1) / 2; out = posmod(out, L); ++k; } break;
    case D_SKITTER: { if (L == 1) { out = 0; break; } do { out = live.range(0, L - 1); } while (out == pos); } break; // never the step just played
    case D_ANCHOR: { int i = k % (2 * (L - 1 > 0 ? L - 1 : 1)); out = (i % 2 == 0) ? 0 : (i / 2 + 1) % L; ++k; } break;
    default: out = posmod(pos + 1, L);
  }
  prev = pos; pos = out;
  return out;
}
