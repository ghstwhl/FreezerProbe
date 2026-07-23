# FreezerProbe - ESP32 Temperature Monitor

ESP32-based temperature monitoring system for freezers and refrigerators with push notifications via Prowl API.

## Features

- 🌡️ DS18B20 temperature sensor monitoring
- 📱 Push notifications via Prowl API
- 📧 Email notifications via SMTP
- 📊 Temperature history with web-based graphing
- 🔄 Over-The-Air (OTA) updates with optional password protection
- 📶 WiFi Manager for easy setup (no hardcoded credentials)
- 🌐 Web interface accessible via browser
- 💾 Persistent settings stored in flash memory
- 🔔 Smart alerting with cooldown to prevent spam
- 🏷️ Configurable device name for multi-device setups
- 🔒 Secure field handling for API keys and passwords (masked display)

## Hardware Requirements

### Required Components

1. **ESP32 Development Board**
   - Any ESP32 board with WiFi support
   - Recommended: ESP32 DevKit v1 or similar

2. **DS18B20 Temperature Sensor**
   - **Waterproof version recommended** for freezer use
   - Operating range: -55°C to +125°C
   - Accuracy: ±0.5°C

3. **Pull-up Resistor**
   - **4.7kΩ resistor** (required for DS18B20)
   - Connect between data line and VCC (3.3V)

4. **Power Supply**
   - 5V power supply (USB or dedicated adapter)
   - **Use reliable power supply** - not just any USB charger
   - Minimum 500mA current capacity

## Required Libraries

### External Libraries (Must Install)

The following libraries must be installed via Arduino Library Manager or Arduino CLI:

1. **OneWire** by Paul Stoffregen
   - For 1-Wire communication with DS18B20 sensor
   - Install: `arduino-cli lib install OneWire`

2. **DallasTemperature** by Miles Burton
   - High-level interface for DS18B20 sensor
   - Install: `arduino-cli lib install DallasTemperature`

3. **WiFiManager** by tzapu
   - Captive portal for WiFi configuration
   - Install: `arduino-cli lib install WiFiManager`

4. **ESP Mail Client** by Mobizt
   - SMTP email notification support
   - Install: `arduino-cli lib install "ESP Mail Client"`

### Built-in ESP32 Libraries (No Installation Needed)

The following libraries are included with the ESP32 Arduino core:
- WiFi - WiFi connectivity
- WebServer - HTTP web server
- HTTPClient - HTTP client for Prowl API
- Preferences - Flash-based persistent storage
- ESPmDNS - mDNS hostname support (.local addresses)
- ArduinoOTA - Over-The-Air firmware updates

**Note**: These built-in libraries are automatically available after installing the ESP32 board support in Arduino IDE/CLI.

### Installing All Libraries at Once

```bash
# Install ESP32 core
arduino-cli core install esp32:esp32

# Install all required external libraries
arduino-cli lib install OneWire
arduino-cli lib install DallasTemperature
arduino-cli lib install WiFiManager
arduino-cli lib install "ESP Mail Client"
```

## Hardware Considerations

1. **Power Supply Quality**
   - Use a high-quality 5V power adapter
   - Poor power quality can cause ESP32 instability
   - Consider a power bank or UPS for critical monitoring

2. **DS18B20 Wiring**
   - Red wire → 3.3V (VCC)
   - Black wire → GND
   - Yellow/White wire → GPIO4 (DATA) with 4.7kΩ pull-up to VCC
   - Use shielded cable if length exceeds 3 meters

3. **Cable Length**
   - DS18B20 works well with long cables (up to 100m with proper setup)
   - For cables >3m, use shielded/twisted pair
   - Keep cable away from high-voltage AC wiring

4. **Sensor Placement**
   - Mount sensor in freezer away from door shelves
   - Avoid areas that get direct airflow when door opens
   - Secure cable in door seal carefully to avoid damage
   - Consider drilling small hole vs. running through door seal

5. **Waterproofing**
   - Use waterproof DS18B20 probe (stainless steel tube)
   - Seal cable entry point if drilling through freezer wall
   - Avoid condensation on ESP32 board itself

