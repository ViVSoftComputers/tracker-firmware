#!/usr/bin/env python3
"""
Tracker Tool - CLI for T1000-E/T096 GPS cached tracker firmware.

Direct node targeting via serial/BLE, zero LoRa RF broadcast of tracker commands.

Usage:
  python tracker_tool.py --port COM3 status
  python tracker_tool.py --port COM3 gpx-export
  python tracker_tool.py --port COM3 clear
  python tracker_tool.py --port COM3 toggle

Requirements:
  pip install meshtastic
"""

import argparse
import sys
import time
import xml.etree.ElementTree as ET
from datetime import datetime

try:
    import meshtastic
    import meshtastic.serial_interface
except ImportError:
    print("Error: Meshtastic python library is required.")
    print("Install: pip install meshtastic")
    sys.exit(1)


def get_interface(port):
    """Create a serial interface to the device."""
    try:
        return meshtastic.serial_interface.SerialInterface(devPath=port)
    except Exception as e:
        print(f"Error opening serial interface: {e}")
        sys.exit(1)


def cmd_status(iface):
    """Print tracker status."""
    node = iface.localNode
    pos = node.getPosition()

    print("=== Tracker Status ===")
    print(f"Node: {node.getShortName()} (0x{node.nodeNum:08x})")
    print(f"Firmware: {node.getFirmwareVersion()}")

    if pos:
        print(f"GPS Fix: lat={pos.get('latitude_i', 0) * 1e-7:.6f}, lon={pos.get('longitude_i', 0) * 1e-7:.6f}")
        print(f"Altitude: {pos.get('altitude', 0)}m")
        ts = pos.get('timestamp', 0)
        if ts > 0:
            print(f"Fix time: {datetime.utcfromtimestamp(ts).strftime('%Y-%m-%d %H:%M:%S')} UTC")

    iface.close()


def cmd_gpx(iface, output_path):
    """Export track points as GPX."""
    node = iface.localNode

    gpx = ET.Element('gpx', {
        'version': '1.1',
        'creator': 'tracker_tool.py',
        'xmlns': 'http://www.topografix.com/GPX/1/1'
    })

    trk = ET.SubElement(gpx, 'trk')
    ET.SubElement(trk, 'name').text = f"T1000-E Tracker - {node.getShortName()}"
    trkseg = ET.SubElement(trk, 'trkseg')

    # Fetch position log from device
    positions = node.getPositionLog()
    point_count = 0

    for pos in positions:
        lat = pos.get('latitude_i', 0) * 1e-7
        lon = pos.get('longitude_i', 0) * 1e-7
        alt = pos.get('altitude', 0)
        ts = pos.get('timestamp', 0)

        if ts < 1700000000:
            continue

        trkpt = ET.SubElement(trkseg, 'trkpt', {
            'lat': f'{lat:.7f}',
            'lon': f'{lon:.7f}'
        })
        ET.SubElement(trkpt, 'ele').text = str(alt)
        ET.SubElement(trkpt, 'time').text = datetime.utcfromtimestamp(ts).strftime('%Y-%m-%dT%H:%M:%SZ')
        point_count += 1

    tree = ET.ElementTree(gpx)
    ET.indent(tree, space='  ')
    tree.write(output_path, encoding='utf-8', xml_declaration=True)

    print(f"Exported {point_count} track points to {output_path}")
    iface.close()


def cmd_clear(iface):
    """Clear the local track cache on the device."""
    print("Sending clear-cache command...")
    # Admin message to clear tracker cache
    iface.localNode.sendText("__tracker_clear__", destinationId=iface.localNode.nodeNum, wantAck=True)
    time.sleep(1)
    print("Clear command sent.")
    iface.close()


def cmd_toggle(iface):
    """Toggle tracking on/off."""
    print("Sending toggle-tracking command...")
    iface.localNode.sendText("__tracker_toggle__", destinationId=iface.localNode.nodeNum, wantAck=True)
    time.sleep(1)
    print("Toggle command sent.")
    iface.close()


def main():
    parser = argparse.ArgumentParser(description='T1000-E/T096 Tracker CLI Tool')
    parser.add_argument('--port', '-p', required=True, help='Serial port (e.g., COM3, /dev/ttyACM0)')

    sub = parser.add_subparsers(dest='command', required=True)
    sub.add_parser('status', help='Show tracker status')

    gpx_parser = sub.add_parser('gpx-export', help='Export GPS track as GPX')
    gpx_parser.add_argument('--output', '-o', default='track.gpx', help='Output file path')

    sub.add_parser('clear', help='Clear track cache')
    sub.add_parser('toggle', help='Toggle tracking on/off')

    args = parser.parse_args()

    iface = get_interface(args.port)

    if args.command == 'status':
        cmd_status(iface)
    elif args.command == 'gpx-export':
        cmd_gpx(iface, args.output)
    elif args.command == 'clear':
        cmd_clear(iface)
    elif args.command == 'toggle':
        cmd_toggle(iface)


if __name__ == '__main__':
    main()
