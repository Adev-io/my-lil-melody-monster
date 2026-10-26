#include "sched.hpp"   // before plugin.hpp: parsed ahead of `using namespace rack` (rack::engine::Engine clash)
#include "plugin.hpp"
#include "layout.hpp"
#include "TalkingFace.hpp"
#include <cmath>
#include <atomic>

struct MelodyMonster : Module {
  enum ParamId { CHANCE_PARAM, NOTE_PARAM, OCTAVE_PARAM, LENG_PARAM, RATE_PARAM, DIRN_PARAM, TRNS_PARAM, SHFT_PARAM, OCTA_PARAM,
    FRZE_PARAM, MUTE_PARAM, SOLO_PARAM, RUN_PARAM, RSET_PARAM, RSED_PARAM,
    TRACK1_PARAM, TRACK2_PARAM, TRACK3_PARAM, TRACK4_PARAM, TRACK5_PARAM, TRACK6_PARAM, TRACK7_PARAM, TRACK8_PARAM, PARAMS_LEN };
  enum InputId { CLK_INPUT, RST_INPUT, RESEED_INPUT, FREEZE_INPUT, SEED_INPUT, INPUTS_LEN };
  enum OutputId { GATE1_OUTPUT, GATE2_OUTPUT, GATE3_OUTPUT, GATE4_OUTPUT, GATE5_OUTPUT, GATE6_OUTPUT, GATE7_OUTPUT, GATE8_OUTPUT,
    PITCH1_OUTPUT, PITCH2_OUTPUT, PITCH3_OUTPUT, PITCH4_OUTPUT, PITCH5_OUTPUT, PITCH6_OUTPUT, PITCH7_OUTPUT, PITCH8_OUTPUT, CLK_OUTPUT, OUTPUTS_LEN };
  enum LightId { FRZE_LIGHT, MUTE_LIGHT, SOLO_LIGHT, RUN_LIGHT, RSED_LIGHT,
    TRACK1_LIGHT, TRACK2_LIGHT, TRACK3_LIGHT, TRACK4_LIGHT, TRACK5_LIGHT, TRACK6_LIGHT, TRACK7_LIGHT, TRACK8_LIGHT, LIGHTS_LEN };

  ::Engine eng;            // qualified: rack::engine::Engine is also visible
  Sched sched;
  int polyRouting = 0;        // 0 off, 1 pairs, 2 split 4+4, 3 all 8
  bool frozenClockStops = false;
  float lastKnob[9] = {};     // last value seen per knob: only knobs that moved since are pulled into the track
  std::atomic<int> pendingSeed[8];                 // UI thread writes (Seed slider), audio thread applies
  std::atomic<bool> pendingReseedAll{false};       // UI thread sets (menu Reseed all), audio thread applies
  dsp::SchmittTrigger clkTrig, rstTrig, reseedTrig, trackTrig[8], rsetBtn, rsedBtn, muteBtn, soloBtn, frzIn;
  float lastSeedCv = -1.f;

  MelodyMonster() {
    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configParam(CHANCE_PARAM, 0.f, 100.f, 100.f, "Chance", "%");
    configParam(NOTE_PARAM, -100.f, 100.f, 0.f, "Note variation", "%");
    configParam(OCTAVE_PARAM, -100.f, 100.f, 0.f, "Octave variation", "%");
    configParam(LENG_PARAM, 1.f, 64.f, 16.f, "Length"); paramQuantities[LENG_PARAM]->snapEnabled = true;
    { std::vector<std::string> l; for (int i = 0; i < RATE_COUNT; ++i) l.push_back(RATES[i].label); configSwitch(RATE_PARAM, 0.f, RATE_COUNT - 1, RATE_X1, "Rate", l); }
    { std::vector<std::string> l; for (int i = 0; i < DIRN_COUNT; ++i) l.push_back(DIRN_NAMES[i]); configSwitch(DIRN_PARAM, 0.f, DIRN_COUNT - 1, 0.f, "Direction", l); }
    configParam(TRNS_PARAM, -7.f, 7.f, 0.f, "Transpose", " degrees"); paramQuantities[TRNS_PARAM]->snapEnabled = true;
    configParam(SHFT_PARAM, -63.f, 63.f, 0.f, "Shift", " steps"); paramQuantities[SHFT_PARAM]->snapEnabled = true;
    configParam(OCTA_PARAM, -3.f, 3.f, 0.f, "Octave shift"); paramQuantities[OCTA_PARAM]->snapEnabled = true;
    configSwitch(FRZE_PARAM, 0.f, 1.f, 0.f, "Freeze", {"Off", "On"});
    configButton(MUTE_PARAM, "Mute"); configButton(SOLO_PARAM, "Solo");
    configSwitch(RUN_PARAM, 0.f, 1.f, 1.f, "Run", {"Stopped", "Running"});
    paramQuantities[RUN_PARAM]->randomizeEnabled = false; paramQuantities[FRZE_PARAM]->randomizeEnabled = false;
    configButton(RSET_PARAM, "Reset"); configButton(RSED_PARAM, "Reseed");
    for (int i = 0; i < 8; ++i) configButton(TRACK1_PARAM + i, string::f("Track %d", i + 1));
    configInput(CLK_INPUT, "Clock"); configInput(RST_INPUT, "Reset"); configInput(RESEED_INPUT, "Reseed all");
    configInput(FREEZE_INPUT, "Freeze"); configInput(SEED_INPUT, "Seed (0-10V = 0-999)");
    for (int i = 0; i < 8; ++i) { configOutput(GATE1_OUTPUT + i, string::f("Gate %d", i + 1)); configOutput(PITCH1_OUTPUT + i, string::f("Pitch %d", i + 1)); }
    configOutput(CLK_OUTPUT, "Clock");
    for (int i = 0; i < 8; ++i) { eng.p[i].seed = (int)(random::u32() % 1000u); pendingSeed[i] = -1; } // independent random seeds on first add; dataFromJson overrides
    eng.init((uint32_t)random::u32());
    sched.eng = &eng;
    pushTrackToKnobs();
  }

