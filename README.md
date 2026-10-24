# Astronomy Desk

7 inch touchscreen desk. Clock, date, weather, moon, tonight list, star map, planets, comets, asteroids, meteor showers, launches, the ISS and Tiangong, telescopes, space weather, and search. A photograph is shown with the facts when this desk has one.

Board: Waveshare ESP32-S3-Touch-LCD-7, 800 by 480.

Version on the desk: 1.0

## Folders

`ota` is what the desk downloads.
CHECK reads `ota/version.txt`.
INSTALL reads `ota/astro_7in.bin`.

`sketch/astro_desk` is the sketch. Open `astro_desk.ino` in that folder. The folder name matches the sketch name. The star list, asteroid orbits, satellite paths, world picture, and photo names are in that same folder.

`libraries` has three folders.
GFX Library for Arduino 1.6.8.
TJpg_Decoder 1.1.0.
ArduinoJson 7.4.2 by Benoit Blanchon. That is the library kept for these desks. It is not Arduino_JSON.
The sketch includes GFX and TJpg_Decoder. ArduinoJson is in this folder so the same copy is here.
Copy those folders into the Arduino libraries folder.

`ARDUINO-IDE.txt` is the Tools menu for this firmware.

`Astronomy-Desk-User-Manual.pdf` is the printed manual. `Astronomy-Desk-User-Manual.txt` is the same manual.

`catalogue.zip` is the photographs. Unzip it and leave the catalogue folder as it is. A fact page shows a photograph when that file is there.

## Update from the desk

MORE, then ABOUT, then CHECK FOR UPDATES.
If `ota/version.txt` is a higher number than the version on the desk, tap INSTALL.
Leave the desk plugged in until it restarts.

A desk already on 1.0 says the newest firmware is already installed.
Raise the version in the sketch and in `ota/version.txt` together when you ship a newer bin.

The desk looks at this repo:

https://github.com/stefi01/Astronomy-7-inch-display

That address is the one in the sketch. CHECK reads `ota/version.txt`. INSTALL reads `ota/astro_7in.bin`.
