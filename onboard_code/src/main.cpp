#include <ESP32Servo.h>
#include "SerialCommunicator.hpp"
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

#define STEP_DELAY_MS 10

#define COMPLEMENTARY_ALPHA 0.1

#define DEFAULT_DESIRED_PITCH 0



static const uint16_t LISTEN_PORT = 5005;
MPU6050 mpu;


// SerialCommunicator SC;
UdpCommunicator comm(WIFI_SSID, WIFI_PASS, LISTEN_PORT);

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
	// Serial.println(wifi_ok ? "WiFi connected" : "WiFi NOT connected");
}

void set_speed(uint8_t speed, int ch1, int ch2, bool reverse) {
	if (speed > PWM_MAX / 2 ) {
		if (reverse) {
			ledcWrite(ch1, 0);
			ledcWrite(ch2, (speed - PWM_MAX / 2) * 2);
		}
		else {
			ledcWrite(ch1, (speed - PWM_MAX / 2) * 2);
			ledcWrite(ch2, 0);
		}
	}
	else {
		if (reverse) {
			ledcWrite(ch1, (PWM_MAX / 2 - speed) * 2);
			ledcWrite(ch2, 0);
		}
		else {
			ledcWrite(ch1, 0);
			ledcWrite(ch2, (PWM_MAX / 2 - speed) * 2);
		}
	}
}

uint8_t left_speed = 128;
uint8_t right_speed = 128;


float tilt = 0.0f;
float dt   = 0.02f;

#define ALPHA 0.02f   // accel trust: raise if gyro drifts, lower if accel is noisy

void loop() {
	int16_t ax, ay, az, gx, gy, gz;
	mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

	float ax_g = ax / 16384.0f;
	float ay_g = ay / 16384.0f;
	float az_g = az / 16384.0f;

	// ---- Accel tilt: atan2 of the two axes NOT aligned with gravity ----
	// When balanced, one axis reads ~1g — that's your "up" axis.
	// The tilt is the angle away from that.
	//
	// MPU flat (Z up):        accel_tilt = atan2(ax_g, az_g)
	// MPU rotated, Y up:      accel_tilt = atan2(ax_g, ay_g)
	// MPU rotated, X up:      accel_tilt = atan2(ay_g, ax_g)
	//
	// Just power on the robot BALANCED and Serial.print ax_g, ay_g, az_g
	// — whichever reads ~1.0 is your up-axis. Put it as the SECOND arg below.

	float accel_tilt = atan2f(az_g, ax_g) * 180.0f / PI;  // <-- adjust axes here

	// Gyro rate on the tilt axis (matching axis, in deg/s)
	float gyro_rate = gy / 131.0f;   // <-- adjust axis here (gx, gy, or gz)

	// Complementary filter
	tilt = (1.0f - ALPHA) * (tilt + gyro_rate * dt)
		 +         ALPHA  *  accel_tilt;

	Serial.print("Tilt: ");
	Serial.println(tilt);

	delay((int)(dt * 1000));

	comm.read();
	// if (comm.data_updated()) {
		left_speed = comm.get_left();
		right_speed = comm.get_right();

	// }
	return ;
	if (tilt - DEFAULT_DESIRED_PITCH > 10) {
		left_speed += 100;
		right_speed += 100;
	}
	else if (tilt - DEFAULT_DESIRED_PITCH < -10) {
		left_speed -= 100;
		right_speed -= 100;
	}
	set_speed(left_speed, CH_FWD, CH_REV, reverse_left);
	set_speed(right_speed, CH_FWD2, CH_REV2, reverse_right);

	delay(20); // maintain loop timing
}
//
//
// #include "MPU6050.h"
// #include <Wire.h>
// #include <Arduino.h>
//
// MPU6050 mpu;
//
//
// void setup(void) {
// 	Serial.begin(115200);
// 	delay(1000);
// 	Serial.println("Adafruit MPU6050 test! 1");
//
// 	Wire.begin(); // SDA, SCL for ESP32-C3
// 	delay(200);
// 	Wire.setClock(400000); // fast I2C for better reliability
// 	delay(200);
// 	mpu.initialize();
// 		uint8_t error, address;
// 		int nDevices = 0;
//
// 	for (address = 1; address < 127; address++) {
// 		Wire.beginTransmission(address);
// 		error = Wire.endTransmission();
//
// 		if (error == 0) {
// 			Serial.print("Found device at 0x");
// 			if (address < 16) Serial.print("0");
// 			Serial.println(address, HEX);
// 			nDevices++;
// 		} else if (error == 4) {
// 			Serial.print("Unknown error at 0x");
// 			if (address < 16) Serial.print("0");
// 			Serial.println(address, HEX);
// 		}
// 	}
//
//
// }
//
// void loop() {
// 	Serial.println("MPU6050 test!");
// 	if (mpu.testConnection()) {
// 		Serial.println("MPU6050 connection successful!");
// 	}
// 		int16_t ax, ay, az, gx, gy, gz;
// 		mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
// 		float pitch = atan2(ax, sqrt(ay * ay + az * az)) * 180.0 / PI;
// 		float roll = atan2(-ay, az) * 180.0 / PI;
// 		Serial.println(pitch);
//
// 	delay(500);
// }


// #include <Arduino.h>
// #include <Wire.h>
//
// #define SDA_PIN 6
// #define SCL_PIN 7
//
// void setup() {
// 	Serial.begin(115200);
// 	delay(1000);
//
// 	Wire.begin(SDA_PIN, SCL_PIN);
//
// 	Serial.println("I2C Scanner");
// }
//
// void loop() {
// 	uint8_t error, address;
// 	int nDevices = 0;
//
// 	Serial.println("Scanning...");
//
// 	for (address = 1; address < 127; address++) {
// 		Wire.beginTransmission(address);
// 		error = Wire.endTransmission();
//
// 		if (error == 0) {
// 			Serial.print("Found device at 0x");
// 			if (address < 16) Serial.print("0");
// 			Serial.println(address, HEX);
// 			nDevices++;
// 		}
// 		else if (error == 4) {
// 			Serial.print("Unknown error at 0x");
// 			if (address < 16) Serial.print("0");
// 			Serial.println(address, HEX);
// 		}
// 	}
//
// 	if (nDevices == 0) {
// 		Serial.println("No I2C devices found\n");
// 	} else {
// 		Serial.println("Done\n");
// 	}
//
// 	delay(2000);
// }
//



