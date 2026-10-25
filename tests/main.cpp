#include "t.hpp"
#include <cstring>
void test_smoke(); void test_rng(); void test_scale(); void test_rates(); void test_pattern();
void test_render(); void test_walks(); void test_engine(); void test_follow(); void test_sched(); void test_golden(bool);
void test_face();
int main(int argc, char** argv) {
  bool writeGolden = argc > 1 && std::strcmp(argv[1], "--write-golden") == 0;
  test_smoke(); test_rng(); test_scale(); test_rates(); test_pattern();
  test_render(); test_walks(); test_engine(); test_follow(); test_sched(); test_golden(writeGolden);
  test_face();
  return TEST_SUMMARY();
}
