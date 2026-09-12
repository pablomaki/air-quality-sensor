# Scripts

Utility scripts for reading the air quality sensor over Matter and forwarding its
measurements to Prometheus and MQTT.

The sensor is a Matter over Thread device. It has no vendor cloud and no local
HTTP API, so the only way to read it is to speak Matter to it. This host does
that by joining the sensor's Matter fabric as a second administrator, alongside
Apple Home - Matter's multi-admin support means both controllers see the device
at the same time and neither disturbs the other.

Reading the values back out of Apple Home instead is not possible: Apple exposes
no local API for accessory state, so the data has to come from the device.

## Files

- **aqs_matter_client.py**: Subscribes to the sensors' Matter attributes through
  a [python-matter-server](https://github.com/home-assistant-libs/python-matter-server)
  instance, exposes them as Prometheus metrics and publishes them to MQTT.
- **aqs_matter_commission.py**: Joins a sensor to this host's Matter fabric.

Both scripts are run by the containers defined in [docker/](../docker/); see that
directory's README for the deployment.

## Exported metrics

Every metric carries `sensor` and `node_id` labels, so one process serves all
sensors on the fabric.

| Metric | Unit | Matter cluster |
| --- | --- | --- |
| `aqs_temperature_celsius` | °C | Temperature Measurement (0x0402) |
| `aqs_humidity_percent` | % RH | Relative Humidity Measurement (0x0405) |
| `aqs_pressure_hpa` | hPa | Pressure Measurement (0x0403) |
| `aqs_co2_ppm` | ppm | CO2 Concentration Measurement (0x040D) |
| `aqs_air_quality` | 0 unknown … 6 extremely poor | Air Quality (0x005B) |
| `aqs_node_available` | 0 or 1 | reachability, from the Matter server |
| `aqs_last_update_timestamp_seconds` | unix time | last attribute report received |
| `aqs_bridge_connected` | 0 or 1 | bridge to Matter server connection |

A measurement the firmware reports as null - no valid sample - is removed from
the metrics rather than published as zero, so it reads as absent in Prometheus.

The same values are published to MQTT as `<sensor name>/<measurement>`, matching
the topic layout the old BLE client used, with `air_quality` published as its
readable name for [MQTTThing](https://github.com/arachnetech/homebridge-mqttthing).

The VOC and IAQ indices the BLE firmware exposed are not available: the firmware
folds them into the Air Quality rating and does not publish the raw index over
Matter. Battery level is likewise not published.

## Dependencies

- **Python 3.9+**
- **`aiohttp`** - WebSocket client for the Matter server
- **`paho-mqtt`** - MQTT communication
- **`prometheus-client`** - Metrics collection and HTTP server
- A running **python-matter-server**, see [docker/](../docker/)

## Host setup

The sensor is a Thread minimal end device with no IP connectivity of its own.
This host reaches it over IPv6 through a Thread border router - an Apple TV or
HomePod if the sensor is in Apple Home - which advertises a route to the Thread
mesh prefix on the LAN. Linux ignores those advertisements by default, so enable
them for the LAN interface:

```bash
sudo tee /etc/sysctl.d/99-thread.conf <<'EOF'
net.ipv6.conf.eth0.accept_ra = 2
net.ipv6.conf.eth0.accept_ra_rt_info_max_plen = 64
EOF
sudo sysctl -p /etc/sysctl.d/99-thread.conf
```

Replace `eth0` with the actual interface. Verify that a route to the mesh prefix
appears, which needs the border router to be up:

```bash
ip -6 route show | grep -i fd
```

Containers speaking Matter must use host networking; IPv6 and mDNS do not
survive Docker's default bridge.

Matter discovers devices over mDNS, and its Linux implementation does that
through the host's Avahi daemon rather than resolving on its own. Avahi has to
be installed and running, and its D-Bus socket has to be visible to the Matter
server container - which is what the `/run/dbus` mount in the compose file is
for:

```bash
sudo apt install avahi-daemon
sudo systemctl enable --now avahi-daemon
```

## Adding a sensor to this host's fabric

If the sensor is already in Apple Home, open pairing mode there rather than
factory resetting it - that keeps the Apple Home pairing intact and adds this
host as a second administrator:

1. In the Home app, open the accessory's settings.
2. Choose *Turn On Pairing Mode* and note the setup code it shows.
3. Run the commissioning helper with that code:

```bash
docker exec -it aqs_matter_client python aqs_matter_commission.py 1234-567-8901
```

The helper prints the node id the sensor was assigned. Put it in `NODE_NAMES` in
[docker-compose.yml](../docker/docker-compose.yml) to control the `sensor` label
its metrics carry, then restart the bridge.

For a factory-fresh sensor that is not in any fabric yet, commissioning has to
run over Bluetooth and has to supply the Thread network credentials first. Start
the Matter server with `--bluetooth-adapter 0`, push the dataset, then pair with
the code from the sensor's onboarding label:

```bash
docker exec -it aqs_matter_client python aqs_matter_commission.py --ble 1234-567-8901
```

## Troubleshooting commissioning

`Commission with code failed` means the Matter server never completed the
pairing. The reason is in its own log, which is far more specific than the
helper's error:

```bash
docker compose logs --tail=100 matter-server
```

The usual causes, all of which the helper cannot distinguish between:

- **The device was never discovered.** Check that Avahi is running on the host
  and that the Matter server container has `/run/dbus` mounted. Confirm the
  sensor is actually advertising while pairing mode is open:

  ```bash
  avahi-browse -rt _matterc._udp
  ```

  An empty result means either pairing mode is closed or the border router is
  not relaying the Thread mesh's mDNS onto the LAN.

- **The device was discovered but could not be reached.** This is the IPv6 route
  described above: without it the host resolves the sensor's address and then
  cannot route to it. Verify with `ip -6 route show | grep -i fd`, and ping the
  address `avahi-browse` reported.

- **The commissioning window expired.** It is open for a limited time, and the
  code changes every time it is reopened. Open pairing mode in the Home app
  immediately before running the helper, not minutes ahead.

- **The fabric limit was reached.** The firmware allows five, so this only
  happens after repeated failed attempts have left stale fabrics behind.

## Running outside Docker

```bash
pip install aiohttp paho-mqtt prometheus-client
MATTER_SERVER_URL=ws://localhost:5580/ws PROMETHEUS_PORT=8000 python -u aqs_matter_client.py
```

## Configuration

Both scripts are configured through environment variables.

| Variable | Default | Description |
| --- | --- | --- |
| `MATTER_SERVER_URL` | `ws://localhost:5580/ws` | Matter server WebSocket API |
| `NODE_NAMES` | *(empty)* | `<node id>:<name>` pairs, comma separated |
| `PROMETHEUS_PORT` | `8000` | Port the metrics endpoint listens on |
| `BROKER` | *(empty)* | MQTT broker address; empty disables MQTT |
| `PORT` | `1883` | MQTT broker port |
| `USERNAME` | *(empty)* | MQTT username |
| `PASSWORD` | *(empty)* | MQTT password |

Without `NODE_NAMES` a sensor is named from its Matter NodeLabel, falling back to
its ProductName and then to `node_<id>`.

## Notes on reporting rate

The bridge never polls. It holds a Matter subscription and the sensors push
reports to it, so how often values change is set by the firmware's
`CONFIG_REPORT_INTERVAL_MS` and by the subscription intervals the Matter server
negotiates. Scraping Prometheus faster than the firmware reports just re-reads
the last value; use `aqs_last_update_timestamp_seconds` to alert on a sensor that
has gone quiet.
