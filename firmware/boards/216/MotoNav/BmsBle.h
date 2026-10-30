// BmsBle.h — 电池 BMS 只读蓝牙客户端（NimBLE 主机直调，ESP32-S3 / Arduino-ESP32 3.3.x）
//
// 功能：非阻塞扫描（RSSI、名字）、手动选择设备、连接、GATT 服务/特征/属性枚举、
//       读取“可读”特征一次并记录、对 notify / indicate 特征写标准 CCCD(0x2902) 订阅、
//       原始十六进制日志（USB 串口 115200）、断线自动重连（非阻塞，带间隔）。
// 安全：绝不写任何特征值；唯一的写操作是蓝牙标准的 CCCD 描述符（开启通知），
//       不发起配对，不发送任何 BMS 命令。协议解析交给 BmsProtocol（当前为桩）。
//
// 与手机连接的关系：手机连接由 Arduino BLEServer 管理（外设角色）；这里用 ble_gap_connect
// 以中心角色单独建立第二条连接，事件走自己的回调，不经过 BLEServer。
// Arduino-ESP32 3.3.10 预编译 NimBLE：MAX_CONNECTIONS=3，CENTRAL/PERIPHERAL/OBSERVER 均开启。
// 所有 NimBLE 回调只把事件放进队列，界面和日志在 loop()（tick）里处理。
#pragma once
#include <Arduino.h>
#include <host/ble_gap.h>
#include <host/ble_gatt.h>
#include <host/ble_hs_id.h>
#include <host/ble_hs_adv.h>
#include <host/ble_uuid.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "BmsData.h"
#include "BmsProtocol.h"
#include "SqPages.h"

class BmsBle {
public:
  static const int MAX_SCAN = 12, MAX_SVC = 12, MAX_CHR = 40;
  sq::ScanItem items[MAX_SCAN];
  ble_addr_t itemAddr[MAX_SCAN];
  int itemN = 0;
  sq::LinkInfo link;               // 给界面看的状态
  bool scanning = false;
  bool changed = false;            // 界面需要重画
  BmsData *data = nullptr;
  uint32_t dropped = 0;

  void begin(BmsData *d) {
    data = d;
    evq = xQueueCreate(32, sizeof(Ev));
    advq = xQueueCreate(16, sizeof(Adv));
    rxq = xQueueCreate(10, sizeof(Rx));
  }

  // 选定目标（来自保存的设置或界面点击）。type: 0 公共地址 1 随机地址
  void setTarget(const ble_addr_t &a, const char *name) {
    target = a; haveTarget = true;
    snprintf(link.name, sizeof link.name, "%s", name ? name : "");
    addrStr(a, link.addr);
    link.state = 1; retryAt = millis(); changed = true;
  }
  void forget() { disconnect(); haveTarget = false; link = sq::LinkInfo(); changed = true; }
  bool hasTarget() const { return haveTarget; }
  const ble_addr_t &targetAddr() const { return target; }

  void startScan(uint32_t ms = 8000) {
    if (scanning || connecting) return;
    uint8_t own; if (ble_hs_id_infer_auto(0, &own)) return;
    ble_gap_disc_params p{};
    p.itvl = 0x60; p.window = 0x30;        // 60ms 周期扫 30ms：给手机连接留出空中时间
    p.passive = 0;                         // 主动扫描（发标准 SCAN_REQ 以拿到名字），不连接、不写入
    p.filter_duplicates = 0;               // 需要持续更新 RSSI
    int rc = ble_gap_disc(own, ms, &p, gapCb, this);
    Serial.printf("BMS scan start rc=%d\n", rc);
    if (!rc) { scanning = true; itemN = 0; changed = true; }
  }
  void stopScan() { if (scanning) { ble_gap_disc_cancel(); scanning = false; changed = true; } }

