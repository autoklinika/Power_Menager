# DFR0473 × 3 — instrukcja podłączenia do KAmod ESP32 POW+RS485

**Power Manager V1; status ENGINEERING DRAFT. Bez zatwierdzonego Gate 1 nie podłączać rzeczywistego ECU.** 
Dotyczy trzech modułów **DFRobot DFR0473 (Gravity: Digital 10A Relay Module)**, wspólnego pomiaru prądu/napięcia **Adafruit INA228 QT high-side** i sterownika **KAmod ESP32 POW+RS485**.

## 1. Zasilanie i sterowanie (3-pin Gravity: minus, plus, D)

Każdy DFR0473 ma wejścia:
- `−` / GND: masa logiki,
- `+` / VCC: zasilanie logiki **3,3 V** z KAmod,
- `D`: wejście sterujące; HIGH ≥ 2,8 V = **przekaźnik ON**, LOW ≤ 0,5 V = **OFF**.

DFRobot deklaruje zakres zasilania modułu **2,8–5,5 V**. Dlatego nie podajemy na `+` napięcia szyny DUT 12/24 V i **nie zasilamy przekaźników z GPIO**. GPIO steruje wyłącznie `D`.

| DFR0473 | `D` (sygnał) | `+` (3V3) | `−` (GND) |
|---|---|---|---|
| K1 / OUT1 | ESP32 GPIO16 → **KAmod J1 fizyczny pin 26** | KAmod J1 **pin 17** (3,3 V) | KAmod J1 **pin 25** (GND) |
| K2 / OUT2 | ESP32 GPIO17 → **KAmod J1 fizyczny pin 11** | KAmod J1 **pin 17** (3,3 V) | KAmod J1 **pin 25** (GND) |
| K3 / OUT3 | ESP32 GPIO18 → **KAmod J1 fizyczny pin 33** | KAmod J1 **pin 17** (3,3 V) | KAmod J1 **pin 34** (GND) |

Zasilanie 3V3 i GND można rozdzielić równolegle na wszystkie trzy moduły, ale **przed pierwszym uruchomieniem sprawdź pobór prądu każdej cewki i łączne obciążenie 3,3 V**, wraz z ESP32 i INA228. Regulator 3,3 V KAmod ma znamionową wydajność ciągłą 1 A, co nie jest równoznaczne z 1 A wolnym dla akcesoriów.

Do **każdej** linii `D` należy dodać **rezystor pull-down 10 kΩ do GND**, możliwie blisko wejścia przekaźnika:
```text
J1 pin 26 = GPIO16 ─── D(K1) ─┐
                              └──[10 kΩ]── GND
J1 pin 11 = GPIO17 ─── D(K2) ─┐
                              └──[10 kΩ]── GND
J1 pin 33 = GPIO18 ─── D(K3) ─┐
                              └──[10 kΩ]── GND
```
Rezystory ograniczają przypadkowe załączenie przy stanie wysokiej impedancji, **nie zastępują fizycznej blokady wyjść i testów reset/brownout/flash**.

**Nie myl fizycznego numeru pinu J1 z numerem GPIO ESP32.** KAmod J1 jest złączem 40-pin RPi ze *specyficznym* przyporządkowaniem GPIO ESP32; nie stosować ogólnego pinoutu GPIO Raspberry Pi jako nazewnictwa sygnałów ESP32.

## 2. Styki obciążenia (niezależne elektrycznie od GPIO)

Na module DFR0473 użyć tylko **COM** i **NO**:
- COM = wejście plusowego napięcia zasilania DUT z wspólnej szyny **za INA228** i bezpiecznikiem danej gałęzi,
- NO = wyjście plusowego napięcia po załączeniu przekaźnika, do konkretnego pinu zasilania DUT,
- NC (normalnie zamknięty) = **nie używać**,
- jeżeli występuje dodatkowe puste zaciskowe pole, pozostawić je niepodłączone i kierować się nadrukiem PCB, nie pozycją zacisku.

```text
                  PSU_DUT + (12/24 V DC)
                            |
                      [F_MAIN]  bezpiecznik główny
                            |
                        INA228 VIN+
                            |
                  [bocznik 15 mΩ]
                            |
                        INA228 VIN−
                            |
           +----------------+----------------+
           |                |                |
          [F1]             [F2]             [F3]   indywidualne bezpieczniki
           |                |                |
         COM K1           COM K2           COM K3
         NO  K1           NO  K2           NO  K3
           |                |                |
        OUT1 (+)         OUT2 (+)         OUT3 (+)
           |                |                |
        DUT KL30       DUT KL15      DUT wake/aux (*) 

            PSU_DUT - -------------- DUT GND
```

