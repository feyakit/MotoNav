// BmsData.h — 比亚迪 7645（24S 磷酸铁锂，76.8V / 45Ah）电池数据模型（纯 C++，不依赖 Arduino）
// 每个字段单独标记“有效”；超过 staleMs 没有新数据 => 全部视为过期，界面显示 "--" 并提示。
// 数据来源 src 必须如实标注：SIM（模拟，仅用于 UI 开发）或 LIVE（经过验证的协议解析结果）。
// 当前协议未知，BmsProtocol 不会产生 LIVE 数据。
#pragma once
#include <stdint.h>

struct BmsData {
  enum Src : uint8_t { NONE = 0, SIM = 1, LIVE = 2 };
  enum Field : uint8_t { F_SOC, F_VOLT, F_CURR, F_REMAIN, F_FULL, F_TEMP, F_SOH, F_CYCLES, F_CELLS, F_ALARM, F_COUNT };
  static const int CELLS = 24;

  Src src = NONE;
  float soc = 0, volt = 0, curr = 0, remainAh = 0, fullAh = 0, tempC = 0, soh = 0;
  int cycles = 0;
  uint16_t cellMv[CELLS] = {0};
  uint8_t cellN = 0;
  uint32_t alarms = 0;                 // 0 = 无告警（只有 F_ALARM 有效时才可信）
  uint32_t valid = 0;                  // 位掩码：1 << Field
  uint32_t updatedMs = 0;
  uint32_t staleMs = 15000;            // 超过 15 秒无更新 => 过期

  void clear() { valid = 0; src = NONE; cellN = 0; }
  void set(Field f) { valid |= 1u << f; }
  bool stale(uint32_t now) const {
    if (src == NONE) return true;
    if (src == SIM) return false;
    return (uint32_t)(now - updatedMs) > staleMs;
  }
  bool has(Field f, uint32_t now) const { return (valid & (1u << f)) && !stale(now); }
  // 曾经有过数据但已过期（用于“数据过期”提示，而不是假装没有连过）
  bool expired(uint32_t now) const { return src != NONE && valid && stale(now); }

  struct CellStats { int maxI = -1, minI = -1; uint16_t maxMv = 0, minMv = 0, deltaMv = 0; };
  CellStats cellStats() const {
    CellStats s;
    for (int i = 0; i < cellN && i < CELLS; i++) {
      uint16_t v = cellMv[i];
      if (v == 0) continue;                                 // 0 = 该串未知
      if (s.maxI < 0 || v > s.maxMv) { s.maxMv = v; s.maxI = i; }
      if (s.minI < 0 || v < s.minMv) { s.minMv = v; s.minI = i; }
    }
    if (s.maxI >= 0) s.deltaMv = s.maxMv - s.minMv;
    return s;
  }

  // 仅用于 UI 开发的模拟数据（效果图里的演示值），src = SIM，界面会明确标注“模拟数据”。
  void loadSimulation(uint32_t now) {
    clear();
    src = SIM;
    soc = 40.6f; volt = 78.89f; remainAh = 18.84f; fullAh = 45.0f; curr = 0.0f; tempC = 22; soh = 100; cycles = 0;
    cellN = CELLS;
    for (int i = 0; i < CELLS; i++) cellMv[i] = 3287;
    cellMv[2] = 3288; cellMv[23] = 3286; cellMv[10] = 3286 + 1; cellMv[17] = 3288 - 1;
    alarms = 0;
    for (int f = F_SOC; f <= F_ALARM; f++) if (f != F_CYCLES) set((Field)f);   // 循环次数：演示里也没有 => "--"
    updatedMs = now;
  }
};
