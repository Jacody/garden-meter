#include <Arduino.h>
#include "FS.h"
#include "SPIFFS.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <time.h>  // Für die Echtzeitfunktionen

// Konfigurationsdatei mit sensiblen Daten (WLAN-Zugangsdaten, etc.)
#include "config.h"

// NTP-Server für die Zeiteinstellung
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;  // GMT+1 für Deutschland
const int   daylightOffset_sec = 3600;  // Sommerzeit-Offset

// Webserver auf Port 80
WebServer server(80);

// Sensor Pins
#define SOIL_SENSOR_PIN 35      // Analog pin G35 für den Bodenfeuchtesensor
#define LIGHT_SENSOR_PIN 34     // Analog pin G34 für den Photoresistor
#define DHTPIN 32               // Digital pin G32 für den DHT Sensor
#define DHTTYPE DHT11           // DHT 11 Sensor

// Dateinamen für die Messdaten
#define DATA_FILE "/sensor_data.csv"

// Kalibrierungswerte für die Bodenfeuchte
#define SOIL_DRY_VALUE 0        // ADC-Wert bei trockenem Boden (0-300)
#define SOIL_HUMID_VALUE 500    // ADC-Wert bei feuchtem Boden (300-700)
#define SOIL_WET_VALUE 950      // ADC-Wert bei sehr nassem Boden/Wasser (700-950)

// Kalibrierungswerte für die Umrechnung in Watt/m²
#define DARK_ADC_VALUE 4095      // ADC-Wert bei Dunkelheit (ESP32: 0-4095)
#define BRIGHT_ADC_VALUE 0       // ADC-Wert bei hellem Licht
#define DARK_IRRADIANCE 0.0      // W/m² bei Dunkelheit
#define BRIGHT_IRRADIANCE 1000.0 // W/m² bei hellem Sonnenlicht (ca. 1000 W/m²)

// Maximale Anzahl von Versuchen beim Sensorauslesen
const int MAX_RETRIES = 3;

// Variablen
unsigned long lastMeasurement = 0;
const unsigned long measurementInterval = 2000; // 2 Sekunden Intervall

// DHT Sensor Objekt
DHT dht(DHTPIN, DHTTYPE);

// Aktuelle Messwerte
int currentSoilValue = 0;
String currentSoilStatus = "";  // Textbeschreibung des Bodenzustands
int currentLightValue = 0;
float currentIrradiance = 0.0;  // Aktuelle Strahlungsleistung in W/m²
float currentTemperature = 0.0;
float currentHumidity = 0.0;

// Flag für erfolgreiche NTP-Synchronisation
bool timeIsSynchronized = false;

// JSON-Puffer für Webserver
StaticJsonDocument<500> jsonDoc;
char jsonBuffer[500];

// Hilfsfunktion um einen Zeitstempel-String zu erzeugen
String getTimeStamp() {
  struct tm timeinfo;
  char timeStringBuff[30];
  
  if (!timeIsSynchronized) {
    return "Zeit nicht synchronisiert";
  }
  
  if(!getLocalTime(&timeinfo)){
    Serial.println("Konnte Zeit nicht abrufen");
    return "Zeit nicht verfügbar";
  }
  
  // Format: JJJJ-MM-TT HH:MM:SS
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(timeStringBuff);
}

