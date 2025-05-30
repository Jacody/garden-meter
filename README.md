# Garden Meter

Collect all relevant environmental data for your garden with an ESP32 microcontroller.

## Hardware Requirements

- ESP32 development board
- DHT11 temperature and humidity sensor
- Soil moisture sensor
- Photoresistor (LDR)
- Breadboard and connecting wires

## Installation

1. Clone the repository:
```bash
git clone https://github.com/Jacody/garden-meter.git
cd garden-meter
```

2. Open the project in Arduino IDE or PlatformIO

## Configuration

1. Copy the template header file:
```bash
cp config_template.h config.h
```

2. Edit `config.h` and enter your own values:
   - WiFi SSID and password
   - API keys (if needed)
   - Server addresses (if needed)

**Important:** The `config.h` file is not committed to Git and should contain your real credentials.

## Usage

1. Upload the code to your ESP32
2. Open the serial monitor to see the IP address
3. Visit the IP address in your browser
4. The dashboard shows all sensor data in real-time

## Sensors

- **Soil Moisture**: Measures soil moisture content
- **Light Intensity**: Measures light intensity in W/m²
- **Temperature**: Ambient temperature in °C
- **Humidity**: Relative humidity in %

## Contributing

1. Fork the repository
2. Create a feature branch
3. Commit your changes
4. Push the branch
5. Create a Pull Request
