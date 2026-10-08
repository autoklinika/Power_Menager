# Power Manager V1 — Modbus RTU over RS485

## Warstwa łącza

- RS485 half-duplex, 2-przewodowe A+/B−, 115200 baud, 8N1, slave address 1.
- Brak Wi-Fi, BT, TCP/IP i telemetrii push. Master ECU Platform odczytuje holding registers.
- Supported function codes: `0x03` (Read Holding Registers) and `0x10` (Write Multiple Registers).
- RTU CRC16/Modbus (polynomial 0xA001, seed 0xFFFF, low byte first).
- 0-based addresses. W klientach posługujących się numeracją `4xxxx`, holding `0x0000` bywa pokazywany jako 40001.
- Slave nigdy sam nie inicjuje transmisji.
- CRC/address error: no response. Invalid address or value: Modbus exception.
- Brak uwierzytelnienia: `boot_id` ma odrzucać stary ruch po resecie, NIE chroni przed złośliwym masterem mającym dostęp do RS485.

## Rejestry telemetryczne FC03 (0x0000–0x0013, tylko odczyt)

| Offset hex | Wartość | Jednostka |
|---|---|---|
| 0000 | PROTOCOL_VERSION = 0x0100 | major.minor |
| 0001 | STATE (0 SAFE_OFF, 1 ARMED, 2 FAULT) | enum |
| 0002 | OUTPUT_MASK bits 0..2 | 0=OFF, 1=ON |
| 0003 | FAULT_FLAGS | bitmapa |
| 0004,0005 | VBUS (uint32 MSW,LSW) | mV |
| 0006,0007 | CURRENT (int32 MSW,LSW) | mA |
| 0008,0009 | POWER (int32 MSW,LSW) | mW |
| 000A | SAMPLE_AGE_MS, 0xFFFF if invalid | ms |
| 000B,000C | UPTIME (uint32 MSW,LSW) | seconds |
| 000D | FLAGS: bit0 sample valid, bit1 output mode physically enabled, bit2 sample fresh | bitset |
| 000E | LAST_ACCEPTED_SEQUENCE | modulo 65536 |
| 000F | CURRENT_LIMIT = 2000 | mA |
| 0010,0011 | BOOT_ID (uint32 MSW,LSW) | freshness marker |
| 0012 | VOLTAGE_LIMIT = 28000 | mV |
| 0013 | LEASE_REMAINING | ms |

`VBUS` jest wspólnym napięciem przed przekaźnikami; `CURRENT` jest sumą prądów wszystkich trzech gałęzi zasilanych **przez bocznik**. Nie są to trzy oddzielne pomiary. Dane są próbkowane około co 25 ms; odczyt nie powinien być traktowany jako oscyloskop ani zabezpieczenie przed zwarciem.

### FAULT_FLAGS

| Bit | Znaczenie |
|---|---|
| 0 | SENSOR_FAULT (I2C / ID / odczyt) |
| 1 | OVER_VOLTAGE |
| 2 | OVER_CURRENT |
| 3 | COMM_TIMEOUT |
| 4 | SENSOR_STALE |

FAULT jest zatrzaskiwany. Po ustaniu przyczyny należy jawnie wykonać `CLEAR_FAULT` i osobno `ARM`.

## Jedyna operacja zapisu FC10

```
start address = 0x0100, quantity = 5 (dokładnie)
values:
  [0] boot_id_hi
  [1] boot_id_lo
  [2] sequence
  [3] opcode
  [4] output_mask
```

Wszystkie polecenia są zapisywane **atomowo jako pięć rejestrów**. Opcodes:

| Opcode | Nazwa | Warunek |
|---|---|---|
| 0 | EMERGENCY_OFF | Mask=0; działa niezależnie od tokenu i sekwencji; zachowuje istniejący FAULT |
| 1 | ARM | SAFE_OFF, brak fault, świeży pomiar, sprzętowe wyjścia odblokowane, mask=0 |
| 2 | SET_OUTPUTS | stan ARMED, mask 0..7; ustawia wszystkie wyjścia atomowo |
| 3 | KEEPALIVE | ARMED, mask=0 |
| 4 | CLEAR_FAULT | FAULT, przyczyna ustąpiła, mask=0; powrót SAFE_OFF |
| 5 | DISARM | wyłącza wszystko; zachowuje istniejący FAULT, mask=0 |

Dla operacji poza EMERGENCY_OFF: `boot_id` musi zgadzać się z bieżącym (0x0010–0011), a `sequence` musi być **dokładnie o jeden większe** od LAST_ACCEPTED_SEQUENCE (z zawinięciem modulo 65536). Sekwencja jest aktualizowana tylko po zaakceptowanym poleceniu.

Zalecana kolejność aplikacji:
1. READ status + boot_id.
2. Sprawdź wersję protokołu i `FLAGS`; zweryfikuj parametry DUT Profile.
3. Przy braku błędów wykonaj ARM, a potem SET_OUTPUTS.
4. KEEPALIVE co 250–500 ms, cykliczny FC03 dla napięcia/prądu.
5. Odliczanie lease trwa 1500 ms od ostatniego zaakceptowanego ARM/SET_OUTPUTS/KEEPALIVE.
6. Po utracie linku, przekroczeniu limitu, błędzie I²C lub starym pomiarze wyjścia wymuszają OFF i przechodzą do FAULT.
7. Przy wznowieniu nie ma automatycznego załączenia; konieczna obsługa błędu i ARM.

Master po timeout polecenia **nie** może powtarzać tego samego `SET_OUTPUTS` w nieskończoność: najpierw odczytuje stan i LAST_ACCEPTED_SEQUENCE, a następnie ustala czy poprzedni zapis został przyjęty. Ponawianie zapisu z tą samą sekwencją jest odrzucane jako replay.

## Modbus exception codes

- `0x01` ILLEGAL_FUNCTION
- `0x02` ILLEGAL_DATA_ADDRESS / wrong FC10 region/length
- `0x03` ILLEGAL_DATA_VALUE / blocked state, sequence, challenge, limits

### Procedura odtworzenia po restarcie

Po nowym `BOOT_ID` master musi anulować poprzednią sesję Bench Runtime, wyczyścić swoje założenia o stanie przekaźników, odczytać nowy status i dopiero na nowej sekwencji przeprowadzić procedurę ARM. Rola ECU Platform ogranicza się do warstwy aplikacyjnej/Bench Runtime. CORE pozostaje bez modyfikacji.

## Bezpieczeństwo

Nie przesyłać Modbus poleceń bez pośrednictwa autoryzowanego backendu. WebGUI nie ma dostępu do RS485, a `boot_id` nie jest hasłem. Izolacja elektryczna i fizyczna kontrola magistrali wymagają osobnej oceny.
