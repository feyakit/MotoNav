# linktop 分支说明（MotoNav × BYD 7645 BMS，2.16 方屏）

基线：`adapt-216` @ 83b4390。只改 `firmware/boards/216/`（及新增工具/测试/文档），175 / 185b 板型不动。
**未在真机上运行过。** 下面所有“可用”都指：能编译 + 桌面模拟预览正确；实机行为需要你按第 5 节验证。

## 1. 源码审查要点（P0）

| 模块 | 现状 | 处理 |
| --- | --- | --- |
| 显示 | Arduino_GFX `Arduino_CO5300` QSPI，PSRAM 画布 + 双缓冲 + 核心 0 推屏任务 + 自写快速刷屏 | 原样保留；新页面直接写同一块 RGB565 帧缓冲 |
| 触摸 | SensorLib CST92xx，中断触发轮询；滑动换页（跟手）、来电按住接听 | 保留；加了方屏页面的点按、列表上下滚动 |
| UI 框架 | **没有 LVGL**；全部是手写绘图（5x7 位图字放大、AA 矢量图标），中文靠手机发来的 4 位灰度“文字图片” | 新增 `SqGfx.h`（AA 圆角卡片/圆弧/多边形/文字）+ `SqFont.h`（Noto Sans CJK SC 位图字，固件内置中文） |
| 页面 | `page`：0 导航/首页，1 天气，2 音乐；布局按圆屏（圆环、半径 229） | 加 3 电池、4 电芯、5 蓝牙、6 菜单、7 设置、8 关于；导航箭头模式和音乐页换成方屏布局 |
| BLE | Arduino-ESP32 3.3.10 自带 BLE 库（NimBLE 后端）；手机连接 = 外设 GATT 服务（NUS 风格 UUID）+ HID 媒体键 + AMS（ESP 作为 GATT 客户端访问 iPhone 媒体服务，同一条连接）；配对码验证、最多记 3 台手机 | 不动。BMS 用 NimBLE 主机 API 以中心角色另开一条连接 |
| 导航/音乐协议 | 文字指令 + 'B' 文字图片 + 'M' 大块数据（地图），见 MotoNav.ino 头部注释 | 不改协议；只新增可选 `#T,HHMM`（手机时间，旧 App 不发则状态栏不显示时间） |
| 构建 | `build.ps1 -Board 216`（Windows）；`build.py` 只建 175 | Linux 命令见第 3 节 |

风险：`MotoNav.ino` 9600+ 行单文件；地图模式（`drawMapFrame`）是圆形雷达，本分支没重做，只把顶部状态栏换成方屏样式（半透明盖在地图上）。

## 2. 改了什么

新增文件（`firmware/boards/216/MotoNav/`）：
- `SqGfx.h` 纯 C++ 抗锯齿绘图；`SqFont.h` 自动生成的 16/20/24/32/64/96 px 字体（`firmware/tools/gen_sqfont.py`，改界面中文后重跑）。
- `SqPages.h` 所有方屏页面 + 布局常量 + 点按判定。固件和桌面预览共用这一份代码。
- `BmsData.h` 数据模型：SOC/电压/电流/剩余/额定容量/温度/SOH/循环/24 串/告警，逐字段有效位；`src` = NONE / SIM / LIVE；LIVE 数据 15 s 无更新即过期（界面显示 `--` + “数据过期”）。
- `BmsProtocol.h` **协议解析桩**：对任何数据返回 false，不写入数据模型。没有真实抓包前不假设 JK/JBD/Daly。
- `BmsBle.h` 只读 BMS 客户端：非阻塞扫描（RSSI、名字，按信号排序）、手动选择、连接、服务/特征/属性枚举、可读特征读一次、对 notify/indicate 写**标准 CCCD(0x2902)** 订阅、原始 hex 串口日志、15 s 间隔非阻塞自动重连、GATT 单步 10 s 超时。**不写任何特征值，不配对，不发命令。**

