# Power Manager V1 — połączenia elektryczne (projekt roboczy)

**Status: ENGINEERING DRAFT — nie podłączać ECU przed sprawdzeniem obwodów.**

## Zakres zatwierdzony
- Sterownik: KAmod ESP32 POW+RS485 (ESP32-WROOM).
- Wyjścia: 3 x DFRobot DFR0473 (przekaźniki aktywowane poziomem HIGH).
- Pomiar: 1 x Adafruit INA228 QT, **jeden wspólny pomiar** szyny zasilającej DUT.
- Komunikacja ECU Platform: Modbus RTU przez fizyczne RS485, ECU Platform = master, Power Manager = slave.
- Trzy wyjścia przełączają **wyłącznie dodatni potencjał DC** z jednej wspólnej szyny po boczniku. Ich znaczenie określa DUT Profile, nie firmware.

## Schemat funkcjonalny: DOSTAWA napięcia DC, nie schemat PCB/KiCad

```text
  PSU_DUT (+)    [F_MAIN]          INA228 QT (15 mOhm, high-side)
      o ----------[fuse]------- VIN+ o----[SHUNT]----o VIN-
                                  |                   |
                             VBUS jumper             +----[F1]---- COM K1
                             VIN+ -> VBUS            |               NO ---- OUT1 (+)
                                                     +----[F2]---- COM K2
                                                     |               NO ---- OUT2 (+)
                                                     +----[F3]---- COM K3
                                                                     NO ---- OUT3 (+)

  PSU_DUT (-) ------------------------------------------------------------- DUT GND
                    |
                    +---- INA228 GND and logic-reference GND

  KAmod J3: independent 8..32 V DC supply (not routed through K1/K2/K3).
  KAmod GND, INA228 GND and DUT negative need a defined common reference.
  ECU Platform RS485 A/B via J3; shared reference/ground path must be engineered.
```

**Uwaga:** przy takim high-side INA228 mierzy napięcie **wspólnej szyny zasilania przed przekaźnikami**, nie indywidualne napięcie na wyjściach OUT1–OUT3. Pomiar prądu obejmuje sumę prądów pobieranych ze wspólnej szyny po boczniku, również gdy jednocześnie aktywne są różne kanały. To nie zapewnia potwierdzenia zamknięcia styków ani zasilania konkretnych pinów DUT.

**WAŻNE: fabryczny Adafruit INA228 QT skonfigurowany jest domyślnie do low-side.** Dla zaprojektowanego high-side należy wykonać połączenie VBUS–VIN+ zworką lutowniczą zgodnie z dokumentacją Adafruit. Przed włączeniem zweryfikować multimetrem topologię własnego egzemplarza.

## Połączenia niskonapięciowe

Szczegółowa tabela fizycznych pinów J1, połączenia styków COM/NO oraz procedura testowa: [DFR0473_WIRING_PL.md](DFR0473_WIRING_PL.md).


| KAmod ESP32 POW+RS485 | Element | Podłączenie |
|---|---|---|
| GPIO16, J1 fizyczny pin 26 | DFR0473 #1 | D (IN) |
| GPIO17, J1 fizyczny pin 11 | DFR0473 #2 | D (IN) |
| GPIO18, J1 fizyczny pin 33 | DFR0473 #3 | D (IN) |
| 3V3, J1 fizyczny pin 17 | 3 x DFR0473 | `+` / VCC 3,3 V (potwierdzić pobór prądu wszystkich modułów, regulator KAmod 1 A ciągle) |
| GND, J1 fizyczny pin 25 lub 34 | 3 x DFR0473 | `−` / GND |
| GPIO33 | INA228 | SDA |
| GPIO32 | INA228 | SCL |
| 3V3 | INA228 | VIN / logic power |
| GND | INA228 | GND |
| J3 styk 1 | Zasilanie KAmod | GND |
| J3 styk 2 | Zasilanie KAmod | DC +8..32 V |
| J3 styk 3 | RS485 | B− |
| J3 styk 4 | RS485 | A+ |

GPIO34/ALERT nie jest używany przez firmware V1. Pomiar i odcięcie programowe działają w trybie okresowego odpytywania I²C.