  // ---- selected-track knob sync ----
  void pushTrackToKnobs() {
    const TrackParams& p = eng.p[eng.selected];
    float v[9] = {(float)p.chance, (float)p.note, (float)p.octave, (float)p.length, (float)p.rateIdx, (float)p.dirn, (float)p.trans, (float)p.shift, (float)p.octa};
    for (int i = 0; i < 9; ++i) { params[CHANCE_PARAM + i].setValue(v[i]); lastKnob[i] = v[i]; }
  }
  void pullKnobsToTrack() {
    TrackParams& p = eng.p[eng.selected];
    float v[9]; for (int i = 0; i < 9; ++i) v[i] = params[CHANCE_PARAM + i].getValue();
    if (v[0] != lastKnob[0]) p.chance = (int)std::round(v[0]);
    if (v[1] != lastKnob[1]) p.note = (int)std::round(v[1]);
    if (v[2] != lastKnob[2]) p.octave = (int)std::round(v[2]);
    if (v[3] != lastKnob[3]) p.length = clamp((int)std::round(v[3]), 1, 64);
    if (v[4] != lastKnob[4]) p.rateIdx = clamp((int)std::round(v[4]), 0, RATE_COUNT - 1);
    if (v[5] != lastKnob[5]) p.dirn = clamp((int)std::round(v[5]), 0, DIRN_COUNT - 1);
    if (v[6] != lastKnob[6]) p.trans = (int)std::round(v[6]);
    if (v[7] != lastKnob[7]) p.shift = (int)std::round(v[7]);
    if (v[8] != lastKnob[8]) p.octa = (int)std::round(v[8]);
    for (int i = 0; i < 9; ++i) lastKnob[i] = v[i];
  }
  void selectTrack(int i) { if (i == eng.selected) return; eng.selected = i; pushTrackToKnobs(); }

  // Initialize: Rack has already reset the params (engine thread, module locked)
  void onReset() override {
    for (int i = 0; i < 8; ++i) { eng.p[i] = TrackParams(); eng.p[i].seed = (int)(random::u32() % 1000u); }
    eng.g = Globals(); polyRouting = 0; frozenClockStops = false; eng.selected = 0;
    eng.init((uint32_t)random::u32()); sched.reset(); pushTrackToKnobs();
  }

  FaceTap face;
  void process(const ProcessArgs& args) override;
  json_t* dataToJson() override;
  void dataFromJson(json_t* root) override;
};

