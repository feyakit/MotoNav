#pragma once
#include <stddef.h>
#include <stdint.h>
// Extract a short, printable local name from a BLE advertising payload.
// No peer identity or BMS protocol is inferred from the name.
inline bool bmsAdvertisementName(const uint8_t *data, size_t size, char *out, size_t capacity) {
  if (!data || !out || !capacity) return false;
  for (size_t off=0; off<size;) {
    size_t len=data[off];
    if (!len) return false;
    if (len > size - off - 1) return false;
    if ((data[off+1]==8 || data[off+1]==9) && len>1) {
      size_t n=len-1; if(n>=capacity)n=capacity-1;
      for(size_t i=0;i<n;++i) { uint8_t c=data[off+2+i]; out[i]=(c>=32 && c<127) ? (char)c : '?'; }
      out[n]=0; return true;
    }
    off += len+1;
  }
  return false;
}