`MotoNav.ino` 改动（都标了 `linktop`）：
- 状态栏 → 方屏样式：时间（有 `#T` 时）、电池包 SOC（点它 → 电池主页）、模拟数据标记、手机蓝牙、BMS 链接。主板锂电池电量移到“关于”页。
- 导航箭头模式 → 方屏卡片（转向卡片：图标+距离+路名图片；车道；剩余里程；车速/限速/到达时间/行程进度）。`-DSQ_NAV_CARDS=0` 可回到原圆形布局。地图模式未改。
- 音乐页 → 方屏布局（封面为装饰占位，协议里没有专辑图；进度条只在 AMS 提供时长时显示）；按钮逻辑/HID/AMS 不变，只改了按钮位置判定。
- 手势：主页上滑 = 功能菜单（下滑仍是天气，左右仍是天气/音乐）；菜单任意方向滑回导航；电芯页左右滑回电池页；其他新页面滑回菜单；电芯/蓝牙页上下滑 = 滚动列表。导航中距路口 < 200 m 自动回导航（和天气页一致）。
- 电池页点圆弧 → 电芯页；电芯页右上按钮切换 列表 / 柱状图（固定 2.50–3.65 V 量程，不放大毫伏差）。
- 蓝牙页：电池行（点：断开/重连）、手机行、扫描结果（点 = 选为电池并连接，保存到 NVS，开机自动重连）、“扫描设备”按钮（8 s）。
- 设置页：模拟数据开/关（默认开，`-DBMS_SIM_DEFAULT=0` 改默认）、断开并忘记 BMS。
- 串口命令（115200）：`bms scan` / `bms list` / `bms connect <序号>` / `bms disconnect` / `bms forget`。
- 固件版本号 `v7.12.1-216-linktop`。

## 3. 构建

环境：arduino-cli 1.5.2（linux arm64），esp32:esp32 **3.3.10**，仓库自带 `firmware/libraries`（GFX 1.6.9、SensorLib 0.5.0）。

```sh
arduino-cli compile --jobs 2 --libraries firmware/libraries --build-path /tmp/mnbuild \
  --fqbn 'esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi,CDCOnBoot=cdc' \
  --output-dir /tmp/mnout firmware/boards/216/MotoNav
```

结果（2026-10-30，云端 aarch64 Debian 13）：**编译通过**，`EXIT=0`。
- `Sketch uses 1582763 bytes (50%) of program storage space. Maximum is 3145728 bytes.`
- `Global variables use 89812 bytes (27%) of dynamic memory, leaving 237868 bytes for local variables.`
- 对比 adapt-216 基线（feature/byd-bms 记录的同核心构建）：约 1,277,611 B / 88,516 B。增加的约 300 KB 主要是内置中文位图字体（SqFont.h，约 262 KB 字形数据），静态 RAM 只多约 1.3 KB；地图状态栏备份缓冲（44 KB）按需从 PSRAM 分配。
- 这只证明能编译链接；运行时堆、双连接、帧率都没有实测。
- 注意：arduino-cli 会在草图目录生成 `build/`，已加入 .gitignore。

桌面模拟预览（不是实机截图）：

```sh
python3 firmware/tests/render_square.py firmware/.preview design_mockup.png   # 需要 g++、Pillow
```
同时跑数据模型自检（最高/最低/压差、过期判定）。图在 `docs/linktop/`。

## 4. 双 BLE（手机 + BMS）可行性——结论：配置上可行，未实测

