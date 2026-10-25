#include "t.hpp"
#include "rates.hpp"
#include <cstring>
void test_rates() {
  CHECK_EQ(RATE_COUNT, 29);
  CHECK_EQ(RATE_X1, 13);
  CHECK(std::strcmp(RATES[RATE_X1].label, "x1") == 0);
  for (int i = 1; i < RATE_COUNT; ++i)  // strictly ascending
    CHECK(RATES[i].num * RATES[i - 1].den > RATES[i - 1].num * RATES[i].den);
  // exactness: over 64*den ticks a rate advances exactly 64*num steps
  for (int i = 0; i < RATE_COUNT; ++i) {
    RateStepper st; long adv = 0;
    long ticks = 64L * RATES[i].den;
    for (long t = 0; t < ticks; ++t) for (int s = 0; s < SUBTICKS; ++s) adv += st.step(i);
    CHECK_EQ(adv, 64L * RATES[i].num);
  }
  // divided rate lands on the downbeat: /4 advances on the very first sub-tick after reset
  { RateStepper st; int first = st.step(RATE_X1 - 5); CHECK_EQ(RATES[RATE_X1 - 5].den, 4); CHECK_EQ(first, 1); }
  // x1.5 = 3 advances per 2 ticks, spread: never 2 advances in one sub-tick
  { RateStepper st; int maxAdv = 0; for (int t = 0; t < 2 * SUBTICKS; ++t) { int a = st.step(RATE_X1 + 3); if (a > maxAdv) maxAdv = a; } CHECK_EQ(maxAdv, 1); }
}
