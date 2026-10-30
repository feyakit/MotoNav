# 2.16 BMS integration: work in progress

Base: adapt-216 at 83b4390. This branch is not a functional BMS integration and must not be used to assess battery safety.

## Implemented and tested

`boards/216/MotoNav/BmsData.h` defines a 24-cell data model, field validity, connection state, timestamp, timeout and maximum cell spread. The protocol entry point deliberately rejects all packets. `tests/BmsCharacteristicsTest.cpp` checks discovered handle boundaries, properties, and rejection after incomplete discovery. `tests/BmsDataTest.cpp` exercises simulated values, missing values, stale/disconnected data, millisecond rollover and rejection of unknown packets. It runs on a host with `g++ -std=c++17 firmware/tests/BmsDataTest.cpp -o /tmp/bms-test && /tmp/bms-test`.

Existing host tests for media, rotation, idle shutdown and the macOS flasher passed. The 2.16 UI includes a BMS page reached by swiping left from weather, and an SOC label on the waiting home screen. The data model is not connected to BLE. On the 115200 baud USB serial console, send `bms scan` followed by newline to run a five-second passive BLE scan. Reports include address, address type, RSSI and raw advertising bytes; the bounded queue can drop reports. The scan does not identify a BMS. After explicitly identifying the correct peer, `bms connect 0 AA:BB:CC:DD:EE:FF` (public address; use 1 for random) connects and logs discovered services and characteristics. `bms disconnect` disconnects or cancels the attempt. Discovery is asynchronous, limited to 16 services and a bounded event queue, and reports incomplete results on overflow. Once discovery completes, `bms subscribe <decimal-value-handle> n` selects a discovered notify-capable characteristic; use `i` for indications. The firmware discovers the standard CCCD (0x2902) and writes only the standard enable value. It logs notifications up to 128 bytes, flags truncation, and reports queue drops; logs are diagnostic, not a reliable complete capture. It does not read characteristics or send BMS protocol commands. Reconnection retries the explicitly selected address every 20 seconds in RAM until `bms disconnect` or restart; it rediscovers services but never automatically resubscribes because handles may change. Battery values are invalidated across link transitions. No protocol data is decoded. These diagnostics have not been tested on hardware. No device test has been performed. Before adding GATT discovery, the default 2.16 build with ESP32 core 3.3.10 succeeded: application 1,277,611 / 3,145,728 bytes; static RAM 88,516 / 327,680 bytes. The corresponding separately compiled simulation build succeeded: application 1,277,719 bytes; static RAM 88,516 bytes. After adding GATT discovery, the default build succeeded with application 1,280,319 bytes and static RAM 88,652 bytes. After notification logging and reconnect support, the default build succeeded with application 1,282,731 bytes and static RAM 89,068 bytes. The current simulation build is being checked separately. These figures are not measurements of runtime heap or concurrent BLE use. To enable the explicitly labelled simulated fixture, add `--build-property compiler.cpp.extra_flags=-DMOTONAV_BMS_SIMULATION=1` to the Arduino CLI build command. Never use simulation to assess a real pack.

## Architecture findings and remaining work

The 2.16 sketch uses CO5300, CST92xx touch, a phone BLE GATT server and HID, with an Apple Media client operating on the phone connection. `AppleMediaClient::begin()` installs a custom GAP handler. A BMS client must share or dispatch that handler without stealing phone notifications; connection counts, RAM use, navigation throughput and stack configuration need measurement. Page transitions and call/pairing overlays have their own redraw paths, so a new page must handle those paths as well as touch and idle power-off. The existing PMU battery display represents the device battery, not the vehicle pack.

The base branch's `ADAPT_216.md` identifies unverified panel orientation, touch coordinates, power and layout assumptions. `firmware/build.py` currently builds only 175; `firmware/build.ps1 -Board 216` is the documented 216 build path. The cloud Arduino CLI requires `network.proxy` configured to the environment proxy; the ESP32 3.3.10 core was installed for compilation.

Remaining: validate device selection, discovery, notification subscription and reconnection on hardware; measure coexistence and runtime memory; a verified, read-only protocol decoder; integrated BLE simulation and UI validation; 2.16 compile and hardware regression testing. No BMS UUIDs or data formats have been inferred, and no BMS commands have been implemented. Only the Bluetooth-standard CCCD UUID is used for the explicitly requested subscription.

## Future real-device verification

First verify navigation, media, touch and display on the 2.16 board independently. With the user's BMS nearby, capture advertisements and discovered services/characteristics and raw notification bytes using an explicitly selected device. Compare multiple captures with readings from the official app, including changed readings, to identify and validate the format; do not transmit unidentified commands or change protection, activation or MOS settings. Then validate simultaneous phone navigation/music and BMS connections, disconnect/reconnect, stale data, heap and long-running behavior. Do not flash automatically.

## Preview

`docs/bms/battery-preview.png` is a 480×480 software reconstruction using the firmware bitmap font, positions and simulation fixture. It is not a device screenshot.

## Bench and protocol-identification checklist

1. Run `for t in firmware/tests/*Test.cpp; do g++ -std=c++17 "$t" -o /tmp/motonav-test && /tmp/motonav-test || exit 1; done`, then the idle timer and flash tool tests. Build board 216 with the locked ESP32 core 3.3.10. The simulation flag changes only the visible fixture; verify the `BMS SIMULATION` label, all 24 cells, SOC, voltage and spread, and page transitions on the board.
2. With the owner identifying the target, send `bms scan` on USB serial at 115200, then `bms connect <type> <address>` using the reported type and address. Discovery prints characteristic properties and decimal value handles. After confirming a candidate, use `bms subscribe <value-handle> n` for a notify property (0x10) or `i` for indicate (0x20). This writes the Bluetooth-standard CCCD; it does not send any unidentified BMS request. Use `bms disconnect` to stop retries and disconnect. Random addresses may change, so a saved session address can cease to work.
3. Capture serial logs while comparing changes with the official app. Logs can contain truncated or dropped packets and are not evidence that a frame is complete; preserve timestamps externally. Determine framing, checksum, scale, units and current sign across multiple known states before implementing a decoder. The existing parser rejects every packet. Do not use this version for pack protection decisions.
4. Exercise phone navigation, media, touch, pairing, screen overlays, BLE disconnect/reconnect, data expiration, heap and throughput on real hardware. Connection diagnostics and CCCD subscription are not hardware tested. The pre-existing idle power-off policy can shut the screen down after five minutes without navigation. Automatic retries stop if the control event queue overflows; if a previous GATT callback never completes, further connection attempts remain blocked until restart rather than reusing stale handles.

Linux build from the repository root (no flashing):

```sh
arduino-cli compile --jobs 4 --libraries firmware/libraries \
  --build-path firmware/.build/216/build \
  --fqbn 'esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi,CDCOnBoot=cdc' \
  --output-dir firmware/.build/216/output firmware/boards/216/MotoNav
```

For the simulation build, use a separate build/output directory and add `--build-property 'compiler.cpp.extra_flags=-DMOTONAV_BMS_SIMULATION=1'`. A build result does not establish display geometry, radio coexistence, actual BMS behavior or runtime heap headroom.