- 3.3.10 预编译 NimBLE：`MAX_CONNECTIONS=3`，CENTRAL / PERIPHERAL / OBSERVER 全开，控制器 `BLE_MAX_ACT=6`，`GATT_MAX_PROCS=4`。一条手机连接（含 HID + AMS）+ 一条 BMS 连接在协议栈层面允许。
- BMS 连接用 `ble_gap_connect` 自带回调，事件不经过 Arduino `BLEServer`，不会被当成手机连接；AMS 的全局监听只认自己的连接句柄。
- 扫描用 60 ms 周期 / 30 ms 窗口，给手机连接留空中时间；扫描期间不发起连接。
- 风险（必须实测）：
  1. 地图大块数据（2M PHY、长包）与 BMS 连接事件抢空口 → 地图帧率/丢包；
  2. `MAX_BONDS=3` 与手机配对共用——如果电池要求配对，会占用名额；本固件**不会**主动配对，读到“需要加密”只记日志；
  3. 全局安全回调（配对码界面）若被 BMS 触发会误显示配对码——目前不发起配对，理论上不会触发；
  4. NimBLE 主机任务栈 5120 B，回调里只入队，不做重活。
- 若实测不稳定：退路是手机中转（App 同时连 BMS，把解析后的数值用新文字指令转发给仪表），不需要 ESP 做中心角色。

## 5. 刷写与测试步骤

**先备份现有固件**（用户正在使用）：`esptool.py --chip esp32s3 read_flash 0 0x1000000 backup_216.bin`。不要用自动刷写。
1. 刷入 `/tmp/mnout/MotoNav.ino.bin`（应用分区 0x10000，保留设置）或用 `flash_macos.py --board 216` 的升级模式。
2. 回归：开机动画、首页、手机连接/配对、导航箭头页（新卡片布局）、地图模式、天气、音乐三个按钮、来电、到达特效、旋转、5 分钟无导航关机。
3. 新页面：主页上滑进菜单 → 六个按钮；点状态栏 SOC → 电池页（应显示“模拟数据”橙色标记）；点圆弧 → 电芯页，上下滑滚动、右上切柱状图；设置页关掉模拟 → 全部 `--`。
4. BMS：蓝牙页“扫描设备” → 找到电池（名字/RSSI）→ 点它 → 串口看到 `BMS SVC/CHR/READ/subscribe` 日志 → 小程序同时显示数值时看 `BMS RX ... hex`。**注意：很多 BMS 同一时间只允许一个中心连接**，ESP 连上时小程序可能连不上，反之亦然——抓对照数据时可以先用手机 nRF Connect。
5. 双连接压力：导航（地图模式）+ 音乐 + BMS 同时连 30 分钟，观察帧率、断线、重启、`ESP.getFreeHeap()`（串口）。

## 6. 需要你提供的数据（协议适配的前提）

1. **nRF Connect GATT 导出**：电池的广播名、MAC、全部服务/特征 UUID 和属性（截图或 Log 导出均可）。
2. **notify 原始 hex**：在 nRF Connect 里订阅所有 notify 特征，同时打开「亚迪智电」记录屏幕上的 SOC、总电压、电流（含正负）、温度、24 串电压；至少 3 组不同状态（静置 / 放电 / 充电）。每组截屏 + 同时刻的 hex。
3. 如果 nRF Connect 订阅后**没有数据**（说明小程序是先写查询命令再收回复）：需要小程序发出的写入内容——安卓开“蓝牙 HCI 信息收集日志”（开发者选项）后用小程序操作一遍，导出 `btsnoop_hci.log` 给我。只有确认是只读查询命令后才会加入白名单。
4. 是否需要绑定/密码（小程序首次连接是否要求输入码）。

## 7. 未解决问题

- 协议未知 → 无真实电池数据（BmsProtocol 是桩）。
- 地图模式仍是圆形雷达布局，只换了状态栏。
- 设备没有可靠时钟：状态栏时间需要 App 发 `#T,HHMM`（App 未改）；剩余时间（分钟）同理没显示，只显示剩余里程和到达时刻。
- 音乐封面没有数据源（协议无专辑图），显示装饰占位；效果图里的“收藏”心形按钮未实现（无对应协议）。
- 页面全屏重画用逐像素 AA，单页约几十 ms（估计值，未实测），列表滚动可能不够顺滑。
- 所有触摸坐标、方向沿用 adapt-216 的未实测假设（ADAPT_216.md 第 3 节）。
