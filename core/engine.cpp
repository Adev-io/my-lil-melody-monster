#include "engine.hpp"

void Engine::init(uint32_t liveSeed) {
  for (int i = 0; i < NUM_TRACKS; ++i) {
    generatePattern(t[i].slots, p[i].seed);
    t[i].live = Rng(liveSeed + 0x1000u * (uint32_t)i);
    t[i].seeded = derive((uint32_t)p[i].seed, 0, 0x6u);
    t[i].walk.reset(p[i].dirn, p[i].length, p[i].seed, 0);
    t[i].rate.reset(); t[i].subCount = 0;
    t[i].passIdx = 0; t[i].advancesInPass = 0; t[i].armedSeed = -1; t[i].armedPending = false;
    t[i].lastDirn = p[i].dirn; t[i].lastLength = p[i].length; t[i].lastRateIdx = p[i].rateIdx;
    t[i].breathing = false;
    o_[i] = TrackOut();
  }
}

int Engine::rollSeed() { return t[0].live.range(0, 999); }
bool Engine::anySolo() const { for (int i = 0; i < NUM_TRACKS; ++i) if (p[i].solo) return true; return false; }
bool Engine::audible(int i) const { if (p[i].mute) return false; if (anySolo()) return p[i].solo; return true; }

void Engine::setFreeze(bool f) {
  g.freeze = f;
  for (int i = 0; i < NUM_TRACKS; ++i) o_[i].gateHeld = f; // Sched::gate() masks by audible(i) every sample, so unmuting during a freeze blooms too
}

void Engine::reset() {
  for (int i = 0; i < NUM_TRACKS; ++i) { t[i].walk.reset(p[i].dirn, p[i].length, p[i].seed, 0); t[i].rate.reset(); t[i].subCount = 0; t[i].passIdx = 0; t[i].advancesInPass = 0; t[i].breathing = false;
    t[i].seeded = derive((uint32_t)p[i].seed, 0, 0x6u); }
}

void Engine::armReseed(int i, int seed) { t[i].armedSeed = seed < 0 ? rollSeed() : seed; t[i].armedPending = true;
  if (dirnIsLive(p[i].dirn)) applyReseed(i); }
void Engine::armReseedAll() { for (int i = 0; i < NUM_TRACKS; ++i) armReseed(i, -1); }

void Engine::applyPatternNow(int i) { generatePattern(t[i].slots, p[i].seed); t[i].passIdx = 0; t[i].seeded = derive((uint32_t)p[i].seed, 0, 0x6u); }

void Engine::applyReseed(int i) {
  p[i].seed = t[i].armedSeed; t[i].armedSeed = -1; t[i].armedPending = false;
  generatePattern(t[i].slots, p[i].seed);
  t[i].passIdx = 0; t[i].advancesInPass = 0;
  t[i].seeded = derive((uint32_t)p[i].seed, 0, 0x6u);
  t[i].walk.reset(p[i].dirn, p[i].length, p[i].seed, 0);
  t[i].breathing = false;
}

int Engine::sourceIndex(int i) const {
  int cur = i; unsigned visited = 0;
  while (true) {
    visited |= 1u << cur;
    int f = p[cur].follow;
    if (f < 1 || f > NUM_TRACKS) return cur;          // end of chain
    int nxt = f - 1;
    if (visited & (1u << nxt)) return i;              // cycle -> follower plays its own pattern
    cur = nxt;
  }
}

void Engine::startPass(int i) {
  t[i].passIdx++;
  t[i].advancesInPass = 0;
  if (t[i].armedPending) { applyReseed(i); return; }           // lands at origin: walk.reset above
  // evolve: re-roll a share of slots from the lineage stream
  if (p[i].evolve > 0) {
    Rng r = derive((uint32_t)p[i].seed, (uint32_t)t[i].passIdx, 0x0u);
    for (int s = 0; s < p[i].length; ++s) if (r.u100() < p[i].evolve) rollSlot(t[i].slots[s], r);
  }
  // breathe decision
  if (p[i].breathe > 0) { Rng b = derive((uint32_t)p[i].seed, (uint32_t)t[i].passIdx, 0xBu); t[i].breathing = b.u100() < p[i].breathe; }
  else t[i].breathing = false;
  t[i].seeded = derive((uint32_t)p[i].seed, (uint32_t)t[i].passIdx, 0x6u);
  t[i].walk.newPass(p[i].dirn, p[i].length, p[i].seed, t[i].passIdx);
}

void Engine::doAdvance(int i) {
  TrackState& s = t[i];
  // direction or length changed on the panel → restart the walk cleanly
  if (s.lastDirn != p[i].dirn || s.lastLength != p[i].length) {
    s.lastDirn = p[i].dirn; s.lastLength = p[i].length;
    s.walk.reset(p[i].dirn, p[i].length, p[i].seed, s.passIdx); s.advancesInPass = 0;
  }
  int step = s.walk.advance(p[i].dirn, p[i].length, s.seeded, s.live);
  if (p[i].dirn == D_TIDE) step = posmod(step + s.walk.tideOffset, p[i].length);
  int idx = posmod(step + p[i].shift, p[i].length);
  const Slot* src = t[sourceIndex(i)].slots;
  NoteOut n = renderSlot(src[idx], p[i], g);
  TrackOut& o = o_[i];
  o.advanced = true; o.stepIndex = idx; o.breathing = s.breathing;
  if (n.plays && !s.breathing && audible(i)) {
    o.noteOn = true; o.semitone = n.semitone; o.gateLenPct = n.gateLenPct; o.slopPct = n.slopPct; o.tie = n.tie;
  } // muted or breathing: no event, pitch holds its last value
  if (++s.advancesInPass >= p[i].length) { o.passEnd = true; startPass(i); }
}

void Engine::subTick(int subIdx) {
  for (int i = 0; i < NUM_TRACKS; ++i) { o_[i].noteOn = false; o_[i].passEnd = false; o_[i].advanced = false; o_[i].breathing = t[i].breathing; }
  if (!g.run) return;
  for (int i = 0; i < NUM_TRACKS; ++i) {
    TrackState& s = t[i];
    if (s.lastRateIdx != p[i].rateIdx) {                       // re-anchor to the tick grid, never an extra note
      s.lastRateIdx = p[i].rateIdx;
      if (!s.rate.primed) { const Rate& r = RATES[p[i].rateIdx]; int T = r.den * SUBTICKS;
        s.rate.acc = (int)((((long long)(s.subCount - 1) * r.num) % T + T) % T); }
    }
    if (s.rate.primed && subIdx != 0) continue;                // after reset/init the origin lands on the next tick boundary
    int adv = s.rate.step(p[i].rateIdx);
    ++s.subCount;
    if (g.freeze) continue;                                    // the grid keeps running; frozen steps are dropped (spec 6.4)
    for (int k = 0; k < adv; ++k) doAdvance(i);
  }
}
void Engine::setSolo(int i) { bool on = !p[i].solo; for (int k = 0; k < NUM_TRACKS; ++k) p[k].solo = false; p[i].solo = on; }