6. **ESP32 Location**
   - Keep ESP32 board **outside** the freezer
   - Only the sensor probe goes inside
   - Protect from moisture and condensation

7. **WiFi Signal**
   - Ensure good WiFi signal at installation location
   - Metal freezer walls can block signal
   - Consider WiFi extender if needed

## Wiring Diagram

```
DS18B20 Sensor          ESP32
┌─────────────┐       ┌─────────┐
│             │       │         │
│  VCC (Red)  ├───────┤ 3.3V    │
│             │       │         │
│  GND (Blk)  ├───────┤ GND     │
│             │       │         │
│  DATA (Yel) ├───────┤ GPIO4   │
│             │   │   │         │
└─────────────┘   │   └─────────┘
                  │
                 ┌┴┐
                 │ │ 4.7kΩ
                 └┬┘
                  │
                  └─── 3.3V
```

## Before First Use

### 1. Hardware Setup

1. Connect DS18B20 sensor as shown in wiring diagram
2. **Don't forget the 4.7kΩ pull-up resistor** (critical!)
3. Connect ESP32 to computer via USB for initial upload

### 2. Development Environment

This project uses a dev container with Arduino CLI. To rebuild the container with all dependencies:

```bash
# Rebuild dev container
Ctrl+Shift+P → "Dev Containers: Rebuild Container"
```

### 3. Change Partition Scheme

**IMPORTANT**: This sketch is too large for the default ESP32 partition scheme. You must use a partition scheme with more app space.

#### Using Arduino CLI:

```bash
# Compile with Minimal SPIFFS partition scheme (1.9MB APP with OTA support)
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs FreezerProbe.ino

# Upload with partition scheme
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs FreezerProbe.ino
```

#### Using Arduino IDE:

1. Open **Tools** menu
2. Select **Partition Scheme → Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)**
3. Compile and upload as normal

This partition scheme provides 1.9MB for the application while maintaining OTA update capability.

### 4. Configure GPIO Pin (Optional)

If you're not using GPIO4 for the DS18B20 data line, update line 27 in `FreezerProbe.ino`:

```cpp
#define ONE_WIRE_BUS 4        // Change to your GPIO pin
```

### 5. Compile and Upload

Using Arduino CLI:

```bash
# Compile for ESP32 with Minimal SPIFFS partition scheme
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs FreezerProbe.ino

# Upload (replace /dev/ttyUSB0 with your port)
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs FreezerProbe.ino
```

Or use the Arduino IDE / VS Code Arduino extension (remember to select the partition scheme in Tools menu).

### 6. Initial WiFi Setup

