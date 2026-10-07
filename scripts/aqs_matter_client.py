"""Bridge Matter sensor attributes to Prometheus and MQTT.

Connects to a python-matter-server instance over its WebSocket API, subscribes
to attribute updates for the commissioned Air Quality Sensor nodes and mirrors
every reported measurement into a Prometheus gauge and an MQTT topic.

The bridge is a passive listener: it never polls the devices. Matter pushes
attribute reports on its own subscription, so the freshness of a value is
governed by the firmware's CONFIG_REPORT_INTERVAL_MS and the server's
subscription intervals, not by anything here.
"""

import asyncio
import json
import os
import time
from dataclasses import dataclass
from datetime import datetime
from typing import Callable, Optional

import aiohttp
import paho.mqtt.client as mqtt
from prometheus_client import Gauge, start_http_server


def tprint(message):
    """Print message with a timestamp."""
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    print(f"[{timestamp}] {message}")


MATTER_SERVER_URL = os.getenv("MATTER_SERVER_URL", "ws://localhost:5580/ws")

# Optional "<node_id>:<name>,<node_id>:<name>" override. Nodes left out are
# named from their Matter NodeLabel, falling back to ProductName then node_<id>.
NODE_NAMES = os.getenv("NODE_NAMES", "")

BROKER = os.getenv("BROKER", "")
PORT = int(os.getenv("PORT", "1883"))
USERNAME = os.getenv("USERNAME", "")
PASSWORD = os.getenv("PASSWORD", "")

PROMETHEUS_PORT = int(os.getenv("PROMETHEUS_PORT", "8000"))

LABELS = ["sensor", "node_id"]

# Matter Basic Information cluster (0x0028) attributes used for naming.
BASIC_INFORMATION_CLUSTER = 40
PRODUCT_NAME_ATTRIBUTE = 3
NODE_LABEL_ATTRIBUTE = 5

AIR_QUALITY_NAMES = {
    0: "UNKNOWN",
    1: "GOOD",
    2: "FAIR",
    3: "MODERATE",
    4: "POOR",
    5: "VERY_POOR",
    6: "EXTREMELY_POOR",
}


def air_quality_name(value):
    """Map an AirQuality cluster enum value to its readable name."""
    return AIR_QUALITY_NAMES.get(int(value), "UNKNOWN")


@dataclass(frozen=True)
class Measurement:
    """A Matter attribute mirrored to one Prometheus gauge and MQTT topic.

    scale converts the cluster's raw integer representation to the unit named
    by the gauge. to_text, when set, produces the human readable payload
    published to MQTT in place of the numeric value.
    """

    name: str
    scale: float
    gauge: Gauge
    to_text: Optional[Callable[[float], str]] = None


# Keyed by (cluster_id, attribute_id) rather than by endpoint: the endpoint
# numbering is a firmware detail, while the cluster identifies the measurement.
MEASUREMENTS = {
    (1026, 0): Measurement(
        "temperature",
        0.01,
        Gauge("aqs_temperature_celsius", "Temperature in Celsius", LABELS),
    ),
    (1029, 0): Measurement(
        "humidity",
        0.01,
        Gauge("aqs_humidity_percent", "Relative humidity percentage", LABELS),
    ),
    (1027, 0): Measurement(
        "pressure",
        1.0,
        Gauge("aqs_pressure_hpa", "Atmospheric pressure in hPa", LABELS),
    ),
    (1037, 0): Measurement(
        "co2_concentration",
        1.0,
        Gauge("aqs_co2_ppm", "CO2 concentration in ppm", LABELS),
    ),
    (91, 0): Measurement(
        "air_quality",
        1.0,
        Gauge("aqs_air_quality", "Air quality rating, 0 unknown to 6 extremely poor", LABELS),
        air_quality_name,
    ),
}

NODE_AVAILABLE = Gauge("aqs_node_available", "Whether the Matter node is reachable", LABELS)
LAST_UPDATE = Gauge(
    "aqs_last_update_timestamp_seconds", "Unix time of the last attribute report", LABELS
)
BRIDGE_CONNECTED = Gauge(
    "aqs_bridge_connected", "Whether the bridge is connected to the Matter server"
)


def parse_node_names(spec):
    """Parse the NODE_NAMES environment override into a node id to name map."""
    names = {}
    for entry in spec.split(","):
        entry = entry.strip()
        if not entry:
            continue
        node_id, _, name = entry.partition(":")
        try:
            names[int(node_id)] = name.strip()
        except ValueError:
            tprint(f"Ignoring malformed NODE_NAMES entry: {entry!r}")
    return names


def setup_mqtt_client():
    """Create and start an MQTT client, or None when no broker is configured."""
    if not BROKER:
        tprint("No MQTT broker configured, publishing to Prometheus only.")
        return None

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    if USERNAME:
        client.username_pw_set(USERNAME, PASSWORD)
    client.on_connect = lambda c, u, f, rc, p: tprint(f"MQTT connect result: {rc}")
    client.connect_async(BROKER, PORT, 60)
    client.loop_start()
    return client


