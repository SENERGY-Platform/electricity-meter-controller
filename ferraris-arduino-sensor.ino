#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <PubSubClient.h>
#include <TimeLib.h>
#include <SoftwareReset.h>

const int sensor_pin = A0;
const int external_led_pin = 7;

const int new_avg_threshold = 2000;
const int detection_threshold = 100;
const int lower_limit_distance = 5;
const int revolutions_per_kWh = 375;

byte mac[] = { 0x08, 0x00, 0x15, 0xBC, 0xD7, 0x4B };
IPAddress fallback_ip(192, 168, 188, 178);
unsigned int localPort = 8888;  // local port to listen for UDP packets

const char mqtt_server[] = "fgseitsrancher.wifa.intern.uni-leipzig.de";
const int mqtt_port = 1883;
const char user[] = "sepl";
const char pw[] = "sepl";
const char client_name[] = "ferraris_arduino_001";
const char topic[] = "ferraris_arduino_001/consumption";

const int timeZone = 1;     // Central European Time
char timeServer[] = "time.nist.gov"; // time.nist.gov NTP server



EthernetClient ethClient;
EthernetUDP Udp;
PubSubClient client(ethClient);


void blinkLED(int times=1, int wait=200) {
  for (int bl = 0; bl < times; bl++) {
    digitalWrite(external_led_pin, HIGH);
    delay(wait);
    digitalWrite(external_led_pin, LOW);
    if (times > 1 and bl != times-1) {
      delay(wait);
    }
  }
}


void resetBoard(bool feedback=false) {
  if (feedback == true) {
    blinkLED(10, 50);
  }
  Serial.println();
  Serial.println("Resetting board");
  Serial.println();
  delay(10);
  softwareReset(STANDARD);
}


void setup() {
  Serial.begin(9600);

  // wait for serial port to connect. Needed for native USB port only
  while (!Serial) {
    ;
  }

  // set LED pin
  pinMode(external_led_pin, OUTPUT);

  // print start message
  delay(2000);
  Serial.println("---------------------");
  Serial.println();
  Serial.println("Starting ...");
  blinkLED(1, 1000);
  Serial.println();
  
  // set server for MQTT client
  client.setServer(mqtt_server, mqtt_port);

  // start ethernet
  Serial.println("Waiting for network:");
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // try to configure using IP address instead of DHCP:
    Ethernet.begin(mac, fallback_ip);
    delay(1500);
    blinkLED(5, 50);
  } else {
    delay(1500);
    blinkLED();
  }
  // give the Ethernet shield some time to initialize

  // print network info
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
      if (Ethernet.localIP() == fallback_ip) {
        Serial.print(" (static fallback ip)");
      }
      Serial.println();
    }
  }
  Serial.println();

  // start UDP and set local time
  Serial.println("Waiting for NTP sync ...");
  Udp.begin(localPort);
  // give some time to initialize:
  delay(1000);
  //setSyncProvider(getNtpTime);
  time_t ntp_time = getNtpTime();
  if (ntp_time > 0) {
    setTime(ntp_time);
    Serial.println("Success: " + dateTimeISO8601());
    blinkLED();
  } else {
    Serial.println("Sync with NTP server failed");
    blinkLED(5, 50);
    resetBoard();
  }
  delay(500);
  Serial.println();

  // start MQTT client
  Serial.println("Waiting for MQTT connection ...");
  delay(1500);
  if (client.connect(client_name, user, pw)) {
    Serial.println("Connected to MQTT broker");
    blinkLED();
  } else {
    Serial.println("Connection to MQTT broker failed");
    blinkLED(5, 50);
    resetBoard();
  }
  Serial.println();

  // print final info
  Serial.println("Waiting for first calibration ...");
}



String dateTimeISO8601() {
  return String(year()) + "-" + addZero(month()) + "-" + addZero(day()) + "T" + addZero(hour()) + ":" + addZero(minute()) + ":" + addZero(second()) + "Z+1";
}

String addZero(int digits) {
  if(digits < 10) {
    return "0" + String(digits);
  } else {
    return String(digits);
  }
}




const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime() {
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  //Serial.println("Transmit NTP Request");
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      //Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  //Serial.println("No NTP Response");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(char* address) {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:                 
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
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

long lowest = 0;

void loop() {
  int reading = analogRead(sensor_pin);
  /*if (lowest == 0) {
    lowest = reading;
  }
  if (reading < lowest) {
    lowest = reading;
    Serial.println("low="+String(lowest));
  }*/
  if (calibrated == true) {
    if (reading < (current_avg - lower_limit_distance)) {
      //Serial.println(detection_threshold_count);
      if (detection_threshold_count <= detection_threshold) {
        detection_threshold_count++;
      }
      if (detection_threshold_count == detection_threshold) {
        if (detected == false) {
          detected = true;
          Serial.println("Detected = true");
          //digitalWrite(external_led_pin, HIGH);
          if (client.connected()) {
            String payload_str = "{\"value\":" + String(Ws_per_revolution) + ",\"unit\":\"Ws\",\"time\":\"" + dateTimeISO8601() + "\"}";
            //Serial.println(payload_str.c_str());
            client.publish(topic, payload_str.c_str());
          }
        }
      }
    } else {
      detection_threshold_count = 1;
      if (detected == true) {
        detected = false;
        Serial.println("Detected = false");
        //digitalWrite(external_led_pin, LOW);
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
    if (calibrated == false) {
      calibrated = true;
      blinkLED();
    }
    Serial.println("Calibrated with average: " + String(current_avg));
  }
  tmp_avg = average;
  iteration++;
  Ethernet.maintain();
  if (client.connected()) {
    long now = millis();
    if (now - last_loop > 5000) {
      last_loop = now;
      client.loop();
      blinkLED(1, 0);
    }
  } else {
    Serial.println();
    Serial.println("MQTT connection lost");
    resetBoard(true);
  }
  delay(7);
}
