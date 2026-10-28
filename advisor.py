#!/usr/bin/env python3
"""
AgriSense AI Advisor - Advanced Edition (Standard Library Only)

Run on a PC / Raspberry Pi on the same WiFi as the ESP32:
    python advisor.py                      # serve on port 5000 for the webpage button
    python advisor.py 192.168.1.77 --once  # print one advanced report for the ESP32 at that IP
"""
import json
import math
import re
import socket
import sys
import urllib.request
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

PORT = 5000
DEFAULT_ESP = ""      # optional fixed ESP32 address, e.g. "192.168.1.77"
LOOKBACK = 60         # expanded lookback window for robust analytics (minutes)

COMPONENTS = [
    {"part": "ESP32-C6-DevKitC-1", "role": "Main controller: WiFi, web server, LittleFS CSV log", "pins": "-"},
    {"part": "16x2 LCD with I2C backpack (addr 0x27)", "role": "Local display, scrolled by touch sensor", "pins": "SDA=GPIO0, SCL=GPIO1"},
    {"part": "DHT11", "role": "Air temperature and humidity (+/-2 C, +/-5 %RH)", "pins": "GPIO9"},
    {"part": "Soil moisture sensor module", "role": "Soil moisture % (analog)", "pins": "AO=GPIO4, DO=GPIO6 (DO unused)"},
    {"part": "LDR light sensor module", "role": "Light level % (analog)", "pins": "GPIO2"},
    {"part": "Water pump + driver transistor", "role": "Waters the plant when soil is dry", "pins": "GPIO23"},
    {"part": "LED (with resistor/transistor)", "role": "Light output controlled by light level", "pins": "GPIO10"},
    {"part": "Fan + driver transistor", "role": "Ventilation when humidity is high", "pins": "GPIO18"},
    {"part": "Capacitive touch sensor module (TTP223-style)", "role": "Scrolls LCD screens", "pins": "GPIO5"},
    {"part": "Water reservoir, 500 ml", "role": "Pump supply; level estimated from pump run-time", "pins": "-"},
]

ORDER = {"urgent": 0, "soon": 1, "info": 2, "ok": 3}


def parse_ts(s):
    try:
        return datetime.strptime(s, "%Y-%m-%d %H:%M:%S")
    except (ValueError, TypeError):
        return None


def linear_regression(pts):
    """Calculates slope and intercept for a list of (x, y) coordinates."""
    n = len(pts)
    if n < 2:
        return 0.0, 0.0
    mx = sum(p[0] for p in pts) / n
    my = sum(p[1] for p in pts) / n
    den = sum((p[0] - mx) ** 2 for p in pts)
    if den == 0:
        return 0.0, my
    num = sum((p[0] - mx) * (p[1] - my) for p in pts)
    slope = num / den
    intercept = my - slope * mx
    return slope, intercept


def slope_per_hour(rows, key):
    pts = []
    for r in rows:
        t = parse_ts(r.get("ts", ""))
        if t and key in r:
            try:
                pts.append((t.timestamp() / 3600.0, float(r[key])))
            except (ValueError, TypeError):
                continue
    if len(pts) < 5:
        return None
    x0 = pts[0][0]
    normalized_pts = [(p[0] - x0, p[1]) for p in pts]
    slope, _ = linear_regression(normalized_pts)
    return slope


def calculate_vpd(temp, hum):
    """Approximate Vapor Pressure Deficit (kPa) for plant transpiration stress analysis."""
    try:
        svp = 0.61078 * math.exp((17.27 * temp) / (temp + 237.3))
        avp = svp * (hum / 100.0)
        return max(0.0, svp - avp)
    except Exception:
        return 0.0


def consumption_ml_per_hour(rows):
    if len(rows) < 3:
        return None
    t0, t1 = parse_ts(rows[0].get("ts", "")), parse_ts(rows[-1].get("ts", ""))
    if not t0 or not t1:
        return None
    hours = (t1 - t0).total_seconds() / 3600.0
    if hours < 0.1:
        return None
    used = 0
    for a, b in zip(rows, rows[1:]):
        if "ml" in a and "ml" in b:
            used += max(0, float(a["ml"]) - float(b["ml"]))
    return used / hours


def fmt_hours(h):
    if h < 1:
        return f"{h * 60:.0f} min"
    if h < 48:
        return f"{h:.1f} hours"
    return f"{h / 24:.1f} days"


def add(actions, prio, title, detail):
    actions.append({"priority": prio, "title": title, "detail": detail})


