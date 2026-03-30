"""
PID Tuner — sends CommunicationData_t over UDP.

Struct layout (7 floats, little-endian):
  float pid_p, pid_i, pid_d, desired_angle, limit_p, limit_i, limit_d

No extra dependencies — uses only the standard library.
python pid_tuner.py

Settings (IP, port) are saved to pid_tuner_config.json next to the script.
"""

import json
import socket
import struct
import threading
import time
import tkinter as tk
from pathlib import Path
from tkinter import messagebox, ttk

# ── Config ────────────────────────────────────────────────────────────────────
SEND_HZ     = 20
STRUCT_FMT  = "<7f"   # 7 little-endian floats = 28 bytes
CONFIG_PATH = Path(__file__).with_name("pid_tuner_config.json")

DEFAULTS = {
    "udp_host": "192.168.1.100",
    "udp_port": "5005",
}

# ── Slider definitions  (label, key, min, max, default) ───────────────────────
SLIDERS = [
    ("P gain",        "pid_p",         -50.0,  20.0,  0.0),
    ("I gain",        "pid_i",          -10.0,   10.0,  0.0),
    ("D gain",        "pid_d",         -50.0,  20.0,  0.0),
    ("Desired angle", "desired_angle", -4.4,  0.0,  -2.20),
    ("Limit P",       "limit_p",         0.0, 255.0,  0.0),
    ("Limit I",       "limit_i",         0.0, 255.0,  0.0),
    ("Limit D",       "limit_d",         0.0, 255.0,  0.0),
]

# ──────────────────────────────────────────────────────────────────────────────

def load_config() -> dict:
    try:
        return {**DEFAULTS, **json.loads(CONFIG_PATH.read_text())}
    except Exception:
        return dict(DEFAULTS)

def save_config(cfg: dict):
    try:
        CONFIG_PATH.write_text(json.dumps(cfg, indent=2))
    except Exception:
        pass


