"""Check for and install firmware updates on the sensors over Matter OTA.

The Matter server acts as the OTA provider. It offers the images described by
the manifests in its --ota-provider-dir, matched to a sensor by vendor and
product ID, so each variant only ever sees its own images. The image travels
over Thread, which for a full image takes several minutes.
"""

import argparse
import asyncio
import json
import os
import sys

import aiohttp

MATTER_SERVER_URL = os.getenv("MATTER_SERVER_URL", "ws://localhost:5580/ws")


def parse_args():
    """Parse the subcommand and its arguments from the command line."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default=MATTER_SERVER_URL, help="Matter server WebSocket URL")

    commands = parser.add_subparsers(dest="command", required=True)

    check = commands.add_parser("check", help="Show the update offered to a node, if any")
    check.add_argument("node_id", type=int, help="Node id to check")

    update = commands.add_parser("update", help="Install an update on a node")
    update.add_argument("node_id", type=int, help="Node id to update")
    update.add_argument("version", nargs="?", help="Software version to install, the offered one if omitted")

    return parser.parse_args()


async def request(ws, command, args=None):
    """Send one command and return its result, raising on a server error."""
    await ws.send_json({"message_id": command, "command": command, "args": args or {}})
    while True:
        message = await ws.receive_json()
        if message.get("message_id") != command:
            continue
        if "error_code" in message:
            raise RuntimeError(f"{command} failed: {json.dumps(message)}")
        return message.get("result")


async def run(args):
    """Connect to the Matter server and carry out the requested subcommand."""
    async with aiohttp.ClientSession() as session:
        async with session.ws_connect(args.url, heartbeat=30) as ws:
            await ws.receive_json()

            offered = await request(ws, "check_node_update", {"node_id": args.node_id})
            if offered is None:
                print(f"No update available for node {args.node_id}.")
                return
            print(
                f"Node {args.node_id}: version {offered['software_version_string']} "
                f"({offered['software_version']}) available from {offered['update_source']}."
            )
            if args.command == "check":
                return

            version = args.version
            if version is None:
                version = offered["software_version"]
            elif version.isdigit():
                version = int(version)

            print("Installing, this takes several minutes over Thread...")
            await request(ws, "update_node", {"node_id": args.node_id, "software_version": version})
            print(f"Node {args.node_id} updated to {version}.")


def main():
    """Run the requested subcommand, reporting any server error."""
    args = parse_args()
    try:
        asyncio.run(run(args))
    except RuntimeError as e:
        print(e, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
