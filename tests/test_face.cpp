#include "t.hpp"
#include "FaceTap.h"
void test_face() {
  // 48 kHz: stride = 48000*0.04/48 = 40 samples per point
  FaceTap t;
  for (int i = 0; i < 40; ++i) t.push(i == 7 ? 1.f : 0.f, 48000.f);   // 1-sample pulse inside one stride
  CHECK(t.buf[0] == 1.f);
  CHECK_EQ(t.head, 1);
  FaceTap n;
  for (int i = 0; i < 40; ++i) n.push(i == 3 ? -0.9f : 0.5f, 48000.f);
  CHECK(n.buf[0] == -0.9f);                                            // largest magnitude keeps its sign
  FaceTap w;
  for (int i = 0; i < 40 * FaceTap::N * 2; ++i) w.push(0.25f, 48000.f);
  CHECK_EQ(w.head, 0);                                                 // wraps after N points
  FaceTap s;
  for (int i = 0; i < 39; ++i) s.push(1.f, 48000.f);
  CHECK_EQ(s.head, 0);                                                 // nothing before a full stride
  FaceTap lo;
  lo.push(1.f, 100.f);
  CHECK_EQ(lo.head, 1);                                                // stride never below 1
}