// HTML für die Hauptseite
const char* htmlPage = R"html(
<!DOCTYPE html>
<html lang="de">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 Sensoren Dashboard</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 0; padding: 20px; text-align: center; }
    .container { max-width: 800px; margin: 0 auto; }
    .card { background: #f5f5f5; border-radius: 10px; padding: 20px; margin-bottom: 20px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }
    h1 { color: #333; }
    .data { font-size: 24px; margin: 10px 0; }
    .raw { font-size: 16px; color: #666; }
    button { background: #4CAF50; color: white; border: none; padding: 10px 20px; border-radius: 5px; cursor: pointer; font-size: 16px; }
    button:hover { background: #45a049; }
    .sensor-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; }
  </style>
</head>
<body>
  <div class="container">
    <h1>ESP32 Sensoren Dashboard</h1>
    <div class="sensor-grid">
      <div class="card">
        <h2>Bodenfeuchte</h2>
        <div class="data" id="status">--</div>
        <div class="raw" id="soilRaw">ADC-Wert: --</div>
      </div>
      <div class="card">
        <h2>Lichtintensität</h2>
        <div class="data" id="irradiance">--</div>
        <div class="raw" id="lightRaw">ADC-Wert: --</div>
      </div>
      <div class="card">
        <h2>Temperatur</h2>
        <div class="data" id="temperature">--</div>
      </div>
      <div class="card">
        <h2>Luftfeuchtigkeit</h2>
        <div class="data" id="humidity">--</div>
      </div>
    </div>
    <button onclick="fetchData()">Aktualisieren</button>
  </div>

  <script>
    // Daten beim Laden der Seite abrufen
    document.addEventListener('DOMContentLoaded', fetchData);
    
    // Daten vom Server abrufen
    function fetchData() {
      fetch('/api/data')
        .then(response => response.json())
        .then(data => {
          document.getElementById('status').textContent = data.status;
          document.getElementById('soilRaw').textContent = 'ADC-Wert: ' + data.soilRaw;
          document.getElementById('irradiance').textContent = data.irradiance.toFixed(2) + ' W/m²';
          document.getElementById('lightRaw').textContent = 'ADC-Wert: ' + data.lightRaw;
          document.getElementById('temperature').textContent = data.temperature.toFixed(1) + ' °C';
          document.getElementById('humidity').textContent = data.humidity.toFixed(1) + ' %';
        })
        .catch(error => console.error('Fehler beim Abrufen der Daten:', error));
    }
    
    // Daten alle 5 Sekunden aktualisieren
    setInterval(fetchData, 5000);
  </script>
</body>
</html>
)html";

// WLAN-Verbindung herstellen
void setupWiFi() {
  Serial.println("Verbinde mit WLAN...");
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("Verbunden mit WLAN. IP-Adresse: ");
    Serial.println(WiFi.localIP());
    
    // Zeitserver konfigurieren
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    Serial.println("Zeit wird synchronisiert...");
    
    // Warten auf Zeitsynchronisation
    struct tm timeinfo;
    int timeoutCounter = 0;
    while (!getLocalTime(&timeinfo) && timeoutCounter < 10) {
      Serial.println("Warten auf Zeitsynchronisation...");
      delay(1000);
      timeoutCounter++;
    }
    
    if (timeoutCounter < 10) {
      timeIsSynchronized = true;
      Serial.println("Zeit synchronisiert: " + getTimeStamp());
    } else {
      Serial.println("Zeitsynchronisation fehlgeschlagen!");
    }
    
    // mDNS-Responder starten
    if (MDNS.begin("esp32soilsensor")) {
      Serial.println("mDNS-Responder gestartet - Erreichbar über: http://esp32soilsensor.local");
    }
  } else {
    Serial.println();
    Serial.println("WLAN-Verbindung fehlgeschlagen!");
  }
}

// Webserver-Routen konfigurieren
void setupWebServer() {
  // Route für Hauptseite
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", htmlPage);
  });
  
  // API-Endpunkt für Sensordaten
  server.on("/api/data", HTTP_GET, []() {
    jsonDoc["soilRaw"] = currentSoilValue;
    jsonDoc["status"] = currentSoilStatus;
    jsonDoc["lightRaw"] = currentLightValue;
    jsonDoc["irradiance"] = currentIrradiance;
    jsonDoc["temperature"] = currentTemperature;
    jsonDoc["humidity"] = currentHumidity;
    
    serializeJson(jsonDoc, jsonBuffer);
    server.send(200, "application/json", jsonBuffer);
  });
  
  // Datei herunterladen
  server.on("/download", HTTP_GET, []() {
    Serial.println("Download-Anfrage erhalten");
    
    if (SPIFFS.exists(DATA_FILE)) {
      Serial.println("Datei gefunden, versuche zu öffnen...");
      File file = SPIFFS.open(DATA_FILE, FILE_READ);
      if (file) {
        Serial.println("Datei erfolgreich geöffnet, Größe: " + String(file.size()) + " Bytes");
        server.streamFile(file, "text/csv");
        file.close();
        Serial.println("Datei erfolgreich gestreamt");
        return;
      } else {
        Serial.println("Fehler: Datei existiert, konnte aber nicht geöffnet werden!");
      }
    } else {
      Serial.println("Fehler: Datei nicht gefunden!");
    }
    
    // Liste alle Dateien im SPIFFS auf
    Serial.println("Vorhandene Dateien im SPIFFS:");
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while(file) {
      Serial.println(" - " + String(file.name()) + " (" + String(file.size()) + " Bytes)");
      file = root.openNextFile();
    }
    
    server.send(404, "text/plain", "Datei nicht gefunden");
  });
  
  // 404 für nicht gefundene Seiten
  server.onNotFound([]() {
    server.send(404, "text/plain", "Seite nicht gefunden");
  });
  
  // Webserver starten
  server.begin();
  Serial.println("Webserver gestartet");
}