class MatterBridge:
    """Mirrors attribute reports from a python-matter-server into Prometheus and MQTT."""

    def __init__(self, mqtt_client):
        self._mqtt = mqtt_client
        self._overrides = parse_node_names(NODE_NAMES)
        self._names = {}

    async def run(self):
        """Connect to the Matter server, reconnecting with backoff on failure."""
        backoff = 1
        async with aiohttp.ClientSession() as session:
            while True:
                try:
                    await self._session(session)
                    backoff = 1
                except Exception as e:
                    tprint(f"Matter server connection lost: {e}")
                finally:
                    BRIDGE_CONNECTED.set(0)
                tprint(f"Reconnecting in {backoff} s.")
                await asyncio.sleep(backoff)
                backoff = min(backoff * 2, 60)

    async def _session(self, session):
        """Hold one WebSocket session, seeding state then streaming updates."""
        async with session.ws_connect(MATTER_SERVER_URL, heartbeat=30) as ws:
            info = await ws.receive_json()
            tprint(f"Connected to Matter server, SDK {info.get('sdk_version')}")

            await ws.send_json({"message_id": "1", "command": "start_listening", "args": {}})
            nodes = await self._await_result(ws, "1")
            BRIDGE_CONNECTED.set(1)

            for node in nodes:
                self._seed_node(node)

            async for msg in ws:
                if msg.type != aiohttp.WSMsgType.TEXT:
                    break
                self._handle_message(json.loads(msg.data))

    async def _await_result(self, ws, message_id):
        """Read messages until the reply to message_id arrives, and return it."""
        while True:
            message = await ws.receive_json()
            if message.get("message_id") != message_id:
                continue
            if "error_code" in message:
                raise RuntimeError(f"start_listening failed: {message}")
            return message.get("result", [])

    def _handle_message(self, message):
        """Dispatch one server push, ignoring events the bridge does not use."""
        event = message.get("event")
        data = message.get("data")

        if event == "attribute_updated":
            node_id, path, value = data
            self._update_attribute(int(node_id), path, value)
        elif event in ("node_added", "node_updated"):
            self._seed_node(data)
        elif event == "node_removed":
            self._forget_node(int(data))

    def _seed_node(self, node):
        """Adopt a node's current attribute snapshot and reachability."""
        node_id = int(node["node_id"])
        attributes = node.get("attributes", {})
        self._names[node_id] = self._resolve_name(node_id, attributes)
        tprint(f"Tracking node {node_id} as {self._names[node_id]!r}")

        NODE_AVAILABLE.labels(*self._labels(node_id)).set(1 if node.get("available") else 0)
        for path, value in attributes.items():
            self._update_attribute(node_id, path, value, announce=False)

    def _forget_node(self, node_id):
        """Drop every series belonging to a node that left the fabric."""
        labels = self._labels(node_id)
        for measurement in MEASUREMENTS.values():
            self._clear(measurement.gauge, labels)
        self._clear(NODE_AVAILABLE, labels)
        self._clear(LAST_UPDATE, labels)
        self._names.pop(node_id, None)
        tprint(f"Node {node_id} removed from the fabric.")

    def _resolve_name(self, node_id, attributes):
        """Pick the metric label for a node from the override or its own labels."""
        if node_id in self._overrides:
            return self._overrides[node_id]
        for attribute in (NODE_LABEL_ATTRIBUTE, PRODUCT_NAME_ATTRIBUTE):
            value = attributes.get(f"0/{BASIC_INFORMATION_CLUSTER}/{attribute}")
            if value:
                return str(value)
        return f"node_{node_id}"

    def _labels(self, node_id):
        """Return the label values used for every series of a node."""
        return (self._names.get(node_id, f"node_{node_id}"), str(node_id))

    def _update_attribute(self, node_id, path, value, announce=True):
        """Apply one attribute report to its gauge and MQTT topic."""
        try:
            _, cluster_id, attribute_id = (int(part) for part in path.split("/"))
        except ValueError:
            return

        measurement = MEASUREMENTS.get((cluster_id, attribute_id))
        if measurement is None:
            return

        labels = self._labels(node_id)

        # A null MeasuredValue means the firmware has no valid sample, which is
        # not the same as a zero: drop the series so it reads as absent.
        if value is None:
            self._clear(measurement.gauge, labels)
            if announce:
                tprint(f"{labels[0]} {measurement.name}: no valid measurement")
            return

        scaled = float(value) * measurement.scale
        measurement.gauge.labels(*labels).set(scaled)
        LAST_UPDATE.labels(*labels).set(time.time())

        payload = measurement.to_text(scaled) if measurement.to_text else round(scaled, 2)
        if announce:
            tprint(f"{labels[0]} {measurement.name}: {payload}")
        self._publish(f"{labels[0]}/{measurement.name}", payload)

    def _publish(self, topic, payload):
        """Publish one value to MQTT when a broker is configured."""
        if self._mqtt is None:
            return
        try:
            self._mqtt.publish(topic, payload)
        except Exception as e:
            tprint(f"Publishing to {topic} failed: {e}")

    @staticmethod
    def _clear(gauge, labels):
        """Remove a labelled series, tolerating one that was never set."""
        try:
            gauge.remove(*labels)
        except KeyError:
            pass


async def main():
    """Start the metrics endpoint and run the bridge until interrupted."""
    start_http_server(PROMETHEUS_PORT)
    tprint(f"Prometheus metrics on :{PROMETHEUS_PORT}/metrics")
    await MatterBridge(setup_mqtt_client()).run()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        tprint("Stopped by user.")