void MelodyMonster::process(const ProcessArgs& args) {
  // ---- UI-thread handoffs ----
  for (int i = 0; i < 8; ++i) { int sd = pendingSeed[i].load(std::memory_order_relaxed); if (sd >= 0) { sd = pendingSeed[i].exchange(-1); if (sd >= 0) { eng.p[i].seed = sd; eng.applyPatternNow(i); } } }
  if (pendingReseedAll.exchange(false)) eng.armReseedAll();
  // ---- buttons ----
  for (int i = 0; i < 8; ++i) { if (trackTrig[i].process(params[TRACK1_PARAM + i].getValue())) selectTrack(i); lights[TRACK1_LIGHT + i].setBrightness(eng.selected == i ? 1.f : 0.15f); }
  pullKnobsToTrack();
  TrackParams& sp = eng.p[eng.selected];
  if (muteBtn.process(params[MUTE_PARAM].getValue())) sp.mute = !sp.mute;
  if (soloBtn.process(params[SOLO_PARAM].getValue())) eng.setSolo(eng.selected); // exclusive (manual behaviour)
  if (rsedBtn.process(params[RSED_PARAM].getValue())) eng.armReseed(eng.selected, -1);
  if (reseedTrig.process(inputs[RESEED_INPUT].getVoltage(), 0.1f, 1.f)) eng.armReseedAll();
  if (inputs[SEED_INPUT].isConnected()) {
    float v = inputs[SEED_INPUT].getVoltage(); int sd = clamp((int)std::round(v / 10.f * 999.f), 0, 999);
    if (lastSeedCv < 0.f || sd != (int)lastSeedCv) eng.armReseed(eng.selected, sd);
    lastSeedCv = (float)sd;
  } else lastSeedCv = -1.f;
  // transport: Rack params are the owners; engine globals mirror them every sample. Sched owns the RUN transition.
  bool run = params[RUN_PARAM].getValue() > 0.5f;
  eng.g.run = run;
  frzIn.process(inputs[FREEZE_INPUT].getVoltage(), 0.1f, 1.f);
  bool frz = params[FRZE_PARAM].getValue() > 0.5f || frzIn.isHigh();
  if (frz != eng.g.freeze) eng.setFreeze(frz);
  bool rstBtn = rsetBtn.process(params[RSET_PARAM].getValue()); bool rstIn = rstTrig.process(inputs[RST_INPUT].getVoltage(), 0.1f, 1.f);
  if (rstBtn || rstIn) sched.reset(); // immediate, works while stopped; both triggers always processed (no short-circuit)
  lights[FRZE_LIGHT].setBrightness(frz); lights[RUN_LIGHT].setBrightness(run);
  lights[MUTE_LIGHT].setBrightness(sp.mute); lights[SOLO_LIGHT].setBrightness(sp.solo);
  lights[RSED_LIGHT].setBrightness(eng.t[eng.selected].armedPending ? 1.f : 0.f);
  // ---- clock + engine ----
  sched.setExternal(inputs[CLK_INPUT].isConnected());
  bool edge = clkTrig.process(inputs[CLK_INPUT].getVoltage(), 0.1f, 1.f);
  sched.tick(args.sampleTime, edge, eng.g.bpm, run);
  // ---- outputs ----
  float pv[8], gv[8];
  for (int i = 0; i < 8; ++i) {
    const TrackParams& p = eng.p[i];
    pv[i] = ((sched.semitone(i) - 48) / 12.f) * p.pitchScale / 100.f + p.pitchOffset;
    gv[i] = sched.gate(i) ? p.gateLevel : 0.f;
  }
  { float gs = 0.f; for (int i = 0; i < 8; ++i) gs += gv[i]; face.push(clamp(gs / 20.f, 0.f, 1.f), args.sampleRate); }
  // poly routing: channel 0 of every jack is always its own track (mono contract). A hub jack (last of its group)
  // adds the other group members on channels 1..n-1 in ascending track order.
  int groupSize = polyRouting == 1 ? 2 : polyRouting == 2 ? 4 : polyRouting == 3 ? 8 : 1;
  for (int i = 0; i < 8; ++i) {
    bool hub = groupSize > 1 && (i % groupSize) == groupSize - 1;
    int ch = hub ? groupSize : 1;
    if (outputs[GATE1_OUTPUT + i].getChannels() != ch) outputs[GATE1_OUTPUT + i].setChannels(ch);
    if (outputs[PITCH1_OUTPUT + i].getChannels() != ch) outputs[PITCH1_OUTPUT + i].setChannels(ch);
    outputs[GATE1_OUTPUT + i].setVoltage(gv[i], 0); outputs[PITCH1_OUTPUT + i].setVoltage(pv[i], 0);
    if (hub) { int c = 1; for (int src = i - groupSize + 1; src < i; ++src, ++c) { outputs[GATE1_OUTPUT + i].setVoltage(gv[src], c); outputs[PITCH1_OUTPUT + i].setVoltage(pv[src], c); } }
  }
  bool clkHigh = sched.clockHigh() && run && !(eng.g.freeze && frozenClockStops);
  outputs[CLK_OUTPUT].setVoltage(clkHigh ? 10.f : 0.f);
}

