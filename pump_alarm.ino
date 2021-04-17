#include <Ethernet.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <Wire.h>

// I2C address of the 24AA02E48
#define I2C_ADDRESS 0x50

byte mac[6] = {0x98, 0x76, 0xb6, 0x11, 0x9a, 0xcc};

// For use with https://www.crowdsupply.com/silicognition/poe-featherwing
byte readRegister(byte r) {
  Wire.beginTransmission(I2C_ADDRESS);
  Wire.write(r);  // Register to read
  Wire.endTransmission();

  Wire.requestFrom(I2C_ADDRESS, 1); // Read a byte
  while (!Wire.available()) {
    // Wait
  }
  return Wire.read();
}
EthernetClient ethClient;
PubSubClient mqttClient(ethClient);

// 0 = low, 1 = transitioning (prevents spurious highs), 2 = high
int state = 1;
  
void setup() {
  Serial.begin(9600);
  while (!Serial) {}  // wait for serial port to connect. Needed for native USB port only
  // Start I2C bus
  Wire.begin();

//  I don't have a PoE featherwing yet, so skip this and use the hardcoded mac
//  (Would read the MAC programmed in the 24AA02E48 chip)
  for (byte i = 0;  i < 6; i++) {
    mac[i] = readRegister(0xFA + i);
  }

  Serial.println("booted!");

  pinMode(2, INPUT_PULLUP);

  Ethernet.init(10);
  Serial.println("Ethernet prepped!");
  Ethernet.begin(mac);

  Serial.println("Connected!");
  Serial.print("IP: ");
  Serial.println(Ethernet.localIP());
  Serial.print("DNS: ");
  Serial.println(Ethernet.dnsServerIP());
  
  mqttClient.setServer("bilbo.wearden.me", 1883);
  mqttClient.setCallback(handleMessage);
  mqttClient.connect("pump-alarm");
  mqttClient.subscribe("pump-alarm/high-water-detected");
  mqttClient.subscribe("pump-alarm/active");

  Serial.println("mqtt connected!");

  mqttClient.publish("pump-alarm/active", "1");
}

void loop() {
  int readResult = digitalRead(2);
  int transition = readResult ? 1 : -1;
  mqttClient.loop();
  if (state == 1) {
    mqttClient.publish("pump-alarm/high-water-detected", readResult ? "1" : "0");
  }
  state = max(min(state + transition, 2), 0);
  delay(500);
}

void handleMessage(char* topic, byte* payload, unsigned int length) {
  Serial.print("message from topic ");
  Serial.println(topic);
  if (strcmp("pump-alarm/high-water-detected", topic) == 0 && length == 0) {
    mqttClient.publish("pump-alarm/high-water-detected",
      state == 2 ? "1" : "0", 1);
  } else if (strcmp("pump-alarm/active", topic) == 0 && length == 0) {
    mqttClient.publish("pump-alarm/active", "1", 1);
  }
}
