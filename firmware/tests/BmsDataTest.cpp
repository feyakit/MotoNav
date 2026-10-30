#include "../boards/216/MotoNav/BmsData.h"
#include <cassert>
#include <cmath>
int main() {
  Bms::Data d;
  float delta = 0;
  assert(!d.fresh(0, 1000));
  assert(!d.maxDelta(delta));
  d.link = Bms::Link::Connected;
  d.valid = Bms::Cells | Bms::Soc | Bms::Voltage | Bms::Current | Bms::Temperature;
  d.soc = 73; d.voltage = 79.2f; d.current = -5.0f; d.temperature = 25;
  for (int i = 0; i < 24; ++i) d.cells[i] = 3.3f;
  d.cells[0] = 3.28f; d.cells[23] = 3.32f;
  d.updatedAt = 0xfffffff0u; d.hasUpdate = true;
  assert(d.fresh(0x10u, 32));
  assert(!d.fresh(0x11u, 32));
  assert(d.maxDelta(delta) && std::fabs(delta - 0.04f) < 0.00001f);
  const uint8_t unknown[] = {1, 2, 3};
  assert(!Bms::decode(unknown, sizeof(unknown), d));
  assert(d.soc == 73 && d.updatedAt == 0xfffffff0u);
  d.link = Bms::Link::Disconnected;
  assert(!d.fresh(0x10u, 32));
}
