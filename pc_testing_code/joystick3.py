#!/usr/bin/env python3
import sys
import socket
import pygame
import pygame.font
import math

# Configuration
JOYSTICK_RADIUS = 100
WINDOW_SIZE = 300
FPS = 60

class PygameTankController:
	def __init__(self, ip, port):
		self.dst_ip = ip
		self.dst_port = int(port)
		self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

		# Pygame init
		pygame.init()
		self.screen = pygame.display.set_mode((WINDOW_SIZE, WINDOW_SIZE))
		pygame.display.set_caption("Tank 2WD Controller")
		self.clock = pygame.time.Clock()

		# Joystick state
		self.joystick_center = (WINDOW_SIZE // 2, WINDOW_SIZE // 2)
		self.joystick_pos = self.joystick_center
		self.dragging = False

		# Reverse flags
		self.left_reverse = False
		self.right_reverse = False

		# Sensitivity (exponent <1: fast start, slow max; exponent=1: linear)
		self.sensitivity = 0.5

	def _clamp_u8(self, v):
		if v < 0:
			return 0
		if v > 255:
			return 255
		return v

	def _nonlinear_scale(self, value, exponent=None):
		if exponent is None:
			exponent = self.sensitivity
		sign = 1 if value >= 0 else -1
		return sign * (abs(value) ** exponent)

	def _compute_motor_speeds(self, x, y):
		# Normalize x,y to [-1,1]
		dx = (x - self.joystick_center[0]) / JOYSTICK_RADIUS
		dy = -(y - self.joystick_center[1]) / JOYSTICK_RADIUS  # y axis inverted
		dx = max(-1.0, min(1.0, dx))
		dy = max(-1.0, min(1.0, dy))

		# Tank mixing
		left = dy + dx
		right = dy - dx

		# Normalize to [-1,1]
		max_val = max(abs(left), abs(right))
		if max_val > 1:
			left /= max_val
			right /= max_val

		# Apply reverse
		if self.left_reverse:
			left = -left
		if self.right_reverse:
			right = -right

		# Apply nonlinear scaling
		left = self._nonlinear_scale(left)
		right = self._nonlinear_scale(right)

		# Map [-1,1] -> [0,255]
		left_val = self._clamp_u8(int((left + 1) / 2 * 255))
		right_val = self._clamp_u8(int((right + 1) / 2 * 255))
		return left_val, right_val

	def send_packet(self, left, right):
		try:
			packet = bytes((left, right))
			self.sock.sendto(packet, (self.dst_ip, self.dst_port))
		except Exception as e:
			print("UDP send failed:", e, file=sys.stderr)

	def run(self):
		running = True
		while running:
			for event in pygame.event.get():
				if event.type == pygame.QUIT:
					running = False
				elif event.type == pygame.MOUSEBUTTONDOWN:
					if math.hypot(event.pos[0] - self.joystick_center[0],
								  event.pos[1] - self.joystick_center[1]) <= JOYSTICK_RADIUS:
						self.dragging = True
				elif event.type == pygame.MOUSEBUTTONUP:
					self.dragging = False
					self.joystick_pos = self.joystick_center
				elif event.type == pygame.MOUSEMOTION and self.dragging:
					dx = event.pos[0] - self.joystick_center[0]
					dy = event.pos[1] - self.joystick_center[1]
					dist = math.hypot(dx, dy)
					if dist > JOYSTICK_RADIUS:
						dx = dx / dist * JOYSTICK_RADIUS
						dy = dy / dist * JOYSTICK_RADIUS
					self.joystick_pos = (self.joystick_center[0] + dx,
										 self.joystick_center[1] + dy)
				elif event.type == pygame.KEYDOWN:
					if event.key == pygame.K_q:
						self.left_reverse = not self.left_reverse
					elif event.key == pygame.K_e:
						self.right_reverse = not self.right_reverse
					elif event.key == pygame.K_UP:
						self.sensitivity = min(1.0, self.sensitivity + 0.05)
					elif event.key == pygame.K_DOWN:
						self.sensitivity = max(0.1, self.sensitivity - 0.05)

			# Compute motor values
			left, right = self._compute_motor_speeds(*self.joystick_pos)
			self.send_packet(left, right)

			# Draw UI
			self.screen.fill((30, 30, 30))
			pygame.draw.circle(self.screen, (100, 100, 100), self.joystick_center, JOYSTICK_RADIUS, 5)
			pygame.draw.circle(self.screen, (200, 50, 50), (int(self.joystick_pos[0]), int(self.joystick_pos[1])), 20)

			# Reverse indicators
			if self.left_reverse:
				pygame.draw.circle(self.screen, (0, 255, 0), (50, WINDOW_SIZE - 50), 10)
			if self.right_reverse:
				pygame.draw.circle(self.screen, (0, 255, 0), (WINDOW_SIZE - 50, WINDOW_SIZE - 50), 10)

			# Sensitivity text
			font = pygame.font.SysFont(None, 24)
			txt = font.render(f"Sensitivity: {self.sensitivity:.2f}", True, (255, 255, 255))
			self.screen.blit(txt, (10, 10))

			pygame.display.flip()
			self.clock.tick(FPS)

		self.sock.close()
		pygame.quit()


def main():
	if len(sys.argv) < 3:
		print(
			"Usage: python3 tank_joystick.py <ip> <port>\n"
			"Example: python3 tank_joystick.py 192.168.1.50 5005",
			file=sys.stderr
		)
		sys.exit(2)

	ip = sys.argv[1]
	port = int(sys.argv[2])

	controller = PygameTankController(ip, port)
	controller.run()


if __name__ == "__main__":
	main()
