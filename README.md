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

### Home Assistant Automation Beispiel

```yaml
# Automatische Ladestrom-Anpassung basierend auf Überschuss
automation:
  - alias: "Solar Ladestrom anpassen"
    trigger:
      - platform: state
        entity_id: sensor.grid_power
    action:
      - service: number.set_value
        target:
          entity_id: number.victron_mppt_charge_current_limit
        data:
          value: >
            {{ [0, (states('sensor.available_power') | float / 48) | round(1), 50] | sort | list | first }}
```

### Quellen

- [ESPHome Victron VE.Direct Component](https://github.com/krahabb/esphome-victron-vedirect)
- [Victron VE.Direct HEX Protocol](https://www.victronenergy.com/upload/documents/BlueSolar-HEX-protocol.pdf)
- [ESPHome ESP32-P4 Support](https://esphome.io/components/ethernet.html)
- [Waveshare ESP32-P4-Nano](https://devices.esphome.io/devices/waveshare-esp32-p4-nano/)