  void connectItem(int i) {
    if (i < 0 || i >= itemN) return;
    if (conn != NONE || connecting) disconnect();
    setTarget(itemAddr[i], items[i].name);
    autoRetry = true;
    retryAt = millis();
  }
  void disconnect() {
    autoRetry = false;
    if (connecting) { ble_gap_conn_cancel(); connecting = false; }
    if (conn != NONE) ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
    if (haveTarget) link.state = 1;
    changed = true;
  }
  void enableAutoRetry(bool on) { autoRetry = on; }

  void serialCommand(const char *s) {
    if (!strcmp(s, "bms scan")) startScan();
    else if (!strcmp(s, "bms list")) for (int i = 0; i < itemN; i++) Serial.printf("  [%d] %s type=%d rssi=%d name=%s\n", i, items[i].addr, itemAddr[i].type, items[i].rssi, items[i].name);
    else if (!strncmp(s, "bms connect ", 12)) connectItem(atoi(s + 12));
    else if (!strcmp(s, "bms disconnect")) disconnect();
    else if (!strcmp(s, "bms forget")) { forget(); forgetReq = true; }
    else Serial.println("cmds: bms scan | bms list | bms connect <i> | bms disconnect | bms forget");
  }
  bool forgetReq = false;          // 让 .ino 清掉保存的目标

  void tick() {
    uint32_t now = millis();
    Adv a;
    while (xQueueReceive(advq, &a, 0) == pdTRUE) mergeAdv(a);
    Ev e;
    for (int n = 0; n < 8 && xQueueReceive(evq, &e, 0) == pdTRUE; n++) handle(e, now);
    Rx r;
    for (int n = 0; n < 4 && xQueueReceive(rxq, &r, 0) == pdTRUE; n++) {
      link.rx++; link.lastRxMs = now; changed = true;
      Serial.printf("BMS RX %s h=0x%04X len=%u%s:", r.ind ? "IND" : "NTF", r.handle, r.len, r.len > sizeof r.data ? " (truncated)" : "");
      for (int i = 0; i < r.len && i < (int)sizeof r.data; i++) Serial.printf(" %02X", r.data[i]);
      Serial.println();
      if (data && BmsProtocol::parse(r.handle, r.data, r.len < sizeof r.data ? r.len : sizeof r.data, *data, now)) changed = true;
    }
    if (dropped != droppedShown) { droppedShown = dropped; Serial.printf("BMS RX queue dropped=%lu\n", (unsigned long)dropped); }
    // GATT 步骤超时：断开重来，不卡死
    if (conn != NONE && opBusy && now - opAt > 10000) { Serial.println("BMS GATT timeout, disconnect"); ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM); opBusy = false; }
    // 连接 RSSI
    if (conn != NONE && now - rssiAt > 2000) { rssiAt = now; int8_t v; if (!ble_gap_conn_rssi(conn, &v)) { if (link.rssi != v) changed = true; link.rssi = v; } }
    // 非阻塞自动重连
    if (haveTarget && autoRetry && conn == NONE && !connecting && !scanning && (long)(now - retryAt) >= 0) {
      uint8_t own;
      if (!ble_hs_id_infer_auto(0, &own)) {
        int rc = ble_gap_connect(own, &target, 6000, nullptr, gapCb, this);
        Serial.printf("BMS connect %s rc=%d\n", link.addr, rc);
        if (!rc) { connecting = true; link.state = 2; changed = true; }
      }
      retryAt = now + 15000;
    }
  }

