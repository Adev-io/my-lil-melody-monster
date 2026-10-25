#include "t.hpp"
#include "sched.hpp"
// 1 kHz "audio" rate keeps numbers readable: 120 BPM -> 500 samples per x1 tick.
static const float DT = 0.001f;
struct Rig { Engine e; Sched s; Rig(int seed = 1) { e.p[0].seed = seed; e.p[0].length = 8; e.p[0].gateLen = 50; e.init(seed); s.eng = &e; } };
static int highSamples(Rig& r, int track, int n, bool ext = false, int extEvery = 0) {
  int cnt = 0; for (int i = 0; i < n; ++i) { bool edge = ext && extEvery && (i % extEvery == 0); r.s.tick(DT, edge, 120, true); if (r.s.gate(track)) ++cnt; } return cnt; }
void test_sched() {
  // internal clock: the very first sample emits tick 0 and a note; gate is high on sample 0 (slop 0)
  { Rig r; r.s.tick(DT, false, 120, true); r.s.tick(DT, false, 120, true); CHECK(r.s.gate(0)); }
  // gate length 50% of a 500-sample step -> ~250 high samples per step over 8 steps (chance 100)
  { Rig r; int h = highSamples(r, 0, 4000); CHECK(h >= 8 * 240 && h <= 8 * 260); }
  // external mode: nothing moves before the first edge
  { Rig r; r.s.setExternal(true); for (int i = 0; i < 2000; ++i) { r.s.tick(DT, false, 120, true); CHECK(!r.s.gate(0)); } CHECK_EQ(r.e.t[0].advancesInPass, 0); }
  // external edges every 400 samples: one advance per edge, period measured from the second edge
  { Rig r; r.s.setExternal(true); highSamples(r, 0, 4001, true, 400); CHECK_EQ(r.e.t[0].advancesInPass + r.e.t[0].passIdx * 8, 11); CHECK(r.s.tickPeriod > 0.39f && r.s.tickPeriod < 0.41f); }
  // RUN off: gates fall at once, time does not move, resume continues the same phase
  { Rig r; highSamples(r, 0, 600); r.s.tick(DT, false, 120, false); CHECK(!r.s.gate(0)); CHECK(!r.s.clockHigh()); int before = r.e.t[0].advancesInPass; for (int i = 0; i < 3000; ++i) r.s.tick(DT, false, 120, false); CHECK_EQ(r.e.t[0].advancesInPass, before); }
  // mute mid-gate drops the gate immediately and holds pitch; a tied gate does not resurrect on unmute
  { Rig r; r.s.tick(DT, false, 120, true); int semi = r.s.semitone(0); r.e.p[0].mute = true; r.s.tick(DT, false, 120, true); CHECK(!r.s.gate(0)); highSamples(r, 0, 2000); CHECK_EQ(r.s.semitone(0), semi); }
  { Rig r; r.e.p[0].tie = 100; highSamples(r, 0, 10); r.e.p[0].mute = true; r.s.tick(DT, false, 120, true); r.e.p[0].mute = false; r.e.p[0].chance = 0; r.s.tick(DT, false, 120, true); CHECK(!r.s.gate(0)); }
  // slop: pitch commits at the delayed gate rise, not before
  { Rig r; r.e.p[0].slop = 100; r.e.t[0].slots[0].slopVal = 99; r.e.t[0].slots[0].degree = 5; r.e.t[0].slots[0].gateRoll = 0;
    int s0 = r.s.semitone(0); r.s.tick(DT, false, 120, true); CHECK_EQ(r.s.semitone(0), s0); CHECK(!r.s.gate(0));
    for (int i = 0; i < 260; ++i) r.s.tick(DT, false, 120, true);
    CHECK(r.s.gate(0)); CHECK(r.s.semitone(0) != s0); }
  // tie: gate never falls between tied notes
  { Rig r; r.e.p[0].tie = 100; highSamples(r, 0, 10); for (int i = 0; i < 3000; ++i) { r.s.tick(DT, false, 120, true); CHECK(r.s.gate(0)); } }
  // freeze: timers pause, gates held for audible tracks, clock out keeps running; unmuting during a freeze blooms that track
  { Rig r; r.e.p[1].mute = true; highSamples(r, 0, 100); r.e.setFreeze(true); int pulses = r.s.clockPulses; for (int i = 0; i < 1000; ++i) { r.s.tick(DT, false, 120, true); CHECK(r.s.gate(0)); CHECK(!r.s.gate(1)); } CHECK(r.s.clockPulses >= pulses + 2);
    r.e.p[1].mute = false; CHECK(r.s.gate(1)); }
  // reset clears gate timers and tie
  { Rig r; r.e.p[0].tie = 100; highSamples(r, 0, 10); r.s.reset(); CHECK(!r.s.gate(0)); }
  // switching to external zeroes clock state: no sub-ticks until an edge
  { Rig r; highSamples(r, 0, 300); r.s.setExternal(true); int a = r.e.t[0].advancesInPass; for (int i = 0; i < 1000; ++i) r.s.tick(DT, false, 120, true); CHECK_EQ(r.e.t[0].advancesInPass, a); }
  // tie through a breathing pass: gate falls within one step of the pass starting
  { Rig r; r.e.p[0].tie = 100; highSamples(r, 0, 4000); r.e.p[0].breathe = 100; highSamples(r, 0, 4000); int h = 0; for (int i = 0; i < 1000; ++i) { r.s.tick(DT, false, 120, true); if (r.s.gate(0)) ++h; } CHECK_EQ(h, 0); }
  // retrigger with slop 0 has a one-sample low gap between consecutive notes
  { Rig r; r.e.p[0].gateLen = 100; int lows = 0; for (int i = 0; i < 4000; ++i) { r.s.tick(DT, false, 120, true); if (!r.s.gate(0)) ++lows; } CHECK(lows >= 7 && lows <= 9); }
  // CLK OUT 50% duty at 120 BPM / 1 kHz: ~250 high samples per 500-sample tick
  { Rig r; int hi = 0; for (int i = 0; i < 5000; ++i) { r.s.tick(DT, false, 120, true); if (r.s.clockHigh()) ++hi; } CHECK(hi >= 2450 && hi <= 2550); }
}
