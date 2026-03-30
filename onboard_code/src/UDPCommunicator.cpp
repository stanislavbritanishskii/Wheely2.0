#include "UDPCommunicator.hpp"

UdpCommunicator::UdpCommunicator(const char* ssid,
								const char* password,
								uint16_t local_port,
								float pid_p,
								float pid_i,
								float pid_d,
								float desired_angle,
								float limit_p,
								float limit_i,
								float limit_d,
								uint32_t wifi_timeout_ms)
{
	_ssid = ssid;
	_password = password;
	_local_port = local_port;
	_wifi_timeout_ms = wifi_timeout_ms;

	_data_updated = false;
	_udp_started = false;

	// initialize directly
	data.pid_p = pid_p;
	data.pid_i = pid_i;
	data.pid_d = pid_d;
	data.desired_angle = desired_angle;
	data.limit_p = limit_p;
	data.limit_i = limit_i;
	data.limit_d = limit_d;


	memset(buf, 0, sizeof(buf));
}

bool UdpCommunicator::begin()
{
	bool ok = connect_wifi_();
	start_udp_();
	return ok;
}

bool UdpCommunicator::connect_wifi_()
{
	if (_ssid == NULL || _password == NULL)
	{
		return false;
	}

	WiFi.mode(WIFI_STA);
	WiFi.setSleep(false);
	WiFi.begin(_ssid, _password);

	uint32_t t0 = millis();
	while (WiFi.status() != WL_CONNECTED)
	{
		if ((millis() - t0) >= _wifi_timeout_ms)
		{
			return false;
		}
		delay(50);
	}
	return true;
}

void UdpCommunicator::start_udp_()
{
	if (_udp_started)
	{
		return;
	}
	udp.begin(_local_port);
	_udp_started = true;
}

void UdpCommunicator::read()
{
	_data_updated = false;

	if (!_udp_started)
	{
		return;
	}

	int packetSize = udp.parsePacket();

	if (packetSize != (int)sizeof(CommunicationData_t))
	{
		while (udp.available() > 0)
		{
			udp.read();
		}
		return;
	}

	int n = udp.read(buf, sizeof(buf));

	if (n != (int)sizeof(CommunicationData_t))
	{
		return;
	}

	memcpy(&data, buf, sizeof(data));
	_data_updated = true;
}

bool UdpCommunicator::data_updated()
{
	return _data_updated;
}

const CommunicationData_t& UdpCommunicator::get_data()
{
	return data;
}

// ===== getters =====

float UdpCommunicator::get_pid_p()
{
	return data.pid_p;
}

float UdpCommunicator::get_pid_i()
{
	return data.pid_i;
}

float UdpCommunicator::get_pid_d()
{
	return data.pid_d;
}

float UdpCommunicator::get_desired_angle()
{
	return data.desired_angle;
}

float UdpCommunicator::get_limit_p()
{
	return data.limit_p;
}

float UdpCommunicator::get_limit_i()
{
	return data.limit_i;
}

float UdpCommunicator::get_limit_d()
{
	return data.limit_d;
}

// ===== wifi helpers =====

bool UdpCommunicator::wifi_connected()
{
	return WiFi.status() == WL_CONNECTED;
}

IPAddress UdpCommunicator::local_ip()
{
	return WiFi.localIP();
}

uint16_t UdpCommunicator::local_port()
{
	return _local_port;
}
