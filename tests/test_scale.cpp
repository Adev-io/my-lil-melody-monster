#include "t.hpp"
#include "scale.hpp"
void test_scale() {
  CHECK_EQ(SCALE_COUNT, 13);
  // Major, root C: degree 0..7 → C D E F G A B C
  CHECK_EQ(degreeToSemitone(SCALE_MAJOR, 0, 0), 0);
  CHECK_EQ(degreeToSemitone(SCALE_MAJOR, 0, 2), 4);
  CHECK_EQ(degreeToSemitone(SCALE_MAJOR, 0, 7), 12);
  CHECK_EQ(degreeToSemitone(SCALE_MAJOR, 0, -1), -1);   // B below
  CHECK_EQ(degreeToSemitone(SCALE_MAJOR, 0, -7), -12);
  CHECK_EQ(degreeToSemitone(SCALE_MINOR_PENT, 2, 1), 5); // D minor pent: D F → F = 5
  CHECK_EQ(foldSemitone(5), 17); CHECK_EQ(foldSemitone(120), 108); CHECK_EQ(foldSemitone(60), 60);
  // consonance snap in a 7-note scale: degree 1 → 0 or 2 (nearest, tie → down), 3 → 2 or 4, 5 → 4, 6 → 7
  CHECK_EQ(snapConsonant(SCALE_MAJOR, 1), 0);
  CHECK_EQ(snapConsonant(SCALE_MAJOR, 3), 2);
  CHECK_EQ(snapConsonant(SCALE_MAJOR, 5), 4);
  CHECK_EQ(snapConsonant(SCALE_MAJOR, 6), 7);
  CHECK_EQ(snapConsonant(SCALE_MAJOR, -1), 0);
  CHECK_EQ(snapConsonant(SCALE_MAJOR, 9), 9);  // 9 = 7+2 → third, already consonant
  // every scale: degrees stay inside 12 semitones per octave, strictly increasing
  for (int s = 0; s < SCALE_COUNT; ++s) {
    for (int d = 1; d < SCALES[s].n; ++d) CHECK(SCALES[s].semis[d] > SCALES[s].semis[d - 1]);
    CHECK(SCALES[s].semis[SCALES[s].n - 1] < 12);
  }
}
