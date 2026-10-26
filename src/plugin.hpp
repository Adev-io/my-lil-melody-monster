#pragma once
#include <rack.hpp>
using namespace rack;
extern Plugin* pluginInstance;
extern Model* modelMelodyMonster;

// House light colour, the same as the panel (tools/panel.py), My lil Acid Bass and My lil Toolbox.
struct LimeLight : GrayModuleLightWidget { LimeLight() { addBaseColor(nvgRGB(0xc8, 0xff, 0x3a)); } };
