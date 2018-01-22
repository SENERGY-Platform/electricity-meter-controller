const int sensor_pin = A0;
const int new_avg_threshold = 2000;
const int detection_threshold = 100;
const int lower_limit_distance = 7;
const int external_led_pin = 7;
const int revolutions_per_kWh = 375;

int Ws_per_revolution;

void setup() {
  Serial.begin(9600);
  pinMode(external_led_pin, OUTPUT);
  Ws_per_revolution = 3600000 / revolutions_per_kWh;
}


bool detected = false;
long reading_sum = 0;
long current_avg = 0;
long tmp_avg = 0;
long iteration = 1;
int new_avg_threshold_count = 1;
bool calibrated = false;
int detection_threshold_count = 1;


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
  delay(7);
}