json_t* MelodyMonster::dataToJson() {
  json_t* root = json_object();
  json_t* tracks = json_array();
  for (int i = 0; i < 8; ++i) {
    const TrackParams& p = eng.p[i]; json_t* t = json_object();
#define JI(k) json_object_set_new(t, #k, json_integer(p.k))
#define JF(k) json_object_set_new(t, #k, json_real(p.k))
#define JB(k) json_object_set_new(t, #k, json_boolean(p.k))
    JI(seed); JI(chance); JI(note); JI(octave); JI(length); JI(rateIdx); JI(dirn); JI(trans); JI(shift); JI(octa);
    JI(evolve); JI(breathe); JI(gateLen); JI(tie); JI(slop); JF(gateLevel); JF(pitchScale); JF(pitchOffset); JI(follow); JB(mute); JB(solo);
#undef JI
#undef JF
#undef JB
    json_t* slots = json_array();
    for (int s = 0; s < MAX_STEPS; ++s) { const Slot& sl = eng.t[i].slots[s]; json_t* a = json_array();
      int f[9] = {sl.degree, sl.gateRoll, sl.noteRoll, sl.noteAmt, sl.octRoll, sl.octAmt, sl.weightRoll, sl.slopVal, sl.tieVal};
      for (int k = 0; k < 9; ++k) { json_array_append_new(a, json_integer(f[k])); } json_array_append_new(slots, a); }
    json_object_set_new(t, "slots", slots);
    // runtime so a reload resumes mid-pattern (live RNG and the per-pass seeded stream position are not saved by design)
    const TrackState& ts = eng.t[i]; json_t* rt = json_object();
    int rti[] = {ts.passIdx, ts.advancesInPass, ts.armedSeed, ts.armedPending, ts.breathing, ts.rate.acc, ts.rate.primed, ts.subCount,
      ts.walk.pos, ts.walk.sign, ts.walk.k, ts.walk.tideOffset, ts.walk.permPos, ts.walk.pocketStart, ts.walk.pocketSize, ts.walk.pocketLaps, ts.walk.pocketPos, ts.walk.prev, ts.walk.gravityP};
    const char* rtk[] = {"passIdx","advancesInPass","armedSeed","armedPending","breathing","rateAcc","ratePrimed","subCount","pos","sign","k","tideOffset","permPos","pocketStart","pocketSize","pocketLaps","pocketPos","prev","gravityP"};
    for (int k = 0; k < 19; ++k) json_object_set_new(rt, rtk[k], json_integer(rti[k]));
    json_t* perm = json_array(); for (int k = 0; k < MAX_STEPS; ++k) { json_array_append_new(perm, json_integer(ts.walk.perm[k])); } json_object_set_new(rt, "perm", perm);
    json_object_set_new(t, "runtime", rt);
    json_array_append_new(tracks, t);
  }
  json_object_set_new(root, "tracks", tracks);
  json_object_set_new(root, "scale", json_integer(eng.g.scale)); json_object_set_new(root, "root", json_integer(eng.g.root));
  json_object_set_new(root, "weight", json_integer(eng.g.weight)); json_object_set_new(root, "bpm", json_integer(eng.g.bpm));
  json_object_set_new(root, "selected", json_integer(eng.selected)); json_object_set_new(root, "polyRouting", json_integer(polyRouting));
  json_object_set_new(root, "frozenClockStops", json_boolean(frozenClockStops));
  return root;
}
void MelodyMonster::dataFromJson(json_t* root) {
  json_t* tracks = json_object_get(root, "tracks");
  if (tracks) for (int i = 0; i < 8 && i < (int)json_array_size(tracks); ++i) {
    json_t* t = json_array_get(tracks, i); TrackParams& p = eng.p[i];
    // every loaded field is clamped: a hand-edited or corrupt patch must never index out of range or make length 0
#define GI(k, lo, hi) if (json_t* j = json_object_get(t, #k)) if (json_is_integer(j)) p.k = clamp((int)json_integer_value(j), lo, hi)
#define GF(k, lo, hi) if (json_t* j = json_object_get(t, #k)) if (json_is_number(j)) p.k = clamp((float)json_number_value(j), (float)lo, (float)hi)
#define GB(k) if (json_t* j = json_object_get(t, #k)) if (json_is_boolean(j)) p.k = json_boolean_value(j)
    GI(seed, 0, 999); GI(chance, 0, 100); GI(note, -100, 100); GI(octave, -100, 100); GI(length, 1, 64); GI(rateIdx, 0, RATE_COUNT - 1); GI(dirn, 0, DIRN_COUNT - 1);
    GI(trans, -7, 7); GI(shift, -63, 63); GI(octa, -3, 3); GI(evolve, 0, 100); GI(breathe, 0, 100); GI(gateLen, 5, 100); GI(tie, 0, 100); GI(slop, 0, 100);
    GF(gateLevel, 1, 10); GF(pitchScale, 5, 200); GF(pitchOffset, -10, 10); GI(follow, 0, 8); GB(mute); GB(solo);
#undef GI
#undef GF
#undef GB
    generatePattern(eng.t[i].slots, p.seed);
    if (json_t* slots = json_object_get(t, "slots")) for (int s = 0; s < MAX_STEPS && s < (int)json_array_size(slots); ++s) {
      json_t* a = json_array_get(slots, s); if (!json_is_array(a) || json_array_size(a) < 9) continue; Slot& sl = eng.t[i].slots[s];
      int v[9]; bool ok = true; for (int k = 0; k < 9; ++k) { json_t* e = json_array_get(a, k); if (!json_is_integer(e)) { ok = false; break; } v[k] = (int)json_integer_value(e); } if (!ok) continue;
      sl.degree = (int8_t)clamp(v[0], -7, 7); sl.gateRoll = clamp(v[1], 0, 99); sl.noteRoll = clamp(v[2], 0, 99); sl.noteAmt = clamp(v[3], 0, 99);
      sl.octRoll = clamp(v[4], 0, 99); sl.octAmt = clamp(v[5], 0, 99); sl.weightRoll = clamp(v[6], 0, 99); sl.slopVal = clamp(v[7], 0, 99); sl.tieVal = clamp(v[8], 0, 99); }
    TrackState& ts = eng.t[i];
    ts.walk.reset(p.dirn, p.length, p.seed, 0); ts.lastDirn = p.dirn; ts.lastLength = p.length; ts.lastRateIdx = p.rateIdx;
    if (json_t* rt = json_object_get(t, "runtime")) {
#define GR(k, dst, lo, hi) if (json_t* j = json_object_get(rt, k)) if (json_is_integer(j)) dst = clamp((int)json_integer_value(j), lo, hi)
      const int L1 = p.length - 1; const int accMax = RATES[p.rateIdx].den * SUBTICKS - 1;
      GR("passIdx", ts.passIdx, 0, 1 << 30); GR("advancesInPass", ts.advancesInPass, 0, 63); GR("armedSeed", ts.armedSeed, -1, 999);
      int b = 0; GR("armedPending", b, 0, 1); ts.armedPending = b && ts.armedSeed >= 0; // pending without a seed is not a state
      b = 0; GR("breathing", b, 0, 1); ts.breathing = b;
      GR("advancesInPass", ts.advancesInPass, 0, L1);
      GR("rateAcc", ts.rate.acc, 0, accMax); b = 1; GR("ratePrimed", b, 0, 1); ts.rate.primed = b; GR("subCount", ts.subCount, 0, 1 << 30);
      GR("pos", ts.walk.pos, -1, L1); GR("sign", ts.walk.sign, -1, 1); if (ts.walk.sign == 0) ts.walk.sign = 1; GR("k", ts.walk.k, 0, 1 << 30); GR("tideOffset", ts.walk.tideOffset, 0, L1); GR("permPos", ts.walk.permPos, 0, L1);
      GR("pocketStart", ts.walk.pocketStart, 0, L1); GR("pocketSize", ts.walk.pocketSize, 1, p.length); GR("pocketLaps", ts.walk.pocketLaps, 0, 8); GR("pocketPos", ts.walk.pocketPos, 0, L1); GR("prev", ts.walk.prev, -1, L1); GR("gravityP", ts.walk.gravityP, 0, 100);
#undef GR
      // perm must be a permutation of 0..length-1, otherwise fall back to identity
      if (json_t* perm = json_object_get(rt, "perm")) { bool seen[MAX_STEPS] = {}; bool valid = json_is_array(perm) && (int)json_array_size(perm) >= p.length;
        for (int k = 0; valid && k < p.length; ++k) { json_t* e = json_array_get(perm, k); int v = json_is_integer(e) ? (int)json_integer_value(e) : -1; if (v < 0 || v > L1 || seen[v]) valid = false; else { seen[v] = true; ts.walk.perm[k] = v; } }
        if (!valid) for (int k = 0; k < MAX_STEPS; ++k) ts.walk.perm[k] = k; }
      ts.seeded = derive((uint32_t)p.seed, (uint32_t)ts.passIdx, 0x6u);
    }
  }
#define GG(k, dst, lo, hi) if (json_t* j = json_object_get(root, k)) if (json_is_integer(j)) dst = clamp((int)json_integer_value(j), lo, hi)
  GG("scale", eng.g.scale, 0, SCALE_COUNT - 1); GG("root", eng.g.root, 0, 11); GG("weight", eng.g.weight, 0, 100); GG("bpm", eng.g.bpm, 20, 300);
  GG("selected", eng.selected, 0, 7); GG("polyRouting", polyRouting, 0, 3);
#undef GG
  if (json_t* j = json_object_get(root, "frozenClockStops")) if (json_is_boolean(j)) frozenClockStops = json_boolean_value(j);
  { bool seen = false; for (int i = 0; i < 8; ++i) { if (eng.p[i].solo) { if (seen) eng.p[i].solo = false; seen = true; } } } // solo is exclusive: keep the first
  pushTrackToKnobs();
}


