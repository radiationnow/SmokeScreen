# SmokeScreen
SmokeScreen is a NodeMCU/ESP32-based smoker temperature monitor/controller using MQTT for communication.

(Well, it'll be a controller some day.  For now, just a monitor.)

![SmokeScreen Schematic](/SmokeScreen%20Schematic.png)

Simple circuit: NodeMCU ESP32 board, 2x 15k resistors to form the bottom leg of voltage dividers, and a pair of panel mount 2.5mm TS (mono) jacks.  (Fritzing only had stereo, but use mono.) 

Probe coefficients are hand calibrated using a known-good instant read thermometer, an ice bath, a warm bath, and a boiling bath.  Probes tested with include Thermoworks High Temp Air, Thermoworks normal probe, and a clone ET-733 probe.  Pit thermistor voltage divider center measured on pin 32, meat thermistor on pin 33.

Loosely inspired by CapnBry's HeaterMeter - https://github.com/CapnBry/HeaterMeter/.
