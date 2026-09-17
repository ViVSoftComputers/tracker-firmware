import argparse
import sys
import time
import xml.etree.ElementTree as ET
from datetime import datetime, timezone

try:
    import meshtastic
    import meshtastic.serial_interface
    from pubsub import pub
except ImportError:
    raise SystemExit("Install dependencies with: pip install meshtastic")


def export_to_gpx(points, filename="tracklog.gpx"):
    gpx = ET.Element("gpx", {
        "version": "1.1",
        "creator": "Seeed T1000-E Cached Tracker",
        "xmlns": "http://www.topografix.com/GPX/1/1"
    })
    trk = ET.SubElement(gpx, "trk")
    ET.SubElement(trk, "name").text = f"T1000-E Track - {datetime.now().strftime('%Y-%m-%d %H:%M')}"
    seg = ET.SubElement(trk, "trkseg")
    for pt in points:
        p = ET.SubElement(seg, "trkpt", lat=str(pt["lat"]), lon=str(pt["lon"]))
        if pt.get("alt") is not None:
            ET.SubElement(p, "ele").text = str(pt["alt"])
        if pt.get("time"):
            ET.SubElement(p, "time").text = datetime.fromtimestamp(pt["time"], timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    with open(filename, "w", encoding="utf-8") as f:
        f.write(ET.tostring(gpx, encoding="unicode", xml_declaration=True))
    print(f"[+] Exported {len(points)} points to {filename}")


def main():
    parser = argparse.ArgumentParser(description="T1000-E Cached Phone Tracker CLI")
    parser.add_argument("command", choices=["status", "on", "off", "sync", "dump", "clear", "test"],
                        help="Command to send to the tracker")
    parser.add_argument("--port", help="Serial port (e.g. COM3). Auto-detected if omitted.")
    parser.add_argument("-o", "--output", default="tracklog.gpx", help="Output GPX file for dump command")
    args = parser.parse_args()

    try:
        iface = meshtastic.serial_interface.SerialInterface(devPath=args.port) if args.port else meshtastic.serial_interface.SerialInterface()
    except Exception as e:
        print(f"[-] Failed to connect to Meshtastic device: {e}")
        sys.exit(1)

    points = []
    complete = False

    def receive(packet, interface):
        nonlocal complete
        text = packet.get("decoded", {}).get("text", "")
        for line in text.splitlines():
            print(f"<- {line}")
            if line.startswith("DUMP COMPLETE"):
                complete = True
            elif line.startswith("$TRK,"):
                parts = line.split(",")
                try:
                    points.append({
                        "lat": float(parts[1]),
                        "lon": float(parts[2]),
                        "alt": int(parts[3]),
                        "time": int(parts[4]),
                        "hdop": int(parts[5]) if len(parts) > 5 else 0
                    })
                except (ValueError, IndexError):
                    pass

    pub.subscribe(receive, "meshtastic.receive.text")
    dest = iface.myInfo.my_node_num if iface.myInfo else meshtastic.LOCAL_ADDR

    cmd_str = f"tracker:{args.command}"
    print(f"-> Sending '{cmd_str}' directly to node {dest} (no RF broadcast)...")
    iface.sendText(cmd_str, destinationId=dest)

    timeout = 30 if args.command == "dump" else 5
    end = time.time() + timeout
    while time.time() < end and not complete:
        time.sleep(0.1)

    try:
        iface.close()
    except Exception:
        pass

    if args.command == "dump":
        if points:
            export_to_gpx(points, args.output)
        else:
            print("[!] No track points received from device.")


if __name__ == "__main__":
    main()
