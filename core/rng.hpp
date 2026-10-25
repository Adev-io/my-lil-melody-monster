#pragma once
#include <cstdint>
// splitmix32 for seeding, xoshiro128** for the stream. Public-domain algorithms (Vigna/Blackman), own code.
inline uint32_t splitmix32(uint32_t& x) {
  uint32_t z = (x += 0x9E3779B9u);
  z = (z ^ (z >> 16)) * 0x85EBCA6Bu;
  z = (z ^ (z >> 13)) * 0xC2B2AE35u;
  return z ^ (z >> 16);
}
struct Rng {
  uint32_t s[4];
  explicit Rng(uint32_t seed = 1) { reseed(seed); }
  void reseed(uint32_t seed) {
    uint32_t x = seed * 0x9E3779B9u ^ 0xA5A5u;
    for (int i = 0; i < 4; ++i) s[i] = splitmix32(x);
    if (!(s[0] | s[1] | s[2] | s[3])) s[0] = 1;
  }
  static uint32_t rotl(uint32_t v, int k) { return (v << k) | (v >> (32 - k)); }
  uint32_t next() {
    uint32_t r = rotl(s[1] * 5u, 7) * 9u, t = s[1] << 9;
    s[2] ^= s[0]; s[3] ^= s[1]; s[1] ^= s[2]; s[0] ^= s[3]; s[2] ^= t; s[3] = rotl(s[3], 11);
    return r;
  }
  int u100() { return (int)(next() % 100u); }
  int range(int lo, int hi) { if (hi <= lo) return lo; return lo + (int)(next() % (uint32_t)(hi - lo + 1)); } // inclusive; ponytail: modulo bias ~1e-8, accepted
};
// Derived stream: same (seed,a,b) always yields the same stream.
inline Rng derive(uint32_t seed, uint32_t a, uint32_t b) {
  return Rng(seed ^ (a * 0x85EBCA6Bu) ^ (b * 0x27D4EB2Fu));
}
