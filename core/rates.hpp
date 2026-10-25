#pragma once
struct Rate { const char* label; int num; int den; }; // steps per x1 tick = num/den
static const int SUBTICKS = 64;                        // engine sub-ticks per x1 tick
static const Rate RATES[] = {
  {"/64",1,64},{"/32",1,32},{"/16",1,16},{"/8",1,8},{"/7",1,7},{"/6",1,6},{"/5.3",3,16},{"/5",1,5},
  {"/4",1,4},{"/3",1,3},{"/2.6",3,8},{"/2",1,2},{"/1.5",2,3},{"x1",1,1},{"x1.25",5,4},{"x1.3",4,3},
  {"x1.5",3,2},{"x2",2,1},{"x2.6",8,3},{"x3",3,1},{"x4",4,1},{"x5",5,1},{"x5.3",16,3},{"x6",6,1},
  {"x7",7,1},{"x8",8,1},{"x16",16,1},{"x32",32,1},{"x64",64,1},
};
static const int RATE_COUNT = 29;
static const int RATE_X1 = 13;
// Bresenham: acc += num per sub-tick; advance each time acc reaches den*SUBTICKS. Starts "full" so
// the first sub-tick after reset advances (divided rates land on the downbeat).
struct RateStepper {
  int acc;
  RateStepper() { reset(); }
  void reset() { acc = 0; primed = true; }
  bool primed;
  int step(int rateIdx) {
    const Rate& r = RATES[rateIdx];
    int threshold = r.den * SUBTICKS;
    if (primed) { primed = false; acc = threshold - r.num; } // first sub-tick lands exactly on the threshold
    acc += r.num;
    int adv = 0;
    while (acc >= threshold) { acc -= threshold; ++adv; }
    return adv;
  }
};
