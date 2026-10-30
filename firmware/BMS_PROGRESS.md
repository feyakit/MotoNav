# 2.16 BMS integration: work in progress

Base: adapt-216 at 83b4390. This branch is not a functional BMS integration and must not be used to assess battery safety.

## Implemented and tested

`boards/216/MotoNav/BmsData.h` defines a 24-cell data model, field validity, connection state, timestamp, timeout and maximum cell spread. The protocol entry point deliberately rejects all packets. `tests/BmsDataTest.cpp` exercises simulated values, missing values, stale/disconnected data, millisecond rollover and rejection of unknown packets. It runs on a host with `g++ -std=c++17 firmware/tests/BmsDataTest.cpp -o /tmp/bms-test && /tmp/bms-test`.

Existing host tests for media, rotation, idle shutdown and the macOS flasher passed. The BMS model is not yet connected to the firmware UI or BLE stack. No device test or 2.16 firmware build has been performed.

## Architecture findings and remaining work

The 2.16 sketch uses CO5300, CST92xx touch, a phone BLE GATT server and HID, with an Apple Media client operating on the phone connection. `AppleMediaClient::begin()` installs a custom GAP handler. A BMS client must share or dispatch that handler without stealing phone notifications; connection counts, RAM use, navigation throughput and stack configuration need measurement. Page transitions and call/pairing overlays have their own redraw paths, so a new page must handle those paths as well as touch and idle power-off. The existing PMU battery display represents the device battery, not the vehicle pack.

The base branch's `ADAPT_216.md` identifies unverified panel orientation, touch coordinates, power and layout assumptions. `firmware/build.py` currently builds only 175; `firmware/build.ps1 -Board 216` is the documented 216 build path. Toolchain installation in this environment failed while downloading Arduino indexes (connection refused), so no firmware binary or size report is claimed.

Remaining: passive scan with explicit target selection, discovery, selected notification subscription and bounded raw logging; coexistence and reconnection/timeout handling; a verified, read-only protocol decoder; SOC and 24-cell UI; integrated simulation; 2.16 compile and hardware regression testing. No UUIDs or data formats have been inferred, and no BMS commands have been implemented.

## Future real-device verification

First verify navigation, media, touch and display on the 2.16 board independently. With the user's BMS nearby, capture advertisements and discovered services/characteristics and raw notification bytes using an explicitly selected device. Compare multiple captures with readings from the official app, including changed readings, to identify and validate the format; do not transmit unidentified commands or change protection, activation or MOS settings. Then validate simultaneous phone navigation/music and BMS connections, disconnect/reconnect, stale data, heap and long-running behavior. Do not flash automatically.
