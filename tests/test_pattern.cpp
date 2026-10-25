#include "t.hpp"
#include "track.hpp"
#include <cstdio>
#include <cstdint>
// word-wise FNV-style hash over slot fields in uint32: independent of struct padding and of sizeof(long)
static uint32_t hashPattern(const Slot* s) {
  uint32_t h = 2166136261u;
  for (int i = 0; i < 64; ++i) {
    uint32_t f[9] = {(uint32_t)(int32_t)s[i].degree, s[i].gateRoll, s[i].noteRoll, s[i].noteAmt, s[i].octRoll, s[i].octAmt, s[i].weightRoll, s[i].slopVal, s[i].tieVal};
    for (int k = 0; k < 9; ++k) h = (h ^ f[k]) * 16777619u;
  }
  return h;
}
void test_pattern() {
  Slot a[64], b[64], c[64];
  generatePattern(a, 42); generatePattern(b, 42); generatePattern(c, 43);
  CHECK_EQ(hashPattern(a), hashPattern(b));
  CHECK(hashPattern(a) != hashPattern(c));
  int lo = 99, hi = -99; long sum = 0;
  for (int i = 0; i < 64; ++i) { if (a[i].degree < lo) lo = a[i].degree; if (a[i].degree > hi) hi = a[i].degree; sum += a[i].degree; }
  CHECK(lo >= -7 && hi <= 7);
  CHECK(sum > -64 && sum < 64);          // centred-ish on root
  for (int i = 0; i < 64; ++i) CHECK(a[i].gateRoll < 100 && a[i].noteAmt < 100 && a[i].tieVal < 100);
  // print the hash so Task A9 can pin it
  std::printf("pattern hash seed42 = %u\n", (unsigned)hashPattern(a));
}