1. After first boot, the ESP32 will create a WiFi access point named **"FreezerProbe-Setup"**
2. Connect to this AP with your phone or computer
3. A captive portal should open automatically (or go to http://192.168.4.1)
4. Select your WiFi network and enter the password
5. Click "Save" - the ESP32 will restart and connect to your network

### 7. Access Web Interface

After connecting to WiFi, access the web interface:

- **By hostname**: http://[devicename].local (mDNS) - defaults to http://freezerprobe.local
- **By IP**: Check serial monitor for IP address

**Note**: The hostname is automatically generated from your configured device name. Spaces and special characters are converted to hyphens and the name is lowercased for DNS compatibility.

### 8. Configure Settings

1. Set **Device Name** (e.g., "Basement Freezer", "Garage Fridge")
   - Used in notifications and as the mDNS hostname
   - Only letters, numbers, spaces, hyphens, underscores, and periods allowed
   - Cannot start or end with a hyphen
   - Hostname is auto-generated: "Basement Freezer" becomes http://basement-freezer.local
2. Set **Lower Temperature Threshold** (e.g., -25°C)
3. Set **Upper Temperature Threshold** (e.g., -15°C)
4. **(Optional)** Get a Prowl API key from https://www.prowlapp.com and enter it
5. **(Optional)** Configure OTA Password to secure firmware updates
6. **(Optional)** Enable Email Notifications and configure:
   - **Sender Email Address** - The "from" address for notifications
   - **Recipient Email Address** - Where alerts will be sent
   - **SMTP Server** - Your email provider's SMTP server (e.g., smtp.gmail.com)
   - **SMTP Port** - Usually 587 (TLS) or 465 (SSL)
   - **SMTP Username** (optional) - Leave empty to use sender email
   - **SMTP Password** - Your email password or app-specific password
7. Click "Save Settings"

**Note**: For Gmail, you'll need to create an [App Password](https://myaccount.google.com/apppasswords) instead of using your regular password.

### 9. Secure Field Handling

The web interface includes special security handling for sensitive fields to prevent accidental disclosure or deletion:

#### Protected Fields

Three fields have enhanced security:
- **Prowl API Key** - Push notification credentials
- **OTA Password** - Over-The-Air update authentication
- **SMTP Password** - Email notification credentials

#### How Secure Fields Work

**When First Configured:**
1. Enter the sensitive value (API key or password)
2. Click "Save Settings"
3. The field becomes **masked** (displays as `••••••••••••••••`)
4. The field is **disabled** for editing
5. A green checkmark (✓) indicates the value is set
6. A **"Change Key"** or **"Change Password"** button appears

**On Subsequent Visits:**
- Secure fields show as masked (`••••••••••••••••`)
- You can edit other settings without affecting these fields
- The saved values are preserved when you modify other settings
- Fields remain masked to prevent accidental disclosure

**To Change a Secure Field:**
1. Click the **"Change Key"** or **"Change Password"** button
2. The field clears and becomes editable
3. Enter the new value
4. Click "Save Settings"
5. The field becomes masked again

**Security Benefits:**
- Prevents shoulder surfing (values not visible on screen)
- Prevents accidental deletion when editing other settings
- Protects credentials on shared/public computers
- Values only transmitted when explicitly changed

**To Remove a Secure Field:**
1. Click the "Change" button to enable editing
2. Clear the field completely (leave empty)
3. Click "Save Settings"
4. The secure value is removed from storage

**Note**: Secure values are never displayed in plain text once saved, and are never returned by the `/status` API endpoint. Only a boolean flag indicates whether each field is configured.

### 10. Adjust Default Thresholds (Optional)

For different default values, edit lines 71-72 in `FreezerProbe.ino`:

```cpp
float lowerThreshold = -20.0;  // Change default lower threshold
float upperThreshold = -10.0;  // Change default upper threshold
```

### 11. Testing Checklist

- [ ] Verify sensor reading in Serial Monitor (115200 baud)
- [ ] Access web interface at http://freezerprobe.local
- [ ] Check temperature updates every 10 seconds
- [ ] View temperature history graph
- [ ] Test low threshold with ice bath
- [ ] Verify notification arrives via Prowl
- [ ] Reboot ESP32 and confirm settings persist
- [ ] Test OTA update capability

## Usage

### Web Interface

The web interface provides:

- **Real-time temperature display** with color coding
  - 🔵 Blue: Below lower threshold
  - 🟢 Green: Within normal range
  - 🔴 Red: Above upper threshold
- **Temperature history graph** (last 48 hours)
- **Threshold configuration**
- **Prowl API key setup** with masked display
- **OTA password configuration** for secure firmware updates
- **Email notification settings** with SMTP password protection
- **Secure field handling** - API keys and passwords shown as masked after saving
- **WiFi reset** option

### Notifications

Notifications are sent via **Prowl** and/or **Email** when:
- Temperature drops below lower threshold
- Temperature rises above upper threshold  
- Temperature returns to normal range (recovery notification)

All notifications include the **device name** to help identify which freezer/fridge sent the alert.

**Note**: 5-minute cooldown between alerts prevents spam

#### Email Configuration

Common SMTP server settings:

| Provider | SMTP Server | Port | Notes |
|----------|-------------|------|-------|
| Gmail | smtp.gmail.com | 587 | Requires [App Password](https://myaccount.google.com/apppasswords) |
| Outlook/Hotmail | smtp-mail.outlook.com | 587 | Use your regular password |
| Yahoo | smtp.mail.yahoo.com | 587 | May require app-specific password |
| Custom/Self-hosted | Your server | 587/465 | Check with your provider |

### OTA Updates

Update firmware over WiFi without USB cable:

```bash
# Using Arduino CLI (replace [devicename] with your configured device name)
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs FreezerProbe.ino
arduino-cli upload -p [devicename].local --fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs FreezerProbe.ino
```

Or use the Arduino IDE with Network Port option. The device will appear as your configured device name in the network ports list. **Remember**: Make sure the same partition scheme is selected in Tools menu.

**OTA Password Protection**: If you've configured an OTA password in the web interface, you'll be prompted to enter it when uploading firmware. This prevents unauthorized firmware updates. Leave the OTA password field empty to disable authentication.

### Reset WiFi Settings

If you need to reconfigure WiFi:
1. Click "Reset WiFi Settings" button on web interface, or
2. Flash new credentials via USB

## Troubleshooting

### Compilation Error: "Sketch too big"

**Error message**: `Sketch uses X bytes (>100%) of program storage space. Maximum is 1310720 bytes.` or `text section exceeds available space in board`

**Solution**: You must use a partition scheme with more app space. The sketch is too large for the default partition.

**Fix**:
- **Arduino CLI**: Add `:PartitionScheme=min_spiffs` to the FQBN:
  ```bash
  arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs FreezerProbe.ino
  ```
- **Arduino IDE**: Tools → Partition Scheme → Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)

See section "3. Change Partition Scheme" above for detailed instructions.

### Sensor Not Detected

- Check wiring (especially the 4.7kΩ pull-up resistor)
- Verify DS18B20 is genuine (many counterfeits exist)
- Try a different GPIO pin
- Check for loose connections

### Can't Connect to WiFi

- Ensure WiFi credentials are correct
- Check WiFi signal strength at installation location
- Look for "FreezerProbe-Setup" AP to reconfigure
- Metal surfaces can block WiFi signals

### No Web Interface

- Check serial monitor for IP address and hostname
- Try http://[devicename].local where [devicename] is your configured device name in lowercase with hyphens
- Try the direct IP address if mDNS isn't working
- Verify you're on the same network
- Disable VPN if active
- Some networks don't support mDNS (especially corporate/guest networks) - use IP address instead

### Notifications Not Working

**Prowl:**
- Verify Prowl API key is entered correctly
- Check Prowl app is installed on your device
- Test API key at https://www.prowlapp.com
- Check ESP32 has internet access (not just LAN)

**Email:**
- Verify all required fields are filled (sender, recipient, SMTP server)
- Check SMTP port is correct (usually 587 for TLS)
- For Gmail, use an [App Password](https://myaccount.google.com/apppasswords), not your regular password
- Check serial monitor for SMTP error messages
- Some email providers block SMTP access by default - check provider settings
- Verify SMTP username/password if your server requires authentication

### Temperature Readings Incorrect

- Allow 30 seconds for sensor to stabilize
- Verify sensor is not damaged
- Check if sensor is touching metal (electrical isolation needed)
- Calibrate against known reference

## Technical Specifications

- **Temperature Reading Interval**: 10 seconds
- **Temperature History**: 288 readings (48 hours)
- **Alert Cooldown**: 5 minutes
- **Web Server Port**: 80
- **OTA Port**: 3232
- **Sensor Resolution**: 12-bit (0.0625°C precision)
- **Device Name**: 1-63 characters, letters/numbers/spaces/hyphens/underscores/periods, auto-sanitized for DNS
- **mDNS Hostname**: Auto-generated from device name (lowercase, hyphens replace special chars)

## API Endpoints

- `GET /` - Web interface
- `GET /status` - JSON status (temp, thresholds, connection info)
  - **Note**: Secure fields (Prowl API key, OTA password, SMTP password) are never returned. Only boolean flags indicate if they're configured.
- `GET /history` - JSON temperature history
- `POST /settings` - Update settings (secure fields only updated if explicitly provided)
- `POST /reset` - Reset WiFi settings

## License

Open source - feel free to modify and improve!

## Support

For issues or questions:
- Check the serial monitor output (115200 baud)
- Verify all hardware connections
- See FUTURE_IMPROVEMENTS.md for enhancement ideas
