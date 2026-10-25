#include "t.hpp"
#include "engine.hpp"
#include <cstdio>
#include <cstring>
#include <cstdint>
// Lines 1-5: pattern hashes for seeds 0 1 42 500 999. Then 64 steps of track 0 (seed 42, minor pent, C, length 16, forward).
// Compared to tests/golden_seed42.txt. Regenerate deliberately with: make -C tests golden
static const char* GOLDEN = "tests/golden_seed42.txt";
static uint32_t hashSlots(const Slot* s) { uint32_t h = 2166136261u; for (int i = 0; i < 64; ++i) {
  uint32_t f[9] = {(uint32_t)(int32_t)s[i].degree, s[i].gateRoll, s[i].noteRoll, s[i].noteAmt, s[i].octRoll, s[i].octAmt, s[i].weightRoll, s[i].slopVal, s[i].tieVal};
  for (int k = 0; k < 9; ++k) h = (h ^ f[k]) * 16777619u; } return h; }
void test_golden(bool write) {
  char buf[64 * 16 + 256]; int len = 0;
  int seeds[5] = {0, 1, 42, 500, 999};
  for (int k = 0; k < 5; ++k) { Slot pat[64]; generatePattern(pat, seeds[k]); len += std::sprintf(buf + len, "seed %d hash %u\n", seeds[k], (unsigned)hashSlots(pat)); }
  Engine e; e.p[0].seed = 42; e.p[0].length = 16; e.init(0);
  for (int t = 0; t < 64; ++t) for (int s = 0; s < SUBTICKS; ++s) { e.subTick(s); if (e.out(0).advanced)
    len += std::sprintf(buf + len, "%d:%d:%d\n", e.out(0).stepIndex, e.out(0).noteOn ? 1 : 0, e.out(0).semitone); }
  if (write) { FILE* f = std::fopen(GOLDEN, "wb"); std::fwrite(buf, 1, len, f); std::fclose(f); std::printf("golden written\n"); return; }
  FILE* f = std::fopen(GOLDEN, "rb"); CHECK(f != 0); if (!f) return;
  char ref[64 * 16 + 256]; int rl = (int)std::fread(ref, 1, sizeof ref, f); std::fclose(f);
  CHECK_EQ(rl, len); CHECK(std::memcmp(ref, buf, len < rl ? len : rl) == 0);
}
