#pragma once
struct Scale { const char* name; int n; int semis[12]; };
enum { SCALE_MAJOR, SCALE_NAT_MINOR, SCALE_HARM_MINOR, SCALE_MEL_MINOR, SCALE_DORIAN, SCALE_PHRYGIAN,
       SCALE_LYDIAN, SCALE_MIXOLYDIAN, SCALE_MAJOR_PENT, SCALE_MINOR_PENT, SCALE_BLUES, SCALE_INSEN,
       SCALE_WHOLE, SCALE_COUNT };
static const Scale SCALES[SCALE_COUNT] = {
  {"Major",            7, {0,2,4,5,7,9,11}},
  {"Natural minor",    7, {0,2,3,5,7,8,10}},
  {"Harmonic minor",   7, {0,2,3,5,7,8,11}},
  {"Melodic minor",    7, {0,2,3,5,7,9,11}},
  {"Dorian",           7, {0,2,3,5,7,9,10}},
  {"Phrygian",         7, {0,1,3,5,7,8,10}},
  {"Lydian",           7, {0,2,4,6,7,9,11}},
  {"Mixolydian",       7, {0,2,4,5,7,9,10}},
  {"Major pentatonic", 5, {0,2,4,7,9}},
  {"Minor pentatonic", 5, {0,3,5,7,10}},
  {"Blues",            6, {0,3,5,6,7,10}},
  {"In-Sen",           5, {0,1,5,7,10}},
  {"Whole tone",       6, {0,2,4,6,8,10}},
};
static const char* const ROOT_NAMES[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
inline int floordiv(int a, int b) { int q = a / b; if ((a % b != 0) && ((a < 0) != (b < 0))) --q; return q; }
inline int posmod(int a, int b) { int m = a % b; return m < 0 ? m + b : m; }
// semitone offset from C for scale degree `deg` (0 = root), any octave
inline int degreeToSemitone(int scaleIdx, int root, int deg) {
  const Scale& s = SCALES[scaleIdx];
  int oct = floordiv(deg, s.n);
  return root + 12 * oct + s.semis[deg - oct * s.n];
}
// keep MIDI-style semitone inside C0..C8 (12..108) by octave folding
inline int foldSemitone(int st) { while (st < 12) st += 12; while (st > 108) st -= 12; return st; }
// nearest degree whose in-scale position is root/3rd/5th (0,2,4 mod n); ties round down
inline int snapConsonant(int scaleIdx, int deg) {
  int n = SCALES[scaleIdx].n;
  int best = deg, bestDist = 1 << 20;
  for (int cand = deg - n; cand <= deg + n; ++cand) {
    int p = posmod(cand, n);
    if (p != 0 && p != 2 % n && p != 4 % n) continue;
    int dist = cand > deg ? cand - deg : deg - cand;
    if (dist < bestDist || (dist == bestDist && cand < best)) { best = cand; bestDist = dist; }
  }
  return best;
}