**Na GPIO16, GPIO17 i GPIO18 wymagany jest fizyczny rezystor 10 kΩ do GND przy module sterującym**: po resecie/bootloaderze ESP32 styki NO mają pozostać otwarte. Nie korzystać z NC. Zweryfikować miernikiem poziom logiczny, prąd sterowania i stan przekaźników podczas resetu, brownout, programowania USB i zaniku zasilania KAmod. Zabezpieczenie tylko firmware jest niewystarczające. Task Watchdog ESP-IDF ma limit 5 s i wymusza restart; nie zastępuje sprzętowego watchdog/latch lub szybkiego wyłącznika prądowego.

## Obwody mocy i zabezpieczenia

- Przekaźnik DFR0473: znamionowo 10 A @ 28 V DC dla właściwego obciążenia; dopuszczalne przełączanie maks. 30 V DC. **Nie** interpretować jako bezpieczne 10 A przy dowolnym typie obciążenia, udarze ani indukcyjności.
- Adafruit INA228 QT: rezystor 15 mΩ; zakres do 10 A, jednak termika i przewody/złącza muszą być zweryfikowane; przy 10 A na boczniku wydziela się 1,5 W.
- Wersja firmware ustawia **konserwatywny limit 2 A sumarycznie oraz 28 V**. Są to limity software, **nie zabezpieczenie zwarciowe**. Ich zmiana wymaga osobnego przeglądu i testu.
- Bezpiecznik główny przed czujnikiem, niezależne bezpieczniki F1–F3 na gałęziach, odpowiednie przekroje przewodów i złącza. Dobór wartości wymaga charakterystyki DUT, źródła zasilania i okablowania.
- Zalecane ograniczenie prądu / elektroniczne odcięcie o reakcji szybszej niż mechaniczne przekaźniki. Relays+I²C polling+software NIE zapewniają szybkiego wyłączenia zwarcia.
- Przy obciążeniach indukcyjnych dodać odpowiednio dobrane tłumienie przepięć po analizie energetycznej. Nie przełączać bez takiej analizy cewki/solenoidu EGR/VGT.
- Linie RS485 wymagają wspólnego potencjału odniesienia, poprawnej topologii magistrali i terminacji 120 Ω na **obu końcach**, nie na każdym węźle.
- KAmod i RS485 nie tworzą automatycznie separacji galwanicznej. Gdy DUT i ECU Platform mają różne masy lub PSU, separację zaprojektować przed połączeniem.

## Złącza obudowy — do zatwierdzenia przed wersją wykonawczą

W projekcie obudowy przewidziano DB25, DB9 oraz DFK-MSTB 2.5/2-GF-5.08.
**Nie przypisujemy ich styków do wyjść / zasilania bez wspólnego pinoutu i schematu złącz.**
DB25/DB9 nie powinny być traktowane jako dowolne złącza dużoprądowe; wymagane sprawdzenie prądu i napięcia znamionowego konkretnej wkładki i każdego styku.
Nie łączyć obwodów mocy z pinami logicznymi ECU Platform.

## Materiały źródłowe

- KAmod: https://wiki.kamamilabs.com/index.php?title=KAmod_ESP32_POW_RS485_(PL)
- DFR0473: https://wiki.dfrobot.com/dfr0473/
- Adafruit INA228: https://learn.adafruit.com/adafruit-ina228-i2c-power-monitor
- ESP-IDF RS485: https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/peripherals/uart.html

## Otwarte bramki (nie zgadywać)
- Zmapować fizyczne piny J1 na egzemplarzu KAmod i sprawdzić, czy GPIO16..18 są wyprowadzone.
- Sprawdzić sumaryczny pobór trzech DFR0473 z 3V3.
- Zatwierdzić limity PSU, fuses, przekroje i mechaniczny pinout DB25/DB9/DFK.
- Zweryfikować zachowanie styków NO przy zaniku zasilania i szybkość sprzętowego odcięcia.
- Przeprowadzić test na rezystorach obciążenia, NIE na naprawianym ECU.
