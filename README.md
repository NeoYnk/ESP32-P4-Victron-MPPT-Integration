# Test-Martin
ESP32 Stuff

## ESP32-P4 Victron MPPT Integration

Dieses Projekt verbindet einen **ESP32-P4 mit POE/Ethernet** mit **Home Assistant** und liest/schreibt Werte von einem **Victron MPPT Laderegler** über VE.Direct.

### Hardware-Setup

```
[Home Assistant] <--Ethernet/POE--> [ESP32-P4] <--USB-OTG--> [VE.Direct USB Adapter] <--VE.Direct--> [Victron MPPT]
```

**Benötigte Hardware:**
- ESP32-P4 Board mit Ethernet/POE (z.B. Waveshare ESP32-P4-ETH)
- VE.Direct zu USB Adapter Kabel (Victron ASS030530010)
- Victron MPPT Laderegler (SmartSolar, BlueSolar)

### Projektstruktur

```
esphome/
├── esp32-p4-victron-mppt.yaml       # Hauptkonfiguration (Standard)
├── esp32-p4-victron-mppt-full.yaml  # Erweitert mit Custom Component für 0x2015
├── secrets.yaml.example              # Vorlage für Secrets
├── .gitignore
└── custom_components/
    └── victron_charge_limit/         # Custom Component für Register 0x2015
        ├── __init__.py
        ├── victron_charge_limit.h
        └── victron_charge_limit.cpp
```

### Installation

1. **Secrets erstellen:**
   ```bash
   cd esphome
   cp secrets.yaml.example secrets.yaml
   # API Key generieren:
   openssl rand -base64 32
   # Füge den Key in secrets.yaml ein
   ```

2. **GPIO-Pins anpassen:**
   Passe in der YAML-Datei die GPIO-Pins für dein spezifisches ESP32-P4 Board an:
   - Ethernet: `mdc_pin`, `mdio_pin`, `power_pin`, `clk.pin`
   - UART/USB-OTG: `tx_pin`, `rx_pin`

3. **ESPHome flashen:**
   ```bash
   esphome run esp32-p4-victron-mppt-full.yaml
   ```

### Verfügbare Sensoren (Lesbar)

| Sensor | Register | Einheit | Beschreibung |
|--------|----------|---------|--------------|
| Battery Voltage | 0xED8D | V | Batteriespannung |
| Battery Current | 0xED8F | A | Batteriestrom |
| Battery Power | 0xED8E | W | Batterieleistung |
| Panel Voltage | 0xEDBB | V | PV-Spannung |
| Panel Current | 0xEDBD | A | PV-Strom |
| Panel Power | 0xEDBC | W | PV-Leistung |
| Yield Today | 0xEDD3 | kWh | Ertrag heute |
| Yield Yesterday | 0xEDD1 | kWh | Ertrag gestern |
| Total Yield | 0xEDDC | kWh | Gesamtertrag |
| Battery Temperature | 0xEDEC | °C | Batterietemperatur |
| Internal Temperature | 0xEDDB | °C | Interne Temperatur |
| State of Charge | 0x0FFF | % | Ladezustand (wenn BMS) |
| Device State | 0x0201 | - | Gerätestatus |
| MPPT Mode | 0xEDB3 | - | MPPT Tracker Modus |
| Error Code | 0xEDDA | - | Fehlercode |

### Schreibbare Werte

| Parameter | Register | Einheit | Beschreibung |
|-----------|----------|---------|--------------|
| **Charge Current Limit** | **0x2015** | A | **Dynamisches Stromlimit** |
| Max Battery Current | 0xEDF0 | A | Persistentes Max-Stromlimit |
| Absorption Voltage | 0xEDF7 | V | Absorptionsspannung |
| Float Voltage | 0xEDF6 | V | Erhaltungsspannung |
| Equalisation Voltage | 0xEDF4 | V | Ausgleichsspannung |
| Device Mode | 0x0200 | - | Ein/Aus/Nur Laden |
| Battery Type | 0xEDF1 | - | Batterietyp |
| Adaptive Mode | 0xEDFE | - | Adaptiver Modus |

### Wichtiger Hinweis zu Register 0x2015

Das Register **0x2015 (Charge Current Limit)** ist speziell für **dynamische/remote Strombegrenzung** vorgesehen:

- Verwende **0x2015** für häufige Updates (z.B. alle paar Sekunden)
- Verwende **0xEDF0** (BAT_MAX_CURRENT) nur für seltene, persistente Änderungen
- Schreiben auf 0xEDF0 bei hoher Frequenz kann den Flash-Speicher des MPPT beschädigen!