int readSoilSensor() {
  int value = analogRead(SOIL_SENSOR_PIN);
  return value;
}

int readLightSensor() {
  int value = analogRead(LIGHT_SENSOR_PIN);
  return value;
}

bool readDHTSensor(float &temperature, float &humidity) {
  for(int i = 0; i < MAX_RETRIES; i++) {
    humidity = dht.readHumidity();
    temperature = dht.readTemperature();
    
    if (!isnan(humidity) && !isnan(temperature)) {
      return true;
    }
    
    Serial.print("Versuch ");
    Serial.print(i + 1);
    Serial.println(" fehlgeschlagen, versuche erneut...");
    delay(1000); // Warte 1 Sekunde zwischen den Versuchen
  }
  return false;
}

// Bestimmt den Bodenzustand basierend auf dem ADC-Wert
String getSoilStatus(int adcValue) {
  if (adcValue < 300) {
    return "Trockener Boden";
  } else if (adcValue < 700) {
    return "Feuchter Boden";
  } else {
    return "Sehr nasser Boden / Im Wasser";
  }
}

// Umrechnung des ADC-Werts in W/m²
float convertToIrradiance(int adcValue) {
  // Lineare Umrechnung zwischen den Kalibrierungspunkten
  // Hinweis: Da der ADC-Wert bei uns umgekehrt ist (niedriger = heller),
  // müssen wir die Formel entsprechend anpassen
  float percentage = (float)(DARK_ADC_VALUE - adcValue) / (DARK_ADC_VALUE - BRIGHT_ADC_VALUE);
  float irradiance = DARK_IRRADIANCE + percentage * (BRIGHT_IRRADIANCE - DARK_IRRADIANCE);
  
  // Begrenze auf sinnvollen Bereich
  if (irradiance < 0) irradiance = 0;
  if (irradiance > BRIGHT_IRRADIANCE) irradiance = BRIGHT_IRRADIANCE;
  
  return irradiance;
}

