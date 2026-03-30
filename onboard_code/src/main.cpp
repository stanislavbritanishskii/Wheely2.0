#include <ESP32Servo.h>
#include "UDPCommunicator.hpp"
#include "local_secrets.hpp"
#include <Arduino.h>
#include "MPU6050.h"
#include <Wire.h>
#include <Arduino.h>

#define PIN_FWD 7
#define PIN_REV 5

#define PIN_FWD2 4
#define PIN_REV2 3

#define reverse_left 0
#define reverse_right 0


// PWM config
#define PWM_FREQ 20000
#define PWM_RES 8
#define PWM_MAX 255

// Channels
#define CH_FWD 0
#define CH_REV 1

#define CH_FWD2 2
#define CH_REV2 3

#define STEP_DELAY_MS 2 

#define COMPLEMENTARY_ALPHA 0.1

#define DEFAULT_DESIRED_PITCH 0

#define DEFAULT_P 1
#define DEFAULT_I 1
#define DEFAULT_D 1
#define DEFAULT_LIMIT_P 10
#define DEFAULT_LIMIT_I 10
#define DEFAULT_LIMIT_D 10

#define ALPHA 0.05f   // accel trust: raise if gyro drifts, lower if accel is noisy


static const uint16_t LISTEN_PORT = 5005;
MPU6050 mpu;


// SerialCommunicator SC;
UdpCommunicator comm(WIFI_SSID, WIFI_PASS, LISTEN_PORT, DEFAULT_P, DEFAULT_I, DEFAULT_D, DEFAULT_DESIRED_PITCH, DEFAULT_LIMIT_P, DEFAULT_LIMIT_I, DEFAULT_LIMIT_D);

void setup() {
	Serial.begin(115200);
	delay(1000);

	Wire.begin(); // SDA, SCL for ESP32-C3
	delay(200);
	Wire.setClock(400000); // fast I2C for better reliability
	delay(200);
	mpu.initialize();
	uint8_t error, address;
	int nDevices = 0;

	for (address = 1; address < 127; address++) {
		Wire.beginTransmission(address);
		error = Wire.endTransmission();

		if (error == 0) {
			Serial.print("Found device at 0x");
			if (address < 16) Serial.print("0");
			Serial.println(address, HEX);
			nDevices++;
		} else if (error == 4) {
			Serial.print("Unknown error at 0x");
			if (address < 16) Serial.print("0");
			Serial.println(address, HEX);
		}
	}
	// Serial.begin(115200);
	// Serial.println("hello world");
	ledcSetup(CH_FWD, PWM_FREQ, PWM_RES);
	ledcSetup(CH_REV, PWM_FREQ, PWM_RES);
	ledcSetup(CH_FWD2, PWM_FREQ, PWM_RES);
	ledcSetup(CH_REV2, PWM_FREQ, PWM_RES);

	ledcAttachPin(PIN_FWD, CH_FWD);
	ledcAttachPin(PIN_REV, CH_REV);
	ledcAttachPin(PIN_FWD2, CH_FWD2);
	ledcAttachPin(PIN_REV2, CH_REV2);

	ledcWrite(CH_FWD, 0);
	ledcWrite(CH_REV, 0);
	ledcWrite(CH_FWD2, 0);
	ledcWrite(CH_REV2, 0);

	bool wifi_ok = comm.begin();
	/// Serial.println(wifi_ok ? "WiFi connected" : "WiFi NOT connected");
}

int scale_speed(int speed)
{
	float extra = 110;
	float factor = (255 - extra) / 256;
	int res =0;
	if (speed < -2)
	{
		res = static_cast<int>(-extra + static_cast<float> (speed) * factor);
	}
	else if (speed > 2)
	{
		res = static_cast<int>(extra + static_cast<float> (speed) * factor);
	}
	// Serial.println(res);
	return res;
}