Das Custom Component `victron_charge_limit` implementiert direkten Zugriff auf Register 0x2015.

### Home Assistant Automation Beispiele

#### 1. Dynamische Ladestrom-Anpassung basierend auf Netzüberschuss

```yaml
automation:
  - alias: "Solar Ladestrom an Überschuss anpassen"
    description: "Passt den Ladestrom basierend auf verfügbarer Überschussleistung an"
    trigger:
      - platform: state
        entity_id: sensor.grid_power
    condition:
      - condition: state
        entity_id: text_sensor.victron_mppt_device_state
        state:
          - "Bulk"
          - "Absorption"
    action:
      - service: number.set_value
        target:
          entity_id: number.victron_mppt_charge_current_limit
        data:
          value: >
            {% set available = states('sensor.available_power') | float(0) %}
            {% set battery_voltage = states('sensor.victron_mppt_battery_voltage') | float(48) %}
            {% set max_current = 50 %}
            {% set calculated = (available / battery_voltage) | round(1) %}
            {{ [0, calculated, max_current] | sort | list | nth(1) }}
```

#### 2. Nachtabsenkung - Laden nur bei günstigem Stromtarif

```yaml
automation:
  - alias: "MPPT Nachtmodus aktivieren"
    description: "Reduziert Ladestrom nachts, außer bei günstigem Tarif"
    trigger:
      - platform: time
        at: "22:00:00"
    action:
      - choose:
          - conditions:
              - condition: numeric_state
                entity_id: sensor.electricity_price
                below: 0.10
            sequence:
              - service: number.set_value
                target:
                  entity_id: number.victron_mppt_charge_current_limit
                data:
                  value: 30
          - conditions: []
            sequence:
              - service: number.set_value
                target:
                  entity_id: number.victron_mppt_charge_current_limit
                data:
                  value: 5

  - alias: "MPPT Tagmodus aktivieren"
    trigger:
      - platform: time
        at: "06:00:00"
    action:
      - service: number.set_value
        target:
          entity_id: number.victron_mppt_charge_current_limit
        data:
          value: 50
```

#### 3. Temperaturbasierte Ladestrom-Begrenzung

```yaml
automation:
  - alias: "MPPT Temperaturschutz"
    description: "Reduziert Ladestrom bei hoher MPPT-Temperatur"
    trigger:
      - platform: numeric_state
        entity_id: sensor.victron_mppt_internal_temperature
        above: 55
    action:
      - service: number.set_value
        target:
          entity_id: number.victron_mppt_charge_current_limit
        data:
          value: 20
      - service: notify.mobile_app
        data:
          title: "MPPT Warnung"
          message: "Temperatur hoch ({{ states('sensor.victron_mppt_internal_temperature') }}°C) - Ladestrom reduziert"

  - alias: "MPPT Temperatur normal"
    trigger:
      - platform: numeric_state
        entity_id: sensor.victron_mppt_internal_temperature
        below: 45
    action:
      - service: number.set_value
        target:
          entity_id: number.victron_mppt_charge_current_limit
        data:
          value: 50
```

#### 4. SOC-basierte Ladesteuerung (mit BMS)

```yaml
automation:
  - alias: "MPPT SOC-basierte Ladung"
    description: "Steuert Ladestrom basierend auf Batterieladezustand"
    trigger:
      - platform: state
        entity_id: sensor.victron_mppt_state_of_charge
    action:
      - service: number.set_value
        target:
          entity_id: number.victron_mppt_charge_current_limit
        data:
          value: >
            {% set soc = states('sensor.victron_mppt_state_of_charge') | float(50) %}
            {% if soc < 20 %}
              50
            {% elif soc < 50 %}
              40
            {% elif soc < 80 %}
              30
            {% elif soc < 95 %}
              20
            {% else %}
              5
            {% endif %}
```

#### 5. Fehlerbehandlung und Benachrichtigung

```yaml
automation:
  - alias: "MPPT Fehler Benachrichtigung"
    description: "Sendet Benachrichtigung bei MPPT-Fehlern"
    trigger:
      - platform: state
        entity_id: text_sensor.victron_mppt_error_code
    condition:
      - condition: not
        conditions:
          - condition: state
            entity_id: text_sensor.victron_mppt_error_code
            state: "No error"
    action:
      - service: notify.mobile_app
        data:
          title: "MPPT Fehler!"
          message: "Fehlercode: {{ states('text_sensor.victron_mppt_error_code') }}"
          data:
            priority: high
            tag: "mppt_error"
      - service: persistent_notification.create
        data:
          title: "Victron MPPT Fehler"
          message: >
            Fehler: {{ states('text_sensor.victron_mppt_error_code') }}
            Status: {{ states('text_sensor.victron_mppt_device_state') }}
            Zeit: {{ now().strftime('%Y-%m-%d %H:%M:%S') }}
```

