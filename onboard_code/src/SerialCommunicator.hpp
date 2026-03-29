#ifndef SERIALCOMMUNICATOR_HPP
#define SERIALCOMMUNICATOR_HPP

#include <Arduino.h>

typedef struct CommunicationData {
	uint8_t speed_left;
	uint8_t speed_right;

} CommunicationData_t;

class SerialCommunicator {
public:
	SerialCommunicator();

	void read();
	bool data_updated();

	uint8_t get_left();
	uint8_t get_right();
private:
	CommunicationData_t data;
	bool _data_updated;
	uint8_t *buf;
};

#endif
