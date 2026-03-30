#ifndef UDPCOMMUNICATOR_HPP
#define UDPCOMMUNICATOR_HPP

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <string.h>

typedef struct CommunicationData {
	float pid_p;
	float pid_i;
	float pid_d;

	float desired_angle;

	float limit_p;
	float limit_i;
	float limit_d;

} CommunicationData_t;

class UdpCommunicator {
public:
	UdpCommunicator(const char *ssid,
					const char *password,
					uint16_t local_port,
					float pid_p,
	float pid_i,
	float pid_d,

	float desired_angle,

	float limit_p,
	float limit_i,
	float limit_d,
					uint32_t wifi_timeout_ms = 15000);

	bool begin();
	void read();
	bool data_updated();

	// getters
	float get_pid_p();
	float get_pid_i();
	float get_pid_d();

	float get_desired_angle();

	float get_limit_p();
	float get_limit_i();
	float get_limit_d();

	const CommunicationData_t& get_data();

	bool wifi_connected();
	IPAddress local_ip();
	uint16_t local_port();

private:
	bool connect_wifi_();
	void start_udp_();

private:
	CommunicationData_t data{};

	bool _data_updated;

	uint8_t buf[sizeof(CommunicationData_t)]{};

	const char *_ssid;
	const char *_password;
	uint16_t _local_port;
	uint32_t _wifi_timeout_ms;

	WiFiUDP udp;
	bool _udp_started;
};

#endif