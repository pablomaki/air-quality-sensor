"""Inspect and prune the nodes on the Matter server's fabric.

Failed commissioning attempts can leave node entries behind in the Matter
server's storage that no longer refer to a reachable device. This lists what the
fabric holds and removes the entries that are no longer wanted.
"""

import argparse
import asyncio
import json
import os
import sys

import aiohttp

MATTER_SERVER_URL = os.getenv("MATTER_SERVER_URL", "ws://localhost:5580/ws")

BASIC_INFORMATION_CLUSTER = 40
PRODUCT_NAME_ATTRIBUTE = 3
NODE_LABEL_ATTRIBUTE = 5


def parse_args():
    """Parse the subcommand and its arguments from the command line."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default=MATTER_SERVER_URL, help="Matter server WebSocket URL")

    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("list", help="List the nodes on the fabric")

    remove = commands.add_parser("remove", help="Remove nodes from the fabric")
    remove.add_argument("node_ids", nargs="+", type=int, help="Node ids to remove")

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


def describe(node):
    """Build a one line summary of a node for the listing."""
    node_id = node["node_id"]
    attributes = node.get("attributes", {})
    name = (
        attributes.get(f"0/{BASIC_INFORMATION_CLUSTER}/{NODE_LABEL_ATTRIBUTE}")
        or attributes.get(f"0/{BASIC_INFORMATION_CLUSTER}/{PRODUCT_NAME_ATTRIBUTE}")
        or "<unknown>"
    )
    state = "available" if node.get("available") else "UNREACHABLE"
    return f"  node {node_id:<4} {state:<12} {name}  ({len(attributes)} attributes)"


async def run(args):
    """Connect to the Matter server and carry out the requested subcommand."""
    async with aiohttp.ClientSession() as session:
        async with session.ws_connect(args.url, heartbeat=30) as ws:
            await ws.receive_json()

            if args.command == "list":
                nodes = await request(ws, "get_nodes")
                if not nodes:
                    print("No nodes on the fabric.")
                    return
                print(f"{len(nodes)} node(s) on the fabric:")
                for node in sorted(nodes, key=lambda n: n["node_id"]):
                    print(describe(node))
                return

            for node_id in args.node_ids:
                await request(ws, "remove_node", {"node_id": node_id})
                print(f"Removed node {node_id}.")


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
