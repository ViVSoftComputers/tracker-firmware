#!/usr/bin/env python3
"""
T1000-E Meshtastic Offline Tracker Tool
Communicates with the T1000-E tracker firmware to query status, toggle mode,
clear the LittleFS ring-buffer cache, and stream out trackpoints directly to GPX format.
"""

import sys
import time
import argparse
import datetime
import xml.etree.ElementTree as ET
from xml.dom import minidom

try:
    import meshtastic
    import meshtastic.serial_interface
    from pubsub import pub
except ImportError:
    print("Error: Meshtastic python library is required.")
    print("Run: pip install meshtastic")
    sys.exit(1)


def create_gpx(trackpoints, output_path="tracklog.gpx"):
    """Converts a list of dict trackpoints into standard GPX format."""
    gpx = ET.Element("gpx", {
        "version": "1.1",
        "creator": "T1000-E Cached Phone Tracker",
        "xmlns": "http://www.topografix.com/GPX/1/1",
        "xmlns:xsi": "http://www.w3.org/2001/XMLSchema-instance",
        "xsi:schemaLocation": "http://www.topografix.com/GPX/1/1 http://www.topografix.com/GPX/1/1/gpx.xsd"
    })
    
    metadata = ET.SubElement(gpx, "metadata")
    ET.SubElement(metadata, "name").text = "T1000-E Track Log"
    ET.SubElement(metadata, "time").text = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    
    trk = ET.SubElement(gpx, "trk")
    now_str = datetime.datetime.now().strftime("%Y-%m-%d %H:%M")
    ET.SubElement(trk, "name").text = f"T1000-E Track {now_str}"
    
    trkseg = ET.SubElement(trk, "trkseg")
    
    # Sort trackpoints by timestamp
    trackpoints.sort(key=lambda x: x.get("time", 0))
    
    for pt in trackpoints:
        trkpt = ET.SubElement(trkseg, "trkpt", {
            "lat": f"{pt['lat']:.6f}".rstrip('0').rstrip('.'),
            "lon": f"{pt['lon']:.6f}".rstrip('0').rstrip('.')
        })
        if "alt" in pt:
            ET.SubElement(trkpt, "ele").text = str(pt["alt"])
        if "time" in pt and pt["time"] > 0:
            dt = datetime.datetime.fromtimestamp(pt["time"], datetime.timezone.utc)
            ET.SubElement(trkpt, "time").text = dt.strftime("%Y-%m-%dT%H:%M:%SZ")
        if "hdop" in pt and pt["hdop"] > 0:
            ET.SubElement(trkpt, "hdop").text = f"{pt['hdop'] / 100.0:.2f}"

    xml_str = ET.tostring(gpx, encoding="utf-8")
    pretty_xml = minidom.parseString(xml_str).toprettyxml(indent="  ", encoding="utf-8")
    
    with open(output_path, "wb") as f:
        f.write(pretty_xml)
    print(f"\n[GPX] Saved {len(trackpoints)} trackpoint(s) to '{output_path}'")


def main():
    parser = argparse.ArgumentParser(description="T1000-E Tracker Companion Tool")
    parser.add_argument("command", choices=["status", "on", "off", "dump", "clear", "test"],
                        help="Command to send to the tracker")
    parser.add_argument("--port", default=None, help="Serial port (e.g. COM3 or /dev/ttyACM0). Auto-detects if omitted.")
    parser.add_argument("--out", default="tracklog.gpx", help="Output file for GPX export (default: tracklog.gpx)")
    parser.add_argument("--timeout", type=int, default=15, help="Timeout in seconds to wait for responses")
    args = parser.parse_args()

    port = args.port
    print(f"Connecting to Meshtastic device on port {port or 'auto'}...")
    
    try:
        interface = meshtastic.serial_interface.SerialInterface(devPath=port)
    except Exception as e:
        print(f"Error opening serial interface: {e}")
        sys.exit(1)

    trackpoints = []
    received_count = 0
    complete = False
    dump_started = False

    def on_receive(packet, interface):
        nonlocal dump_started, complete, received_count
        if 'decoded' in packet and 'text' in packet.get('decoded', {}):
            text = packet['decoded']['text']
            print(f"[Response] {text}")
            received_count += 1
            dump_started = True
            
            # Check for completion
            if "DUMP COMPLETE" in text or "CACHE EMPTY" in text:
                complete = True

            # Parse lines (single or multi-point packets)
            for line in text.strip().split('\n'):
                line = line.strip()
                if line.startswith('$TRK,'):
                    parts = line.split(',')
                    if len(parts) >= 6 and parts[1] != 'CORRUPT':
                        try:
                            lat = float(parts[1])
                            lon = float(parts[2])
                            alt = int(parts[3])
                            timestamp = int(parts[4])
                            hdop = int(parts[5])
                            trackpoints.append({
                                "lat": lat,
                                "lon": lon,
                                "alt": alt,
                                "time": timestamp,
                                "hdop": hdop
                            })
                        except ValueError:
                            pass

    pub.subscribe(on_receive, "meshtastic.receive.text")

    cmd_map = {
        "status": "tracker:status",
        "on": "tracker:on",
        "off": "tracker:off",
        "dump": "tracker:dump",
        "clear": "tracker:clear",
        "test": "tracker:test"
    }

    send_cmd = cmd_map[args.command]
    print(f"Sending: {send_cmd}")
    interface.sendText(send_cmd)

    start_time = time.time()
    last_recv_time = time.time()
    
    # Wait for completion or timeout
    while time.time() - start_time < args.timeout:
        if complete:
            break
        if dump_started and received_count > 0:
            # If we are in dump mode and haven't received anything for 3 seconds, break
            if time.time() - last_recv_time > 3.0 and args.command == "dump":
                break
        time.sleep(0.1)

    print(f"Finished. Received {received_count} packet(s), {len(trackpoints)} trackpoint(s).")
    interface.close()

    if args.command == "dump" and trackpoints:
        create_gpx(trackpoints, args.out)


if __name__ == "__main__":
    main()