struct IntSliderQ : Quantity { int* v; int lo, hi, def; std::string lbl, unit;
  IntSliderQ(int* v, int lo, int hi, int def, std::string lbl, std::string unit = "") : v(v), lo(lo), hi(hi), def(def), lbl(lbl), unit(unit) {}
  void setValue(float x) override { *v = clamp((int)std::round(x), lo, hi); }
  float getValue() override { return (float)*v; } float getMinValue() override { return (float)lo; } float getMaxValue() override { return (float)hi; }
  float getDefaultValue() override { return (float)def; } std::string getLabel() override { return lbl; } std::string getUnit() override { return unit; }
  int getDisplayPrecision() override { return 0; } };
struct FloatSliderQ : Quantity { float* v; float lo, hi, step, def; std::string lbl, unit;
  FloatSliderQ(float* v, float lo, float hi, float step, float def, std::string lbl, std::string unit) : v(v), lo(lo), hi(hi), step(step), def(def), lbl(lbl), unit(unit) {}
  void setValue(float x) override { *v = clamp(std::round(x / step) * step, lo, hi); }
  float getValue() override { return *v; } float getMinValue() override { return lo; } float getMaxValue() override { return hi; }
  float getDefaultValue() override { return def; } std::string getLabel() override { return lbl; } std::string getUnit() override { return unit; }
  int getDisplayPrecision() override { return 2; } };
