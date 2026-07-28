# FreezerProbe - ESP32 Temperature Monitor

ESP32-based temperature monitoring system for freezers and refrigerators with push notifications via Prowl API.

## Features

- 🌡️ DS18B20 temperature sensor monitoring
- 📱 Push notifications via Prowl API
- 📧 Email notifications via SMTP
- 📊 Temperature history with web-based graphing
- � Alert history table showing all sent notifications
- ⚠️ Sensor disconnection alerts with automatic recovery detection
- 🕒 NTP time synchronization with configurable timezone
- 🔄 Over-The-Air (OTA) updates with optional password protection
- 📶 WiFi Manager for easy setup (no hardcoded credentials)
- 🌐 Web interface accessible via browser
- 💾 Persistent settings stored in flash memory
- 🔔 Smart alerting with 3-reading threshold and 5-minute cooldown to prevent false alarms
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

You can use either **Arduino CLI** (recommended for VS Code) or the **Arduino IDE**.

#### Option A: Arduino CLI (VS Code Dev Container)

This project includes a dev container with Arduino CLI and all dependencies pre-installed.

```bash
# Rebuild dev container to install all dependencies
Ctrl+Shift+P → "Dev Containers: Rebuild Container"
```

#### Option B: Arduino IDE

**Install ESP32 Board Support**:
1. Open **File → Preferences**
2. Add to "Additional Board Manager URLs":
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Open **Tools → Board → Boards Manager**
4. Search "esp32" and install **"esp32 by Espressif Systems"**

**Install Required Libraries**:
1. Open **Tools → Manage Libraries** (or Ctrl+Shift+I)
2. Install these libraries:
   - **OneWire**
   - **DallasTemperature**
   - **WiFiManager** by tzapu
   - **ESP Mail Client**

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

#### Option A: Using Arduino CLI

```bash
# Compile for ESP32 with Minimal SPIFFS partition scheme
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs FreezerProbe.ino

# Upload via USB (replace /dev/ttyUSB0 with your port: COM3, /dev/ttyUSB0, etc.)
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs FreezerProbe.ino
```

**Find your port**:
```bash
# Linux/Mac
ls /dev/tty.* /dev/ttyUSB*

# Windows (in PowerShell)
[System.IO.Ports.SerialPort]::getportnames()
```

#### Option B: Using Arduino IDE

**Configure Board Settings**:
1. **Tools → Board** → Select "ESP32 Dev Module" (or your specific ESP32 board)
2. **Tools → Partition Scheme** → Select **"Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)"** ⚠️ **REQUIRED**
3. **Tools → Upload Speed** → 921600 (or 115200 if upload fails)
4. **Tools → Port** → Select your ESP32's COM/serial port

**Compile and Upload**:
1. Click the **Verify** button (✓) or **Sketch → Verify/Compile** to compile
2. Click the **Upload** button (→) or **Sketch → Upload** to upload
3. Wait for "Hard resetting via RTS pin..." message

**Monitor Serial Output** (optional but recommended):
- **Tools → Serial Monitor**
- Set baud rate to **115200**
- Watch for IP address and connection status

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
2. Select **Timezone** from the dropdown
   - Choose your local timezone for accurate timestamp display
   - Time is synchronized via NTP automatically at startup
   - NTP updates occur every 5 minutes to maintain accuracy
   - Timestamps in temperature and alert history use your selected timezone
3. Set **Lower Temperature Threshold** (e.g., -25°C)
4. Set **Upper Temperature Threshold** (e.g., -15°C)
5. **(Optional)** Get a Prowl API key from https://www.prowlapp.com and enter it
6. **(Optional)** Configure OTA Password to secure firmware updates
7. **(Optional)** Enable Email Notifications and configure:
   - **Sender Email Address** - The "from" address for notifications
   - **Recipient Email Address** - Where alerts will be sent
   - **SMTP Server** - Your email provider's SMTP server (e.g., smtp.gmail.com)
   - **SMTP Port** - Usually 587 (TLS) or 465 (SSL)
   - **SMTP Username** (optional) - Leave empty to use sender email
   - **SMTP Password** - Your email password or app-specific password
