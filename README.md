Smart Medicine Dispenser is an automated medicine-dispensing system built on an ESP32 microcontroller, designed to remind users and dispense medication at scheduled times.

Features
- Dual medication timer scheduling via onboard OLED display and physical buttons
- Servo-driven dispensing mechanism
- Real-time clock (RTC) for accurate scheduling
- Wi-Fi access point with a live web dashboard to monitor dispensing status remotely

Hardware Used
- ESP32
- SSD1306 OLED display
- DS3231 RTC module
- Servo motor
- Buzzer
- Push buttons

How it works:
The device tracks two medication times set through onboard controls. At the scheduled time, it sounds an alert and sweeps a servo motor to dispense 
the dose, while a built-in web server lets you check status (pending, dispensing, completed) from any browser connected to its Wi-Fi hotspot.

My Role
Collaborated with a team to design, test, and debug the embedded system, 
including sensor/actuator integration and scheduling logic.
