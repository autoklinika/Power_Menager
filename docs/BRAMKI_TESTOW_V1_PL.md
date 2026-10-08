# Power Manager V1 — bramki weryfikacyjne

Status: **niezatwierdzony do pracy na rzeczywistym ECU**.

## Gate 0: software / host

- [x] Podział: czysty C++ policy + Modbus parser + HAL ESP-IDF.
- [x] Blokada fabryczna `CONFIG_PM_ENABLE_PHYSICAL_OUTPUTS=n`.
- [x] Kontrola ramki CRC, adresu, dopuszczalnych FC, zakresów i atomowego zapisu.
- [x] Challenge `boot_id` + monotoniczna sekwencja (ochrona przed przypadkowym replay, nie autoryzacja).
- [x] Latching faults / watchdog 1500 ms / sensor freshness 300 ms w logice.
- [ ] CMake host tests PASS w GitHub Actions.
- [ ] Kompilacja ESP-IDF z docelową wersją toolchain PASS.
- [ ] Static analysis i code review PASS.

## Gate 1: bench bez DUT

- [ ] Sprawdź połączenia wg hardware netlist, brak zwarć i poprawne odniesienie GND.
- [ ] Dodatkowe fizyczne pull-down 10 kΩ na trzech GPIO.
- [ ] Zbadaj obciążenie regulatora 3V3 przy 3 załączonych modułach DFR0473.
- [ ] Zbadaj styki NO podczas boot/reset/USB flash/brownout/cold start.
- [ ] Zweryfikuj adres I²C 0x40, ID 0x5449/0x228x i zworkę INA228 high-side.
- [ ] Porównaj z miernikiem VBUS oraz sumaryczny prąd dla obciążeń rezystancyjnych.
- [ ] Zmierz fizyczne zmiany GPIO/cewek dopiero po `PM_ENABLE_PHYSICAL_OUTPUTS=y`.
- [ ] Rozłącz RS485 podczas aktywnej sesji: oczekiwane OFF + FAULT.
- [ ] Wymuś awarię INA228: oczekiwane OFF + SENSOR_FAULT.
- [ ] Wymuś ponad 2 A przy ograniczonym źródle: OFF (test nie służy do charakteryzacji zwarcia).
- [ ] Wymuś > 28 V bez DUT i przy bezpiecznych warunkach: OVER_VOLTAGE.
- [ ] Sprawdź każdą ramkę CRC/uszkodzony adres/niepoprawną sekwencję.
- [ ] Sprawdź fizyczny wyłącznik awaryjny i skuteczność niezależnego ograniczenia prądu.
- [ ] Sprawdź termikę bocznika, styków, bezpieczników, obudowy, przewodów i złącz.

## Gate 2: integracja z ECU Platform

- [ ] Bench Runtime jako jedyny master; WebGUI wyłącznie klientem.
- [ ] Profil DUT zarządza kolejnością załączania (np. supply, ignition, wake).
- [ ] GUI pokazuje pomiar wspólny, nie trzy fałszywe prądy.
- [ ] Timeout i reconnect wymagają jawnej reaktywacji, brak samoczynnego wznowienia.
- [ ] Testy integracyjne z testowym obciążeniem.
- [ ] Approval do podłączenia prawdziwego ECU i dopiero potem dołączenie DUT.

**Nie scalać do `main` bez zgody właściciela.** Bez Gate 1 i Gate 2 firmware nie jest bezpieczną jednostką wykonawczą.
