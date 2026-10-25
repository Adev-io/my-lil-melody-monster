#pragma once
#include <cstdint>
#include "rng.hpp"
#include "scale.hpp"
#include "rates.hpp"

static const int MAX_STEPS = 64;
static const int NUM_TRACKS = 8;

struct Slot {
  int8_t degree;                       // -7..7 scale degrees from root
  uint8_t gateRoll, noteRoll, noteAmt, octRoll, octAmt, weightRoll, slopVal, tieVal; // 0..99
};

struct TrackParams {
  int seed = 0, chance = 100, note = 0, octave = 0;
  int length = 16, rateIdx = RATE_X1, dirn = 0, trans = 0, shift = 0, octa = 0;
  int evolve = 0, breathe = 0, gateLen = 50, tie = 0, slop = 0;
  float gateLevel = 10.f, pitchScale = 100.f, pitchOffset = 0.f;
  int follow = 0;                      // 0 = off, 1..8 = track
  bool mute = false, solo = false;
};

struct Globals {
  int scale = SCALE_MINOR_PENT, root = 0, weight = 0, bpm = 120;
  bool run = true, freeze = false;
};

struct NoteOut { bool plays; int semitone; int gateLenPct; int slopPct; bool tie; };

void rollSlot(Slot& s, Rng& rng);
void generatePattern(Slot out[MAX_STEPS], int seed);
NoteOut renderSlot(const Slot& s, const TrackParams& p, const Globals& g);

enum Dirn { D_FORWARD, D_REVERSE, D_PENDULUM, D_PONG, D_DRUNK, D_RANDOM, D_TIDE, D_SHUFFLE, D_POOLS,
            D_STRIDE, D_GRAVITY, D_CONVERGE, D_DIVERGE, D_SKITTER, D_ANCHOR, DIRN_COUNT };
static const char* const DIRN_NAMES[DIRN_COUNT] = {"Forward","Reverse","Pendulum","Pong","Drunk","Random",
  "Tide","Shuffle","Pools","Stride","Gravity","Converge","Diverge","Skitter","Anchor"};
inline bool dirnIsLive(int d) { return d == D_DRUNK || d == D_RANDOM || d == D_POOLS || d == D_SKITTER; }

// Walk cursor. advance() returns the step index to play (before shift).
struct Walk {
  int pos = 0;        // last returned step
  int sign = 1;       // pendulum/pong direction
  int k = 0;          // generic counter (stride / converge / diverge / anchor / tide pass count)
  int tideOffset = 0;
  int perm[MAX_STEPS];
  int permPos = 0;
  int pocketStart = 0, pocketSize = 3, pocketLaps = 2, pocketPos = 0;
  int prev = -1;
  int gravityP = 10;  // % chance per step to snap home

  static int origin(int dirn, int length);
  // called at reseed/reset and at every pass boundary for Shuffle (new deal)
  void reset(int dirn, int length, int seed, int passIdx);
  void newPass(int dirn, int length, int seed, int passIdx); // tide offset++, reshuffle
  int advance(int dirn, int length, Rng& seeded, Rng& live);
};
