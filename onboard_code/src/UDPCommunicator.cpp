#include "UDPCommunicator.hpp"

UdpCommunicator::UdpCommunicator(const char *ssid,
								const char *password,
								uint16_t local_port,
								uint32_t wifi_timeout_ms) {
	_ssid = ssid;
	_password = password;
	_local_port = local_port;
	_wifi_timeout_ms = wifi_timeout_ms;

	_data_updated = false;
	_udp_started = false;

	memset(&data, 0, sizeof(data));
	data.speed_left = 128;
	data.speed_right = 128;
	memset(buf, 0, sizeof(buf));
}

bool UdpCommunicator::begin() {
	bool ok = connect_wifi_();
	start_udp_();
	return ok;
}

bool UdpCommunicator::connect_wifi_() {
	if (_ssid == NULL || _password == NULL) {
		return false;
	}

	WiFi.mode(WIFI_STA);
	WiFi.setSleep(false);
	WiFi.begin(_ssid, _password);

	uint32_t t0 = millis();
	while (WiFi.status() != WL_CONNECTED) {
		if ((millis() - t0) >= _wifi_timeout_ms) {
			return false;
		}
		delay(50);
	}
	return true;
}

void UdpCommunicator::start_udp_() {
	if (_udp_started) {
		return;
	}
	udp.begin(_local_port);
	_udp_started = true;
}

void UdpCommunicator::read() {
	_data_updated = false;

	if (!_udp_started) {
		return;
	}

	int packetSize = udp.parsePacket();
	if (packetSize != (int) sizeof(CommunicationData_t)) {
		while (udp.available() > 0) {
			udp.read();
		}
		return;
	}

	int n = udp.read(buf, sizeof(buf));
	if (n != (int) sizeof(CommunicationData_t)) {
		return;
	}

	memcpy(&data, buf, sizeof(data));
	_data_updated = true;
}

bool UdpCommunicator::data_updated() {
	return _data_updated;
}

uint8_t UdpCommunicator::get_right() {
	return data.speed_left;
}

uint8_t UdpCommunicator::get_left() {
	return data.speed_right;
}