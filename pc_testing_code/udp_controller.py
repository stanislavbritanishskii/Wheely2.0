#!/usr/bin/env python3
import sys
import socket
import tkinter as tk

FIELDS = [
	("left", 128),
	("right", 128),
]

class UdpSliderUI:
	def __init__(self, ip, port):
		self.dst_ip = ip
		self.dst_port = int(port)

		self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
		self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

		self.root = tk.Tk()
		self.root.title("UDP Control (CommunicationData_t: 2x uint8)")

		self.values = [tk.IntVar(value=init) for (_, init) in FIELDS]
		self.scales = []
		self._send_scheduled = False

		for i, (name, _) in enumerate(FIELDS):
			row = tk.Frame(self.root)
			row.pack(fill="x", padx=10, pady=6)

			lbl = tk.Label(row, text=name, width=16, anchor="w")
			lbl.pack(side="left")

			scale = tk.Scale(
				row,
				from_=0,
				to=255,
				orient="horizontal",
				length=420,
				variable=self.values[i],
				command=self._on_slider_change,
				showvalue=True,
				resolution=1
			)
			scale.pack(side="left", fill="x", expand=True)
			self.scales.append(scale)

		btnrow = tk.Frame(self.root)
		btnrow.pack(fill="x", padx=10, pady=10)

		send_btn = tk.Button(btnrow, text="Send now", command=self.send_packet)
		send_btn.pack(side="left")

		quit_btn = tk.Button(btnrow, text="Quit", command=self.close)
		quit_btn.pack(side="right")

		self.root.after(50, self.send_packet)

	def _on_slider_change(self, _unused=None):
		if not self._send_scheduled:
			self._send_scheduled = True
			self.root.after(10, self._send_debounced)

	def _send_debounced(self):
		self._send_scheduled = False
		self.send_packet()

	def _clamp_u8(self, v):
		if v < 0:
			return 0
		if v > 255:
			return 255
		return v

	def get_packet_bytes(self):
		speed = self._clamp_u8(int(self.values[0].get())) & 0xFF
		turn = self._clamp_u8(int(self.values[1].get())) & 0xFF
		return bytes((speed, turn))

	def send_packet(self):
		try:
			packet = self.get_packet_bytes()
			print(packet)
			self.sock.sendto(packet, (self.dst_ip, self.dst_port))
		except Exception as e:
			print("UDP send failed:", e, file=sys.stderr)

	def run(self):
		self.root.protocol("WM_DELETE_WINDOW", self.close)
		self.root.mainloop()

	def close(self):
		try:
			if self.sock is not None:
				self.sock.close()
		except Exception:
			pass
		self.root.destroy()

def main():
	if len(sys.argv) < 3:
		print(
			"Usage: python3 sliders_udp.py <ip> <port>\n"
			"Example: python3 sliders_udp.py 192.168.1.50 5005",
			file=sys.stderr
		)
		sys.exit(2)

	ip = sys.argv[1]
	port = int(sys.argv[2])

	app = UdpSliderUI(ip, port)
	app.run()

if __name__ == "__main__":
	main()

