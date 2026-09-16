import argparse
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
    gpx = ET.Element("gpx", {"version": "1.1", "creator": "T1000-E Cached Tracker", "xmlns": "http://www.topografix.com/GPX/1/1"})
    trk = ET.SubElement(gpx, "trk")
    ET.SubElement(trk, "name").text = "T1000-E Track"
    seg = ET.SubElement(trk, "trkseg")
    for pt in points:
        p = ET.SubElement(seg, "trkpt", lat=str(pt["lat"]), lon=str(pt["lon"]))
        if pt.get("alt") is not None:
            ET.SubElement(p, "ele").text = str(pt["alt"])
        if pt.get("time"):
            ET.SubElement(p, "time").text = datetime.fromtimestamp(pt["time"], timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    with open(filename, "w", encoding="utf-8") as f:
        f.write(ET.tostring(gpx, encoding="unicode", xml_declaration=True))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=["status", "on", "off", "sync", "dump", "clear", "test"])
    parser.add_argument("--port")
    parser.add_argument("-o", "--output", default="tracklog.gpx")
    args = parser.parse_args()
    iface = meshtastic.serial_interface.SerialInterface(devPath=args.port) if args.port else meshtastic.serial_interface.SerialInterface()
    points = []
    complete = False

    def receive(packet, interface):
        nonlocal complete
        text = packet.get("decoded", {}).get("text", "")
        for line in text.splitlines():
            if line.startswith("DUMP COMPLETE"):
                complete = True
            elif line.startswith("$TRK,"):
                parts = line.split(",")
                try:
                    points.append({"lat": float(parts[1]), "lon": float(parts[2]), "alt": int(parts[3]), "time": int(parts[4])})
                except (ValueError, IndexError):
                    pass

    pub.subscribe(receive, "meshtastic.receive.text")
    dest = iface.myInfo.my_node_num if iface.myInfo else meshtastic.LOCAL_ADDR
    iface.sendText(f"tracker:{args.command}", destinationId=dest)
    end = time.time() + (25 if args.command == "dump" else 4)
    while time.time() < end and not complete:
        time.sleep(0.1)
    iface.close()
    if args.command == "dump" and points:
        export_to_gpx(points, args.output)


if __name__ == "__main__":
    main()
