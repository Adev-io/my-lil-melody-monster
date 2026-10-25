#pragma once
#include <cstdio>
#include <cstdlib>
inline int& g_checks_() { static int v = 0; return v; }
inline int& g_fails_() { static int v = 0; return v; }
#define g_checks g_checks_()
#define g_fails g_fails_()
#define CHECK(cond) do { ++g_checks; if (!(cond)) { ++g_fails; \
  std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_EQ(a, b) do { ++g_checks; long long _a = (long long)(a), _b = (long long)(b); \
  if (_a != _b) { ++g_fails; std::printf("FAIL %s:%d: %s == %s (%lld vs %lld)\n", \
  __FILE__, __LINE__, #a, #b, _a, _b); } } while (0)
#define TEST_SUMMARY() (std::printf("%d checks, %d failures\n", g_checks, g_fails), g_fails ? 1 : 0)
