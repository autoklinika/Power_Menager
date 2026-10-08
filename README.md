# Power Manager V1

Samodzielny kontroler wykonawczy i telemetrii DUT dla **ECU Platform V2**. Implementacja niezależna od ECU Platform/CORE.

**Status:** fundament firmware ESP-IDF, niezatwierdzony do podłączania naprawianych ECU. Domyślnie fizyczne wyjścia są **zablokowane podczas kompilacji**.

## Hardware i funkcje

- KAmod ESP32 POW+RS485, RS485/Modbus RTU slave address 1, 115200 8N1.
- 3 × DFRobot DFR0473, trzy sterowane styki NO.
- 1 × Adafruit INA228 QT (15 mΩ), **jeden wspólny pomiar** napięcia źródła i sumarycznego prądu trzech obwodów przechodzących przez bocznik.
- Fail-closed: safe OFF on boot, stop after 1500 ms without keepalive, sensor failure, stale telemetry or software limit. Fault latches require explicit clearing and rearming.
- Konserwatywne limity programowe: 28 V i 2 A (do czasu zatwierdzenia elektrycznego).

## Dokumenty

- [Schemat funkcjonalny i połączenia](docs/HARDWARE_V1_PL.md)
- [Podłączenie INA228 QT — krok po kroku, J1 piny 1/3/5/6](docs/INA228_WIRING_PL.md)
- [Podłączenie 3 przekaźników DFR0473 — dokładne J1 i COM/NO](docs/DFR0473_WIRING_PL.md)
- [Kontrakt Modbus RTU V1](docs/MODBUS_RTU_V1_PL.md)
- [Plan testów / bramki bezpieczeństwa](docs/BRAMKI_TESTOW_V1_PL.md)

## Układ kodu

- `core/` — zależności tylko od standardowego C++17 (model stanu, protokół, pomiar math).
- `main/` — adapter sprzętowy ESP-IDF dla UART2 RS485, GPIO16/17/18, I²C INA228.
- `tests/` — testy hostowe Modbus, fail-closed, timeouts, INA228 scaling.
- `.github/workflows/` — testy hostowe i build ESP-IDF.

## Host tests (Linux/macOS, CMake + C++17)

```bash
cmake -S tests -B build/host -DCMAKE_BUILD_TYPE=Debug
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

## ESP-IDF firmware (target: esp32; ESP-IDF v5.4.1)

```bash
idf.py set-target esp32
idf.py build
# To jest na tym etapie kompilacja, NIE flash na urządzeniu podłączonym do ECU.
```

Konfiguracja fabryczna `CONFIG_PM_ENABLE_PHYSICAL_OUTPUTS=n` uniemożliwia ARM i fizyczne załączenie przekaźników. Aktywacja wymaga pomyślnych testów Gate 1 i decyzji o zwolnieniu sprzętu do eksploatacji, dopiero potem `idf.py menuconfig` (Power Manager — safety). Zmienianie flagi bez walidacji obwodów zasilania może spowodować uszkodzenie DUT.

## Granice integracji

ECU Platform pozostaje masterem. Komunikacja przechodzi przez backend/Bench Runtime oraz DUT Profile; **WebGUI pozostaje klientem**. Power Manager nie wymaga zmian w multiplatformowym CORE ECU Platform V2.

Repo ma własny cykl życia, testy i review. **Bez zgody właściciela nie scalamy PR do `main`.**
