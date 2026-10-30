// BmsProtocol.h — 比亚迪 7645 / 「亚迪智电」BMS 协议解析：【待适配的桩（STUB）】
//
// 现状：协议未知。没有真实抓包之前，不假设 JK / JBD / Daly 或任何通用协议。
// parse() 目前对任何数据都返回 false，绝不写入 BmsData，界面因此只会显示 "--"（或明确标注的模拟数据）。
//
// 拿到真实抓包后的适配步骤（见 firmware/LINKTOP_NOTES.md）：
//   1. 用 nRF Connect 导出 GATT 服务/特征列表，以及小程序显示已知数值时的 notify 原始十六进制；
//   2. 多组已知状态（不同 SOC、充电/放电电流、温度）对照，确定帧头、长度、命令字、校验、各字段编码和比例；
//   3. 若需要主动发送查询命令才能拿到数据，必须先由用户确认该命令只读且安全，再加白名单；
//   4. 在 firmware/tests 里加入真实抓包样本的解析测试，通过后才把 src 设为 BmsData::LIVE。
// 禁止：修改保护参数、均衡参数、充放电 MOS 控制、任何未知写命令、绕过认证。
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "BmsData.h"

namespace BmsProtocol {

static const bool kVerified = false;          // 协议未验证

// handle = 通知来源的特征值句柄；data/len = 原始字节。返回 true 表示解析出有效数据并已写入 out。
inline bool parse(uint16_t handle, const uint8_t *data, size_t len, BmsData &out, uint32_t now) {
  (void)handle; (void)data; (void)len; (void)out; (void)now;
  return false;                               // STUB：协议未知，拒绝所有数据
}

}  // namespace BmsProtocol
