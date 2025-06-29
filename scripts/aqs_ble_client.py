import asyncio
from bleak import BleakClient, BleakScanner
from bleak.exc import BleakError
import paho.mqtt.client as mqtt
from paho.mqtt.client import CallbackAPIVersion
import os

# Configuration
SENSOR_NAME = os.getenv("SENSOR_NAME", "air_quality_sensor_001")

# MQTT settings
BROKER = os.getenv("BROKER", "192.168.0.3")
PORT = int(os.getenv("PORT", "1883"))
USERNAME = os.getenv("USERNAME", "admin")
PASSWORD = os.getenv("PASSWORD", "1234")

# Characteristic specific optional functions for processing data
def air_quality_from_voc_index(voc_index):
    """Convert VOC index to air quality level."""
    if voc_index >= 0 and voc_index < 80:
        return "EXCELLENT"
    if voc_index >= 80 and voc_index < 120:
        return "GOOD"
    if voc_index >= 120 and voc_index < 200:
        return "FAIR"
    if voc_index >= 200 and voc_index < 300:
        return "INFERIOR"
    if voc_index >= 300 and voc_index <= 500:
        return "POOR"
    else:
        return "UNKNOWN"

# BLE settings
TARGET_ADDRESS = os.getenv("SENSOR_ADDRESS", "FF:33:0F:C1:C7:BF")
CHARACTERISTICS = {
    "battery_level": {
        "uuid": "00002a19-0000-1000-8000-00805f9b34fb",
        "scale": 1,
        "value": 0.0,
        "action": None,
    },
    "pressure": {
        "uuid": "00002a6d-0000-1000-8000-00805f9b34fb",
        "scale": 0.01,
        "value": 0.0,
        "action": None,
    },
    "voc_air_quality": {
        "uuid": "8caa4e2a-31ef-4e50-a19d-bdfd38918119",
        "scale": 0.1,
        "value": 0.0,
        "action": air_quality_from_voc_index,
    },
    "Temperature": {
        "uuid": "00002a6e-0000-1000-8000-00805f9b34fb",
        "scale": 0.01,
        "value": 0.0,
        "action": None,
    },
    "CO2 Concentration": {
        "uuid": "00002b8c-0000-1000-8000-00805f9b34fb",
        "scale": 0.1,
        "value": 0.0,
        "action": None,
    },
    "Humidity": {
        "uuid": "00002a6f-0000-1000-8000-00805f9b34fb",
        "scale": 0.01,
        "value": 0.0,
        "action": None,
    }
}

def on_connect(client, userdata, flags, rc, properties):
    if (rc == 0):
        print(f"Connected to MQTT broker with result code {rc}")
    else:
        print(f"Failed to connect, return code {rc}")

async def setup_mqtt_client():
    mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    mqtt_client.username_pw_set(USERNAME, PASSWORD)
    mqtt_client.on_connect = on_connect
    mqtt_client.connect(BROKER, PORT, 60)

    #  Start background thread for MQTT networking
    mqtt_client.loop_start()

    return mqtt_client

async def connect_and_read():
    try:
        async with BleakClient(TARGET_ADDRESS, timeout=60) as client:
            print("Connected to device.")
            for name, char in CHARACTERISTICS.items():
                try:
                    raw_value = await client.read_gatt_char(char["uuid"])
                    int_value = int.from_bytes(raw_value, byteorder='little')
                    scaled_value = int_value * char["scale"]
                    if char["action"]:
                        processed_value = char["action"](scaled_value)
                        char["value"] = processed_value
                    else:
                        char["value"] = scaled_value 
                except Exception as e:
                    print(f"Error reading {name}: {e}")
    except BleakError as e:
        print(f"Connection failed: {e}")
        return False
    return True

async def publish_mqtt_data(mqtt_client):
    try:
        if not mqtt_client.is_connected():
            print("MQTT client not connected. Attempting reconnect...")
            mqtt_client.reconnect()
            print("Reconnected to MQTT broker.")

        for name, char in CHARACTERISTICS.items():
            val = char["value"]
            topic = f"{SENSOR_NAME}/{name}"
            if isinstance(val, float):
                if val == -1.0:
                    print(f"Value for {name} is invalid, not publishing to {topic}")
                    continue
                print(f"Value for {name}: {val:.1f}, publishing to {topic}")
            else:
                print(f"Value for {name}: {val}, publishing to {topic}")
            mqtt_client.publish(topic, char["value"])

    except Exception as e:
        print(f"Publishing data failed: {e}")

async def main():
    mqtt_client = await setup_mqtt_client()
    while True:
        success = await connect_and_read()
        if (success):
            await publish_mqtt_data(mqtt_client)

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("Stopped by user.")
