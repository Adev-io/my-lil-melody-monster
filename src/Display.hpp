#pragma once
#include "plugin.hpp"
#include "engine.hpp"
struct MelodyMonster;
struct MonsterDisplay : TransparentWidget {
  MelodyMonster* module = nullptr;
  std::shared_ptr<Font> font;
  void drawLayer(const DrawArgs& args, int layer) override;
};