def analyse(d, rows):
    cfg = d.get("cfg", {"cap": 500, "pOn": 30, "pOff": 60, "fOn": 75})
    actions = []
    rec = rows[-LOOKBACK:] if len(rows) >= LOOKBACK else rows
    
    soil = float(d.get("soil", 0))
    hum = float(d.get("hum", 0))
    temp = float(d.get("temp", 0))
    light = float(d.get("light", 0))
    ml = float(d.get("ml", 0))
    cap = float(cfg.get("cap", 500))
    pct = 100.0 * ml / cap if cap > 0 else 0

    # ---- System Clock / Data Health ----
    if d.get("time") == "unsynced":
        add(actions, "soon", "ESP32 Clock Unsynced", "Timestamps rely on default fallback. Check internet connection for NTP sync.")
    if len(rows) < 10:
        add(actions, "info", "Telemetry Warming Up", f"Only {len(rows)} data points logged. Advanced predictive models activate fully after 30+ readings.")

    # ---- Advanced Reservoir & Consumption Analytics ----
    rate = consumption_ml_per_hour(rows)
    rate_txt = ""
    if rate and rate > 0.1:
        hours_left = ml / rate if rate > 0 else 999
        rate_txt = f" Current consumption is ~{rate:.1f} ml/h, depleting the tank in ~{fmt_hours(hours_left)}."
    
    refill_msg = f"Reservoir holds ~{ml:.0f}ml / {cap:.0f}ml ({pct:.1f}%).{rate_txt} Remember to tap 'Reservoir Refilled' on the UI after topping up."
    if ml <= 0:
        add(actions, "urgent", "Reservoir Empty!", "The water tank is dry. The pump will run dry and risk motor damage. " + refill_msg)
    elif pct <= 15:
        add(actions, "urgent", "Critical Water Level", "Reservoir is critically low. " + refill_msg)
    elif pct <= 35:
        add(actions, "soon", "Schedule Reservoir Refill", "Water level is dropping. " + refill_msg)
    else:
        add(actions, "ok", "Reservoir Status Healthy", refill_msg)

    # ---- Soil Moisture & Irrigation Intelligence ----
    pOn = float(cfg.get("pOn", 30))
    pOff = float(cfg.get("pOff", 60))
    soil_slope = slope_per_hour(rec, "soil")
    duty = sum(1 for r in rec if r.get("pump")) / len(rec) if rec else 0

    if d.get("pump") and len(rec) >= 10:
        recent_soil_slope = slope_per_hour(rec[-10:], "soil")
        if recent_soil_slope is not None and recent_soil_slope <= 0.1 and duty > 0.5:
            add(actions, "soon", "Pump Active but Soil Moisture Unresponsive", 
                "The pump has been running frequently without a rise in soil moisture. Inspect for tubing kinks, air locks, empty tank, or poor probe placement.")

    if soil < pOn and d.get("pump"):
        add(actions, "info", "Active Irrigation Cycle", f"Soil moisture is at {soil:.1f}% (Target cut-off is {pOff:.1f}%).")
    elif soil_slope is not None and soil_slope < -0.3 and soil > pOn:
        hours_to_trigger = (soil - pOn) / (-soil_slope) if -soil_slope > 0 else 99
        add(actions, "info", "Soil Dry-Down Trajectory", f"Moisture is decreasing at ~{-soil_slope:.2f}%/hour. Irrigation expected in ~{fmt_hours(hours_to_trigger)}.")
    elif soil >= 92:
        add(actions, "soon", "Soil Saturated Risk", f"Moisture is exceptionally high ({soil:.1f}%). Check drainage to prevent root rot.")
    else:
        add(actions, "ok", "Soil Moisture Optimal", f"Current moisture level is balanced at {soil:.1f}%.")

    # ---- Environmental Stress & VPD Analysis ----
    vpd = calculate_vpd(temp, hum)
    if vpd > 1.6:
        add(actions, "soon", "High Vapor Pressure Deficit (VPD)", f"VPD is high at {vpd:.2f} kPa ({temp}C, {hum}%RH). Transpiration stress is elevated; check ventilation.")
    elif vpd < 0.2 and temp < 18:
        add(actions, "info", "Low VPD / High Humidity Risk", f"VPD is low ({vpd:.2f} kPa). Fungal growth risks increase if air stagnation occurs.")

    if hum > float(cfg.get("fOn", 75)):
        if d.get("fan"):
            add(actions, "info", "Ventilation Active", f"Humidity is high ({hum}%), ventilation fan is running to clear air moisture.")
        else:
            add(actions, "soon", "Elevated Humidity Detected", f"Humidity is {hum}%. Consider increasing airflow or checking fan hardware status.")

    # ---- Temperature Thresholds ----
    if temp >= 35:
        add(actions, "soon", "Extreme Heat Warning", f"Ambient temperature is {temp}C. Evaporation rates will spike; verify shade and cooling.")
    elif temp <= 10:
        add(actions, "soon", "Low Temperature Warning", f"Ambient temperature is {temp}C. Biological activity and metabolic rates are heavily restricted.")

    # ---- Hardware Diagnostic Sanity Checks ----
    if len(rec) >= 25:
        temp_set = {r.get("temp") for r in rec if "temp" in r}
        hum_set = {r.get("hum") for r in rec if "hum" in r}
        if len(temp_set) == 1 and len(hum_set) == 1:
            add(actions, "soon", "DHT11 Sensor Frozen", "Temperature and humidity telemetry have flatlined. Check GPIO9 wiring connections and pull-up resistor.")
        
        soil_set = {r.get("soil") for r in rec if "soil" in r}
        if len(soil_set) == 1 and next(iter(soil_set)) in (0, 100):
            add(actions, "soon", "Soil Sensor Rail-Stuck", f"Sensor is locked constantly at {next(iter(soil_set))}%. Inspect Analog pin GPIO4 wiring.")

    # ---- Connectivity & Storage Diagnostics ----
    rssi = int(d.get("rssi", 0))
    if rssi and rssi < -85:
        add(actions, "info", "Sub-optimal WiFi RSSI", f"Signal strength is weak ({rssi} dBm). Packet delivery to the web dashboard may lag.")
    
    fs_total = float(d.get("fsTotal", 1))
    fs_used = float(d.get("fsUsed", 0))
    if fs_total > 0 and (fs_used / fs_total) > 0.85:
        add(actions, "soon", "LittleFS Storage Near Capacity", f"Flash log utilization is at {(fs_used/fs_total)*100:.1f}%. Download CSV logs and execute a log flush.")

    # Sort actions by priority index
    actions.sort(key=lambda a: ORDER.get(a["priority"], 3))
    overall_status = actions[0]["priority"] if actions else "ok"
    
    summary = (f"Temp {temp:.1f}C, Humidity {hum:.1f}%, Soil {soil:.1f}%, Light {light:.1f}%. "
               f"Tank ~{ml:.0f}ml ({pct:.0f}%). Pump {'ON' if d.get('pump') else 'OFF'}, "
               f"Fan {'ON' if d.get('fan') else 'OFF'}, VPD {vpd:.2f} kPa.")

    return {
        "generated": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "status": overall_status,
        "summary": summary,
        "metrics": {
            "vpd": round(vpd, 2),
            "consumptionRateMlPerHour": round(rate, 2) if rate else 0.0,
            "soilSlopePerHour": round(soil_slope, 2) if soil_slope else 0.0
        },
        "actions": actions,
        "components": COMPONENTS,
    }


