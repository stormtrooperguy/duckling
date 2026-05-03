#!/usr/bin/env python3
"""ESP32 Droid Control System — local web UI emulator.

Serves the same HTML/CSS/JS the firmware sends, with mock JSON endpoints
so AJAX clicks and the status poll behave realistically. Lets you preview
UI changes without flashing the device.

Run:    python3 tools/preview.py
Open:   http://localhost:8080

The HTML/CSS/JS in render_page() and the JSON shape in /status mirror
sendPageHTML() and sendStatusJSON() in esp32wifiweb.ino. When the
firmware changes, update this file to match (see CLAUDE.md).
"""

import http.server
import json

PORT = 8080
DROID_NAME = "Grek"
DROID_COLOR = "green"

# Mirror the const arrays in esp32wifiweb.ino: (path, label) per button.
EMOTES = [
    ("angry",   "angry"),
    ("curious", "curious"),
    ("happy",   "happy"),
    ("no",      "no"),
    ("sad",     "sad"),
    ("scared",  "scared"),
    ("sleep",   "go to sleep"),
    ("wake",    "wake up"),
    ("yes",     "yes"),
]

ACTIONS = [
    ("flashlight", "flashlight"),
    ("idle_start", "idle on"),
    ("idle_stop",  "idle off"),
]

EYE_COLORS = [
    ("color_white",  "white"),
    ("color_yellow", "yellow"),
    ("color_green",  "green"),
    ("color_red",    "red"),
    ("color_blue",   "blue"),
    ("color_purple", "purple"),
]

# In-memory state — what the ESP32 would track.
state = {
    "lastEmote": "yellow (startup)",
    "idle": False,
    "maestro": True,
    "dfplayer": True,
    "status": "Ready (emulated)",
}


def buttons_html(items):
    return "\n".join(
        f'<button onclick="t(\'{path}\')" class="button">{label}</button>'
        for path, label in items
    )


def render_page():
    return f"""<!DOCTYPE html><html>
<head><meta name="viewport" content="width=device-width, initial-scale=1">
<link rel="icon" href="data:,">
<style>
* {{ margin: 0; padding: 0; box-sizing: border-box; }}
html {{ font-family: Helvetica, Arial, sans-serif; }}
body {{ background-color: #1a1a1a; color: #ffffff; padding: 11px; padding-bottom: 78px; }}
h1 {{ text-align: center; margin-bottom: 15px; font-size: 24px; }}
h2 {{ text-align: center; margin: 15px 0 8px 0; font-size: 17px; color: #aaa; }}
.button-grid {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(112px, 1fr)); gap: 8px; max-width: 1200px; margin: 0 auto 15px auto; }}
.button {{ background-color: {DROID_COLOR}; border: none; border-radius: 6px; color: white; padding: 15px 11px;
font-family: inherit; font-size: 15px; font-weight: bold; cursor: pointer;
transition: all 0.3s; text-align: center; box-shadow: 0 3px 5px rgba(0,0,0,0.3); }}
.button:hover {{ transform: translateY(-2px); box-shadow: 0 5px 9px rgba(0,0,0,0.4); opacity: 0.9; }}
.button:active {{ transform: translateY(0); box-shadow: 0 2px 3px rgba(0,0,0,0.3); }}
.status-console {{ position: fixed; bottom: 0; left: 0; right: 0; background-color: #2a2a2a;
border-top: 2px solid #444; padding: 8px 11px; box-shadow: 0 -2px 8px rgba(0,0,0,0.5); }}
.status-console h3 {{ margin: 0 0 6px 0; font-size: 11px; color: #888; text-align: center; }}
.status-grid {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(112px, 1fr)); gap: 6px;
max-width: 1200px; margin: 0 auto; font-size: 9px; }}
.status-item {{ background-color: #1a1a1a; padding: 5px 8px; border-radius: 3px; border: 1px solid #444; }}
.status-item strong {{ color: #aaa; margin-right: 5px; }}
</style></head>
<body><h1>BDX Control System ({DROID_NAME})</h1>

<h2>Emotes</h2><div class="button-grid">
{buttons_html(EMOTES)}
</div>

<h2>Actions</h2><div class="button-grid">
{buttons_html(ACTIONS)}
</div>

<h2>Eye Colors</h2><div class="button-grid">
{buttons_html(EYE_COLORS)}
</div>

<div class="status-console"><h3>System Status</h3><div class="status-grid">
<div class="status-item"><strong>Network:</strong> {DROID_NAME} (192.168.4.1)</div>
<div class="status-item"><strong>Maestro:</strong> <span id="ms">&mdash;</span></div>
<div class="status-item"><strong>DFPlayer:</strong> <span id="ds">&mdash;</span></div>
<div class="status-item"><strong>Idle:</strong> <span id="im">&mdash;</span></div>
<div class="status-item"><strong>Status:</strong> <span id="ss">&mdash;</span></div>
<div class="status-item"><strong>Last:</strong> <span id="le">&mdash;</span></div>
</div></div>

<script>
function r(d){{if(!d)return;
document.getElementById('le').textContent=d.lastEmote;
document.getElementById('im').textContent=d.idle?'On':'Off';
document.getElementById('ms').textContent=d.maestro?'Connected':'Disabled';
document.getElementById('ds').textContent=d.dfplayer?'Connected':'Not Available';
document.getElementById('ss').textContent=d.status;}}
async function t(p){{try{{const x=await fetch('/maestro/'+p);r(await x.json());}}catch(e){{}}}}
async function q(){{try{{const x=await fetch('/status');r(await x.json());}}catch(e){{}}}}
setInterval(q,2000);q();
</script>
</body></html>
"""


def find_label(path):
    for items in (EMOTES, ACTIONS, EYE_COLORS):
        for p, label in items:
            if p == path:
                return label
    return None


def dispatch(path):
    if path == "flashlight":
        state["lastEmote"] = "flashlight (toggled)"
        return
    if path == "idle_start":
        state["idle"] = True
        state["lastEmote"] = "idle mode on"
        return
    if path == "idle_stop":
        state["idle"] = False
        state["lastEmote"] = "idle mode off"
        return
    label = find_label(path)
    if label is not None:
        state["lastEmote"] = label
        # Mirror firmware: any user-triggered emote/eye-color cancels idle.
        state["idle"] = False


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        print(f"[{self.command}] {self.path}")

    def do_GET(self):
        if self.path == "/":
            self._send(200, "text/html; charset=utf-8", render_page())
            return
        if self.path == "/status":
            self._send_json(state)
            return
        if self.path.startswith("/maestro/"):
            dispatch(self.path[len("/maestro/"):])
            self._send_json(state)
            return
        self._send(404, "text/plain", "Not Found\n")

    def _send(self, code, content_type, body):
        body_bytes = body.encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body_bytes)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body_bytes)

    def _send_json(self, payload):
        self._send(200, "application/json", json.dumps(payload))


def main():
    addr = ("127.0.0.1", PORT)
    httpd = http.server.HTTPServer(addr, Handler)
    print(f"Droid emulator listening on http://{addr[0]}:{addr[1]}")
    print("Press Ctrl-C to stop.")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down.")
        httpd.server_close()


if __name__ == "__main__":
    main()