struct OwnedSlider : ui::Slider { ~OwnedSlider() override { delete quantity; } };   // ui::Slider does not own its Quantity
static ui::Slider* mkSlider(Quantity* q) { OwnedSlider* s = new OwnedSlider; s->quantity = q; s->box.size.x = 220.f; return s; }

// MonsterDisplay is defined here, after MelodyMonster is a complete type (Display.hpp only forward-declares it)
#include "Display.hpp"
void MonsterDisplay::drawLayer(const DrawArgs& args, int layer) {
  if (layer != 1) return;
  NVGcontext* vg = args.vg;
  nvgBeginPath(vg); nvgRect(vg, 0, 0, box.size.x, box.size.y); nvgFillColor(vg, nvgRGB(0x16, 0x16, 0x1a)); nvgFill(vg);
  if (!module) return;  // module browser preview: background only
  ::Engine& e = module->eng;
  if (!font) font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
  if (!font || font->handle < 0) return; // missing font: never dereference
  nvgScissor(vg, 0, 0, box.size.x, box.size.y);   // nothing spills past the display
  float laneW = box.size.x * 0.62f, laneH = box.size.y / 8.f, x0 = 14.f;
  for (int i = 0; i < 8; ++i) {
    const TrackParams& p = e.p[i]; const TrackOut& o = e.out(i);
    float y = i * laneH; bool dim = o.breathing || !e.audible(i);
    NVGcolor bar = dim ? nvgRGB(0x5a, 0x64, 0x3a) : nvgRGB(0xc8, 0xff, 0x3a);
    nvgFontFaceId(vg, font->handle); nvgFontSize(vg, 9.f); nvgFillColor(vg, i == e.selected ? nvgRGB(0xff, 0x3d, 0x9a) : nvgRGB(0x8d, 0x8d, 0x96));
    nvgText(vg, 2.f, y + laneH * 0.75f, string::f("%d", i + 1).c_str(), NULL);
    const Slot* src = e.t[e.sourceIndex(i)].slots;   // same chain resolution as the engine
    int lo = 127, hi = 0; for (int s = 0; s < p.length; ++s) { NoteOut n = renderSlot(src[s], p, e.g); if (n.semitone < lo) lo = n.semitone; if (n.semitone > hi) hi = n.semitone; }
    float cellW = laneW / p.length;
    int cursorCol = posmod(o.stepIndex - p.shift, p.length);  // stepIndex is the shifted slot; columns are unshifted
    for (int s = 0; s < p.length; ++s) {
      NoteOut n = renderSlot(src[posmod(s + p.shift, p.length)], p, e.g);
      float h = n.plays ? 2.f + (laneH - 5.f) * (hi > lo ? (float)(n.semitone - lo) / (hi - lo) : 0.5f) : 1.5f;
      nvgBeginPath(vg); nvgRect(vg, x0 + s * cellW + 0.5f, y + laneH - 1.5f - h, cellW - 1.f, h);
      nvgFillColor(vg, s == cursorCol ? nvgRGB(0xf2, 0xf2, 0xf4) : bar); nvgFill(vg);
    }
    std::string f; if (p.solo) f += "S"; if (p.mute) f += "M"; if (p.follow) f += string::f("<%d", p.follow);
    nvgFillColor(vg, nvgRGB(0xff, 0x3d, 0x9a)); nvgText(vg, x0 + laneW + 2.f, y + laneH * 0.75f, f.c_str(), NULL);
  }
  // right column: selected track, one item per line
  float rx = box.size.x * 0.72f; const TrackParams& p = e.p[e.selected];
  nvgFontSize(vg, 10.f); nvgFillColor(vg, nvgRGB(0xf2, 0xf2, 0xf4));
  bool arm = e.t[e.selected].armedPending, fz = e.g.freeze;
  std::string lines[8] = { string::f("T%d  SD %d", e.selected + 1, p.seed), string::f("CH %d  NT %d", p.chance, p.note), string::f("OC %d  EV %d", p.octave, p.evolve),
    string::f("RATE %s", RATES[p.rateIdx].label), string::f("LEN %d", p.length), string::f("DIR %s", DIRN_NAMES[p.dirn]),
    string::f("%s %s", ROOT_NAMES[e.g.root], SCALES[e.g.scale].name), arm && fz ? "ARM FRZ" : arm ? "ARM" : fz ? "FRZ" : "" };
  for (int i = 0; i < 8; ++i) nvgText(vg, rx, 11.f + i * 11.f, lines[i].c_str(), NULL);
  nvgResetScissor(vg);
}

