#pragma once
#include <stdint.h>
#include <stddef.h>

// Values are valid only when their corresponding bit is set. No wire format is
// assumed here; a verified protocol decoder must populate these fields.
namespace Bms {
enum Field : uint8_t { Soc = 1, Voltage = 2, Current = 4, Temperature = 8, Cells = 16 };
enum class Link : uint8_t { Disconnected, Connecting, Connected };
struct Data {
  uint8_t valid = 0;
  float soc = 0, voltage = 0, current = 0, temperature = 0;
  float cells[24] = {};
  Link link = Link::Disconnected;
  uint32_t updatedAt = 0;
  bool hasUpdate = false;
  bool fresh(uint32_t now, uint32_t timeout) const {
    return link == Link::Connected && hasUpdate && uint32_t(now - updatedAt) <= timeout;
  }
  bool maxDelta(float &out) const {
    if (!(valid & Cells)) return false;
    float lo = cells[0], hi = cells[0];
    for (size_t i = 1; i < 24; ++i) {
      if (cells[i] < lo) lo = cells[i];
      if (cells[i] > hi) hi = cells[i];
    }
    out = hi - lo;
    return true;
  }
};
// Unknown protocol: no packet is interpreted until a verified decoder replaces
// this implementation. In particular, received bytes never become live values.
inline bool decode(const uint8_t *, size_t, Data &) { return false; }
}
