#pragma once
#include <host/ble_gap.h>
#include <host/ble_gatt.h>
#include <host/ble_hs_id.h>
#include <host/ble_uuid.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// Explicitly selected peer only. Discovery is asynchronous and read-only.
// No characteristic or descriptor writes, pairing, or BMS command is issued.
class BmsDiscovery {
  enum Kind : uint8_t { Connect, Disconnect, Service, ServiceEnd, Characteristic, CharacteristicEnd };
  struct Event { Kind kind; int status; uint16_t conn, first, last; uint8_t props; ble_uuid_any_t uuid; };
  struct Range { uint16_t first, last; };
  QueueHandle_t queue = nullptr;
  bool lost = false;
  portMUX_TYPE lossMux = portMUX_INITIALIZER_UNLOCKED;
  uint16_t conn = 0xffff;
  bool connecting = false, discovering = false, cancelRequested = false;
  Range ranges[16] = {}; unsigned count = 0, index = 0;
  bool post(const Event &e) { if (!queue || xQueueSend(queue, &e, 0) != pdTRUE) { portENTER_CRITICAL(&lossMux); lost = true; portEXIT_CRITICAL(&lossMux); return false; } return true; }
  static int gap(ble_gap_event *ev, void *arg) {
    auto self = static_cast<BmsDiscovery *>(arg); Event e{};
    if (ev->type == BLE_GAP_EVENT_CONNECT) {
      e.kind = Connect; e.status = ev->connect.status; e.conn = ev->connect.conn_handle; if (!self->post(e) && !e.status) ble_gap_terminate(e.conn, BLE_ERR_REM_USER_CONN_TERM);
    } else if (ev->type == BLE_GAP_EVENT_DISCONNECT) {
      e.kind = Disconnect; e.status = ev->disconnect.reason; e.conn = ev->disconnect.conn.conn_handle; self->post(e);
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
  void next() {
    if (index >= count) { discovering = false; Serial.println("BMS discovery complete"); return; }
    int rc = ble_gattc_disc_all_chrs(conn, ranges[index].first, ranges[index].last, chars, this);
    if (rc) { discovering = false; Serial.printf("BMS characteristic discovery failed rc=%d\n", rc); }
  }
public:
  bool isConnected() const { return conn != 0xffff; }
  bool isConnecting() const { return connecting; }
  void begin() { queue = xQueueCreate(24, sizeof(Event)); }
  void command(const char *s) {
    if (!strcmp(s, "bms disconnect")) {
      if (connecting) { cancelRequested = true; int rc = ble_gap_conn_cancel(); Serial.printf("BMS cancel rc=%d\n", rc); }
      else if (conn != 0xffff) { int rc = ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM); Serial.printf("BMS disconnect rc=%d\n", rc); }
      return;
    }
    if (strncmp(s, "bms connect ", 12)) return;
    unsigned type, b[6]; char extra;
    if (sscanf(s + 12, "%u %2x:%2x:%2x:%2x:%2x:%2x %c", &type, &b[0], &b[1], &b[2], &b[3], &b[4], &b[5], &extra) != 7 || type > 1) {
      Serial.println("Usage: bms connect <0 public|1 random> AA:BB:CC:DD:EE:FF"); return;
    }
    if (!queue || connecting || conn != 0xffff) { Serial.println("BMS busy or no queue"); return; }
    ble_addr_t peer{}; peer.type = type; for (int i = 0; i < 6; ++i) peer.val[i] = b[5-i];
    uint8_t own = 0; int rc = ble_hs_id_infer_auto(0, &own);
    if (!rc) rc = ble_gap_connect(own, &peer, 10000, nullptr, gap, this);
    if (!rc) { connecting = true; cancelRequested = false; }
    Serial.printf("BMS connect rc=%d\n", rc);
  }
  void tick() {
    portENTER_CRITICAL(&lossMux); bool overflow = lost; lost = false; portEXIT_CRITICAL(&lossMux);
    if (overflow) { Serial.println("BMS event overflow: disconnecting, results incomplete");
      if (connecting) ble_gap_conn_cancel();
      if (conn != 0xffff) ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
      Event pending;
      while (queue && xQueueReceive(queue, &pending, 0) == pdTRUE)
        if (pending.kind == Connect && !pending.status) ble_gap_terminate(pending.conn, BLE_ERR_REM_USER_CONN_TERM);
      conn = 0xffff; connecting = discovering = false; count = index = 0;
    }
    Event e;
    for (int n = 0; queue && n < 4 && xQueueReceive(queue, &e, 0) == pdTRUE; ++n) {
      if (e.kind == Connect) {
        if (!connecting) { if (!e.status) ble_gap_terminate(e.conn, BLE_ERR_REM_USER_CONN_TERM); continue; }
        connecting = false;
        if (cancelRequested) { cancelRequested = false; if (!e.status) ble_gap_terminate(e.conn, BLE_ERR_REM_USER_CONN_TERM); continue; }
        Serial.printf("BMS connect status=%d\n", e.status);
        if (!e.status) {
          conn = e.conn; count = index = 0; discovering = true;
          int rc = ble_gattc_disc_all_svcs(conn, services, this);
          if (rc) { discovering = false; Serial.printf("BMS service discovery failed rc=%d\n", rc); }
        }
      } else if (e.kind == Disconnect) {
        if (conn == e.conn) { conn = 0xffff; discovering = false; count = index = 0; }
        Serial.printf("BMS disconnected reason=%d\n", e.status);
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
        } else if (e.kind == CharacteristicEnd) {
          if (e.status != BLE_HS_EDONE && e.status != 0) { discovering = false; Serial.printf("BMS char discovery error=%d\n", e.status); }
          else { ++index; next(); }
        }
      }
    }
  }
};