class PIDTuner(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("PID Tuner")
        self.resizable(False, False)
        self.configure(bg="#1e1e2e")
        self.protocol("WM_DELETE_WINDOW", self._on_close)

        self.cfg      = load_config()
        self.sock     = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.target: tuple[str, int] | None = None
        self.dirty    = threading.Event()
        self.auto_send = threading.Event()   # set = continuous 1 Hz sending active
        self.values: dict[str, float] = {key: default for _, key, _, _, default in SLIDERS}

        self._build_ui()
        self._start_sender()

    # ── UI ────────────────────────────────────────────────────────────────────

    def _build_ui(self):
        PAD = dict(padx=16, pady=5)
        BG  = "#1e1e2e"
        FG  = "#cdd6f4"
        ACC = "#89b4fa"
        SLI = "#313244"
        ENT = "#45475a"

        style = ttk.Style(self)
        style.theme_use("clam")
        style.configure("TScale",    background=BG, troughcolor=SLI, sliderlength=18)
        style.configure("TLabel",    background=BG, foreground=FG,   font=("Courier New", 10))
        style.configure("Header.TLabel", background=BG, foreground=ACC,
                        font=("Courier New", 13, "bold"))
        style.configure("Val.TLabel", background=BG, foreground="#a6e3a1",
                        font=("Courier New", 10, "bold"), width=8)
        style.configure("Status.TLabel", background=BG, foreground="#fab387",
                        font=("Courier New", 9))
        style.configure("TButton",   background=SLI, foreground=FG,
                        font=("Courier New", 10), relief="flat", padding=4)
        style.map("TButton",
                  background=[("active", ACC)],
                  foreground=[("active", BG)])
        style.configure("TEntry",    fieldbackground=ENT, foreground=FG,
                        insertcolor=FG, font=("Courier New", 10))

        # ── Header ────────────────────────────────────────────────────────────
        ttk.Label(self, text="[ PID TUNER ]", style="Header.TLabel") \
            .grid(row=0, column=0, columnspan=3, pady=(14, 6))

        # ── UDP host row ──────────────────────────────────────────────────────
        ttk.Label(self, text="UDP host:", style="TLabel") \
            .grid(row=1, column=0, sticky="e", **PAD)

        self.host_var = tk.StringVar(value=self.cfg["udp_host"])
        ttk.Entry(self, textvariable=self.host_var, width=20, style="TEntry") \
            .grid(row=1, column=1, sticky="w", **PAD)

        # ── UDP port row ──────────────────────────────────────────────────────
        ttk.Label(self, text="UDP port:", style="TLabel") \
            .grid(row=2, column=0, sticky="e", **PAD)

        self.udp_port_var = tk.StringVar(value=self.cfg["udp_port"])
        ttk.Entry(self, textvariable=self.udp_port_var, width=8, style="TEntry") \
            .grid(row=2, column=1, sticky="w", **PAD)

        # Connect button spans both rows on the right
        self.connect_btn = ttk.Button(self, text="Connect", command=self._toggle_connect)
        self.connect_btn.grid(row=1, column=2, rowspan=2, padx=16, pady=5, sticky="ns")

        # ── Status ────────────────────────────────────────────────────────────
        self.status_var = tk.StringVar(value="not connected")
        ttk.Label(self, textvariable=self.status_var, style="Status.TLabel") \
            .grid(row=3, column=0, columnspan=3, pady=(2, 6))

        ttk.Separator(self, orient="horizontal") \
            .grid(row=4, column=0, columnspan=3, sticky="ew", padx=16, pady=4)

        # ── Sliders ───────────────────────────────────────────────────────────
        self.val_vars: dict[str, tk.DoubleVar] = {}
        self.val_lbls: dict[str, ttk.Label]    = {}

        for idx, (label, key, lo, hi, default) in enumerate(SLIDERS):
            row = 5 + idx

            ttk.Label(self, text=f"{label:<16}", style="TLabel") \
                .grid(row=row, column=0, sticky="e", **PAD)

            var = tk.DoubleVar(value=default)
            self.val_vars[key] = var

            def _cb(val, k=key):
                self.values[k] = round(float(val), 4)
                self.val_lbls[k].config(text=f"{float(val):+.3f}")
                self.dirty.set()

            ttk.Scale(self, from_=lo, to=hi, orient="horizontal",
                      variable=var, length=320, command=_cb, style="TScale") \
                .grid(row=row, column=1, **PAD)

            lbl = ttk.Label(self, text=f"{default:+.3f}", style="Val.TLabel")
            lbl.grid(row=row, column=2, sticky="w", **PAD)
            self.val_lbls[key] = lbl

        # ── Bottom controls ───────────────────────────────────────────────────
        btn_frame = tk.Frame(self, bg=BG)
        btn_frame.grid(row=5 + len(SLIDERS), column=0, columnspan=3, pady=12)

        ttk.Button(btn_frame, text="Reset all to 0", command=self._reset) \
            .pack(side="left", padx=8)

        ttk.Button(btn_frame, text="Send now", command=self._send) \
            .pack(side="left", padx=8)

        self.auto_var = tk.BooleanVar(value=False)
        ttk.Checkbutton(
            btn_frame, text="Send every 1 s",
            variable=self.auto_var, command=self._toggle_auto,
        ).pack(side="left", padx=8)

    # ── Connection ────────────────────────────────────────────────────────────

    def _toggle_connect(self):
        if self.target is not None:
            self.target = None
            self.connect_btn.config(text="Connect")
            self.status_var.set("disconnected")
            return

        host     = self.host_var.get().strip()
        port_str = self.udp_port_var.get().strip()

        if not host:
            messagebox.showerror("Error", "Enter a UDP host address.")
            return
        try:
            port = int(port_str)
            if not (1 <= port <= 65535):
                raise ValueError
        except ValueError:
            messagebox.showerror("Error", f"Invalid port: {port_str!r}")
            return

        self.target = (host, port)
        self.connect_btn.config(text="Disconnect")
        self.status_var.set(f"sending  →  {host}:{port}")

        self.cfg["udp_host"] = host
        self.cfg["udp_port"] = port_str
        save_config(self.cfg)

        self.dirty.set()

    def _toggle_auto(self):
        if self.auto_var.get():
            self.auto_send.set()
        else:
            self.auto_send.clear()

    # ── UDP send ──────────────────────────────────────────────────────────────

    def _send(self):
        if self.target is None:
            return
        v = self.values
        payload = struct.pack(
            STRUCT_FMT,
            v["pid_p"], v["pid_i"], v["pid_d"],
            v["desired_angle"],
            v["limit_p"], v["limit_i"], v["limit_d"],
        )
        try:
            self.sock.sendto(payload, self.target)
        except OSError as e:
            self.status_var.set(f"send error: {e}")

    def _start_sender(self):
        interval = 1.0 / SEND_HZ

        def loop():
            last_auto = 0.0
            while True:
                # wake up at most every sender-tick to check auto_send
                self.dirty.wait(timeout=interval)
                if self.dirty.is_set():
                    self.dirty.clear()
                    self._send()
                now = time.monotonic()
                if self.auto_send.is_set() and now - last_auto >= 1.0:
                    self._send()
                    last_auto = now

        threading.Thread(target=loop, daemon=True).start()

    # ── Helpers ───────────────────────────────────────────────────────────────

    def _reset(self):
        for _, key, _, _, default in SLIDERS:
            self.val_vars[key].set(default)
            self.values[key] = default
            self.val_lbls[key].config(text=f"{default:+.3f}")
        self.dirty.set()

    def _on_close(self):
        # always save whatever is in the fields, even if never connected
        self.cfg["udp_host"] = self.host_var.get().strip()
        self.cfg["udp_port"] = self.udp_port_var.get().strip()
        save_config(self.cfg)
        self.sock.close()
        self.destroy()


if __name__ == "__main__":
    app = PIDTuner()
    app.mainloop()