struct MelodyMonsterWidget : ModuleWidget {
	void appendContextMenu(Menu* menu) override;

	MelodyMonsterWidget(MelodyMonster* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/MelodyMonster.svg")));
		namespace L = layout::melody;
		auto at = [](const float (&a)[2]) { return mm2px(Vec(a[0], a[1])); };
		{ TalkingFace* f = createWidget<TalkingFace>(mm2px(Vec(L::FACE[0] - L::FACE[2], L::FACE[1] - L::FACE[2]))); f->box.size = mm2px(Vec(2 * L::FACE[2], 2 * L::FACE[2])); if (module) f->tap = &module->face; addChild(f); }

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		static const float* const trk[8] = { L::TRACK1_PARAM, L::TRACK2_PARAM, L::TRACK3_PARAM, L::TRACK4_PARAM, L::TRACK5_PARAM, L::TRACK6_PARAM, L::TRACK7_PARAM, L::TRACK8_PARAM };
		for (int i = 0; i < 8; ++i)
			addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<LimeLight>>>(mm2px(Vec(trk[i][0], trk[i][1])), module, MelodyMonster::TRACK1_PARAM + i, MelodyMonster::TRACK1_LIGHT + i));
		addParam(createParamCentered<Trimpot>(at(L::LENG_PARAM), module, MelodyMonster::LENG_PARAM));
		addParam(createParamCentered<Trimpot>(at(L::RATE_PARAM), module, MelodyMonster::RATE_PARAM));
		addParam(createParamCentered<Trimpot>(at(L::DIRN_PARAM), module, MelodyMonster::DIRN_PARAM));
		addParam(createParamCentered<Trimpot>(at(L::TRNS_PARAM), module, MelodyMonster::TRNS_PARAM));
		addParam(createParamCentered<Trimpot>(at(L::SHFT_PARAM), module, MelodyMonster::SHFT_PARAM));
		addParam(createParamCentered<Trimpot>(at(L::OCTA_PARAM), module, MelodyMonster::OCTA_PARAM));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<LimeLight>>>(at(L::FRZE_PARAM), module, MelodyMonster::FRZE_PARAM, MelodyMonster::FRZE_LIGHT));
		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<LimeLight>>>(at(L::MUTE_PARAM), module, MelodyMonster::MUTE_PARAM, MelodyMonster::MUTE_LIGHT));
		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<LimeLight>>>(at(L::SOLO_PARAM), module, MelodyMonster::SOLO_PARAM, MelodyMonster::SOLO_LIGHT));
		addParam(createParamCentered<Rogan1PSWhite>(at(L::CHANCE_PARAM), module, MelodyMonster::CHANCE_PARAM));
		addParam(createParamCentered<Rogan1PSWhite>(at(L::NOTE_PARAM), module, MelodyMonster::NOTE_PARAM));
		addParam(createParamCentered<Rogan1PSWhite>(at(L::OCTAVE_PARAM), module, MelodyMonster::OCTAVE_PARAM));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<LimeLight>>>(at(L::RUN_PARAM), module, MelodyMonster::RUN_PARAM, MelodyMonster::RUN_LIGHT));
		addParam(createParamCentered<VCVButton>(at(L::RSET_PARAM), module, MelodyMonster::RSET_PARAM));
		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<LimeLight>>>(at(L::RSED_PARAM), module, MelodyMonster::RSED_PARAM, MelodyMonster::RSED_LIGHT));

		addInput(createInputCentered<PJ301MPort>(at(L::CLK_INPUT), module, MelodyMonster::CLK_INPUT));
		addInput(createInputCentered<PJ301MPort>(at(L::RST_INPUT), module, MelodyMonster::RST_INPUT));
		addInput(createInputCentered<PJ301MPort>(at(L::RESEED_INPUT), module, MelodyMonster::RESEED_INPUT));
		addInput(createInputCentered<PJ301MPort>(at(L::FREEZE_INPUT), module, MelodyMonster::FREEZE_INPUT));
		addInput(createInputCentered<PJ301MPort>(at(L::SEED_INPUT), module, MelodyMonster::SEED_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(at(L::CLK_OUTPUT), module, MelodyMonster::CLK_OUTPUT));
		static const float* const gate[8] = { L::GATE1_OUTPUT, L::GATE2_OUTPUT, L::GATE3_OUTPUT, L::GATE4_OUTPUT, L::GATE5_OUTPUT, L::GATE6_OUTPUT, L::GATE7_OUTPUT, L::GATE8_OUTPUT };
		static const float* const pitch[8] = { L::PITCH1_OUTPUT, L::PITCH2_OUTPUT, L::PITCH3_OUTPUT, L::PITCH4_OUTPUT, L::PITCH5_OUTPUT, L::PITCH6_OUTPUT, L::PITCH7_OUTPUT, L::PITCH8_OUTPUT };
		for (int i = 0; i < 8; ++i) {
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(gate[i][0], gate[i][1])), module, MelodyMonster::GATE1_OUTPUT + i));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(pitch[i][0], pitch[i][1])), module, MelodyMonster::PITCH1_OUTPUT + i));
		}

		MonsterDisplay* d = createWidget<MonsterDisplay>(mm2px(Vec(L::DISPLAY[0], L::DISPLAY[1])));
		d->box.size = mm2px(Vec(L::DISPLAY[2], L::DISPLAY[3])); d->module = module; addChild(d);
	}
};


