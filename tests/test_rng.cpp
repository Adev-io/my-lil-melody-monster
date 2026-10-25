#include "t.hpp"
#include "rng.hpp"
void test_rng() {
  Rng a(42), b(42), c(43);
  CHECK_EQ(a.next(), b.next());          // deterministic
  CHECK(a.next() != c.next());           // seed-sensitive
  Rng z(0); CHECK(z.next() != 0 || z.next() != 0); // zero seed still runs
  Rng r(7); bool ok = true;
  for (int i = 0; i < 10000; ++i) { int v = r.u100(); if (v < 0 || v > 99) ok = false; }
  CHECK(ok);
  Rng d1 = derive(5, 3, 0), d2 = derive(5, 3, 0), d3 = derive(5, 4, 0);
  CHECK_EQ(d1.next(), d2.next()); CHECK(d1.next() != d3.next());
  Rng s(1); int lo = 99, hi = -99;
  for (int i = 0; i < 1000; ++i) { int v = s.range(-7, 7); if (v < lo) lo = v; if (v > hi) hi = v; }
  CHECK_EQ(lo, -7); CHECK_EQ(hi, 7);
}
