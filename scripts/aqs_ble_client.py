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

# BLE settings
TARGET_ADDRESS = os.getenv("SENSOR_ADDRESS", "FF:33:0F:C1:C7:BF")
CHARACTERISTICS = {
    "Battery level": {
        "uuid": "00002a19-0000-1000-8000-00805f9b34fb",
        "scale": 1,
        "value": 0.0,
    },
    "Pressure": {
        "uuid": "00002a6d-0000-1000-8000-00805f9b34fb",
        "scale": 0.01,
        "value": 0.0,
    },
    "VOC index": {
        "uuid": "00002be7-0000-1000-8000-00805f9b34fb",
        "scale": 0.1,
        "value": 0.0,
    },
    "Temperature": {
        "uuid": "00002a6e-0000-1000-8000-00805f9b34fb",
        "scale": 0.01,
        "value": 0.0,
    },
    "CO2 concentration": {
        "uuid": "00002b8c-0000-1000-8000-00805f9b34fb",
        "scale": 0.1,
        "value": 0.0,
    },
    "Humidity": {
        "uuid": "00002a6f-0000-1000-8000-00805f9b34fb",
        "scale": 0.01,
        "value": 0.0,
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
            topic = f"{SENSOR_NAME}/{name.replace(' ', '_').lower()}"
            print(f"Value for {name}: {val:.1f}, publishing to {topic}")
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
