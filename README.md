# BLE Treadmill-to-Garmin Bridge (RSC Sensor)

A simple ESP32-based firmware that acts as a bridge between a Bluetooth-enabled treadmill (specifically the **Vigor MTT2520DC**) and **Garmin** watches (like the Fenix 8). 

It solves the common problem of inaccurate indoor distance tracking by feeding real-time speed and cadence data from the treadmill directly to your watch via the standard **Running Speed and Cadence (RSC)** protocol.

## Why this exists?
Many modern treadmills have Bluetooth, but they often use proprietary or FTMS protocols that Garmin watches may not communicate with perfectly. This results in significant discrepancies between the treadmill's distance and what the watch records, even after calibration. This project turns an inexpensive ESP32 board into a "smart foot pod" that talks to both.

## Features
- **Real-time Synchronization:** Connects to the treadmill via BLE as a client.
- **Standard RSC Protocol:** Emulates an RSC Sensor (Foot Pod) that is natively supported by almost all sports watches.
- **High Accuracy:** Eliminates the need for watch-based accelerometer calibration.

## Prerequisites
- **ESP32 Dev Board** (tested on ESP32-C6).
- **Arduino IDE** with ESP32 board support installed.

## Getting Started

### 1. Identify Treadmill MAC Address
Use an app like **nRF Connect** on your smartphone to scan for your treadmill. Note down its MAC address (e.g., `d9:ed:78:01:b2:dc`).

### 2. Configuration
Open the `.ino` file and update the following line with your treadmill's MAC address:
```cpp
static std::string treadmillMac = "YOUR_MAC_ADDRESS_HERE";
```

### 3. Build and Flash
In Arduino IDE, select your board (e.g., ESP32C6 Dev Module).
Choose the correct Serial Port.
Click Upload.

### 4. Pair with Garmin
On your Garmin watch, go to Sensors & Accessories > Add New.
Search for a Foot Pod or RSC Sensor.
Select the device (it should appear as "RSC Pod" or "RSC Sensor").
Ensure Speed and Distance are set to "Always" in the sensor settings on your watch.

### Limitations
Inclination: Currently, this bridge does not support inclination data as it uses the RSC protocol (FTMS implementation for Garmin is a work in progress).
Compatibility: While designed for the Vigor MTT2520DC, it may work with other treadmills using similar BLE GATT services (Service 0x1826, Characteristic 0x2ACD).