(*) KL30, KL15 i wake to przykłady logicznego profilu DUT; nie jest to zatwierdzony pinout DAF SAC ani innych ECU. Jeżeli wejście wake nie jest wejściem napięciowym 12/24 V, **nie** podłączaj go do takiego wyjścia.

Przy tym połączeniu INA228 mierzy **sumaryczny prąd** OUT1+OUT2+OUT3 oraz napięcie wspólnej szyny, **nie** fizyczne napięcie czy prąd każdego z kanałów.

## 3. Dwa napięcia — absolutnie ich nie mieszać

**Sterowanie (niska moc):** VCC DFR0473 = 3,3 V KAmod, masa DFR = GND KAmod, sygnał D z GPIO.

**Styki przełączające (DUT):** do COM podajemy DC DUT **za INA228 i bezpiecznikiem**, do NO odchodzi indywidualne `OUT(+)`. Przez J1/GPIO nie płynie prąd DUT, nie podajemy 12/24 V na GPIO ani złącze 3,3 V.

Do zasilania KAmod służy osobne wejście J3 (8–32 V DC), a nie wyjście przekaźnika. KAmod musi pracować również wtedy, gdy OUT1..OUT3 są wyłączone.

## 4. Parametry i warunki bezpieczeństwa

- DFR0473: **10 A przy 28 V DC (znamionowe obciążenie)**, maksymalne napięcie przełączania DC **30 V**, czas załączenia ≤10 ms, zwolnienia ≤5 ms. Indukcyjność, prądy rozruchowe, przepięcia, złącza i przewody redukują realne dopuszczalne obciążenie.
- Firmware Power Manager V1 ma w tej chwili **limit programowy 28 V i 2000 mA sumarycznie**, nie sprzętowy bezpiecznik. **Nie** używać tego odczytu jako zabezpieczenia zwarcia.
- Bezpieczniki F_MAIN i F1–F3 dobrać do rzeczywistych obwodów, PSU i przekrojów przewodów. Zastosować zewnętrzne szybkie ograniczenie prądu. Przed przełączaniem indukcyjnych EGR/VGT niezbędna jest analiza i tłumienie przepięć.
- Kontroler ma domyślnie `CONFIG_PM_ENABLE_PHYSICAL_OUTPUTS=n`: podłączone moduły **nie powinny fizycznie załączać** aż do formalnej weryfikacji i świadomej zmiany ustawienia firmware.
- Dla podłączonej ECU Platform komunikacja RS485 nie zapewnia automatycznej separacji galwanicznej. Masę i separację należy zaprojektować w całym stanowisku.

## 5. Kolejność uruchomienia bez DUT

1. **Bez zasilania** wykonaj tylko przewody 3,3 V/GND/D, dodaj 3×10 kΩ pull-down, sprawdź polaryzację i miernikiem brak zwarć.
2. Zasil KAmod, przy **odłączonej szynie 12/24 V DUT** sprawdź napięcie 3,3 V pod obciążeniem trzech modułów, pobór prądu i brak samoczynnego załączenia.
3. Po restarcie, programowaniu USB i utracie zasilania sprawdź omomierzem: w stanie OFF styki **COM–NO muszą być rozwarte**, a COM–NC zwarte. W tym teście nie podawaj napięcia DUT.
4. Dołącz zasilacz DUT **z ograniczeniem prądowym**, bezpieczniki oraz niewielkie **obciążenie rezystancyjne**, a nie naprawiane ECU. Sprawdź sumaryczne pomiary INA228.
5. Dopiero po zakończeniu Gate 1 / zmianie flagi firmware testuj kolejno K1/K2/K3 i stany fail-safe (RS485 timeout, I²C fault, brownout); do tego momentu relays pozostają OFF.

## Źródła producentów

- DFRobot DFR0473, parametry i pinout: https://wiki.dfrobot.com/dfr0473/
- DFRobot aktywacja wejścia HIGH, przykład: https://wiki.dfrobot.com/dfr0473/docs/19017
- KAmod ESP32 POW+RS485 i tabela J1: https://wiki.kamamilabs.com/index.php?title=KAmod_ESP32_POW_RS485_(PL)
- Adafruit INA228: https://learn.adafruit.com/adafruit-ina228-i2c-power-monitor