void set_speed(int speed, int ch1, int ch2, bool reverse) {
	// speed = scale_speed(speed);
	speed = scale_speed(speed);
	if ((speed < 0) ^ reverse)
	{
		ledcWrite(ch1, 0);
		ledcWrite(ch2, _abs(speed));
	}
	else
	{
		ledcWrite(ch1, speed);
		ledcWrite(ch2, 0);
	}
	// if (speed > PWM_MAX / 2 ) {
	// 	if (reverse) {
	// 		ledcWrite(ch1, 0);
	// 		ledcWrite(ch2, (speed - PWM_MAX / 2) * 2);
	// 	}
	// 	else {
	// 		ledcWrite(ch1, (speed - PWM_MAX / 2) * 2);
	// 		ledcWrite(ch2, 0);
	// 	}
	// }
	// else {
	// 	if (reverse) {
	// 		ledcWrite(ch1, (PWM_MAX / 2 - speed) * 2);
	// 		ledcWrite(ch2, 0);
	// 	}
	// 	else {
	// 		ledcWrite(ch1, 0);
	// 		ledcWrite(ch2, (PWM_MAX / 2 - speed) * 2);
	// 	}
	// }
}

float left_speed = 0;
float right_speed = 0;


float tilt = DEFAULT_DESIRED_PITCH;
float dt = STEP_DELAY_MS / 1000.f;
float p, i, d, desired_pitch, p_limit, i_limit, d_limit;
float prev_offset = 0;
float integral = 0;

void loop() {
	int16_t ax, ay, az, gx, gy, gz;
	mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
	float ax_g = ax / 16384.0f;
	float ay_g = ay / 16384.0f;
	float az_g = az / 16384.0f;

	float accel_tilt = atan2f(az_g, -ax_g) * 180.0f / PI;
	float gyro_rate = gx / 131.0f;

	tilt = (1.0f - ALPHA) * (tilt + gyro_rate * dt)
		 +         ALPHA  *  accel_tilt;

	// Serial.print("Tilt: ");
	// Serial.println(tilt);

	comm.read();


	p = comm.get_pid_p();
	i = comm.get_pid_i();
	d = comm.get_pid_d();
	desired_pitch = comm.get_desired_angle();
	p_limit = comm.get_limit_p();
	i_limit = comm.get_limit_i();
	d_limit = comm.get_limit_d();

	float current_offset = tilt - desired_pitch;
	if (_abs(current_offset) > 30)
	{
		current_offset = 0;
		integral = 0;
	}
//	Serial.println(accel_tilt);
	// Accumulate and clamp integral
	integral += current_offset;
	if (i != 0)
		integral = constrain(integral, -_abs(i_limit / i), _abs(i_limit / i));  // clamp before multiply

	float term_p = p * current_offset;
	float term_i = i * integral;
	float term_d;
	if (_abs(current_offset - prev_offset) < 3)
		term_d = 0;
	else
		term_d = d * (current_offset - prev_offset);

	// Clamp each term independently
	term_p = constrain(term_p, -p_limit, p_limit);
	term_i = constrain(term_i, -i_limit, i_limit);
	term_d = constrain(term_d, -d_limit, d_limit);

	float speed = term_p + term_i + term_d;

	prev_offset = current_offset;

	left_speed  = speed;
	right_speed = speed;
	left_speed = constrain(left_speed, -190, 190);
	right_speed = constrain(right_speed, -190, 190);
	// Serial.println(left_speed);
	// Serial.println(right_speed);
	// Serial.println();
	set_speed(static_cast<int>(left_speed),  CH_FWD,  CH_REV,  reverse_left);
	set_speed(static_cast<int>(right_speed), CH_FWD2, CH_REV2, reverse_right);
	if (comm.data_updated())
	{
		Serial.println("Communicator updated");
		Serial.println(p);
		Serial.println(i);
		Serial.println(d);
		Serial.println(desired_pitch);
		Serial.println(p_limit);
		Serial.println(i_limit);
		Serial.println(d_limit);
		Serial.println(term_p);
		Serial.println(term_i);
		Serial.println(term_d);
		Serial.println(tilt);
	}
	delay(STEP_DELAY_MS);
}
