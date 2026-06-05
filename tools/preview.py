#!/usr/bin/env python3
"""ESP32 Droid Control System — local web UI emulator.

Serves the same HTML/CSS/JS the firmware sends, with mock action and
Server-Sent Events endpoints. Lets you preview UI changes without
flashing the device.

Run:    python3 tools/preview.py
Open:   http://localhost:8080

The HTML/CSS/JS in render_page() and the JSON shape in build_status_json()
mirror buildPageHtml() and buildStatusJson() in src/main.cpp. When the
firmware changes, update this file to match (see CLAUDE.md).
"""

import http.server
import json
import queue
import threading

PORT = 8080
DROID_NAME = "Grek"
DROID_COLOR = "green"

# Mirror the const arrays in src/main.cpp.
# Emotes: (path, label, emoji)
EMOTES = [
    ("angry",   "angry",       "😠"),
    ("curious", "curious",     "🤔"),
    ("dance",   "dance",       "💃"),
    ("happy",   "happy",       "😊"),
    ("no",      "no",          "👎"),
    ("sad",     "sad",         "😢"),
    ("scared",  "scared",      "😨"),
    ("sleep",   "go to sleep", "😴"),
    ("wake",    "wake up",     "🌅"),
    ("yes",     "yes",         "👍"),
]

# Eye colors: (path, label, css_color)
EYE_COLORS = [
    ("color_white",   "white",   "#FFFFFF"),
    ("color_yellow",  "yellow",  "#FFFF00"),
    ("color_orange",  "orange",  "#FF6400"),  # CRGB(255,100,0)
    ("color_green",   "green",   "#00FF00"),
    ("color_red",     "red",     "#FF0000"),
    ("color_blue",    "blue",    "#0000FF"),
    ("color_purple",  "purple",  "#800080"),
]

# In-memory state — what the ESP32 would track.
state = {
    "lastEmote": "orange (startup)",
    "idle": False,
    "maestro": True,
    "dfplayer": True,
    "flashlight": False,
    "currentEye": "color_orange",
    "status": "Ready (emulated)",
}
state_lock = threading.Lock()

# Active SSE subscribers (one queue per connected client).
sse_clients = set()
sse_clients_lock = threading.Lock()


def _is_light(hex_color):
    """Pick dark text for light backgrounds."""
    r = int(hex_color[1:3], 16)
    g = int(hex_color[3:5], 16)
    b = int(hex_color[5:7], 16)
    return (r * 299 + g * 587 + b * 114) // 1000 > 180


def emote_buttons_html():
    out = []
    for path, label, emoji in EMOTES:
        out.append(
            f'<button data-path="{path}" onclick="t(\'{path}\')" class="button emote">'
            f'<span class="emoji">{emoji}</span><span class="elabel">{label}</span>'
            f'</button>'
        )
    return "\n".join(out)


def color_buttons_html():
    out = []
    for path, label, css_color in EYE_COLORS:
        style = f"background-color:{css_color}"
        if _is_light(css_color):
            style += ";color:#111"
        out.append(
            f'<button data-path="{path}" onclick="t(\'{path}\')" class="button" '
            f'style="{style};">{label}</button>'
        )
    return "\n".join(out)