private:
  static const uint16_t NONE = 0xFFFF;
  enum Kind : uint8_t { CONNECT, DISCONNECT, SVC, SVC_END, CHR, CHR_END, DSC, DSC_END, READ, WRITE, SCAN_END };
  struct Ev { Kind k; int status; uint16_t conn, a, b; uint8_t props; char uuid[40]; uint8_t len; uint8_t data[64]; };
  struct Adv { ble_addr_t addr; int8_t rssi; char name[24]; };
  struct Rx { uint16_t handle, len; bool ind; uint8_t data[160]; };
  struct Svc { uint16_t first, last; };
  struct Chr { uint16_t def, val, end, cccd; uint8_t props; };
  QueueHandle_t evq = nullptr, advq = nullptr, rxq = nullptr;
  ble_addr_t target{};
  bool haveTarget = false, autoRetry = true, connecting = false, opBusy = false;
  uint16_t conn = NONE;
  uint32_t retryAt = 0, opAt = 0, rssiAt = 0, droppedShown = 0;
  Svc svc[MAX_SVC]; int svcN = 0, svcI = 0;
  Chr chr[MAX_CHR]; int chrN = 0, chrI = 0, chrStep = 0;

  static void addrStr(const ble_addr_t &a, char *out) {
    snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", a.val[5], a.val[4], a.val[3], a.val[2], a.val[1], a.val[0]);
  }
  void post(const Ev &e) { if (evq) xQueueSend(evq, &e, 0); }
  static void uuidStr(const ble_uuid_t *u, char *out) { char b[BLE_UUID_STR_LEN]; ble_uuid_to_str(u, b); snprintf(out, 40, "%s", b); }

  static int gapCb(ble_gap_event *ev, void *arg) {
    BmsBle *s = (BmsBle *)arg; Ev e{};
    switch (ev->type) {
      case BLE_GAP_EVENT_DISC: {
        Adv a{}; a.addr = ev->disc.addr; a.rssi = ev->disc.rssi;
        ble_hs_adv_fields f;
        if (!ble_hs_adv_parse_fields(&f, ev->disc.data, ev->disc.length_data) && f.name && f.name_len) {
          int n = f.name_len < 23 ? f.name_len : 23; memcpy(a.name, f.name, n); a.name[n] = 0;
        }
        if (s->advq) xQueueSend(s->advq, &a, 0);
        return 0;
      }
      case BLE_GAP_EVENT_DISC_COMPLETE: e.k = SCAN_END; s->post(e); return 0;
      case BLE_GAP_EVENT_CONNECT: e.k = CONNECT; e.status = ev->connect.status; e.conn = ev->connect.conn_handle; s->post(e); return 0;
      case BLE_GAP_EVENT_DISCONNECT: e.k = DISCONNECT; e.status = ev->disconnect.reason; e.conn = ev->disconnect.conn.conn_handle; s->post(e); return 0;
      case BLE_GAP_EVENT_NOTIFY_RX: {
        if (ev->notify_rx.conn_handle != s->conn) return 0;
        Rx r{}; r.handle = ev->notify_rx.attr_handle; r.ind = ev->notify_rx.indication;
        r.len = OS_MBUF_PKTLEN(ev->notify_rx.om);
        os_mbuf_copydata(ev->notify_rx.om, 0, r.len < sizeof r.data ? r.len : sizeof r.data, r.data);
        if (!s->rxq || xQueueSend(s->rxq, &r, 0) != pdTRUE) s->dropped++;
        return 0;
      }
      default: return 0;
    }
  }
  static int svcCb(uint16_t c, const ble_gatt_error *err, const ble_gatt_svc *sv, void *arg) {
    Ev e{}; e.conn = c; e.status = err->status;
    if (err->status || !sv) e.k = SVC_END;
    else { e.k = SVC; e.a = sv->start_handle; e.b = sv->end_handle; uuidStr(&sv->uuid.u, e.uuid); }
    ((BmsBle *)arg)->post(e); return 0;
  }
  static int chrCb(uint16_t c, const ble_gatt_error *err, const ble_gatt_chr *ch, void *arg) {
    Ev e{}; e.conn = c; e.status = err->status;
    if (err->status || !ch) e.k = CHR_END;
    else { e.k = CHR; e.a = ch->def_handle; e.b = ch->val_handle; e.props = ch->properties; uuidStr(&ch->uuid.u, e.uuid); }
    ((BmsBle *)arg)->post(e); return 0;
  }
  static int dscCb(uint16_t c, const ble_gatt_error *err, uint16_t, const ble_gatt_dsc *d, void *arg) {
    Ev e{}; e.conn = c; e.status = err->status;
    if (err->status || !d) e.k = DSC_END;
    else { e.k = DSC; e.a = d->handle; e.b = ble_uuid_u16(&d->uuid.u); }
    ((BmsBle *)arg)->post(e); return 0;
  }
  static int readCb(uint16_t c, const ble_gatt_error *err, ble_gatt_attr *at, void *arg) {
    Ev e{}; e.k = READ; e.conn = c; e.status = err->status;
    if (!err->status && at) { e.a = at->handle; uint16_t n = OS_MBUF_PKTLEN(at->om); e.b = n; e.len = n < 64 ? n : 64; os_mbuf_copydata(at->om, 0, e.len, e.data); }
    ((BmsBle *)arg)->post(e); return 0;
  }
  static int writeCb(uint16_t c, const ble_gatt_error *err, ble_gatt_attr *, void *arg) {
    Ev e{}; e.k = WRITE; e.conn = c; e.status = err->status; ((BmsBle *)arg)->post(e); return 0;
  }

  void mergeAdv(const Adv &a) {
    char s[18]; addrStr(a.addr, s);
    int i = 0;
    for (; i < itemN; i++) if (!memcmp(itemAddr[i].val, a.addr.val, 6)) break;
    if (i == itemN) { if (itemN >= MAX_SCAN) return; itemN++; items[i].name[0] = 0; }
    itemAddr[i] = a.addr; items[i].rssi = a.rssi; memcpy(items[i].addr, s, 18);
    if (a.name[0]) snprintf(items[i].name, sizeof items[i].name, "%s", a.name);
    // 按 RSSI 排序（插入排序，最多 12 个）
    for (int k = i; k > 0 && items[k].rssi > items[k - 1].rssi; k--) {
      sq::ScanItem t = items[k]; items[k] = items[k - 1]; items[k - 1] = t;
      ble_addr_t ta = itemAddr[k]; itemAddr[k] = itemAddr[k - 1]; itemAddr[k - 1] = ta;
    }
    changed = true;
  }

  static const char *propStr(uint8_t p, char *b) {
    b[0] = 0;
    if (p & BLE_GATT_CHR_PROP_READ) strcat(b, "READ ");
    if (p & BLE_GATT_CHR_PROP_WRITE_NO_RSP) strcat(b, "WRITE_NR ");
    if (p & BLE_GATT_CHR_PROP_WRITE) strcat(b, "WRITE ");
    if (p & BLE_GATT_CHR_PROP_NOTIFY) strcat(b, "NOTIFY ");
    if (p & BLE_GATT_CHR_PROP_INDICATE) strcat(b, "INDICATE ");
    return b;
  }

  // 下一步 GATT 操作（一次只有一个在途）
  void nextOp() {
    opBusy = false;
    if (conn == NONE) return;
    int rc = 0;
    if (svcI < svcN) {                                   // 1) 逐个服务枚举特征
      rc = ble_gattc_disc_all_chrs(conn, svc[svcI].first, svc[svcI].last, chrCb, this);
      if (!rc) { opBusy = true; opAt = millis(); return; }
      Serial.printf("BMS chr disc rc=%d\n", rc); svcI++; return nextOp();
    }
    while (chrI < chrN) {                                // 2) 逐个特征：读一次 / 找 CCCD / 订阅
      Chr &c = chr[chrI];
      if (chrStep == 0) {
        chrStep = 1;
        if (c.props & BLE_GATT_CHR_PROP_READ) {
          rc = ble_gattc_read(conn, c.val, readCb, this);
          if (!rc) { opBusy = true; opAt = millis(); return; }
        }
      }
      if (chrStep == 1) {
        chrStep = 2;
        if ((c.props & (BLE_GATT_CHR_PROP_NOTIFY | BLE_GATT_CHR_PROP_INDICATE)) && c.end > c.val) {
          rc = ble_gattc_disc_all_dscs(conn, c.val, c.end, dscCb, this);
          if (!rc) { opBusy = true; opAt = millis(); return; }
        }
      }
      if (chrStep == 2) {
        chrStep = 3;
        if (c.cccd) {
          // 唯一的写操作：标准 CCCD（0x2902）开启通知/指示。不是特征值，不是 BMS 命令。
          static uint8_t v[2];
          v[0] = (c.props & BLE_GATT_CHR_PROP_NOTIFY) ? 0x01 : 0x02; v[1] = 0;
          rc = ble_gattc_write_flat(conn, c.cccd, v, 2, writeCb, this);
          Serial.printf("BMS subscribe val=0x%04X cccd=0x%04X %s rc=%d\n", c.val, c.cccd, v[0] == 1 ? "notify" : "indicate", rc);
          if (!rc) { opBusy = true; opAt = millis(); return; }
        }
      }
      chrI++; chrStep = 0;
    }
    link.state = link.subs ? 4 : 3; changed = true;
    Serial.printf("BMS discovery done: services=%d chars=%d subscribed=%d (protocol parser: %s)\n",
                  svcN, chrN, link.subs, BmsProtocol::kVerified ? "verified" : "STUB, raw log only");
  }

  void handle(const Ev &e, uint32_t now) {
    char pb[48];
    switch (e.k) {
      case SCAN_END: scanning = false; changed = true; Serial.printf("BMS scan done, %d devices\n", itemN); break;
      case CONNECT:
        connecting = false;
        if (e.status) { Serial.printf("BMS connect failed status=%d\n", e.status); link.state = 5; retryAt = now + 15000; changed = true; break; }
        conn = e.conn; svcN = svcI = chrN = chrI = chrStep = 0; link.services = link.subs = 0; link.rx = 0;
        link.state = 3; changed = true;
        Serial.printf("BMS connected conn=%u %s\n", conn, link.addr);
        if (ble_gattc_disc_all_svcs(conn, svcCb, this) == 0) { opBusy = true; opAt = now; }
        break;
      case DISCONNECT:
        if (e.conn != conn) break;
        Serial.printf("BMS disconnected reason=0x%X\n", e.status);
        conn = NONE; opBusy = false; link.state = haveTarget ? 1 : 0; link.rssi = -999; changed = true;
        retryAt = now + 5000;
        break;
      case SVC:
        if (svcN < MAX_SVC) { svc[svcN++] = {e.a, e.b}; Serial.printf("BMS SVC %s handles 0x%04X-0x%04X\n", e.uuid, e.a, e.b); }
        else Serial.println("BMS SVC table full (ignored)");
        break;
      case SVC_END: link.services = svcN; changed = true; svcI = 0; nextOp(); break;
      case CHR:
        if (chrN < MAX_CHR) {
          if (chrN && chr[chrN - 1].end == 0xFFFF) chr[chrN - 1].end = e.a - 1;
          chr[chrN++] = {e.a, e.b, (uint16_t)0xFFFF, 0, e.props};
          Serial.printf("BMS   CHR %s val=0x%04X props=0x%02X %s\n", e.uuid, e.b, e.props, propStr(e.props, pb));
        }
        break;
      case CHR_END:
        for (int i = 0; i < chrN; i++) if (chr[i].end == 0xFFFF) chr[i].end = svc[svcI].last;   // 本服务最后一个特征
        svcI++; nextOp(); break;
      case DSC: if (e.b == 0x2902 && chrI < chrN) chr[chrI].cccd = e.a; break;
      case DSC_END: nextOp(); break;
      case READ:
        if (e.status) Serial.printf("BMS READ val=0x%04X error=%d%s\n", chr[chrI].val, e.status, e.status == 0x105 || e.status == 0x10F ? " (needs pairing/encryption - not attempted)" : "");
        else {
          Serial.printf("BMS READ val=0x%04X len=%u:", e.a, e.b);
          for (int i = 0; i < e.len; i++) Serial.printf(" %02X", e.data[i]);
          Serial.println(e.b > e.len ? " (truncated)" : "");
        }
        nextOp(); break;
      case WRITE:
        if (!e.status) link.subs++; else Serial.printf("BMS CCCD write error=%d\n", e.status);
        changed = true; nextOp(); break;
    }
  }
};