#### 6. Tägliche Ertragsstatistik

```yaml
automation:
  - alias: "MPPT Täglicher Ertragsbericht"
    trigger:
      - platform: time
        at: "21:00:00"
    action:
      - service: notify.mobile_app
        data:
          title: "Solar Tagesbericht"
          message: >
            Ertrag heute: {{ states('sensor.victron_mppt_yield_today') }} kWh
            Max. Leistung: {{ states('sensor.victron_mppt_max_power_today') }} W
            Gesamtertrag: {{ states('sensor.victron_mppt_total_yield') }} kWh
```

#### 7. Nulleinspeisung / Zero Export

```yaml
automation:
  - alias: "MPPT Nulleinspeisung"
    description: "Verhindert Netzeinspeisung durch dynamische Ladestrom-Anpassung"
    trigger:
      - platform: state
        entity_id: sensor.grid_power
    condition:
      - condition: numeric_state
        entity_id: sensor.grid_power
        below: -50  # Einspeisung erkannt (negative Werte = Export)
    action:
      - service: number.set_value
        target:
          entity_id: number.victron_mppt_charge_current_limit
        data:
          value: >
            {% set current_limit = states('number.victron_mppt_charge_current_limit') | float(25) %}
            {% set grid_export = states('sensor.grid_power') | float(0) | abs %}
            {% set battery_voltage = states('sensor.victron_mppt_battery_voltage') | float(48) %}
            {% set increase = (grid_export / battery_voltage) | round(1) %}
            {{ [current_limit + increase, 50] | min }}
```

#### 8. Wärmepumpen-Integration

```yaml
automation:
  - alias: "MPPT Wärmepumpe Vorrang"
    description: "Reduziert Ladestrom wenn Wärmepumpe läuft"
    trigger:
      - platform: state
        entity_id: switch.heat_pump
        to: "on"
    action:
      - service: number.set_value
        target:
          entity_id: number.victron_mppt_charge_current_limit
        data:
          value: 15

  - alias: "MPPT Wärmepumpe beendet"
    trigger:
      - platform: state
        entity_id: switch.heat_pump
        to: "off"
    action:
      - service: number.set_value
        target:
          entity_id: number.victron_mppt_charge_current_limit
        data:
          value: 50
```

#### 9. Lovelace Dashboard Card (UI)

```yaml
# In configuration.yaml oder lovelace dashboard
type: entities
title: Victron MPPT
entities:
  - entity: sensor.victron_mppt_panel_power
    name: PV Leistung
  - entity: sensor.victron_mppt_battery_voltage
    name: Batteriespannung
  - entity: sensor.victron_mppt_battery_current
    name: Batteriestrom
  - entity: number.victron_mppt_charge_current_limit
    name: Ladestrom Limit
  - entity: text_sensor.victron_mppt_device_state
    name: Status
  - entity: sensor.victron_mppt_yield_today
    name: Ertrag heute
```

#### 10. Input Number Helper für manuelle Steuerung

```yaml
# In configuration.yaml
input_number:
  mppt_manual_current_limit:
    name: "MPPT Manuelles Stromlimit"
    min: 0
    max: 50
    step: 1
    unit_of_measurement: "A"
    icon: mdi:current-dc

input_boolean:
  mppt_manual_mode:
    name: "MPPT Manueller Modus"
    icon: mdi:hand-back-right

automation:
  - alias: "MPPT Manueller Modus"
    trigger:
      - platform: state
        entity_id: input_number.mppt_manual_current_limit
    condition:
      - condition: state
        entity_id: input_boolean.mppt_manual_mode
        state: "on"
    action:
      - service: number.set_value
        target:
          entity_id: number.victron_mppt_charge_current_limit
        data:
          value: "{{ states('input_number.mppt_manual_current_limit') | float }}"
```

### Quellen

- [ESPHome Victron VE.Direct Component](https://github.com/krahabb/esphome-victron-vedirect)
- [Victron VE.Direct HEX Protocol](https://www.victronenergy.com/upload/documents/BlueSolar-HEX-protocol.pdf)
- [ESPHome ESP32-P4 Support](https://esphome.io/components/ethernet.html)
- [Waveshare ESP32-P4-Nano](https://devices.esphome.io/devices/waveshare-esp32-p4-nano/)
