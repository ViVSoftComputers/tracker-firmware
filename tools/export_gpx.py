#!/usr/bin/env python3
"""
Convert Meshtastic LocalGpsTrackLogger CSV to GPX format.

Usage:
  python tools/export_gpx.py http://192.168.87.45/tracklog.csv -o track.gpx
  python tools/export_gpx.py tracklog.csv -o track.gpx
  python tools/export_gpx.py -o track.gpx  (uses default URL)
"""

import sys
import argparse
import csv
from datetime import datetime, timezone
import urllib.request
import xml.etree.ElementTree as ET

def fetch_or_read_csv(source):
    if source.startswith("http://") or source.startswith("https://"):
        req = urllib.request.Request(source, headers={"User-Agent": "Meshtastic-TrackExport/1.0"})
        with urllib.request.urlopen(req, timeout=10) as response:
            lines = [line.decode("utf-8", errors="ignore") for line in response.readlines()]
            return list(csv.DictReader(lines))
    else:
        with open(source, "r", encoding="utf-8", errors="ignore") as f:
            return list(csv.DictReader(f))

def generate_gpx(rows, track_name="Meshtastic Track"):
    gpx = ET.Element("gpx", {
        "version": "1.1",
        "creator": "Meshtastic LocalGpsTrackLogger",
        "xmlns": "http://www.topografix.com/GPX/1/1",
        "xmlns:xsi": "http://www.w3.org/2001/XMLSchema-instance",
        "xsi:schemaLocation": "http://www.topografix.com/GPX/1/1 http://www.topografix.com/GPX/1/1/gpx.xsd"
    })

    metadata = ET.SubElement(gpx, "metadata")
    name_el = ET.SubElement(metadata, "name")
    name_el.text = track_name
    time_el = ET.SubElement(metadata, "time")
    time_el.text = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

    trk = ET.SubElement(gpx, "trk")
    trk_name = ET.SubElement(trk, "name")
    trk_name.text = track_name
    trkseg = ET.SubElement(trk, "trkseg")

    for row in rows:
        lat = row.get("latitude", "").strip()
        lon = row.get("longitude", "").strip()
        if not lat or not lon:
            continue
        
        trkpt = ET.SubElement(trkseg, "trkpt", {"lat": lat, "lon": lon})
        
        alt = row.get("altitude_m", "").strip()
        if alt:
            ele = ET.SubElement(trkpt, "ele")
            ele.text = alt

        dt_str = row.get("date_time", "").strip()
        if dt_str:
            dt_clean = dt_str.replace(" ", "T")
            time_pt = ET.SubElement(trkpt, "time")
            time_pt.text = dt_clean

        hdop = row.get("hdop", "").strip()
        if hdop:
            try:
                hdop_val = float(hdop) / 100.0 if float(hdop) > 20 else float(hdop)
                hdop_el = ET.SubElement(trkpt, "hdop")
                hdop_el.text = f"{hdop_val:.2f}"
            except ValueError:
                pass

    return ET.tostring(gpx, encoding="utf-8", xml_declaration=True).decode("utf-8")

def main():
    parser = argparse.ArgumentParser(description="Export Meshtastic Track CSV to GPX")
    parser.add_argument("source", nargs="?", default="http://192.168.87.45/tracklog.csv", help="CSV file path or HTTP URL")
    parser.add_argument("-o", "--output", default="tracklog.gpx", help="Output GPX filename")
    parser.add_argument("-n", "--name", default="Meshtastic Heltec V3 Track", help="Track name")
    args = parser.parse_args()

    print(f"Reading from {args.source}...")
    rows = fetch_or_read_csv(args.source)
    print(f"Loaded {len(rows)} track points.")

    gpx_data = generate_gpx(rows, track_name=args.name)
    with open(args.output, "w", encoding="utf-8") as f:
        f.write(gpx_data)

    print(f"Saved GPX track to {args.output}")

if __name__ == "__main__":
    main()