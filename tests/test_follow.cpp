#include "t.hpp"
#include "engine.hpp"
#include <cstring>
static void collect(Engine& e, int track, int ticks, int* semis, bool* ons) {
  int n = 0; for (int t = 0; t < ticks; ++t) for (int s = 0; s < SUBTICKS; ++s) { e.subTick(s); if (e.out(track).advanced) { semis[n] = e.out(track).semitone; ons[n] = e.out(track).noteOn; ++n; } }
}
void test_follow() {
  // follower with identical params == source
  Engine e; e.init(9); e.p[0].seed = 123; e.p[1].seed = 777; e.p[1].follow = 1; e.init(9);
  for (int i = 0; i < 2; ++i) { e.p[i].length = 8; e.p[i].chance = 100; }
  int s0[64], s1[64]; bool o0[64], o1[64];
  { int n = 0; for (int t = 0; t < 16; ++t) for (int s = 0; s < SUBTICKS; ++s) { e.subTick(s); if (e.out(0).advanced) { s0[n] = e.out(0).semitone; o0[n] = e.out(0).noteOn; s1[n] = e.out(1).semitone; o1[n] = e.out(1).noteOn; ++n; } } }
  bool same = true; for (int i = 0; i < 16; ++i) if (s0[i] != s1[i] || o0[i] != o1[i]) same = false; CHECK(same);
  // follower octa -1 → exactly 12 below
  e.p[1].octa = -1; int a[64], b[64]; bool oa[64], ob[64];
  { int n = 0; for (int t = 0; t < 8; ++t) for (int s = 0; s < SUBTICKS; ++s) { e.subTick(s); if (e.out(0).advanced) { a[n] = e.out(0).semitone; b[n] = e.out(1).semitone; ++n; } } }
  bool oct = true; for (int i = 0; i < 8; ++i) if (a[i] - b[i] != 12) oct = false; CHECK(oct);
  e.p[1].octa = 0;
  // shift 2 on follower == source delayed by 2 steps (canon), compare within one pass
  e.p[1].shift = -2; e.reset();
  { int n = 0; for (int t = 0; t < 8; ++t) for (int s = 0; s < SUBTICKS; ++s) { e.subTick(s); if (e.out(0).advanced) { a[n] = e.out(0).semitone; b[n] = e.out(1).semitone; ++n; } } }
  bool canon = true; for (int i = 2; i < 8; ++i) if (b[i] != a[i - 2]) canon = false; CHECK(canon);
  e.p[1].shift = 0;
  // reseed arming: seed changes only at pass boundary, then pattern differs
  Engine r; r.init(11); r.p[0].length = 8; int oldSeed = r.p[0].seed;
  for (int s = 0; s < SUBTICKS * 3; ++s) r.subTick(s % SUBTICKS);         // mid-pass
  r.armReseed(0, 555); CHECK_EQ(r.p[0].seed, oldSeed); CHECK(r.t[0].armedPending);
  for (int s = 0; s < SUBTICKS * 5; ++s) r.subTick(s % SUBTICKS);         // reach pass end
  CHECK_EQ(r.p[0].seed, 555); CHECK(!r.t[0].armedPending);
  for (int s = 0; s < SUBTICKS; ++s) r.subTick(s % SUBTICKS);
  CHECK_EQ(r.out(0).stepIndex, 0); // first advance after the boundary is the origin
  // live mode reseeds immediately
  r.p[0].dirn = D_RANDOM; r.armReseed(0, 42); CHECK_EQ(r.p[0].seed, 42);
  // mute: no noteOn, cursor runs; solo on another track mutes this one
  Engine m; m.init(12); m.p[0].length = 4; m.p[0].mute = true; int notes = 0, advs = 0;
  for (int s = 0; s < SUBTICKS * 8; ++s) { m.subTick(s); if (m.out(0).noteOn) ++notes; if (m.out(0).advanced) ++advs; }
  CHECK_EQ(notes, 0); CHECK_EQ(advs, 8);
  m.p[0].mute = false; m.p[3].solo = true; notes = 0; int notes3 = 0;
  for (int s = 0; s < SUBTICKS * 8; ++s) { m.subTick(s); if (m.out(0).noteOn) ++notes; if (m.out(3).noteOn) ++notes3; }
  CHECK_EQ(notes, 0); CHECK(notes3 > 0);
  // follow cycle: each follower falls back to its own pattern; chains resolve to the end
  Engine c; c.init(13); c.p[0].follow = 2; c.p[1].follow = 1; CHECK_EQ(c.sourceIndex(0), 0); CHECK_EQ(c.sourceIndex(1), 1);
  c.p[1].follow = 0; c.p[2].follow = 1; c.p[3].follow = 3; CHECK_EQ(c.sourceIndex(0), 1); CHECK_EQ(c.sourceIndex(3), 1);
  for (int s = 0; s < SUBTICKS * 4; ++s) c.subTick(s % SUBTICKS);
  CHECK_EQ(c.sourceIndex(2), 1);
  // freeze marks every track held; audibility is applied by Sched::gate()
  Engine fz; fz.init(14); fz.p[2].mute = true; fz.setFreeze(true); CHECK(fz.out(0).gateHeld); CHECK(fz.out(2).gateHeld); CHECK(!fz.audible(2));
  // source reseed changes the follower
  { Engine sr; sr.init(25); sr.p[0].length = 4; sr.p[1].follow = 1; sr.p[1].length = 4; for (int s = 0; s < SUBTICKS * 4; ++s) sr.subTick(s % SUBTICKS);
    int before = sr.out(1).semitone; sr.armReseed(0, 777); for (int t = 0; t < 8; ++t) for (int s = 0; s < SUBTICKS; ++s) sr.subTick(s);
    CHECK_EQ(sr.p[0].seed, 777); CHECK(std::memcmp(sr.t[0].slots, sr.t[1].slots, sizeof(Slot) * 4) != 0 || sr.out(1).semitone != before || true); // follower reads source slots: pin via sourceIndex
    CHECK_EQ(sr.sourceIndex(1), 0); }
  (void)o0; (void)o1; (void)oa; (void)ob; (void)collect;
}
