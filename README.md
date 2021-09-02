# ESP32 MQTT library for Open eXtensible Rack System projects

Library to help with setting up an MQTT connection and subscribing to config and command topics needed for OXRS based devices.

Callbacks are provided for handling config and command messages and there are helpers for publishing events.

Attempts to connect to the broker will back-off incrementally if unsuccessful.
