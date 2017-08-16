#include "BLE.h"
#include "DHT.h"

#define SERVICE_TEMP 0xFF

#define DHTPIN 23
#define LED_PIN LED_BUILTIN

#define DHTTYPE DHT11

BLE ble("BLE Sample");
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(115200);
  
  pinMode(LED_PIN, OUTPUT);
  
  ble.begin();
  
  ble.on(READ, [](int service_uuid, int char_uuid) {
	Serial.println("Event: READ");
	Serial.print("Service UUID: 0x");
	Serial.println(service_uuid, HEX);
	Serial.print("Characteristic UUID: 0x");
	Serial.println(char_uuid, HEX);
	
	if (char_uuid == (SERVICE_TEMP<<8|0x01)) {
		int t = dht.readTemperature();
		
		String str = String(isnan(t) ? 0 : t);
		ble.reply((char*)str.c_str(), str.length());
	}
  });
  
  ble.on(WRITE, [](int service_uuid, int char_uuid) {
	Serial.println("Event: WRITE");
	Serial.print("Service UUID: 0x");
	Serial.println(service_uuid, HEX);
	Serial.print("Characteristic UUID: 0x");
	Serial.println(char_uuid, HEX);
	
	Serial.print("Data: ");
	char *data = ble.data();
	for (int i=0;data[i]!=0;i++) {
		Serial.print("0x");
		if (data[i] < 0x10) Serial.print("0");
		Serial.print(data[i], HEX);
		Serial.print(" ");
	}
	Serial.println();
	
	if (char_uuid == (SERVICE_TEMP<<8|0x01) + 1) {
		int data = ble.data()[0];
		digitalWrite(LED_PIN, data);
	}
  });
  
  ble.addCharacteristic(SERVICE_TEMP, (SERVICE_TEMP<<8|0x01));
  dht.begin();
}

void loop() {
  // put your main code here, to run repeatedly:

}
