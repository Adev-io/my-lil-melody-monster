#include "t.hpp"
#include "engine.hpp"
#include <cstring>
static int tickAll(Engine& e, int ticks) { int notes = 0; for (int t = 0; t < ticks; ++t) for (int s = 0; s < SUBTICKS; ++s) { e.subTick(s); if (e.out(0).noteOn) ++notes; } return notes; }
void test_engine() {
  Engine e; e.init(1234);
  e.p[0].length = 8; e.p[0].chance = 100;
  // x1: one note per tick, EOS once per 8 advances
  int eos = 0, notes = 0;
  for (int t = 0; t < 16; ++t) for (int s = 0; s < SUBTICKS; ++s) { e.subTick(s); if (e.out(0).noteOn) ++notes; if (e.out(0).passEnd) ++eos; }
  CHECK_EQ(notes, 16); CHECK_EQ(eos, 2);
  // same seed, fresh engine → identical semitone sequence
  Engine f; f.init(1234); f.p[0].length = 8;
  Engine h; h.init(1234); h.p[0].length = 8;
  bool same = true;
  for (int t = 0; t < 64; ++t) for (int s = 0; s < SUBTICKS; ++s) { f.subTick(s); h.subTick(s); if (f.out(0).noteOn != h.out(0).noteOn || f.out(0).semitone != h.out(0).semitone) same = false; }
  CHECK(same);
  // run off → nothing
  e.g.run = false; CHECK_EQ(tickAll(e, 4), 0); e.g.run = true;
  // freeze → nothing advances, all gates held
  e.setFreeze(true); CHECK_EQ(tickAll(e, 4), 0); for (int i = 0; i < NUM_TRACKS; ++i) CHECK(e.out(i).gateHeld); e.setFreeze(false);
  CHECK(!e.out(0).gateHeld);
  // evolve 0 → pattern unchanged over 100 passes; evolve 100 → changed
  Engine ev; ev.init(1); ev.p[0].length = 4; Slot before[64]; for (int i = 0; i < 64; ++i) before[i] = ev.t[0].slots[i];
  tickAll(ev, 400); bool unchanged = true; for (int i = 0; i < 4; ++i) if (std::memcmp(&before[i], &ev.t[0].slots[i], sizeof(Slot)) != 0) unchanged = false;
  CHECK(unchanged);
  ev.p[0].evolve = 100; tickAll(ev, 4); bool changed = false; for (int i = 0; i < 4; ++i) if (std::memcmp(&before[i], &ev.t[0].slots[i], sizeof(Slot)) != 0) changed = true; CHECK(changed); // pattern changed (a single field may legally re-roll equal)
  // reset while stopped takes effect: next advance after RUN is the origin
  { Engine rs; rs.init(6); rs.p[0].length = 8; tickAll(rs, 3); rs.g.run = false; rs.reset(); rs.g.run = true; for (int s = 0; s < SUBTICKS; ++s) rs.subTick(s); CHECK_EQ(rs.out(0).stepIndex, 0); }
  // rate change mid-run never stalls: switch /64 -> x1 and expect a note within one tick
  { Engine rc; rc.init(7); rc.p[0].length = 8; rc.p[0].rateIdx = 0; tickAll(rc, 2); rc.p[0].rateIdx = RATE_X1; CHECK_EQ(tickAll(rc, 1), 1); }
  // evolve lineage repeatable
  Engine l1, l2; l1.init(5); l2.init(5); l1.p[0].evolve = l2.p[0].evolve = 30; l1.p[0].length = l2.p[0].length = 8;
  tickAll(l1, 80); tickAll(l2, 80); bool lin = true; for (int i = 0; i < 8; ++i) if (l1.t[0].slots[i].degree != l2.t[0].slots[i].degree) lin = false; CHECK(lin);
  // breathe 100 → no notes after the first pass; EOS keeps firing
  Engine b; b.init(2); b.p[0].length = 4; b.p[0].breathe = 100; tickAll(b, 4);
  int n2 = 0, eos2 = 0; for (int t = 0; t < 8; ++t) for (int s = 0; s < SUBTICKS; ++s) { b.subTick(s); if (b.out(0).noteOn) ++n2; if (b.out(0).passEnd) ++eos2; }
  CHECK_EQ(n2, 0); CHECK_EQ(eos2, 2); CHECK(b.out(0).breathing);
  // EOS once per length advances in Random too
  Engine r; r.init(3); r.p[0].length = 8; r.p[0].dirn = D_RANDOM; int eos3 = 0;
  for (int t = 0; t < 32; ++t) for (int s = 0; s < SUBTICKS; ++s) { r.subTick(s); if (r.out(0).passEnd) ++eos3; }
  CHECK_EQ(eos3, 4);
  // rate /2: half the notes
  Engine q; q.init(4); q.p[0].length = 8; q.p[0].rateIdx = RATE_X1 - 2; CHECK_EQ(tickAll(q, 16), 8);
  // --- Gate 1 regressions ---
  // freeze keeps the grid: after a 32-sub-tick freeze an x1 track still advances only at sub-index 0
  { Engine fz; fz.init(21); fz.p[0].length = 8; for (int s = 0; s < SUBTICKS; ++s) fz.subTick(s);
    fz.setFreeze(true); for (int s = 0; s < 32; ++s) fz.subTick(s); fz.setFreeze(false);
    for (int s = 32; s < SUBTICKS; ++s) { fz.subTick(s); CHECK(!fz.out(0).advanced); }
    bool onGrid = true; for (int t = 0; t < 4; ++t) for (int s = 0; s < SUBTICKS; ++s) { fz.subTick(s); if (fz.out(0).advanced != (s == 0)) onGrid = false; } CHECK(onGrid); }
  // reset mid-tick: the origin plays at the next tick boundary, not on the next sub-tick
  { Engine rs; rs.init(22); rs.p[0].length = 8; for (int s = 0; s < SUBTICKS; ++s) rs.subTick(s); for (int s = 0; s < 20; ++s) rs.subTick(s);
    rs.reset(); for (int s = 20; s < SUBTICKS; ++s) { rs.subTick(s); CHECK(!rs.out(0).advanced); }
    rs.subTick(0); CHECK(rs.out(0).advanced); CHECK_EQ(rs.out(0).stepIndex, 0); }
  // rate change mid-tick keeps phase and adds no note: x1 -> x2 at sub-index 10 advances next at 32
  { Engine rc; rc.init(23); rc.p[0].length = 8; for (int s = 0; s < SUBTICKS; ++s) rc.subTick(s); for (int s = 0; s < 10; ++s) rc.subTick(s);
    rc.p[0].rateIdx = RATE_X1 + 4; int firstAdv = -1; for (int s = 10; s < SUBTICKS; ++s) { rc.subTick(s); if (rc.out(0).advanced && firstAdv < 0) firstAdv = s; } CHECK_EQ(firstAdv, 32);
    bool grid = true; for (int t = 0; t < 2; ++t) for (int s = 0; s < SUBTICKS; ++s) { rc.subTick(s); if (rc.out(0).advanced && s % 32 != 0) grid = false; } CHECK(grid); }
  // evolve lineage: whole-slot comparison, different live seeds
  { Engine l1, l2; l1.init(5); l2.init(6); l1.p[0].evolve = l2.p[0].evolve = 30; l1.p[0].length = l2.p[0].length = 8;
    for (int t = 0; t < 80; ++t) for (int s = 0; s < SUBTICKS; ++s) { l1.subTick(s); l2.subTick(s); }
    CHECK(std::memcmp(l1.t[0].slots, l2.t[0].slots, sizeof(Slot) * 8) == 0); }
  // solo is exclusive
  { Engine so; so.init(24); so.setSolo(1); so.setSolo(2); CHECK(!so.p[1].solo); CHECK(so.p[2].solo); so.setSolo(2); CHECK(!so.anySolo()); }
}
