"""Tests for web/server.py: the USB-serial-to-browser bridge.

Run from the repository root:  python -m pytest tests/web
No Arduino is needed: commands go to a list instead of a serial port.
"""
import http.client
import importlib.util
import json
import pathlib
import threading
import types
from http.server import ThreadingHTTPServer

import pytest

ROOT = pathlib.Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("server", ROOT / "web" / "server.py")
server = importlib.util.module_from_spec(spec)
spec.loader.exec_module(server)

HEADER = "H," + ",".join(server.DEFAULT_COLS)


def row(ms, state="RUN", dist="120.0", temp="30.00", vib="35.0"):
    return (f"D,{ms},{state},OK,{dist},{temp},0.00,{vib},0.1,0,0.00,0.00,255,71,123,"
            f"1500,1500,45.0,100,100")


@pytest.fixture
def app():
    args = types.SimpleNamespace(port=None, http_port=0, demo=False, rated_rpm=300,
                                 allow_remote_control=False, local_only=True, no_browser=True,
                                 no_log=True, verbose=False)
    hub = server.Hub(args)
    sent = []
    hub.writer = sent.append            # commands land here instead of on a serial port
    httpd = ThreadingHTTPServer(("127.0.0.1", 0), server.make_handler(hub, args))
    httpd.daemon_threads = True
    threading.Thread(target=httpd.serve_forever, daemon=True).start()
    yield types.SimpleNamespace(hub=hub, sent=sent, port=httpd.server_address[1])
    httpd.shutdown()
    httpd.server_close()


def request(app, method, path, body=None, headers=None):
    conn = http.client.HTTPConnection("127.0.0.1", app.port, timeout=5)
    conn.request(method, path, body=json.dumps(body) if body is not None else None, headers=headers or {})
    res = conn.getresponse()
    data = res.read()
    conn.close()
    return res.status, data


def send(app, cmd, headers=None):
    return request(app, "POST", "/api/cmd", {"cmd": cmd}, headers)[0]


# ------------------------------------------------------------------ commands
def test_valid_commands_reach_the_arduino(app):
    for cmd in ["g", "x", "60", "+", "-", "z", "l", "s"]:
        assert send(app, cmd) == 200
    assert app.sent == ["g", "x", "60", "+", "-", "z", "l", "s"]


@pytest.mark.parametrize("cmd", ["rm -rf /", "n", "v", "gg", "1000", "", "x; g"])
def test_anything_else_is_rejected(app, cmd):
    assert send(app, cmd) == 400
    assert app.sent == []


def test_a_viewer_on_another_device_can_only_stop(app):
    remote = {"X-Forwarded-For": "192.168.1.7"}    # e.g. a phone through a tunnel
    assert send(app, "g", remote) == 403
    assert send(app, "60", remote) == 403
    assert send(app, "x", remote) == 200
    assert app.sent == ["x"]


def test_commands_fail_cleanly_without_an_arduino(app):
    app.hub.writer = None
    assert send(app, "g") == 503


# ------------------------------------------------------------------ data handling
def test_rows_update_peaks_and_counts(app):
    hub = app.hub
    hub.publish(HEADER)
    hub.publish(row(500, temp="31.5", vib="40.0", dist="80.0"))
    hub.publish(row(1000, temp="33.0", vib="35.0", dist="25.0"))
    hub.publish(row(1500, temp="32.0", vib="90.0", dist="149.9"))
    assert len(hub.rows) == 3
    assert hub.peaks["temp_c"] == 33.0
    assert hub.peaks["vib_mg"] == 90.0
    assert hub.peaks["dist_cm"] == 25.0         # 149.9 counts as "nothing in range"


def test_old_rows_are_summarised_not_lost(app):
    hub = app.hub
    hub.publish(HEADER)
    for i in range(server.REPLAY_ROWS + 50):
        hub.publish(row(i * 500))
    assert len(hub.rows) == server.REPLAY_ROWS
    assert hub.evicted["rows"] == 50


def test_new_viewers_get_the_history_first(app):
    hub = app.hub
    hub.publish(HEADER)
    hub.publish("E,1205,STATE,RUN,OK")
    hub.publish(row(1500))
    conn = http.client.HTTPConnection("127.0.0.1", app.port, timeout=5)
    conn.request("GET", "/api/stream")
    res = conn.getresponse()
    assert res.getheader("Content-Type") == "text/event-stream"
    events, data = [], []
    while True:
        line = res.fp.readline().decode().rstrip("\n")
        if line.startswith("event: "):
            events.append(line[7:])
            if line == "event: ready":
                break
        elif line.startswith("data: {\"t\""):
            data.append(json.loads(line[6:])["l"])
    conn.close()
    assert events[0] == "hello"
    assert data == [HEADER, "E,1205,STATE,RUN,OK", row(1500)]


# ------------------------------------------------------------------ demo machine
def test_demo_machine_speaks_the_firmware_format(app):
    demo = server.DemoMachine(app.hub)
    demo.boot()
    for _ in range(20):
        demo.tick()
    rows = [line for _, _, line, _, _ in app.hub.rows]
    assert len(rows) == 20
    assert all(len(r.split(",")) == 1 + len(server.DEFAULT_COLS) for r in rows)
    assert any(line.endswith("SELFTEST,MPU6050=OK DHT22=OK INA219=not fitted 12V_SENSE=not fitted LCD=OK")
               for _, _, line in app.hub.events)


# ------------------------------------------------------------------ web pages
def test_page_and_info_are_served(app):
    status, body = request(app, "GET", "/")
    assert status == 200 and b"<html" in body
    status, body = request(app, "GET", "/api/info")
    info = json.loads(body)
    assert status == 200 and info["can_control"] is True and info["rated_rpm"] == 300


def test_only_known_files_are_served(app):
    for path in ["/server.py", "/../web/server.py", "/data/x.csv", "/README.md"]:
        assert request(app, "GET", path)[0] == 404
