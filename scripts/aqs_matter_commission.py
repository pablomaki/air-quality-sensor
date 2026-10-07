"""Join a sensor to the Matter server's fabric.

Used both for a factory-fresh sensor and, more commonly, to add this host as a
second administrator alongside Apple Home. In the latter case the sensor is
already on Thread, so commissioning runs over the network and needs no
Bluetooth: open pairing mode in the Home app to obtain a setup code, then pass
that code here.
"""

import argparse
import asyncio
import json
import os
import sys

import aiohttp

MATTER_SERVER_URL = os.getenv("MATTER_SERVER_URL", "ws://localhost:5580/ws")


def parse_args():
    """Parse the setup code and commissioning transport from the command line."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("code", help="Manual pairing code or MT:... QR payload")
    parser.add_argument(
        "--ble",
        action="store_true",
        help="Commission a factory-fresh sensor over Bluetooth instead of the network. "
        "Requires the Matter server to be started with --bluetooth-adapter.",
    )
    parser.add_argument("--url", default=MATTER_SERVER_URL, help="Matter server WebSocket URL")
    return parser.parse_args()


async def commission(url, code, network_only):
    """Send the commissioning command and return the server's reply."""
    async with aiohttp.ClientSession() as session:
        async with session.ws_connect(url, heartbeat=30) as ws:
            info = await ws.receive_json()
            print(f"Matter server fabric {info.get('fabric_id')}, SDK {info.get('sdk_version')}")

            await ws.send_json(
                {
                    "message_id": "commission",
                    "command": "commission_with_code",
                    "args": {"code": code, "network_only": network_only},
                }
            )
            print("Commissioning, this takes up to a couple of minutes...")

            while True:
                message = await ws.receive_json()
                if message.get("message_id") == "commission":
                    return message


def main():
    """Commission the sensor and report the node id it was given."""
    args = parse_args()
    code = args.code.replace("-", "").replace(" ", "")

    reply = asyncio.run(commission(args.url, code, network_only=not args.ble))

    if "error_code" in reply:
        print(f"Commissioning failed: {json.dumps(reply, indent=2)}", file=sys.stderr)
        return 1

    node_id = reply.get("result", {}).get("node_id")
    print(f"Commissioned as node {node_id}.")
    print(f"Set NODE_NAMES={node_id}:<name> in docker-compose.yml to label its metrics.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
