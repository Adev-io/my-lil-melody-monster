#pragma once
#include "track.hpp"

struct TrackOut {
  bool noteOn = false;      // true for exactly one subTick when a step sounds
  int semitone = 48;        // last rendered pitch (holds on rests)
  int stepIndex = 0;        // slot index being played (after shift)
  int gateLenPct = 50, slopPct = 0;
  bool tie = false;
  bool passEnd = false;     // true for one subTick at a pass boundary (EOS)
  bool gateHeld = false;    // freeze bloom
  bool breathing = false;   // this pass is silent
  bool advanced = false;    // true for one subTick when the cursor moved (even if resting)
};

struct TrackState {
  Slot slots[MAX_STEPS];
  Walk walk;
  RateStepper rate;
  Rng seeded;               // per-pass seeded stream (Gravity)
  Rng live;                 // never saved
  int passIdx = 0;
  int advancesInPass = 0;
  int armedSeed = -1;       // -1 = not armed
  bool armedPending = false;
  int lastDirn = 0, lastLength = 16, lastRateIdx = RATE_X1;
  bool breathing = false;
  int subCount = 0;   // step() calls since the last reset/prime boundary
};

class Engine {
public:
  TrackParams p[NUM_TRACKS];
  Globals g;
  TrackState t[NUM_TRACKS];
  int selected = 0;

  void init(uint32_t liveSeed);          // generate all patterns from p[i].seed, reset cursors
  void subTick(int subIdx);   // subIdx = 0..SUBTICKS-1 within the current x1 tick
  const TrackOut& out(int i) const { return o_[i]; }
  void reset();                          // RSET: immediate; every track's next advance is its origin (works while stopped)
  void armReseed(int i, int seed);       // seed -1 = roll a fresh random seed
  void armReseedAll();
  void setFreeze(bool f);
  void setSolo(int i);   // exclusive solo toggle (manual behaviour)
  void applyPatternNow(int i);           // regenerate t[i].slots from p[i].seed, passIdx 0 (Seed slider audition)
  int rollSeed();                        // 0..999 from the live stream of track 0
  bool anySolo() const;
  bool audible(int i) const;             // mute/solo resolution
  int sourceIndex(int i) const;          // follow chain resolved with a visited mask; cycle or invalid -> i itself
private:
  TrackOut o_[NUM_TRACKS];
  void doAdvance(int i);
  void startPass(int i);                 // passIdx++ side effects: evolve, breathe, tide/shuffle, armed reseed
  void applyReseed(int i);
};
