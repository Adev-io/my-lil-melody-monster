#include "t.hpp"
#include "track.hpp"
static bool inScale(int semitone, int scaleIdx, int root) {
  int p = posmod(semitone - root, 12);
  for (int i = 0; i < SCALES[scaleIdx].n; ++i) if (SCALES[scaleIdx].semis[i] == p) return true;
  return false;
}
void test_render() {
  Slot pat[64]; generatePattern(pat, 42);
  TrackParams p; Globals g; g.scale = SCALE_MAJOR; g.root = 2; // D major
  // knobs centred: semitone == base melody, always in key, chance 100 plays all
  for (int i = 0; i < 64; ++i) {
    NoteOut o = renderSlot(pat[i], p, g);
    CHECK(o.plays);
    CHECK(inScale(o.semitone, SCALE_MAJOR, 2));
    CHECK_EQ(o.semitone, foldSemitone(48 + degreeToSemitone(SCALE_MAJOR, 2, pat[i].degree)));
  }
  // chance 0 → nothing plays
  p.chance = 0; for (int i = 0; i < 64; ++i) CHECK(!renderSlot(pat[i], p, g).plays); p.chance = 100;
  // NOTE +100 → every note strays up (noteRoll < 100 always true), still in key, never below base
  p.note = 100; int up = 0;
  for (int i = 0; i < 64; ++i) { NoteOut o = renderSlot(pat[i], p, g); CHECK(inScale(o.semitone, SCALE_MAJOR, 2));
    int base = foldSemitone(48 + degreeToSemitone(SCALE_MAJOR, 2, pat[i].degree)); if (o.semitone > base) ++up; }
  CHECK(up > 40); p.note = 0;
  // OCTAVE -100 → multiples of 12 below base (before fold)
  p.octave = -100;
  for (int i = 0; i < 64; ++i) { NoteOut o = renderSlot(pat[i], p, g);
    int base = 48 + degreeToSemitone(SCALE_MAJOR, 2, pat[i].degree); CHECK_EQ(posmod(base - o.semitone, 12), 0); }
  p.octave = 0;
  // weight 100 → every degree lands on root/3rd/5th
  g.weight = 100;
  for (int i = 0; i < 64; ++i) { NoteOut o = renderSlot(pat[i], p, g); int ps = posmod(o.semitone - 2, 12);
    CHECK(ps == 0 || ps == 4 || ps == 7); }
  g.weight = 0;
  // trans +2 = up two degrees; octa -1 = down 12
  p.trans = 2; CHECK_EQ(renderSlot(pat[0], p, g).semitone, foldSemitone(48 + degreeToSemitone(SCALE_MAJOR, 2, pat[0].degree + 2))); p.trans = 0;
  p.octa = -1;  CHECK_EQ(renderSlot(pat[0], p, g).semitone, foldSemitone(36 + degreeToSemitone(SCALE_MAJOR, 2, pat[0].degree))); p.octa = 0;
  // tie / slop / gate length pass through
  p.tie = 100; p.slop = 50; p.gateLen = 80; NoteOut o = renderSlot(pat[0], p, g);
  CHECK(o.tie); CHECK_EQ(o.gateLenPct, 80); CHECK(o.slopPct >= 0 && o.slopPct <= 50);
}
