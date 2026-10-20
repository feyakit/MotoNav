# MotoNav 适配微雪 ESP32-S3-Touch-AMOLED-2.16 说明

本分支（`adapt-216`）在上游 MotoNav（原支持 1.75 英寸）基础上新增 2.16 英寸板型，
代码位于 `firmware/boards/216/`，原有 175 / 185b 板型文件未做任何改动。

目标硬件：用户手中的微雪 ESP32-S3-Touch-AMOLED-2.16（480×480 AMOLED 圆屏）。

## 1. 硬件差异（1.75 → 2.16）

| 项目 | ESP32-S3-Touch-AMOLED-1.75 | ESP32-S3-Touch-AMOLED-2.16 |
| --- | --- | --- |
| 主控 | ESP32-S3R8（16MB Flash / 8MB PSRAM） | 同左 |
| 显示 | 1.75" AMOLED，CO5300，QSPI，466×466 | 2.16" AMOLED，CO5300，QSPI，480×480 |
| 面板构造偏移 | col_offset = 6 | col_offset = 0（官方 HelloWorld 示例） |
| LCD 引脚 | SIO0-3=4/5/6/7，SCL=38，CS=12，RST=39 | 相同 |
| 触摸 | CST9217，I2C SDA=15/SCL=14，INT=11，RST=40 | CST9220（同 CST92xx 系列），引脚相同 |
| 电源管理 | AXP2101（I2C 14/15） | AXP2101（I2C 14/15） |
| IMU | QMI8658 | QMI8658（INT1=GPIO17，INT2=GPIO21） |
| RTC | PCF85063 | PCF85063（INT=GPIO13） |
| 音频 | ES8311 | ES8311 + ES7210（MCLK=42，BCLK=9，LRCK=45，DSDIN=8，ASDOUT=10，PA=46） |
| SD 卡 | SPI CMD=1/CLK=2/D0=3/CS=41 | 相同 |
| 易踩坑 | — | GPIO16 是 SYS_OUT、GPIO17 是 IMU 中断，**不是**背光/TE 引脚（AMOLED 无背光，本板未引出 TE），勿占用 |

参数来源：
- 微雪官方文档 https://docs.waveshare.net/ESP32-S3-Touch-AMOLED-2.16/（外设速查、GPIO 引脚分配、音频引脚表）
- 微雪官方示例 https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.16 （`examples/arduino/01_HelloWorld`：`Arduino_CO5300(bus, RST, 0, W, H, 0, 0, 0, 0)`）

## 2. 改动清单

### 新增 `firmware/boards/216/MotoNav/`（与 175 同构的 13 个文件）

- `MotoNav.ino`：以 1.75 版为基底——板型注释、固件版本 `v7.12.1-216`；
  屏幕几何按 480/466 等比缩放（圆心 233→240、行程环半径 222→229、
  SPEED_Y/ICON_CY/DIST_Y/ROAD_Y/BOTTOM_Y/TEXT_R 等布局常量全部缩放，源码注释中标注了原值）；
  面板构造偏移 6→0（按官方 2.16 示例）。
- `pin_config.h`：按官方引脚表重写（见上表）。特别修正了两个自动生成版本里出现过的错误：
  删除不存在的 `LCD_BL=GPIO16`、`LCD_TE=GPIO17`；I2S 的 DI/DO 按官方音频表对齐（DI=10 来自 ES7210，DO=8 去往 ES8311）；
  补回 `XPOWERS_CHIP_AXP2101`（两块板都是 AXP2101）。
- `BootAnimation.h`：开机动画画布 466→480，圆心 233→240，各半径与文字纵坐标按比例缩放（211→217、203→209、刻度 191/196/199→197/202/205、标题 89→92、品牌名 263→271、分隔线 y 324→334、副标题 348→359、徽章中心 184→190）。
- 其余 10 个头文件（AppleMediaClient/Commands/Data、BrandLogos、DragonRadar、HidMediaKeys、
  IdlePowerOff、RasterContinuity、ScreenRotationCommand、TripProgress）与 1.75 版相同：
  它们不含写死的面板尺寸（均通过 LCD_WIDTH/LCD_HEIGHT 或帧头尺寸工作），原样复制。

说明：早期自动生成的 `Board216Panel.h` / `Board216Init.h`（照 185b 模式写的自定义面板类）
已删除——它们未被 `.ino` 引用，且其中的初始化序列来源未经官方资料证实；
2.16 与 1.75 同为 CO5300，官方示例同样使用 GFX 库自带初始化，故 216 沿用 175 的驱动路径。

### 修改构建/烧录工具（对 175/185b 行为无影响）

- `firmware/build.ps1`：板型校验集加入 `216`；标签 `2.16`；216 与 175 一样生成 SHA-256 清单，
  清单中的板名按所选板型写（已修正一版把板名写死为 2.16 的问题）；216 与 185b 共用 PanelVariant 替换逻辑
  （216 的 pin_config 中 `LCD_INIT_VARIANT` 为预留定义，当前固件未使用）。
- `firmware/flash_macos.py`：新增 `--board {175,216}` 参数（默认 175，行为不变），
  按板型选择源码目录、`firmware/dist/<board>/panel0` 与清单板名校验。

## 3. 未确认 / TODO（需实机或编译验证）

1. **未做真实编译**：适配环境无 Arduino 工具链，仅完成静态检查（全部本地 `#include` 可解析、关键引脚符号齐全）。
   请先在 Windows 跑 `.\firmware\build.ps1 -Board 216` 确认编译通过。
2. **MADCTL**：官方 HelloWorld 在 `begin()` 后多了一步 `bus->writeC8D8(0x36, 0xA0)`；
   MotoNav 1.75 版没有这一步。若 2.16 出现画面镜像/方向不对，优先怀疑此处，需要实机比对。
3. **触摸方向**：CST9220 与 CST9217 共用 SensorLib `TouchDrvCST92xx` 驱动与地址 0x5A，引脚相同；
   坐标是否需要翻转已预留 `BOARD_TOUCH_FLIP_X/Y`（当前为 0），实机点按验证。
4. **手机端光栅尺寸**：导航底图协议帧头自带宽高、固件按帧校验，理论上不受面板尺寸锁定；
   但 Android/iOS App 原按 466 面板设计，其发送尺寸与在 480 画布上的居中表现需实机确认。
5. **电源行为**：AXP2101 + GPIO16 SYS_OUT 的电源路径与 1.75 不完全相同，
   IdlePowerOff 的五分钟无导航关机逻辑需在 2.16 上实测。
6. **音频**：ES8311/ES7210 引脚按官方表填写，但固件音频路径未在 2.16 验证。
7. **IMU 旋转**：QMI8658 中断脚为 GPIO17/21（1.75 不同），若固件用中断而非轮询读 IMU，需另行适配；当前按轮询假设未改。

## 4. 构建与烧录

```powershell
# Windows 构建（仓库根目录）
.\firmware\build.ps1 -Board 216
# 产物：firmware/dist/216/panel0/（app_0x10000 升级镜像、full_0x0 完整镜像、manifest 清单）
```

Mac 烧录：先把 2.16 板子用 USB 线接上，再运行
`python3 firmware/flash_macos.py --board 216`（默认升级模式，保留设置；
首次完整安装加 `--full` 并按提示确认；`--check` 只检查不烧录）。
根目录的「一键烧录固件.command」仍是 1.75 默认入口，2.16 请用上面的命令行方式。

烧录后验证顺序：开机动画是否居中无偏移 → 触摸点按方向 → 蓝牙连手机出导航画面 →
底图居中情况 → 静置五分钟关机。任何一项异常先回看第 3 节对应条目。
