# Docker

Deployment for the host that reads the air quality sensors over Matter and
forwards their measurements to Prometheus and MQTT.

## Files

- **docker-compose.yml**: Defines the Matter server and the bridge service.
- **Dockerfile**: Builds the image for the scripts in [scripts/](../scripts/).

## Services

- **matter-server**: Upstream
  [python-matter-server](https://github.com/home-assistant-libs/python-matter-server).
  It owns the Matter fabric, holds the subscriptions to the sensors and exposes
  them on a WebSocket API at port 5580. Its fabric credentials live in
  `./matter-server-data`, so back that directory up - losing it means
  re-commissioning every sensor.
- **aqs_matter_client**: The bridge, serving Prometheus metrics on port 8000.

Both run with `network_mode: host`, which is required: Matter needs IPv6 and
mDNS on the LAN to reach the sensors through the Thread border router, and
neither survives Docker's default bridge network. See the
[scripts README](../scripts/README.md) for the IPv6 route configuration the host
itself needs.

Unlike the old BLE setup there is one bridge container for all sensors, not one
per sensor - they share a single Matter fabric, and metrics are told apart by
the `sensor` label.

## Usage

Review the environment variables in `docker-compose.yml`, in particular
`NODE_NAMES` and the MQTT broker settings, then start both services:

```bash
docker compose up -d --build
```

Commission the sensors into the fabric before expecting any metrics; the
[scripts README](../scripts/README.md) covers that. To check that the bridge
works, follow its logs:

```bash
docker compose logs -f aqs_matter_client
```

Or scrape the metrics endpoint directly:

```bash
curl -s http://localhost:8000/metrics | grep aqs_
```
