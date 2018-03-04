#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "RemoteDebug.h" // Remote debug over telnet - not recommended for production, only for development			 

const char* ssid = "Hummingbird";
const char* password = "4157315648";

#define LED_PIN 1
#define SWITCH_PIN 4
#define ESC_PIN_A 12
#define ESC_PIN_B 13
#define MOTOR_FB_PIN_A 14
#define MOTOR_FB_PIN_B 16

#define DEBOUNCE_MS 100
#define ESC_FREQ 400
#define PWM_MAX 65535

//uint16_t pulseLenOff = 105;
//uint16_t pulseLenOn = 260;
uint16_t pulseLenOff = 900;
uint16_t pulseLenIdle = 1100;
uint16_t pulseLenOn = 2100;

uint32_t loops = 0;
uint32_t timeCommand = millis();
uint32_t timeNow = millis();
volatile uint32_t timeLastPulseA = micros();
volatile uint32_t timeThisPulseA = micros();
volatile uint32_t timeLastPulseB = micros();
volatile uint32_t timeThisPulseB = micros();

uint32_t escOutVal = 0;
uint32_t onTime = 100;
uint32_t idleTime = 30000;
bool switchPressed = false; // true = pressed
uint16_t motorState = 0; // 3=switch down motor on, 2=switch up still on, 1=idling, 0=off
uint32_t switchLastPressed = 0;

RemoteDebug Debug;

void escOut(uint32_t escOut_us) {
	escOutVal = ((escOut_us * ESC_FREQ) / 1000 * PWM_MAX) / 1000;
	DEBUG_I("%d", escOutVal);
	analogWrite(ESC_PIN_A, escOutVal);
	analogWrite(ESC_PIN_B, escOutVal);
}

void motorAISR() {
	timeThisPulseA = micros();
	DEBUG_I("a%dm%de", timeThisPulseA-timeCommand, timeThisPulseA-timeLastPulseA);
	timeLastPulseA = timeThisPulseA;
}

void motorBISR() {
	timeThisPulseB = micros();
	DEBUG_I("b%dm%de", timeThisPulseB-timeCommand, timeThisPulseB-timeLastPulseB);
	timeLastPulseB = timeThisPulseB;
}

void setup() {
	Serial.begin(115200);
	Serial.println("Booting");
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);
	pinMode(LED_PIN, OUTPUT);
	pinMode(ESC_PIN_A, OUTPUT);
	pinMode(ESC_PIN_B, OUTPUT);

	analogWriteFreq(ESC_FREQ);
	analogWriteRange(PWM_MAX);

	attachInterrupt(MOTOR_FB_PIN_A, motorAISR, CHANGE);
	attachInterrupt(MOTOR_FB_PIN_B, motorBISR, CHANGE);

	ArduinoOTA.onStart([]() {
		String type;
		if (ArduinoOTA.getCommand() == U_FLASH)
			type = "sketch";
		else // U_SPIFFS
			type = "filesystem";

		Serial.println("Start updating " + type);
	});
	ArduinoOTA.onEnd([]() {
		Serial.println("\nEnd");
	});
	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
	});
	ArduinoOTA.onError([](ota_error_t error) {
		Serial.printf("Error[%u]: ", error);
		if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
		else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
		else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
		else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
		else if (error == OTA_END_ERROR) Serial.println("End Failed");
	});
	ArduinoOTA.begin();
	Debug.begin("Telnet_HostName"); // Initiaze the telnet server - this name is used in MDNS.begin
	digitalWrite(LED_PIN, LOW);
}

void loop() {
	ArduinoOTA.handle();
	Debug.handle();
	switchPressed = !digitalRead(SWITCH_PIN);
	if (switchPressed == true) {
		escOut(pulseLenOn);
		motorState = 3;
		digitalWrite(LED_PIN, HIGH);
	} else switch (motorState) {
		case 3:
			switchLastPressed = millis();
			motorState = 2;
			break;
		case 2:
			if (millis() - switchLastPressed > onTime) {
				escOut(pulseLenIdle);
				motorState = 1; // start idling
			}
			break;
		case 1:
			if (millis() - switchLastPressed > idleTime) {
				escOut(pulseLenOff);
				motorState = 0; // stop idling
				digitalWrite(LED_PIN, LOW);
			}
			break;
	}
}