8. Click "Save Settings"

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
- **Temperature history graph** (last 48 hours with local time)
- **Alert history table** showing last 50 notifications sent with timestamps
- **Timezone configuration** with automatic NTP synchronization
- **Threshold configuration**
- **Prowl API key setup** with masked display
- **OTA password configuration** for secure firmware updates
- **Email notification settings** with SMTP password protection
- **Secure field handling** - API keys and passwords shown as masked after saving
- **Free heap display** with color coding in the status bar (🟢 Healthy / 🟠 Warning / 🔴 Critical)
- **WiFi reset** option

### Notifications

Notifications are sent via **Prowl** and/or **Email** when:
- Temperature drops below lower threshold (after 3 consecutive low readings)
- Temperature rises above upper threshold (after 3 consecutive high readings)
- Temperature returns to normal range (after 3 consecutive normal readings)
- **Sensor disconnects or error reading temperature** (after 3 consecutive error readings)
- **Sensor reconnects** after being disconnected (after 3 consecutive normal readings)

All notifications include the **device name** to help identify which freezer/fridge sent the alert.

**Alert Thresholds**: Alerts require **3 consecutive out-of-bounds readings** (30 seconds at 10-second intervals) before triggering. This prevents false alarms from brief temperature spikes when opening doors or temporary sensor glitches.

**Prowl Priority Levels**:
- **High Priority (2)**: Temperature alerts, sensor errors, and delivery failures
- **Normal Priority (0)**: Test messages and recovery notifications via email
- **Low Priority (-2)**: Recovery notifications via Prowl

**Cooldown**: 5-minute cooldown between alerts prevents notification spam

#### Test Notifications on Settings Save

When Prowl or Email settings are updated, a **test notification** is sent to validate the configuration:

- **Prowl API key changed**: A test Prowl notification is sent immediately
- **Email settings changed and email enabled**: A test email is sent with SMTP server details, free heap, and uptime for diagnostics
- The result (success or detailed error) is displayed in the web UI's result banner
- Test notifications are only sent when the corresponding settings actually change — saving other settings does not trigger tests

#### Delivery Error Notifications

If a notification fails to send, the error is handled with multiple layers of visibility:

1. **Alert History**: Every send failure is recorded in the alert history table with priority HIGH, visible in the web UI
2. **GUI Display**: When saving settings, the exact error message (e.g., "SMTP error: Unable to connect to mail.example.com:25") appears in the result banner
3. **Cross-Notification**: If one delivery method fails, the system attempts to notify you via the other method:
   - Prowl fails → an email alert is sent describing the Prowl failure
   - Email fails → a Prowl notification is sent with the SMTP error details
4. **Serial Monitor**: Detailed debug output is logged at 115200 baud, including library-level connection diagnostics

#### Low Memory Alerts

The system monitors free heap memory and sends alerts when resources become critically low:

- **Low heap alert**: Sent when free heap drops below 30 KB (High priority, both Prowl and Email if configured)
- **Heap recovery alert**: Sent when free heap recovers above 30 KB (Normal priority)
- **Cooldown**: 1-hour cooldown between heap alerts to prevent spam
- The web UI status bar shows real-time free heap with color coding:
  - 🟢 Green (≥ 60 KB): Healthy
  - 🟠 Orange (30-60 KB): Warning
  - 🔴 Red (< 30 KB): Critical

#### Sensor Error Alerts

The system monitors sensor connectivity and will send high-priority alerts if:
- The DS18B20 sensor becomes disconnected
- Temperature readings fail or return error values
- Communication errors occur on the 1-Wire bus

**Alert Threshold**: 3 consecutive sensor error readings required before alerting.