def fetch_json(esp, path):
    req = urllib.request.Request(f"http://{esp}{path}", headers={"User-Agent": "AgriSense-AdvancedAdvisor/2.0"})
    with urllib.request.urlopen(req, timeout=5) as r:
        return json.loads(r.read().decode())


def advise(esp):
    d = fetch_json(esp, "/data")
    rows = fetch_json(esp, "/history")
    return analyse(d, rows)


class Handler(BaseHTTPRequestHandler):
    def _cors(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "*")
        self.send_header("Access-Control-Allow-Private-Network", "true")

    def _send(self, code, obj):
        body = json.dumps(obj).encode()
        self.send_response(code)
        self._cors()
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_OPTIONS(self):
        self.send_response(204)
        self._cors()
        self.end_headers()

    def do_GET(self):
        u = urlparse(self.path)
        if u.path != "/advice":
            return self._send(404, {"error": "Endpoint not found. Use /advice?esp=<esp32_ip>"})
        
        query_params = parse_qs(u.query)
        esp = query_params.get("esp", [DEFAULT_ESP])[0]
        
        if not esp or not re.fullmatch(r"[A-Za-z0-9.\-]+(:\d+)?", esp):
            return self._send(400, {"error": "Missing or malformed ESP32 IP address parameter."})
        
        try:
            report = advise(esp)
            self._send(200, report)
        except Exception as e:
            self._send(500, {"error": f"Failed communicating with ESP32 device at {esp}: {str(e)}"})

    def log_message(self, fmt, *args):
        print(f"[AgriSense-Advisor] {fmt % args}")


def lan_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except OSError:
        return "127.0.0.1"


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if "--once" in sys.argv:
        target_esp = args[0] if args else DEFAULT_ESP
        if not target_esp:
            sys.exit("Error: Please provide the ESP32 IP address. Usage: python advisor.py <esp32-ip> --once")
        rep = advise(target_esp)
        print(f"\n[{rep['status'].upper()}] {rep['summary']}\n")
        print(f"Advanced Metrics -> VPD: {rep['metrics']['vpd']} kPa | Rate: {rep['metrics']['consumptionRateMlPerHour']} ml/h")
        print("Prioritized Action Items:")
        for a in rep["actions"]:
            print(f" * [{a['priority'].upper()}] {a['title']}: {a['detail']}")
        sys.exit(0)
        
    print(f"🚀 AgriSense Advanced AI Advisor online.")
    print(f"🔗 Configure your dashboard web button to look toward: http://{lan_ip()}:{PORT}/advice?esp=YOUR_ESP_IP")
    ThreadingHTTPServer(("0.0.0.0", PORT), Handler).serve_forever()