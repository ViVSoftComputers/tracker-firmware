import sys
import time
import argparse
from datetime import datetime, timezone
import xml.etree.ElementTree as ET

try:
    import meshtastic
    import meshtastic.serial_interface
    from pubsub import pub
except ImportError:
    print('Error: meshtastic python library not installed.')
    print('Install it with: pip install meshtastic')
    sys.exit(1)

def export_to_gpx(points, filename="tracklog.gpx"):
    if not points:
        return
    gpx = ET.Element("gpx", {
        "version": "1.1",
        "creator": "T1000-E Cached Phone Tracker",
        "xmlns": "http://www.topografix.com/GPX/1/1",
        "xmlns:xsi": "http://www.w3.org/2001/XMLSchema-instance",
        "xsi:schemaLocation": "http://www.topografix.com/GPX/1/1 http://www.topografix.com/GPX/1/1/gpx.xsd"
    })

    metadata = ET.SubElement(gpx, "metadata")
    name_el = ET.SubElement(metadata, "name")
    name_el.text = "T1000-E Track Log"
    time_el = ET.SubElement(metadata, "time")
    time_el.text = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

    trk = ET.SubElement(gpx, "trk")
    trk_name = ET.SubElement(trk, "name")
    trk_name.text = f"T1000-E Track {datetime.now().strftime('%Y-%m-%d %H:%M')}"
    trkseg = ET.SubElement(trk, "trkseg")

    for pt in points:
        trkpt = ET.SubElement(trkseg, "trkpt", {"lat": str(pt["lat"]), "lon": str(pt["lon"])})
        if pt.get("alt") is not None:
            ele = ET.SubElement(trkpt, "ele")
            ele.text = str(pt["alt"])
        if pt.get("time"):
            time_pt = ET.SubElement(trkpt, "time")
            try:
                dt = datetime.fromtimestamp(pt["time"], tz=timezone.utc)
                time_pt.text = dt.strftime("%Y-%m-%dT%H:%M:%SZ")
            except Exception:
                pass
        if pt.get("hdop") is not None:
            hdop_el = ET.SubElement(trkpt, "hdop")
            hdop_el.text = f"{pt['hdop'] / 100.0:.2f}"

    xml_str = ET.tostring(gpx, encoding="utf-8", xml_declaration=True).decode("utf-8")
    with open(filename, "w", encoding="utf-8") as f:
        f.write(xml_str)
    print(f"\n[GPX] Saved {len(points)} trackpoint(s) to '{filename}'")

def main():
    parser = argparse.ArgumentParser(description='T1000-E Cached Tracker CLI')
    parser.add_argument('command', choices=['status', 'on', 'off', 'dump', 'clear', 'test'], help='Tracker command')
    parser.add_argument('--port', default=None, help='Serial COM port (e.g. COM3)')
    parser.add_argument('-o', '--output', default='tracklog.gpx', help='GPX output file for dump')
    args = parser.parse_args()

    cmd_str = f'tracker:{args.command}'
    print(f'Connecting to Meshtastic device on port {args.port or "auto"}...')
    try:
        if args.port:
            iface = meshtastic.serial_interface.SerialInterface(devPath=args.port)
        else:
            iface = meshtastic.serial_interface.SerialInterface()
    except Exception as e:
        print(f'Failed to connect: {e}')
        sys.exit(1)

    responses = []
    trackpoints = []

    def on_receive(packet, interface):
        try:
            if 'decoded' in packet and 'text' in packet.get('decoded', {}):
                text = packet['decoded']['text']
                responses.append(text)
                print(f'[Response] {text}')

                if text.startswith('$TRK,'):
                    parts = text.split(',')
                    if len(parts) >= 6 and parts[1] != 'CORRUPT':
                        try:
                            lat = float(parts[1])
                            lon = float(parts[2])
                            alt = int(parts[3])
                            timestamp = int(parts[4])
                            hdop = int(parts[5])
                            trackpoints.append({
                                "lat": lat, "lon": lon, "alt": alt,
                                "time": timestamp, "hdop": hdop
                            })
                        except (ValueError, IndexError):
                            pass
        except Exception:
            pass

    pub.subscribe(on_receive, 'meshtastic.receive.text')

    print(f'Sending: {cmd_str}')
    iface.sendText(cmd_str)

    timeout = 15 if args.command == 'dump' else 4
    start = time.time()
    last_count = 0
    while time.time() - start < timeout:
        time.sleep(0.15)
        if len(responses) > last_count:
            last_count = len(responses)
            if args.command == 'dump':
                start = time.time() # Reset timeout as long as packets keep coming

    iface.close()

    if not responses:
        print('No response received (device might be busy).')
    else:
        print(f'Finished. Received {len(responses)} packet(s).')

    if args.command == 'dump' and trackpoints:
        export_to_gpx(trackpoints, args.output)

if __name__ == '__main__':
    main()
