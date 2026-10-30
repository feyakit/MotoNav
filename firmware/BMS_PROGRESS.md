# 2.16 BMS integration: work in progress

Base: adapt-216 at 83b4390. This branch is not a functional BMS integration and must not be used to assess battery safety.

## Implemented and tested

`boards/216/MotoNav/BmsData.h` defines a 24-cell data model, field validity, connection state, timestamp, timeout and maximum cell spread. The protocol entry point deliberately rejects all packets. `tests/BmsDataTest.cpp` exercises simulated values, missing values, stale/disconnected data, millisecond rollover and rejection of unknown packets. It runs on a host with `g++ -std=c++17 firmware/tests/BmsDataTest.cpp -o /tmp/bms-test && /tmp/bms-test`.

Existing host tests for media, rotation, idle shutdown and the macOS flasher passed. The 2.16 UI includes a BMS page reached by swiping left from weather, and an SOC label on the waiting home screen. The data model is not connected to BLE. On the 115200 baud USB serial console, send `bms scan` followed by newline to run a five-second passive BLE scan. Reports include address, address type, RSSI and raw advertising bytes; the bounded queue can drop reports. This does not identify a BMS or connect to it. No device test has been performed. The default 2.16 build with ESP32 core 3.3.10 succeeded: application 1,277,611 / 3,145,728 bytes; static RAM 88,516 / 327,680 bytes. This is not a measurement of runtime heap or concurrent BLE use. To enable the explicitly labelled simulated fixture, add `--build-property compiler.cpp.extra_flags=-DMOTONAV_BMS_SIMULATION=1` to the Arduino CLI build command. Never use simulation to assess a real pack.

## Architecture findings and remaining work

The 2.16 sketch uses CO5300, CST92xx touch, a phone BLE GATT server and HID, with an Apple Media client operating on the phone connection. `AppleMediaClient::begin()` installs a custom GAP handler. A BMS client must share or dispatch that handler without stealing phone notifications; connection counts, RAM use, navigation throughput and stack configuration need measurement. Page transitions and call/pairing overlays have their own redraw paths, so a new page must handle those paths as well as touch and idle power-off. The existing PMU battery display represents the device battery, not the vehicle pack.

The base branch's `ADAPT_216.md` identifies unverified panel orientation, touch coordinates, power and layout assumptions. `firmware/build.py` currently builds only 175; `firmware/build.ps1 -Board 216` is the documented 216 build path. The cloud Arduino CLI requires `network.proxy` configured to the environment proxy; the ESP32 3.3.10 core was installed for compilation.

Remaining: explicit target selection, GATT discovery, selected notification subscription and bounded raw notification logging; coexistence and reconnection/timeout handling; a verified, read-only protocol decoder; integrated BLE simulation and UI validation; 2.16 compile and hardware regression testing. No UUIDs or data formats have been inferred, and no BMS commands have been implemented.

## Future real-device verification

First verify navigation, media, touch and display on the 2.16 board independently. With the user's BMS nearby, capture advertisements and discovered services/characteristics and raw notification bytes using an explicitly selected device. Compare multiple captures with readings from the official app, including changed readings, to identify and validate the format; do not transmit unidentified commands or change protection, activation or MOS settings. Then validate simultaneous phone navigation/music and BMS connections, disconnect/reconnect, stale data, heap and long-running behavior. Do not flash automatically.
