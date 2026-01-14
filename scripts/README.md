# Scripts

This directory contains utility scripts for interacting with the air quality sensor. Each advertised service is read by the script and published to a MQTT topic as is.

## Files

- **aqs_ble_client.py**: A Python script for communicating with the air quality sensor over BLE (Bluetooth Low Energy) and publishing the data to MQTT topics.

## Dependencies

### Python Libraries
- **`paho-mqtt`** - MQTT communication
- **`bleak`** - Bluetooth Low Energy (BLE) communication  
- **`prometheus-client`** - Metrics collection and HTTP server

### System Requirements
- **Python 3.9+**
- **BlueZ** (Linux Bluetooth stack)
- **Bluetooth adapter** with BLE support

## Installation & setup

Install system dependencies
```bash
sudo apt update
sudo apt install python3 python3-pip bluetooth bluez
sudo apt install mosquitto
```

Install python dependencies
```bash
pip install paho-mqtt bleak prometheus-client
```

Setup bluetooth
```bash
sudo systemctl enable bluetooth
sudo systemctl start bluetooth
sudo usermod -a -G bluetooth $USER
```

Pair bluetooth device with the AQS sensor by starting up the AQS sensor so that it enters the pairing mode (indicated by the text "Pairing" on the screen). Then use bluetoothctl to pair the device. Check the AQS device bluetooth MAC address using minicom or similar tool when it is connected to a PC.

```bash
minicom -D /dev/ttyACM0
```

Pairing can be done using `bluetoothctl` or any other bluetooth management tool. Start the bluetoothctl tool and use the following commands:

```bash
sudo bluetoothctl

# Enable agent (handles pairing)
[bluetooth]# agent on
[bluetooth]# default-agent

# Scan for devices
[bluetooth]# scan on

# When you see your device, pair it
[bluetooth]# pair XX:XX:XX:XX:XX:XX

# Trust the device for automatic reconnection
[bluetooth]# trust XX:XX:XX:XX:XX:XX
```

Note that this can be only done when the device is in pairing mode (indicated by the text "Pairing" on the screen).

Setup mosquitto MQTT broker (if you don't have one already)
```bash
sudo systemctl enable mosquitto
sudo systemctl start mosquitto
```

Setup password authentication for mosquitto
```bash
sudo mosquitto_passwd -c /etc/mosquitto/passwd sensor_client
```
And add the following lines to the mosquitto configuration file (e.g., `/etc/mosquitto/mosquitto.conf`):

```bash
allow_anonymous false
password_file /etc/mosquitto/passwd sensor_client
```

Restart mosquitto to apply changes
```bash
sudo systemctl restart mosquitto
```

Setup prometheus metrics server by editing `/etc/prometheus/prometheus.yml` and adding the following job:

```yaml
  - job_name: 'aqs_ble_client'
    static_configs:
      - targets: ['localhost:8000'] # Adjust port or add more entries if necessary
```

In addition, change the retention time and storage settings as needed byu editing `/etc/default/prometheus`. For one year, the content could look like this:

```bash
ARGS="--storage.tsdb.retention.time=365d"
```

Then start/restart prometheus:
```bash
sudo systemctl enable mosquitto
sudo systemctl start mosquitto
```

Lastly, set up grafana to visualize the data collected by prometheus.
```bash
sudo apt-get install -y apt-transport-https software-properties-common
sudo apt-get install -y grafana
sudo systemctl enable grafana-server
sudo systemctl start grafana-server
```

Grafana should now be accessible at `http://<your_server_ip>:3000`. Add prometheus as a data source in grafana and create dashboards as needed.

## Usage
When everything is set up, run the script with the following command:

```bash
python aqs_ble_client.py
```

Make sure to configure the MQTT settings in the script before running it. The script will scan for BLE devices, connect to the air quality sensor, and publish the sensor data to the specified MQTT topics.

### Published Topics
The script publishes the following topics:
- `{SENSOR_NAME}/battery_level`: Battery level of the sensor (%).
- `{SENSOR_NAME}/pressure`: Pressure readings from the sensor (hPa).
- `{SENSOR_NAME}/temperature`: Temperature readings from the sensor (°C).
- `{SENSOR_NAME}/humidity`: Humidity readings from the sensor (%RH).
- `{SENSOR_NAME}/co2_concentration`: CO2 concentration readings from the sensor (ppm).
- `{SENSOR_NAME}/voc_index`: VOC index readings from the sensor (0-500).
- `{SENSOR_NAME}/iaq_index`: IAQ index readings from the sensor (0-500).