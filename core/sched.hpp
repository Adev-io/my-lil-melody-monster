#pragma once
#include "engine.hpp"
// Time -> engine sub-ticks; engine events -> gate/pitch state. No Rack types. One instance per module.
struct Sched {
  Engine* eng = nullptr;
  // clock
  bool extMode = false;
  double tickPeriod = 0.5;      // seconds per x1 tick (BPM or measured), clamped to [0.005, 10]
  double sinceTick = 0.0;       // seconds since the last tick
  double phase = 1.0;           // internal clock phase; starts at 1 so sample 0 is a tick
  int subTicksDone = SUBTICKS;  // sub-ticks emitted in the current period
  bool extEdgeSeen = false;     // external: no sub-ticks before the first edge
  bool running = true;
  float lastDt = 0.f;           // dt of the current sample; a retrigger waits at least this long
  // gates
  struct GateTimer { float on = -1.f, off = -1.f; bool high = false, tied = false; int pendingSemi = 48; };
  GateTimer gt[NUM_TRACKS];
  int semi[NUM_TRACKS];
  bool wasAudible[NUM_TRACKS];
  // clock out
  float clockOutTimer = 0.f; int clockPulses = 0;

  Sched() { for (int i = 0; i < NUM_TRACKS; ++i) { semi[i] = 48; wasAudible[i] = true; } }
  void setExternal(bool ext);                 // cable transition: zero the clock state
  void tick(float dt, bool extEdge, int bpm, bool run); // one audio sample; extEdge = rising edge on CLK IN this sample
  void reset();                               // RSET: engine reset + clear gate timers
  void stop();                                // RUN fell: gates low, tie cleared
  bool gate(int i) const { return running && (gt[i].high || eng->out(i).gateHeld) && eng->audible(i); }
  int semitone(int i) const { return semi[i]; }
  bool clockHigh() const { return clockOutTimer > 0.f; }
private:
  void emitSubTicks(int upTo, float periodForEvents);
  void handleEvents(float period);
};
