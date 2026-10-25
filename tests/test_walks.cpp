#include "t.hpp"
#include "track.hpp"
static void run(int dirn, int L, int n, int* out, int seed = 1) {
  Walk w; Rng seeded(seed), live(99);
  w.reset(dirn, L, seed, 0);
  for (int i = 0; i < n; ++i) {
    out[i] = w.advance(dirn, L, seeded, live);
    if (dirn == D_TIDE) out[i] = posmod(out[i] + w.tideOffset, L);
    if ((i + 1) % L == 0) w.newPass(dirn, L, seed, (i + 1) / L);
  }
}
static bool seq(const int* a, const int* b, int n) { for (int i = 0; i < n; ++i) if (a[i] != b[i]) return false; return true; }
void test_walks() {
  int L = 5, a[40], b[40];
  { int e[] = {0,1,2,3,4,0,1,2,3,4}; run(D_FORWARD, L, 10, a); CHECK(seq(a, e, 10)); }
  { int e[] = {4,3,2,1,0,4,3,2,1,0}; run(D_REVERSE, L, 10, a); CHECK(seq(a, e, 10)); }
  { int e[] = {0,1,2,3,4,4,3,2,1,0,0,1}; run(D_PENDULUM, L, 12, a); CHECK(seq(a, e, 12)); run(D_PENDULUM, L, 30, a); CHECK(seq(a, a + 10, 20)); } // exact period 2L
  { int e[] = {0,1,2,3,4,3,2,1,0,1,2}; run(D_PONG, L, 11, a); CHECK(seq(a, e, 11)); run(D_PONG, L, 24, a); CHECK(seq(a, a + 8, 16)); } // exact period 2L-2
  { int e[] = {0,2,1,3,2,4,3,0,4,1}; run(D_STRIDE, L, 10, a); CHECK(seq(a, e, 10)); }
  { int e[] = {0,4,1,3,2,0,4,1,3,2}; run(D_CONVERGE, L, 10, a); CHECK(seq(a, e, 10)); }
  { int e[] = {2,3,1,4,0,2,3,1,4,0}; run(D_DIVERGE, L, 10, a); CHECK(seq(a, e, 10)); }
  { int e[] = {0,1,0,2,0,3,0,4,0,1}; run(D_ANCHOR, L, 10, a); CHECK(seq(a, e, 10)); }
  // Tide: pass 0 unshifted, pass 1 shifted by one, realigns after L passes
  { run(D_TIDE, L, 30, a); int e[] = {0,1,2,3,4, 1,2,3,4,0, 2,3,4,0,1}; CHECK(seq(a, e, 15)); CHECK(seq(a, a + 25, 5)); }
  // Shuffle: each pass a permutation, deterministic for same seed
  { run(D_SHUFFLE, L, 10, a, 7); run(D_SHUFFLE, L, 10, b, 7); CHECK(seq(a, b, 10));
    for (int pass = 0; pass < 2; ++pass) { int seen[5] = {0}; for (int i = 0; i < L; ++i) seen[a[pass * L + i]]++; for (int i = 0; i < L; ++i) CHECK_EQ(seen[i], 1); } }
  // Gravity: repeatable with same seeded stream; always within range
  { run(D_GRAVITY, L, 40, a, 3); run(D_GRAVITY, L, 40, b, 3); CHECK(seq(a, b, 40)); for (int i = 0; i < 40; ++i) CHECK(a[i] >= 0 && a[i] < L); }
  // Pendulum/Pong/Stride/Anchor have cycles longer than one pass (2L / 2L-2 / 2L / 2L-2); every step is visited within 2 passes
  { int modes[] = {D_PENDULUM, D_PONG, D_STRIDE, D_ANCHOR}; for (int m = 0; m < 4; ++m) { run(modes[m], L, 2 * L, a); int seen[5] = {0}; for (int i = 0; i < 2 * L; ++i) seen[a[i]]++; for (int i = 0; i < L; ++i) CHECK(seen[i] >= 1); } }
  // Live modes: in range; skitter never repeats the step just played
  { run(D_SKITTER, L, 40, a); for (int i = 1; i < 40; ++i) CHECK(a[i] != a[i - 1]); }
  { run(D_DRUNK, L, 40, a); for (int i = 0; i < 40; ++i) CHECK(a[i] >= 0 && a[i] < L); }
  { run(D_RANDOM, L, 40, a); for (int i = 0; i < 40; ++i) CHECK(a[i] >= 0 && a[i] < L); }
  { run(D_POOLS, 8, 40, a); for (int i = 0; i < 40; ++i) CHECK(a[i] >= 0 && a[i] < 8); }
  // length 1 never crashes
  for (int d = 0; d < DIRN_COUNT; ++d) { run(d, 1, 8, a); for (int i = 0; i < 8; ++i) CHECK_EQ(a[i], 0); }
  // origins
  CHECK_EQ(Walk::origin(D_FORWARD, 5), 0); CHECK_EQ(Walk::origin(D_REVERSE, 5), 4); CHECK_EQ(Walk::origin(D_DIVERGE, 5), 2);
  // Gravity: pulls home more often than a plain forward walk would, and is repeatable after a reset at the same pass
  { int g[200]; run(D_GRAVITY, 8, 200, g, 9); int zeros = 0; for (int i = 0; i < 200; ++i) if (g[i] == 0) ++zeros; CHECK(zeros > 200 / 8 + 5);
    Walk w; Rng s1(9), l1(1); w.reset(D_GRAVITY, 8, 9, 0); int a1[20]; for (int i = 0; i < 20; ++i) a1[i] = w.advance(D_GRAVITY, 8, s1, l1);
    Rng s2(9), l2(1); w.reset(D_GRAVITY, 8, 9, 0); int a2[20]; for (int i = 0; i < 20; ++i) a2[i] = w.advance(D_GRAVITY, 8, s2, l2); CHECK(seq(a1, a2, 20)); }
  // even length: Converge/Diverge/Stride/Anchor visit every step within two passes at L=6
  { int modes[] = {D_CONVERGE, D_DIVERGE, D_STRIDE, D_ANCHOR}; for (int m = 0; m < 4; ++m) { int b[12]; run(modes[m], 6, 12, b); int seen[6] = {0}; for (int i = 0; i < 12; ++i) seen[b[i]]++; for (int i = 0; i < 6; ++i) CHECK(seen[i] >= 1); } }
}
