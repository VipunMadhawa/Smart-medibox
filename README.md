# Smart-medibox
ESP32 Medibox - Medication Reminder System

A fully-featured, simulated medication reminder system built on an ESP32 microcontroller using the Wokwi online simulator. This project was developed as an individual assignment for the Embedded Systems and Applications course (EN2853).

📖 Table of Contents
Project Overview

Features

Hardware Simulation Components

Software & Libraries

Getting Started

Prerequisites

Simulation Instructions

Usage

Menu Navigation

Setting the Timezone

Managing Alarms

Environmental Warnings

Project Structure

Demo Video

Author

License

🚀 Project Overview
The Medibox is a smart device designed to help users manage their medication schedule. It connects to the internet to fetch accurate time, allows for custom alarm settings, and monitors environmental conditions to ensure medication is stored correctly. Alarms are triggered with visual and auditory indicators and can be snoozed or stopped via a user-friendly interface.

✨ Features
🕒 NTP Time Synchronization: Fetches the current time from an NTP server over Wi-Fi and displays it on an OLED screen in the user's selected time zone.

⚙️ Interactive Menu System: Navigate through an intuitive menu using buttons to access all features.

🔊 Smart Alarm System: Audible (buzzer) and visual (OLED) alerts with snooze and stop functionality.

🌡️ Environmental Monitoring: Monitors temperature and humidity and provides warnings if conditions are outside the healthy range for medication storage.

🧩 Hardware Simulation Components
This project is simulated in Wokwi and uses the following virtual components:

Microcontroller: ESP32

Display: SSD1306 128x64 OLED (I2C)

Input: Two Push Buttons (for navigation/selection)

Sensors: DHT11 (Temperature & Humidity)

Output: Piezo Buzzer

LED: One RGB LED (for status indicators)

💻 Software & Libraries
Programming Language: C++ (Arduino Framework)

Key Libraries:

WiFi.h (Built-in) for internet connectivity.

NTPClient.h by Fabrice Weinberg for fetching network time.

SSD1306.h by ThingPulse for the OLED display.

DHT.h by Adafruit for reading the DHT11 sensor.

Simulator: Wokwi ESP32 Simulator


