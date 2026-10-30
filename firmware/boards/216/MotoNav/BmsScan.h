#pragma once
#include <host/ble_gap.h>
#include <host/ble_hs_id.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "BmsDiscovery.h"

// Opt-in, passive diagnostic only. Its own discovery callback does not replace
// the phone/AMS custom GAP handler. Reports are bounded and drained on loop().
class BmsScan {
  struct Report { uint8_t addr[6], type, event, len, data[31]; int8_t rssi; };
  QueueHandle_t queue = nullptr;
  BmsDiscovery discovery;
  char line[64] = {}; uint8_t used = 0; bool overflow = false;
  static int callback(ble_gap_event *event, void *arg) {
    auto self = static_cast<BmsScan *>(arg);
    if (event->type == BLE_GAP_EVENT_DISC && self->queue) {
      const auto &d = event->disc; Report r{};
      memcpy(r.addr, d.addr.val, 6); r.type = d.addr.type; r.event = d.event_type; r.rssi = d.rssi;
      r.len = d.length_data > sizeof(r.data) ? sizeof(r.data) : d.length_data;
      if (r.len && d.data) memcpy(r.data, d.data, r.len);
      else r.len = 0;
      xQueueSend(self->queue, &r, 0); // drop when full; never block the BLE host
    }
    return 0;
  }
public:
  bool isConnected() const { return discovery.isConnected(); }
  bool isConnecting() const { return discovery.isConnecting(); }
  void begin() { queue = xQueueCreate(16, sizeof(Report)); discovery.begin(); }
  void tick() {
    // Read at most 32 input bytes and two reports per loop to bound latency.
    for (int n = 0; n < 32 && Serial.available(); ++n) {
      char c = Serial.read();
      if (c == '\r') continue;
      if (c == '\n') {
        line[used] = 0;
        if (!overflow && strcmp(line, "bms scan") == 0) {
          uint8_t own = 0; ble_gap_disc_params params{};
          params.passive = 1; params.filter_duplicates = 1;
          params.itvl = 160; params.window = 32;
          int rc = queue ? ble_hs_id_infer_auto(0, &own) : -1;
          if (!rc) rc = ble_gap_disc(own, 5000, &params, callback, this);
          Serial.printf("BMS passive scan rc=%d duration=5s\n", rc);
        }
        if (!overflow) discovery.command(line);
        used = 0; overflow = false;
      } else if (used < sizeof(line) - 1) line[used++] = c;
      else overflow = true;
    }
    discovery.tick();
    Report r;
    for (int i = 0; queue && i < 2 && xQueueReceive(queue, &r, 0) == pdTRUE; ++i) {
      Serial.printf("BMS adv %02X:%02X:%02X:%02X:%02X:%02X type=%u event=%u rssi=%d data=", r.addr[5], r.addr[4], r.addr[3], r.addr[2], r.addr[1], r.addr[0], r.type, r.event, r.rssi);
      for (int j = 0; j < r.len; ++j) Serial.printf("%02X", r.data[j]);
      Serial.println();
    }
  }
};