When the sensor recovers:
- A low-priority recovery notification is sent (after 3 consecutive successful readings)
- Current temperature is included in the recovery message
- Temperature monitoring resumes automatically

This ensures you're notified of persistent sensor issues while avoiding false alarms from momentary connection problems.

#### Alert History

The web interface includes an **Alert History** table that displays:
- **Timestamp** - When each alert was sent
- **Alert Message** - The full notification text (including delivery failure errors for debugging)
- **Priority** - HIGH (temperature/sensor/delivery failure alerts) or NORMAL (recovery/test)
- **Sent Via** - Shows which notification methods were used (📱 Prowl, 📧 Email). Empty for failed deliveries.

The alert history:
- Stores the last **50 alerts** in memory (including send failures)
- Updates automatically every 30 seconds
- Shows most recent alerts first
- Persists until device restart (not saved to flash)
- Helps diagnose recurring issues, delivery problems, or configuration errors

#### Email Configuration

Common SMTP server settings:

| Provider | SMTP Server | Port | Notes |
|----------|-------------|------|-------|
| Gmail | smtp.gmail.com | 587 | Requires [App Password](https://myaccount.google.com/apppasswords) |
| Outlook/Hotmail | smtp-mail.outlook.com | 587 | Use your regular password |
| Yahoo | smtp.mail.yahoo.com | 587 | May require app-specific password |
| Custom/Self-hosted | Your server | 587/465 | Check with your provider |
| Plain SMTP (no auth) | Your server | 25 | Leave username and password empty; whitelist-based access |

### OTA Updates

Update firmware over WiFi without USB cable after the initial upload.

#### Option A: Using Arduino CLI

```bash
# Compile first
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs FreezerProbe.ino

# Upload via network (replace [devicename] with your configured device name)
arduino-cli upload -p [devicename].local --fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs FreezerProbe.ino

# Alternative: Use IP address if mDNS not working
arduino-cli upload -p 192.168.1.100 --fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs FreezerProbe.ino
```

If you've set an OTA password, you'll be prompted to enter it.

#### Option B: Using Arduino IDE

1. **Select Network Port**:
   - **Tools → Port** → Look for your device name (e.g., "freezerprobe at 192.168.1.100")
   - It will appear as a network port (not a serial port like COM3 or /dev/ttyUSB0)

2. **Verify Settings**:
   - **Tools → Partition Scheme** → **"Minimal SPIFFS (1.9MB APP with OTA)"** ⚠️ Must match existing firmware

3. **Upload**:
   - Click the **Upload** button (→) or **Sketch → Upload**
   - If you've set an OTA password, you'll be prompted to enter it
   - Monitor progress in the Arduino IDE output window

**Important Notes**:
- Partition scheme **must match** the scheme used for the initial USB upload
- For best results, restart the ESP32 and upload within the first minute after boot
- Ensure strong WiFi signal during OTA updates
- If OTA fails, see "OTA Updates Failing" in Troubleshooting section below

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
- **Note**: If notifications are configured, you'll receive a high-priority alert when the sensor disconnects and a recovery notification when it reconnects

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
- Check serial monitor at 115200 baud for detailed SMTP debug output (connection attempts, server responses, library diagnostics)
- Some email providers block SMTP access by default - check provider settings
- Verify SMTP username/password if your server requires authentication
- For plain SMTP without authentication (port 25, whitelist-based): leave username and password **empty**. The system will skip authentication automatically.
- Delivery errors are recorded in the Alert History table and visible in the web UI

### Temperature Readings Incorrect

- Allow 30 seconds for sensor to stabilize
- Verify sensor is not damaged
- Check if sensor is touching metal (electrical isolation needed)
- Calibrate against known reference

### OTA Updates Failing

**"No response from device" after authentication**:

This indicates insufficient memory for the OTA process. The sketch is large (~1.5MB) and requires significant heap memory during updates.

**Solutions**:

1. **Ensure correct partition scheme**: You MUST use "Minimal SPIFFS (1.9MB APP with OTA)" partition scheme
   
   **Arduino CLI**:
   ```bash
   arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs FreezerProbe.ino
   arduino-cli upload --fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs -p [PORT] FreezerProbe.ino
   ```
   
   **Arduino IDE**:
   - **Tools → Partition Scheme** → **"Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)"**

2. **Check free memory**: Connect via serial monitor during OTA. The sketch now prints heap information:
   ```
   === Starting OTA Update ===
   Free Heap: XXXXX bytes
   Free Sketch Space: XXXXXX bytes
   ```
   You need at least 50-100KB of free heap for successful OTA.
   
   **Open Serial Monitor**:
   - **Arduino IDE**: Tools → Serial Monitor (set to 115200 baud)
   - **Arduino CLI**: `arduino-cli monitor -p [PORT] -c baudrate=115200`

3. **Restart the device**: Before attempting OTA, restart the ESP32 to free up maximum memory

4. **Reduce memory usage**: If still failing:
   - Clear browser cache before accessing the web interface
   - Avoid opening multiple browser tabs to the device
   - Wait a few minutes after boot before attempting OTA (let the device stabilize)

5. **Network stability**: Ensure strong WiFi signal during OTA update. Weak signal can cause timeouts.

**Serial Monitor Debugging**:
- **Arduino IDE**: Tools → Serial Monitor (set to 115200 baud)
- **Arduino CLI**: `arduino-cli monitor -p [PORT] -c baudrate=115200`

Connect during OTA to see detailed error messages and memory statistics.

## Technical Specifications

- **Temperature Reading Interval**: 10 seconds
- **Temperature History**: 288 readings (48 hours)
- **Alert History**: 50 most recent alerts (in memory, cleared on restart)
- **Alert Threshold**: 3 consecutive out-of-bounds readings (30 seconds) required before alerting
- **Alert Cooldown**: 5 minutes between temperature alerts
- **Sensor Retry Interval**: 60 seconds between sensor reconnection attempts
- **Prowl Priority Levels**: 2 (high) for alerts/delivery failures, 0 (normal) for test/recovery email, -2 (low) for recovery Prowl
- **NTP Sync Interval**: 5 minutes (validity check only; full reconfiguration only if time appears invalid)
- **NTP Servers**: pool.ntp.org, time.nist.gov
- **Timezone Support**: Configurable via POSIX TZ strings (25+ common timezones)
- **Heap Monitor Interval**: 1 hour (logs free heap to serial; sends alert if below 30 KB)
- **Heap Warning Threshold**: 30 KB (free heap below this triggers alerts)
- **Heap Alert Cooldown**: 1 hour between heap alerts
- **Web Server Port**: 80
- **OTA Port**: 3232
- **SMTP TCP Timeout**: 30 seconds
- **Sensor Resolution**: 12-bit (0.0625°C precision)
- **Device Name**: 1-63 characters, letters/numbers/spaces/hyphens/underscores/periods, auto-sanitized for DNS
- **mDNS Hostname**: Auto-generated from device name (lowercase, hyphens replace special chars)

## API Endpoints

- `GET /` - Web interface
- `GET /status` - JSON status (temp, thresholds, connection info)
  - **Note**: Secure fields (Prowl API key, OTA password, SMTP password) are never returned. Only boolean flags indicate if they're configured.
- `GET /history` - JSON temperature history (last 288 readings, 48 hours)
- `GET /alerts` - JSON alert history (last 50 alerts sent)
- `POST /settings` - Update settings (secure fields only updated if explicitly provided)
- `POST /reset` - Reset WiFi settings

## License

Open source - feel free to modify and improve!

## Support

For issues or questions:
- Check the serial monitor output (115200 baud)
- Verify all hardware connections
- See FUTURE_IMPROVEMENTS.md for enhancement ideas
