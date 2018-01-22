#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>

const int sensor_pin = A0;
const int external_led_pin = 7;

const int new_avg_threshold = 2000;
const int detection_threshold = 100;
const int lower_limit_distance = 7;
const int revolutions_per_kWh = 375;

byte mac[] = { 0x08, 0x00, 0x27, 0xFC, 0xA9, 0x3A };
IPAddress fallback_ip(192, 168, 188, 178);

const char mqtt_server[] = "fgseitsrancher.wifa.intern.uni-leipzig.de";
const int mqtt_port = 1883;
const char user[] = "sepl";
const char pw[] = "sepl";
const char client_name[] = "FC:A9:3A";
const char topic[] = "electricity-meter/FC:A9:3A/consumption";


EthernetClient ethClient;
PubSubClient client(ethClient);


void setup() {
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  pinMode(external_led_pin, OUTPUT);
  
  client.setServer(mqtt_server, mqtt_port);
  
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // try to configure using IP address instead of DHCP:
    Ethernet.begin(mac, fallback_ip);
  }
  // give the Ethernet shield some time to initialize:
  delay(1500);
  /*Serial.println("Configuration:");
  Serial.print("New average threshold: " + String(new_avg_threshold));
  Serial.println("Detection threshold: " + String(detection_threshold));
  Serial.println("Lower limit distance: " + String(lower_limit_distance));
  Serial.println("Revolutions per kWh: " + String(revolutions_per_kWh));
  Serial.println();*/
  Serial.println("Network:");
  Serial.print("MAC address: ");
  for (byte thisByte = 0; thisByte < 6; thisByte++) {
    if (mac[thisByte] < 0x10) {
      Serial.print("0");
    }
    Serial.print(mac[thisByte], HEX);
    if (thisByte < 5) {
      Serial.print(":");
    } else {
      Serial.println();
    }
  }
  Serial.print("IP address: ");
  for (byte thisByte = 0; thisByte < 4; thisByte++) {
    Serial.print(Ethernet.localIP()[thisByte], DEC);
    if (thisByte < 3) {
      Serial.print(".");
    } else {
      Serial.println();
    }
  }
  Serial.println();
  Serial.println("Waiting for MQTT connection ...");
  if (client.connect(client_name, user, pw)) {
    Serial.println("Connected to MQTT broker");
  } else {
    Serial.println("Connection to MQTT broker failed");
  }
  Serial.println();
  Serial.println("Starting routine ...");
  Serial.println("Waiting for first calibration ...");
  Serial.println();
}


bool detected = false;
long reading_sum = 0;
long current_avg = 0;
long tmp_avg = 0;
long iteration = 1;
int new_avg_threshold_count = 1;
bool calibrated = false;
int detection_threshold_count = 1;
int Ws_per_revolution = 3600000 / revolutions_per_kWh;
long last_loop = 0;


void loop() {
  int reading = analogRead(sensor_pin);
  if (calibrated == true) {
    if (reading < current_avg - lower_limit_distance) {
      detection_threshold_count++;
      if (detection_threshold_count == detection_threshold) {
        if (detected == false) {
          detected = true;
          Serial.println("detected = true");
          digitalWrite(external_led_pin, HIGH);
          if (client.connected()) {
            String payload_str = "{\"value\":" + String(Ws_per_revolution) + ",\"unit\":\"Ws\"}";
            char payload[payload_str.length()];
            payload_str.toCharArray(payload, payload_str.length());
            client.publish(topic, payload);
          }
        }
      }
    } else {
      detection_threshold_count = 1;
      if (detected == true) {
        detected = false;
        Serial.println("detected = false");
        digitalWrite(external_led_pin, LOW);
      }
    }
  }
  reading_sum = reading_sum + reading;
  long average = reading_sum / iteration;
  if (average == tmp_avg) {
    new_avg_threshold_count++;
  } else {
    new_avg_threshold_count = 0;
  }
  if (new_avg_threshold_count == new_avg_threshold) {
    current_avg = tmp_avg;
    reading_sum = 0;
    iteration = 1;
    calibrated = true;
    Serial.println("Calibrated with average: " + String(current_avg));
  }
  tmp_avg = average;
  iteration++;
  Ethernet.maintain();
  if (client.connected()) {
    long now = millis();
    if (now - last_loop > 20000) {
      last_loop = now;
      client.loop();
    }
  }
  delay(7);
}
