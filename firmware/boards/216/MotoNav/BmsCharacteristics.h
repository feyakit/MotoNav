#pragma once
#include <stdint.h>
#include <stddef.h>
// Only handles and standard GATT properties learned during this connection.
class BmsCharacteristics {
public:
  struct Entry { uint16_t value, end; uint8_t properties; };
  static constexpr size_t Capacity = 64;
  void clear() { count = 0; invalid = false; }
  bool add(uint16_t definition, uint16_t value, uint16_t serviceEnd, uint8_t properties) {
    if (!definition || value <= definition || value > serviceEnd || count == Capacity) { invalid = true; return false; }
    if (count && definition <= entries[count-1].end && definition > entries[count-1].value)
      entries[count-1].end = definition - 1;
    entries[count++] = {value, serviceEnd, properties};
    return true;
  }
  const Entry *select(uint16_t value, bool indication) const {
    if (invalid) return nullptr;
    for (size_t i = 0; i < count; ++i) {
      const Entry &e = entries[i];
      if (e.value == value && e.end > e.value && (e.properties & (indication ? 0x20 : 0x10))) return &e;
    }
    return nullptr;
  }
private:
  Entry entries[Capacity] = {};
  size_t count = 0;
  bool invalid = false;
};
