#pragma once
#include "BmsCharacteristics.h"
#include <host/ble_gap.h>
#include <host/ble_gatt.h>
#include <host/ble_hs_id.h>
#include <host/ble_uuid.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// Explicitly selected peer only. Discovery is asynchronous and read-only.
// Only an explicitly selected characteristic may have its standard CCCD written.
// No BMS protocol command or pairing operation is issued.
class BmsDiscovery {
  enum Kind : uint8_t { Connect, Disconnect, Service, ServiceEnd, Characteristic, CharacteristicEnd, Descriptor, DescriptorEnd, Subscribed };
  struct Event { Kind kind; int status; uint16_t conn, first, last; uint8_t props; ble_uuid_any_t uuid; };
  struct Range { uint16_t first, last; };
  struct Packet { uint16_t conn, handle, length, copied; bool indication; uint8_t data[128]; };
  QueueHandle_t queue = nullptr, packets = nullptr;
  unsigned droppedPackets = 0;
  portMUX_TYPE packetMux = portMUX_INITIALIZER_UNLOCKED;
  BmsCharacteristics characteristics;
  enum Subscription : uint8_t { None, Descriptors, Writing, Active };
  Subscription subscription = None;
  uint16_t selected = 0, cccd = 0; bool indication = false;
  bool lost = false;
  portMUX_TYPE lossMux = portMUX_INITIALIZER_UNLOCKED;
  uint16_t conn = 0xffff;
  bool connecting = false, discovering = false, discoveryComplete = false, cancelRequested = false, pendingGatt = false;
  bool retryEnabled = false; ble_addr_t peer{}; uint32_t retryAt = 0;
  Range ranges[16] = {}; unsigned count = 0, index = 0;
  bool post(const Event &e) { if (!queue || xQueueSend(queue, &e, 0) != pdTRUE) { portENTER_CRITICAL(&lossMux); lost = true; portEXIT_CRITICAL(&lossMux); return false; } return true; }
  static int gap(ble_gap_event *ev, void *arg) {
    auto self = static_cast<BmsDiscovery *>(arg); Event e{};
    if (ev->type == BLE_GAP_EVENT_CONNECT) {
      e.kind = Connect; e.status = ev->connect.status; e.conn = ev->connect.conn_handle; if (!self->post(e) && !e.status) ble_gap_terminate(e.conn, BLE_ERR_REM_USER_CONN_TERM);
    } else if (ev->type == BLE_GAP_EVENT_DISCONNECT) {
      e.kind = Disconnect; e.status = ev->disconnect.reason; e.conn = ev->disconnect.conn.conn_handle; self->post(e);
    } else if (ev->type == BLE_GAP_EVENT_NOTIFY_RX && self->packets) {
      Packet p{}; p.conn = ev->notify_rx.conn_handle; p.handle = ev->notify_rx.attr_handle;
      p.indication = ev->notify_rx.indication; p.length = OS_MBUF_PKTLEN(ev->notify_rx.om);
      p.copied = p.length < sizeof(p.data) ? p.length : sizeof(p.data);
      if (p.copied) os_mbuf_copydata(ev->notify_rx.om, 0, p.copied, p.data);
      if (xQueueSend(self->packets, &p, 0) != pdTRUE) {
        portENTER_CRITICAL(&self->packetMux); ++self->droppedPackets; portEXIT_CRITICAL(&self->packetMux);
      }
    }
    return 0;
  }
  static int services(uint16_t conn, const ble_gatt_error *err, const ble_gatt_svc *svc, void *arg) {
    auto self = static_cast<BmsDiscovery *>(arg); Event e{}; e.conn = conn; e.status = err->status;
    if (err->status || !svc) e.kind = ServiceEnd;
    else { e.kind = Service; e.first = svc->start_handle; e.last = svc->end_handle; e.uuid = svc->uuid; }
    self->post(e); return 0;
  }
  static int chars(uint16_t conn, const ble_gatt_error *err, const ble_gatt_chr *ch, void *arg) {
    auto self = static_cast<BmsDiscovery *>(arg); Event e{}; e.conn = conn; e.status = err->status;
    if (err->status || !ch) e.kind = CharacteristicEnd;
    else { e.kind = Characteristic; e.first = ch->def_handle; e.last = ch->val_handle; e.props = ch->properties; e.uuid = ch->uuid; }
    self->post(e); return 0;
  }
  static int descriptors(uint16_t conn, const ble_gatt_error *err, uint16_t, const ble_gatt_dsc *d, void *arg) {
    auto self = static_cast<BmsDiscovery *>(arg); Event e{}; e.conn = conn; e.status = err->status;
    if (err->status || !d) e.kind = DescriptorEnd;
    else { e.kind = Descriptor; e.first = d->handle; e.uuid = d->uuid; }
    self->post(e); return 0;
  }
  static int written(uint16_t conn, const ble_gatt_error *err, ble_gatt_attr *, void *arg) {
    auto self = static_cast<BmsDiscovery *>(arg); Event e{}; e.conn = conn; e.kind = Subscribed; e.status = err->status; self->post(e); return 0;
  }
  void next() {
    if (index >= count) { discovering = false; discoveryComplete = true; Serial.println("BMS discovery complete"); return; }
    int rc = ble_gattc_disc_all_chrs(conn, ranges[index].first, ranges[index].last, chars, this);
    if (rc) { discovering = false; Serial.printf("BMS characteristic discovery failed rc=%d\n", rc); } else pendingGatt = true;
  }
public:
  bool isConnected() const { return conn != 0xffff; }
  bool isRetryEnabled() const { return retryEnabled; }
  bool isConnecting() const { return connecting; }
  void begin() { queue = xQueueCreate(24, sizeof(Event)); packets = xQueueCreate(8, sizeof(Packet)); }
  void command(const char *s) {
    if (!strcmp(s, "bms disconnect")) {
      retryEnabled = false;
      if (connecting) { cancelRequested = true; int rc = ble_gap_conn_cancel(); Serial.printf("BMS cancel rc=%d\n", rc); }
      else if (conn != 0xffff) { int rc = ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM); Serial.printf("BMS disconnect rc=%d\n", rc); }
      return;
    }
    if (!strncmp(s, "bms subscribe ", 14)) {
      unsigned handle; char mode, extra;
      if (sscanf(s + 14, "%u %c %c", &handle, &mode, &extra) != 2 || handle > 65535 || (mode != 'n' && mode != 'i')) {
        Serial.println("Usage: bms subscribe <value-handle-decimal> <n|i>"); return;
      }
      if (conn == 0xffff || !discoveryComplete || discovering || pendingGatt || subscription != None || !packets) { Serial.println("BMS busy or not ready"); return; }
      const auto *entry = characteristics.select(handle, mode == 'i');
      if (!entry) { Serial.println("BMS characteristic not discovered, unsupported or no descriptor range"); return; }
      selected = handle; cccd = 0; indication = mode == 'i'; subscription = Descriptors;
      int rc = ble_gattc_disc_all_dscs(conn, entry->value, entry->end, descriptors, this);
      if (rc) subscription = None; else pendingGatt = true;
      Serial.printf("BMS descriptor discovery rc=%d\n", rc); return;
    }
    if (strncmp(s, "bms connect ", 12)) return;
    unsigned type, b[6]; char extra;
    if (sscanf(s + 12, "%u %2x:%2x:%2x:%2x:%2x:%2x %c", &type, &b[0], &b[1], &b[2], &b[3], &b[4], &b[5], &extra) != 7 || type > 1) {
      Serial.println("Usage: bms connect <0 public|1 random> AA:BB:CC:DD:EE:FF"); return;
    }
    if (!queue || connecting || pendingGatt || conn != 0xffff) { Serial.println("BMS busy or no queue"); return; }
    peer = {}; peer.type = type; for (int i = 0; i < 6; ++i) peer.val[i] = b[5-i];
    retryEnabled = true; retryAt = millis();
  }
  void tick() {
    portENTER_CRITICAL(&lossMux); bool overflow = lost; lost = false; portEXIT_CRITICAL(&lossMux);
    if (overflow) { Serial.println("BMS event overflow: disconnecting, results incomplete");
      if (connecting) ble_gap_conn_cancel();
      if (conn != 0xffff) ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
      Event pending;
      while (queue && xQueueReceive(queue, &pending, 0) == pdTRUE)
        if (pending.kind == Connect && !pending.status) ble_gap_terminate(pending.conn, BLE_ERR_REM_USER_CONN_TERM);
      conn = 0xffff; connecting = discovering = discoveryComplete = false; subscription = None; characteristics.clear(); count = index = 0; retryEnabled = false;
    }
    Event e;
    for (int n = 0; queue && n < 4 && xQueueReceive(queue, &e, 0) == pdTRUE; ++n) {
      // Do not start a new link until the previous GATT operation's terminal
      // callback has been consumed; handle values can be reused on reconnect.
      if (e.kind == ServiceEnd || e.kind == CharacteristicEnd || e.kind == DescriptorEnd || e.kind == Subscribed) pendingGatt = false;
      if (e.kind == Connect) {
        if (!connecting) { if (!e.status) ble_gap_terminate(e.conn, BLE_ERR_REM_USER_CONN_TERM); continue; }
        connecting = false;
        if (cancelRequested) { cancelRequested = false; if (!e.status) ble_gap_terminate(e.conn, BLE_ERR_REM_USER_CONN_TERM); continue; }
        Serial.printf("BMS connect status=%d\n", e.status);
        if (e.status) retryAt = millis() + 20000;
        if (!e.status) {
          conn = e.conn; count = index = 0; characteristics.clear(); subscription = None; discoveryComplete = false; discovering = true;
          int rc = ble_gattc_disc_all_svcs(conn, services, this);
          if (rc) { discovering = false; Serial.printf("BMS service discovery failed rc=%d\n", rc); } else pendingGatt = true;
        }
      } else if (e.kind == Disconnect) {
        if (conn == e.conn) { conn = 0xffff; discovering = discoveryComplete = false; subscription = None; characteristics.clear(); count = index = 0; retryAt = millis() + 20000; }
        Serial.printf("BMS disconnected reason=%d\n", e.status);
      } else if (e.conn == conn && subscription == Descriptors && e.kind == Descriptor) {
        if (ble_uuid_u16(&e.uuid.u) == 0x2902) {
          if (cccd) { subscription = None; Serial.println("BMS duplicate CCCD; subscription aborted"); }
          else cccd = e.first;
        }
      } else if (e.conn == conn && subscription == Descriptors && e.kind == DescriptorEnd) {
        if (!retryEnabled || (e.status != BLE_HS_EDONE && e.status != 0) || !cccd) {
          subscription = None; Serial.printf("BMS CCCD discovery failed status=%d\n", e.status);
        } else {
          const uint8_t value[2] = { static_cast<uint8_t>(indication ? 2 : 1), 0 };
          subscription = Writing;
          int rc = ble_gattc_write_flat(conn, cccd, value, sizeof(value), written, this);
          if (rc) subscription = None; else pendingGatt = true;
          Serial.printf("BMS CCCD write rc=%d\n", rc);
        }
      } else if (e.conn == conn && subscription == Writing && e.kind == Subscribed) {
        subscription = e.status ? None : Active;
        Serial.printf("BMS subscription status=%d\n", e.status);
      } else if (e.conn == conn && discovering) {
        if (e.kind == Service) {
          char uuid[BLE_UUID_STR_LEN]; ble_uuid_to_str(&e.uuid.u, uuid);
          Serial.printf("BMS service %s start=%u end=%u\n", uuid, e.first, e.last);
          if (count < 16) ranges[count++] = {e.first, e.last};
          else { discovering = false; Serial.println("BMS too many services; discovery incomplete"); }
        } else if (e.kind == ServiceEnd) {
          if (e.status != BLE_HS_EDONE && e.status != 0) { discovering = false; Serial.printf("BMS service discovery error=%d\n", e.status); }
          else next();
        } else if (e.kind == Characteristic) {
          char uuid[BLE_UUID_STR_LEN]; ble_uuid_to_str(&e.uuid.u, uuid);
          Serial.printf("BMS char %s def=%u value=%u properties=0x%02X\n", uuid, e.first, e.last, e.props);
          if (!characteristics.add(e.first, e.last, ranges[index].last, e.props)) { discovering = false; Serial.println("BMS characteristic list invalid or full"); }
        } else if (e.kind == CharacteristicEnd) {
          if (e.status != BLE_HS_EDONE && e.status != 0) { discovering = false; Serial.printf("BMS char discovery error=%d\n", e.status); }
          else { ++index; next(); }
        }
      }
    }
    if (retryEnabled && !connecting && !pendingGatt && (!queue || uxQueueMessagesWaiting(queue) == 0) && conn == 0xffff && (int32_t)(millis() - retryAt) >= 0) {
      uint8_t own = 0; int rc = ble_hs_id_infer_auto(0, &own);
      if (!rc) rc = ble_gap_connect(own, &peer, 10000, nullptr, gap, this);
      if (!rc) { connecting = true; cancelRequested = false; }
      retryAt = millis() + 20000;
      Serial.printf("BMS connect rc=%d\n", rc);
    }
    Packet p;
    for (int i = 0; packets && i < 2 && xQueueReceive(packets, &p, 0) == pdTRUE; ++i) {
      if (p.conn != conn || p.handle != selected || (subscription != Writing && subscription != Active)) continue;
      Serial.printf("BMS %s handle=%u len=%u%s data=", p.indication ? "indication" : "notification", p.handle, p.length, p.copied < p.length ? " TRUNCATED" : "");
      for (unsigned j = 0; j < p.copied; ++j) Serial.printf("%02X", p.data[j]);
      Serial.println();
    }
    portENTER_CRITICAL(&packetMux); unsigned dropped = droppedPackets; droppedPackets = 0; portEXIT_CRITICAL(&packetMux);
    if (dropped) Serial.printf("BMS notification queue dropped %u packets\n", dropped);
  }
};