void setup() {
  // Serielle Kommunikation starten
  Serial.begin(115200);
  delay(2000); // Längere Verzögerung für Stabilisierung
  
  Serial.println("\nESP32 Bodenfeuchtesensor, Photoresistor und DHT11 Sensor");
  
  Serial.print("Bodenfeuchtesensor Pin konfiguriert: ");
  Serial.println(SOIL_SENSOR_PIN);
  Serial.print("Photoresistor Pin konfiguriert: ");
  Serial.println(LIGHT_SENSOR_PIN);
  Serial.print("DHT Pin konfiguriert: ");
  Serial.println(DHTPIN);
  
  // Pin-Modus für DHT konfigurieren
  pinMode(DHTPIN, INPUT_PULLUP);
  
  // DHT Sensor initialisieren mit zusätzlicher Verzögerung
  dht.begin();
  delay(1000);
  Serial.println("DHT Sensor Initialisierung durchgeführt");
  
  // SPIFFS formatieren und initialisieren
  Serial.println("Formatiere SPIFFS...");
  bool formatted = SPIFFS.format();
  if(formatted) {
    Serial.println("SPIFFS erfolgreich formatiert");
  } else {
    Serial.println("SPIFFS-Formatierung fehlgeschlagen!");
  }
  
  // SPIFFS initialisieren
  if(!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Initialisierung fehlgeschlagen!");
    return;
  }
  
  Serial.println("SPIFFS erfolgreich initialisiert");
  
  // Überprüfen, ob die Datei existiert
  if(SPIFFS.exists(DATA_FILE)) {
    Serial.println("Datendatei existiert bereits");
    File testRead = SPIFFS.open(DATA_FILE, FILE_READ);
    if(testRead) {
      Serial.println("Datei kann geöffnet werden, Größe: " + String(testRead.size()) + " Bytes");
      testRead.close();
    } else {
      Serial.println("Datei existiert, kann aber nicht geöffnet werden!");
    }
  } else {
    Serial.println("Datendatei existiert nicht, wird erstellt...");
    File dataFile = SPIFFS.open(DATA_FILE, FILE_WRITE);
    if(dataFile) {
      dataFile.println("Zeitstempel,Bodenfeuchte-Rohwert,Bodenstatus,Licht-Rohwert,Lichtintensitaet(W/m2),Temperatur(C),Luftfeuchtigkeit(%)");
      dataFile.close();
      Serial.println("Neue Datendatei erstellt");
      
      // Überprüfen, ob die Datei jetzt existiert
      if(SPIFFS.exists(DATA_FILE)) {
        Serial.println("Datei wurde erfolgreich erstellt");
      } else {
        Serial.println("Fehler: Datei wurde nicht erstellt!");
      }
    } else {
      Serial.println("Fehler beim Erstellen der Datendatei!");
    }
  }
  
  // WLAN und Webserver einrichten
  setupWiFi();
  setupWebServer();
}

void loop() {
  // Webserver-Anfragen bearbeiten
  server.handleClient();
  
  unsigned long currentMillis = millis();
  
  if(currentMillis - lastMeasurement >= measurementInterval) {
    lastMeasurement = currentMillis;
    
    // Bodenfeuchtesensor auslesen
    currentSoilValue = readSoilSensor();
    
    // Status ermitteln
    currentSoilStatus = getSoilStatus(currentSoilValue);
    
    // Photoresistor auslesen
    currentLightValue = readLightSensor();
    
    // In W/m² umrechnen
    currentIrradiance = convertToIrradiance(currentLightValue);
    
    // DHT11 Sensor auslesen
    float temperature, humidity;
    Serial.println("Versuche DHT Sensor auszulesen...");
    if(readDHTSensor(temperature, humidity)) {
      // Aktuelle Werte speichern
      currentTemperature = temperature;
      currentHumidity = humidity;
      Serial.println("DHT Messung erfolgreich!");
    } else {
      Serial.println("Fehler: Konnte nach mehreren Versuchen keine gültigen Werte vom DHT Sensor lesen!");
    }
    
    // Aktueller Zeitstempel
    String timeStamp = getTimeStamp();
    
    // Anzeige auf der seriellen Konsole
    Serial.print("Zeit: ");
    Serial.print(timeStamp);
    Serial.print(", Bodenfeuchte-Rohwert: ");
    Serial.print(currentSoilValue);
    Serial.print(", Status: ");
    Serial.print(currentSoilStatus);
    Serial.print(", Licht-Rohwert: ");
    Serial.print(currentLightValue);
    Serial.print(", Lichtintensität: ");
    Serial.print(currentIrradiance, 2);
    Serial.print(" W/m², Temperatur: ");
    Serial.print(currentTemperature, 1);
    Serial.print(" °C, Luftfeuchtigkeit: ");
    Serial.print(currentHumidity, 1);
    Serial.println(" %");
    
    // Daten in Datei speichern
    File dataFile = SPIFFS.open(DATA_FILE, FILE_APPEND);
    if(dataFile) {
      dataFile.print(timeStamp);
      dataFile.print(",");
      dataFile.print(currentSoilValue);
      dataFile.print(",");
      dataFile.print(currentSoilStatus);
      dataFile.print(",");
      dataFile.print(currentLightValue);
      dataFile.print(",");
      dataFile.print(currentIrradiance);
      dataFile.print(",");
      dataFile.print(currentTemperature);
      dataFile.print(",");
      dataFile.println(currentHumidity);
      dataFile.close();
    } else {
      Serial.println("Fehler beim Öffnen der Datendatei!");
    }
  }
}