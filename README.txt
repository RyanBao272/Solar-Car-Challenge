SOLAR CAR TELEMETRY - SEAGULL SOLAR RACING

Live telemetry and low-voltage electronics for Staten Island Tech's solar
car at the 2026 Solar Car Challenge in Texas.

Result: 2nd place, Classic Division - 2026 Solar Car Challenge (460 miles)

HOW THE TELEMETRY WORKS
Motor controller --> Teensy 4.1 --> ESP32 --> driver LCD + Wi-Fi dashboard

code/teensyfulltelemtry
  - Reads the Fardriver motor controller over UART (19200 baud)
  - Decodes RPM, battery voltage, and current from its binary frames
  - Calculates speed and battery % from the controller's settings
  - Sends the data to the ESP32 as JSON 10 times a second

code/esp32telemetrydisplay
  - Shows the data on the driver's LCD in the steering wheel
  - Hosts a Wi-Fi dashboard that updates live over a WebSocket

CAR LOW-VOLTAGE SYSTEM (kicad-schematics/)
Full schematic: kicad-schematics/solar schematic.pdf

  - 24V aux battery, fuse block, 24V to 5V converter, fan, camera
  - Disconnect switches and contactors for the 96V motor and solar array
  - Front and back Arduinos talking over CAN for lights, horn, and signals

Telemetry PCB: teensy.kicad_sch / teensy.kicad_pcb
  (Teensy 4.1, ESP32, RS485-to-TTL converter, buck converter)


PHOTOS AND VIDEOS
See the images/ folder.
