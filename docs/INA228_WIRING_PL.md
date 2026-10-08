# Podłączenie INA228 QT do KAmod i 3 przekaźników — Power Manager V1

**Status: instrukcja okablowania laboratoryjnego — nie podłączać rzeczywistego DUT bez przejścia Gate 1.**

Zatwierdzono **jeden wspólny pomiar** napięcia zasilającego DUT i sumarycznego prądu trzech gałęzi `OUT1..OUT3`. Czujnik INA228 jest włączony **high-side**, w dodatnim przewodzie zasilacza DUT **przed** rozdzieleniem na trzy przekaźniki.

## 1. Przewody cyfrowe KAmod J1 (40-pin, standard RPi)

| INA228 QT, nadruk na module | KAmod J1, fizyczny numer pinu | ESP32 | Funkcja |
|---|---|---|---|
| **VIN** (bez plusa; zasilanie logiki) | **1** — 3.3 V | — | Zasilanie czujnika 3.3 V |
| GND | **6** — GND | — | Masa odniesienia |
| SDA | **3** | GPIO33 | I²C data |
| SCL | **5** | GPIO32 | I²C clock |
| ALRT | nie podłączać | — | V1 nie wykorzystuje IRQ |

**Krytyczne rozróżnienie:** `VIN` jest wejściem 3.3 V zasilającym logikę sensora; `VIN+` jest zaciskiem mierzonego **plusowego przewodu zasilania DUT** (np. 12/24 V DC). **Nie łącz `VIN` logicznego z `VIN+`.** Nie podawaj 12/24 V na złącze KAmod J1 3.3 V, GPIO, ani zasilanie logiki INA228.

Zasilanie KAmod podaj niezależnie na złącze J3 zgodnie z dokumentacją producenta (wejście DC 8–32 V, J3/1 GND, J3/2 plus). Jeżeli źródło KAmod jest to samo co DUT, rozgałęzienie zasilania KAmod wykonaj **przed** bocznikiem INA228, aby nie doliczać poboru samego sterownika.

## 2. Tory zasilania DUT — high-side

```text
          PSU_DUT + (np. 12/24 V DC)
                      │
                 [F_MAIN]  (dobrany bezpiecznik)
                      │
         Adafruit INA228: VIN+  ──[R015 15 mΩ]── VIN−
                      │                         │
                 [zworka VBUS ↔ VIN+]           │
                                                ├──[F1]── COM K1 ── NO K1 ── OUT1 (+)
                                                ├──[F2]── COM K2 ── NO K2 ── OUT2 (+)
                                                └──[F3]── COM K3 ── NO K3 ── OUT3 (+)

          PSU_DUT − ─────────────────────────────────────────────── DUT GND (−)
                            │
                            └── INA GND / KAmod GND*
```

*Masę referencyjną i brak separacji galwanicznej magistrali RS485 trzeba zweryfikować w docelowym układzie z ECU Platform. Przy odrębnych zasilaczach nie łączyć ich „w ciemno”.

Połączenia wykonawcze:
1. Plus zasilacza DUT, **za** bezpiecznikiem głównym, → **VIN+** INA228 (wejście bocznika).
2. **VIN−** INA228 (wyjście bocznika) → trzy osobno zabezpieczone obwody na **COM** K1/K2/K3.
3. Styki **NO** trzech przekaźników → odpowiednio OUT1/OUT2/OUT3 do wejść zasilania DUT; **NC nie używać**.
4. Minus zasilacza DUT → GND DUT, **nie** do **VIN−** INA228 w tym układzie high-side.
5. INA228 **GND** → masa zasilania/układu logicznego KAmod, ze sprawdzonym punktem referencyjnym DUT GND.

## 3. Zworka VBUS na **spodniej stronie** INA228

Adafruit dostarcza INA228 QT z **otwartą** zworką `VBUS`, przeznaczoną dla low-side. Dla naszego high-side **zlutuj pola opisane VBUS**, aby połączyć wejście napięciowe VBUS z zaciskiem **VIN+**. Alternatywnie można połączyć `VBUS` z `VIN+` przewodem, ale w obudowie zalecamy zworkę lutowniczą.

Dokumentacja producenta: https://learn.adafruit.com/adafruit-ina228-i2c-power-monitor/pinouts

Na PCB są **dwa różne napisy**: `VIN` (power 3.3 V) oraz `VIN+` (plus szyny DUT). Nie mylić.

## 4. Co dokładnie mierzymy?

- **Napięcie VBUS** — na wejściu czujnika, od strony źródła, czyli **przed bocznikiem i przed przekaźnikami**.
- **Prąd CURRENT** — algebraiczna suma prądów gałęzi, które naprawdę przepływają przez rezystor R015. Przepływ do OUT1/OUT2/OUT3 powoduje dodatni odczyt, gdy prąd płynie z VIN+ do VIN−.
- **Nie** mierzymy osobnego prądu ani napięcia na wyjściu każdej gałęzi, ani nie potwierdzamy rzeczywistego stanu styków.

Przekaźniki i złącza mają niższe limity niż sam układ INA228 (czujnik do 85 V nie oznacza, że można tyle przyłożyć do DFR0473). Firmware V1 ma limity programowe 28 V i **2 A łącznie**, lecz mechaniczne przekaźniki i odczyt I²C nie zastępują zabezpieczenia zwarciowego.

## 5. Weryfikacja przed pierwszym włączeniem

- [ ] Brak podłączonego ECU/DUT, tylko bezpieczne obciążenie rezystancyjne oraz zasilacz z ograniczeniem prądu.
- [ ] Połączenia `VIN+`, `VIN−` i `VBUS` zgodne z schematem; brak pomylenia `VIN` 3.3 V i `VIN+` szyny DUT.
- [ ] Zwarta zworka VBUS–VIN+ sprawdzona omomierzem **przy odłączonym zasilaniu**.
- [ ] SDA/SCL i 3.3 V/GND na właściwych stykach J1; masa DUT zdefiniowana i sprawdzona.
- [ ] Bezpiecznik główny i każdy bezpiecznik kanałowy, osobny obwód szybkiego ograniczenia prądu.
- [ ] Wszystkie przekaźniki pozostają OFF po starcie KAmod; firmware w domyślnym trybie `CONFIG_PM_ENABLE_PHYSICAL_OUTPUTS=n`.
- [ ] Odczyt INA228 pod `0x40` po I²C, stabilne napięcie zgodne z multimetrem, następnie małe obciążenie i weryfikacja kierunku/kalibracji prądu.
- [ ] Bez podłączania rzeczywistego ECU przetestować fault i timeout; dopiero potem fizyczne załączanie przekaźników po Gate 1.

## Dokumentacja

- KAmod i pinout J1: https://wiki.kamamilabs.com/index.php?title=KAmod_ESP32_POW_RS485_(PL)
- Adafruit INA228, pinout i zworka: https://learn.adafruit.com/adafruit-ina228-i2c-power-monitor/pinouts
- Ogólny schemat zasilania: [HARDWARE_V1_PL](HARDWARE_V1_PL.md)
