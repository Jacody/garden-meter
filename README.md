# Garden Meter

Sammeln Sie alle relevanten Umweltdaten für Ihren Garten mit einem ESP32-Mikrocontroller.

## Hardware-Anforderungen

- ESP32 Entwicklungsboard
- DHT11 Temperatur- und Luftfeuchtigkeitssensor
- Bodenfeuchtesensor
- Photoresistor (LDR)
- Breadboard und Verbindungskabel

## Installation

1. Klonen Sie das Repository:
```bash
git clone https://github.com/Jacody/garden-meter.git
cd garden-meter
```

2. Öffnen Sie das Projekt in der Arduino IDE oder PlatformIO

## Konfiguration

1. Kopieren Sie die Template-Header-Datei:
```bash
cp config_template.h config.h
```

2. Bearbeiten Sie `config.h` und tragen Sie Ihre eigenen Werte ein:
   - WLAN-SSID und Passwort
   - API-Schlüssel (falls benötigt)
   - Server-Adressen (falls benötigt)

**Wichtig:** Die Datei `config.h` wird nicht in Git committed und sollte Ihre echten Zugangsdaten enthalten.

## Verwendung

1. Laden Sie den Code auf Ihren ESP32 hoch
2. Öffnen Sie den seriellen Monitor, um die IP-Adresse zu sehen
3. Besuchen Sie die IP-Adresse in Ihrem Browser
4. Das Dashboard zeigt alle Sensordaten in Echtzeit an

## Sensoren

- **Bodenfeuchte**: Misst die Feuchtigkeit des Bodens
- **Lichtintensität**: Misst die Lichtstärke in W/m²
- **Temperatur**: Umgebungstemperatur in °C
- **Luftfeuchtigkeit**: Relative Luftfeuchtigkeit in %

## Beitragen

1. Forken Sie das Repository
2. Erstellen Sie einen Feature-Branch
3. Committen Sie Ihre Änderungen
4. Pushen Sie den Branch
5. Erstellen Sie einen Pull Request