def find_label(path):
    """Used by dispatch() to update lastEmote — same as firmware uses button.label."""
    for path_, label, *_ in EMOTES:
        if path_ == path:
            return label
    for path_, label, *_ in EYE_COLORS:
        if path_ == path:
            return label
    return None


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
.button-grid {{ display: grid; grid-template-columns: repeat(auto-fit, 88px); gap: 10px;
max-width: 1200px; margin: 0 auto 16px auto; justify-content: center; }}
.button {{ background-color: {DROID_COLOR}; border: none; border-radius: 50%; aspect-ratio: 1 / 1;
color: white; padding: 6px; font-family: inherit; font-size: 12px; font-weight: bold;
cursor: pointer; transition: all 0.3s; text-align: center; box-shadow: 0 3px 5px rgba(0,0,0,0.3);
display: flex; align-items: center; justify-content: center; }}
.button:hover {{ transform: translateY(-2px); box-shadow: 0 5px 9px rgba(0,0,0,0.4); opacity: 0.9; }}
.button:active {{ transform: translateY(0); box-shadow: 0 2px 3px rgba(0,0,0,0.3); }}
.button.on {{ outline: 3px solid #ffffff; outline-offset: -3px; filter: brightness(1.25); }}
.button.emote {{ flex-direction: column; gap: 3px; padding: 6px 4px; }}
.button.emote .emoji {{ font-size: 28px; line-height: 1; }}
.button.emote .elabel {{ font-size: 10px; font-weight: normal; opacity: 0.85; }}
.toggle-grid {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(180px, 1fr)); gap: 8px; max-width: 1200px; margin: 0 auto 15px auto; }}
.toggle {{ background-color: #2a2a2a; border: 1px solid #444; border-radius: 6px; padding: 12px 16px;
cursor: pointer; display: flex; align-items: center; justify-content: space-between; gap: 12px;
font-size: 15px; font-weight: bold; color: white; transition: all 0.2s;
box-shadow: 0 3px 5px rgba(0,0,0,0.3); user-select: none; -webkit-tap-highlight-color: transparent; }}
.toggle:hover {{ background-color: #353535; transform: translateY(-2px); box-shadow: 0 5px 9px rgba(0,0,0,0.4); }}
.toggle:active {{ transform: translateY(0); }}
.toggle-switch {{ width: 42px; height: 24px; background: #555; border-radius: 12px;
position: relative; flex-shrink: 0; transition: background 0.2s; }}
.toggle-switch::before {{ content: ''; position: absolute; top: 3px; left: 3px;
width: 18px; height: 18px; background: white; border-radius: 50%; transition: transform 0.2s; }}
.toggle.on {{ background-color: #1f3a2a; border-color: #4ade80; }}
.toggle.on .toggle-switch {{ background: #4ade80; }}
.toggle.on .toggle-switch::before {{ transform: translateX(18px); }}
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
{emote_buttons_html()}
</div>

<h2>Toggles</h2><div class="toggle-grid">
<div class="toggle" id="tog-flash" onclick="tflash()"><span>Flashlight</span><span class="toggle-switch"></span></div>
<div class="toggle" id="tog-idle" onclick="tidle()"><span>Idle Mode</span><span class="toggle-switch"></span></div>
</div>

<h2>Eye Colors</h2><div class="button-grid">
{color_buttons_html()}
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
let st={{}};
function hl(p,on){{const b=document.querySelector('button[data-path="'+p+'"]');if(b)b.classList.toggle('on',on);}}
function r(d){{if(!d)return;st=d;
document.getElementById('le').textContent=d.lastEmote;
document.getElementById('im').textContent=d.idle?'On':'Off';
document.getElementById('ms').textContent=d.maestro?'Connected':'Disabled';
document.getElementById('ds').textContent=d.dfplayer?'Connected':'Not Available';
document.getElementById('ss').textContent=d.status;
document.querySelectorAll('button.on').forEach(b=>b.classList.remove('on'));
if(d.currentEye)hl(d.currentEye,true);
document.getElementById('tog-flash').classList.toggle('on',!!d.flashlight);
document.getElementById('tog-idle').classList.toggle('on',!!d.idle);}}
async function t(p){{try{{await fetch('/maestro/'+p);}}catch(e){{}}}}
function tflash(){{t('flashlight');}}
function tidle(){{t(st.idle?'idle_stop':'idle_start');}}
const es=new EventSource('/events');
es.onmessage=e=>{{try{{r(JSON.parse(e.data));}}catch(err){{}}}};
</script>
</body></html>
"""


def dispatch(path):
    """Mutate state to mirror the firmware's dispatchMaestroAction."""
    with state_lock:
        if path == "flashlight":
            state["flashlight"] = not state["flashlight"]
            state["lastEmote"] = "flashlight on" if state["flashlight"] else "flashlight off"
        elif path == "idle_start":
            state["idle"] = True
            state["lastEmote"] = "idle mode on"
        elif path == "idle_stop":
            state["idle"] = False
            state["lastEmote"] = "idle mode off"
        else:
            label = find_label(path)
            if label is not None:
                state["lastEmote"] = label
                # Any user-triggered emote/eye-color cancels idle.
                state["idle"] = False
                # Only eye-color buttons update currentEye; emotes leave the
                # eyes alone (preserveLED12 = true on every emote in the
                # firmware, so the highlighted eye-color button stays
                # highlighted across emote clicks).
                if any(p == path for p, _, _ in EYE_COLORS):
                    state["currentEye"] = path


def snapshot_state():
    with state_lock:
        return dict(state)


def push_status():
    """Fan out the current state to every connected SSE client."""
    payload = json.dumps(snapshot_state())
    with sse_clients_lock:
        for q in list(sse_clients):
            try:
                q.put_nowait(payload)
            except queue.Full:
                pass


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        print(f"[{self.command}] {self.path}")

    def do_GET(self):
        if self.path == "/":
            self._send(200, "text/html; charset=utf-8", render_page())
            return
        if self.path == "/events":
            self._stream_sse()
            return
        if self.path.startswith("/maestro/"):
            dispatch(self.path[len("/maestro/"):])
            push_status()
            self._send(200, "text/plain", "OK")
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

    def _stream_sse(self):
        """Subscribe this client and stream JSON messages until disconnect."""
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Connection", "keep-alive")
        self.end_headers()

        my_queue = queue.Queue(maxsize=8)
        with sse_clients_lock:
            sse_clients.add(my_queue)

        # Send current state immediately so the page populates on connect.
        try:
            my_queue.put_nowait(json.dumps(snapshot_state()))
        except queue.Full:
            pass

        try:
            while True:
                payload = my_queue.get()  # blocks until push_status() fires
                self.wfile.write(f"data: {payload}\n\n".encode("utf-8"))
                self.wfile.flush()
        except (BrokenPipeError, ConnectionResetError):
            pass
        finally:
            with sse_clients_lock:
                sse_clients.discard(my_queue)


class ThreadingHTTPServer(http.server.ThreadingHTTPServer):
    daemon_threads = True


def main():
    addr = ("127.0.0.1", PORT)
    httpd = ThreadingHTTPServer(addr, Handler)
    print(f"Droid emulator listening on http://{addr[0]}:{addr[1]}")
    print("Press Ctrl-C to stop.")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down.")
        httpd.server_close()


if __name__ == "__main__":
    main()