void MelodyMonsterWidget::appendContextMenu(Menu* menu) {
  MelodyMonster* m = dynamic_cast<MelodyMonster*>(module); if (!m) return;
  int sel = m->eng.selected; TrackParams* p = &m->eng.p[sel];
  menu->addChild(new MenuSeparator);
  menu->addChild(createMenuLabel(string::f("Track %d", sel + 1)));
  menu->addChild(mkSlider(new IntSliderQ(&p->evolve, 0, 100, 0, "Evolve", "%")));
  menu->addChild(mkSlider(new IntSliderQ(&p->breathe, 0, 100, 0, "Breathe", "%")));
  menu->addChild(mkSlider(new IntSliderQ(&p->gateLen, 5, 100, 50, "Gate length", "%")));
  menu->addChild(mkSlider(new IntSliderQ(&p->tie, 0, 100, 0, "Tie", "%")));
  menu->addChild(mkSlider(new IntSliderQ(&p->slop, 0, 100, 0, "Slop", "%")));
  menu->addChild(mkSlider(new FloatSliderQ(&p->gateLevel, 1.f, 10.f, 1.f, 10.f, "Gate level", " V")));
  menu->addChild(mkSlider(new FloatSliderQ(&p->pitchScale, 5.f, 200.f, 1.f, 100.f, "Pitch scale", "%")));
  menu->addChild(mkSlider(new FloatSliderQ(&p->pitchOffset, -10.f, 10.f, 0.1f, 0.f, "Pitch offset", " V")));
  { std::vector<std::string> l = {"Off"}; for (int i = 1; i <= 8; ++i) l.push_back(string::f("Track %d", i));
    menu->addChild(createIndexSubmenuItem("Follow", l, [=]() { return p->follow; }, [=](int v) { p->follow = v; })); }
  { struct SeedQ : IntSliderQ { MelodyMonster* m; int tr; SeedQ(MelodyMonster* m, int tr) : IntSliderQ(&m->eng.p[tr].seed, 0, 999, m->eng.p[tr].seed, "Seed"), m(m), tr(tr) {}
      void setValue(float x) override { m->pendingSeed[tr] = clamp((int)std::round(x), 0, 999); }   // audio thread applies it
      float getValue() override { int pnd = m->pendingSeed[tr].load(); return (float)(pnd >= 0 ? pnd : m->eng.p[tr].seed); } };
    menu->addChild(mkSlider(new SeedQ(m, sel))); }
  menu->addChild(new MenuSeparator);
  menu->addChild(createMenuLabel("Global"));
  { std::vector<std::string> l; for (int i = 0; i < SCALE_COUNT; ++i) l.push_back(SCALES[i].name);
    menu->addChild(createIndexSubmenuItem("Scale", l, [=]() { return m->eng.g.scale; }, [=](int v) { m->eng.g.scale = v; })); }
  { std::vector<std::string> l; for (int i = 0; i < 12; ++i) l.push_back(ROOT_NAMES[i]);
    menu->addChild(createIndexSubmenuItem("Root note", l, [=]() { return m->eng.g.root; }, [=](int v) { m->eng.g.root = v; })); }
  menu->addChild(mkSlider(new IntSliderQ(&m->eng.g.weight, 0, 100, 0, "Weight", "%")));
  menu->addChild(mkSlider(new IntSliderQ(&m->eng.g.bpm, 20, 300, 120, "BPM")));
  menu->addChild(createIndexSubmenuItem("Frozen clock out", {"Runs", "Stops"}, [=]() { return m->frozenClockStops ? 1 : 0; }, [=](int v) { m->frozenClockStops = v == 1; }));
  menu->addChild(createIndexSubmenuItem("Poly output routing", {"Off", "Pairs", "Split 4+4", "All 8"}, [=]() { return m->polyRouting; }, [=](int v) { m->polyRouting = v; }));
  menu->addChild(createMenuItem("Reseed all", "", [=]() { m->pendingReseedAll = true; }));
}


Model* modelMelodyMonster = createModel<MelodyMonster, MelodyMonsterWidget>("MelodyMonster");