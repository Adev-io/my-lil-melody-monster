#include "sched.hpp"
static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

void Sched::setExternal(bool ext) {
  if (ext == extMode) return;
  extMode = ext; sinceTick = 0.f; subTicksDone = SUBTICKS; extEdgeSeen = false; clockOutTimer = 0.f;
  phase = ext ? 0.f : 1.f;      // defined: unpatching CLK IN restarts the internal clock with an immediate tick; patching waits for the first edge
}

void Sched::stop() { for (int i = 0; i < NUM_TRACKS; ++i) { gt[i].high = false; gt[i].tied = false; gt[i].on = gt[i].off = -1.f; } clockOutTimer = 0.f; }
void Sched::reset() { eng->reset(); stop(); }

void Sched::handleEvents(float period) {
  for (int i = 0; i < NUM_TRACKS; ++i) {
    const TrackOut& o = eng->out(i); GateTimer& g = gt[i];
    if (!o.noteOn) {                                           // rest, breathing pass or muted step: a tied gate ends here
      if (o.advanced && g.tied) { g.high = false; g.tied = false; g.on = g.off = -1.f; }
      continue;
    }
    const Rate& r = RATES[eng->p[i].rateIdx];
    float step = period * (float)r.den / (float)r.num;
    float onDelay = o.slopPct / 100.f * step * 0.5f;
    g.pendingSemi = o.semitone;
    bool legato = g.tied && g.high;
    if (!legato) { g.high = false; if (onDelay < lastDt) onDelay = lastDt; } // retrigger: at least one low sample (the rise lands on the next sample)
    g.on = onDelay;
    g.off = o.tie ? -1.f : onDelay + o.gateLenPct / 100.f * step;
    g.tied = o.tie;
  }
}

void Sched::emitSubTicks(int upTo, float periodForEvents) {
  while (subTicksDone < upTo && subTicksDone < SUBTICKS) { eng->subTick(subTicksDone); ++subTicksDone; handleEvents(periodForEvents); }
}

void Sched::tick(float dt, bool extEdge, int bpm, bool run) {
  if (!run) { if (running) stop(); running = false; return; } running = true; lastDt = dt;
  bool frozen = eng->g.freeze;
  for (int i = 0; i < NUM_TRACKS; ++i) {                       // a track that just became inaudible forgets its gate, tie and timers
    bool a = eng->audible(i);
    if (wasAudible[i] && !a) { gt[i].high = false; gt[i].tied = false; gt[i].on = gt[i].off = -1.f; }
    wasAudible[i] = a;
  }
  // ---- clock ----
  bool isTick = false; float oldPeriod = tickPeriod;
  sinceTick += dt;
  if (extMode) {
    if (extEdge) { isTick = true; if (extEdgeSeen) tickPeriod = clampf(sinceTick, 0.005f, 10.f); extEdgeSeen = true; }
  } else {
    tickPeriod = clampf(60.f / (float)(bpm < 1 ? 1 : bpm), 0.005f, 10.f);
    phase += dt / tickPeriod; if (phase >= 1.f) { phase -= 1.f; isTick = true; }
  }
  if (isTick) {
    emitSubTicks(SUBTICKS, oldPeriod);                         // flush the old period with the old period's timing
    subTicksDone = 0; sinceTick = 0.f; clockOutTimer = tickPeriod * 0.5f; ++clockPulses;
  }
  if (!extMode || extEdgeSeen) {
    int due = (int)(sinceTick / tickPeriod * SUBTICKS) + 1;   // +1: sub-tick 0 on the edge sample
    emitSubTicks(due, tickPeriod);
  }
  if (clockOutTimer > 0.f) clockOutTimer -= dt;
  // ---- gate timers (paused while frozen) ----
  if (frozen) return;
  for (int i = 0; i < NUM_TRACKS; ++i) {
    GateTimer& g = gt[i];
    if (g.on >= 0.f) { g.on -= dt; if (g.on < 0.f) { g.high = true; semi[i] = g.pendingSemi; g.on = -1.f; } }
    if (g.off >= 0.f) { g.off -= dt; if (g.off < 0.f) { if (!g.tied) g.high = false; g.off = -1.f; } }
  }
}
