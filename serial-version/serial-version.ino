const int signal_pin = A0;
const int ir_pwr_pin = 6;
const int dip_pwr_pin = 9;
const int tr_pwr_pin = 10;
const int led_pwr_pin = 11;


struct pdcm {
  int pin;
  char* code_on;
  char* code_off;
};

const pdcm dip_pins[] = {
  {13, "A", "F"},
  {12, "B", "G"},
  {5, "C", "H"},
  {7, "D", "I"},
  {8, "E", "J"}
};

int new_avg_threshold = 0;
int detection_threshold = 0;
int no_detection_threshold = 0;
int lower_limit_distance = 0;
int left_boundary = 0;
int right_boundary = 0;


void setupDipPins(const pdcm *pins, int arr_size) {
  for (int i = 0; i < arr_size / sizeof(pdcm); i++) {
    pinMode(pins[i].pin, INPUT);
  }
}


String readDips(const int *pwr, pdcm *pins, int arr_size) {
  digitalWrite(*pwr, HIGH);
  delay(10);
  String code = "";
  for (int i = 0; i < arr_size / sizeof(pdcm); i++) {
     int state = digitalRead(pins[i].pin);
     if (state == 1) {
        code += pins[i].code_on;
     } else {
        code += pins[i].code_off;
     }
  }
  digitalWrite(*pwr, LOW);
  return code;
}


void blinkLED(const int *led_pin, int times=1, int pause=60) {
  for (int bl = 0; bl < times; bl++) {
    digitalWrite(*led_pin, HIGH);
    delay(pause);
    digitalWrite(*led_pin, LOW);
    if (times > 1 and bl != times-1) {
      delay(pause);
    }
  }
}


int denoisedRead(const int *signal_pin, const int *ir_pwr_pin, int pause=1800) {
  int reading_ir, reading_no_ir;
  digitalWrite(*ir_pwr_pin, HIGH);
  delayMicroseconds(pause);
  reading_ir = analogRead(*signal_pin);
  digitalWrite(*ir_pwr_pin, LOW);
  delayMicroseconds(pause);
  reading_no_ir = analogRead(*signal_pin);
  // debug line
  //Serial.print(String(reading_ir) + " - " + String(reading_no_ir) + " = ");
  return reading_ir - reading_no_ir;
}


String readLine(int timeout = 3000) {
  String line = "";
  bool endline = false;
  int i_count = 10;
  int interval = timeout / i_count;
  if (Serial.available() > 0) {
    while (endline == false) {
      if (Serial.available() > 0) {
        char recieved = Serial.read();
        if (recieved == '\n') {
          endline = true;
        } else {
          line += recieved;
        }
      } else {
        if (i_count < 1) {
          line = "";
          break;
        }
        delay(interval);
        i_count--;
      }
    }
    return line;
  }
}

String hw_id = "";

void setup() {
  Serial.begin(9600);
  // wait for serial port to connect. Needed for native USB port only
  while (!Serial) {
    delay(1);
  }

  // set pins
  pinMode(ir_pwr_pin, OUTPUT);
  pinMode(led_pwr_pin, OUTPUT);
  pinMode(dip_pwr_pin, OUTPUT);
  pinMode(tr_pwr_pin, OUTPUT);
  setupDipPins(dip_pins, sizeof(dip_pins));

  // print start message
  delay(2000);
  hw_id = readDips(&dip_pwr_pin, dip_pins, sizeof(dip_pins));
  blinkLED(&led_pwr_pin, 1, 500);
  delay(500);
  Serial.println(F("RDY"));
}


String command = "";

void getCommand() {
  String line = readLine();
  if (line != "") {
    command = line;
    blinkLED(&led_pwr_pin);
  }
}

int reading;
long current_ms;
bool detected;
long reading_sum;
long average;
long current_avg;
long tmp_avg;
long iteration;
long new_avg_threshold_count;
bool calibrated;
long detection_threshold_count;
long no_detection_threshold_count;
long last_loop;
int i_count;


void loop() {
  getCommand();

  if (command == "ID") {
    Serial.println(hw_id);
    command = "";
    Serial.println(F("RDY"));
  }

  if (command == "CONFH") {
    Serial.println(F("LB:RB:DT:NDT"));
    i_count = 10;
    while (command == "CONFH") {
      String conf_line = readLine();
      if (conf_line != "") {
        int pos = conf_line.indexOf(":");
        left_boundary = conf_line.substring(0, pos).toInt();
        int pos2 = conf_line.indexOf(":", pos+1);
        right_boundary = conf_line.substring(pos+1, pos2).toInt();
        int pos3 = conf_line.indexOf(":", pos2+1);
        detection_threshold = conf_line.substring(pos2+1, pos3).toInt();
        no_detection_threshold = conf_line.substring(pos3+1).toInt();
        Serial.println(String(left_boundary) + ":" + String(right_boundary) + ":" + String(detection_threshold) + ":" + String(no_detection_threshold));
        command = "";
      } else {
        if (i_count < 1) {
          Serial.println(String(left_boundary) + ":" + String(right_boundary) + ":" + String(detection_threshold) + ":" + String(no_detection_threshold));
          command = "";
          break;
        }
        delay(5000 / i_count);
        i_count--;
      }
    }
    Serial.println(F("RDY"));
  }
  
  if (command == "CONFA") {
    Serial.println(F("NAT:LLD:DT:NDT"));
    i_count = 10;
    while (command == "CONFA") {
      String conf_line = readLine();
      if (conf_line != "") {
        int pos = conf_line.indexOf(":");
        new_avg_threshold = conf_line.substring(0, pos).toInt();
        int pos2 = conf_line.indexOf(":", pos+1);
        lower_limit_distance = conf_line.substring(pos+1, pos2).toInt();
        int pos3 = conf_line.indexOf(":", pos2+1);
        detection_threshold = conf_line.substring(pos2+1, pos3).toInt();
        no_detection_threshold = conf_line.substring(pos3+1).toInt();
        Serial.println(String(new_avg_threshold) + ":" + String(detection_threshold) + ":" + String(no_detection_threshold) + ":" + String(lower_limit_distance));
        command = "";
      } else {
        if (i_count < 1) {
          Serial.println(String(new_avg_threshold) + ":" + String(detection_threshold) + ":" + String(no_detection_threshold) + ":" + String(lower_limit_distance));
          command = "";
          break;
        }
        delay(5000 / i_count);
        i_count--;
      }
    }
    Serial.println(F("RDY"));
  }
  
  if (command == "MR") {
    digitalWrite(tr_pwr_pin, HIGH);
    while (command != "STP") {
      getCommand();
      Serial.println(denoisedRead(&signal_pin, &ir_pwr_pin));
      delay(100);
    }
    digitalWrite(tr_pwr_pin, LOW);
    Serial.println(F("RDY"));
  }

  if (command == "FE") {
    digitalWrite(tr_pwr_pin, HIGH);
    int left_edge = -100;
    int right_edge = 0;
    bool change = false;
    while (command != "STP") {
      getCommand();
      reading = denoisedRead(&signal_pin, &ir_pwr_pin);
      if (left_edge == -100) {
        left_edge = reading;
      }
      if (reading >= 0 && reading < left_edge) {
        left_edge = reading;
        change = true;
      }
      if (reading > right_edge) {
        right_edge = reading;
        change = true;
      }
      if (change == true) {
        Serial.print(left_edge);
        Serial.print(F(":"));
        Serial.println(right_edge);
        change = false;
      }
      delay(1);
    }
    digitalWrite(tr_pwr_pin, LOW);
    Serial.println(F("RDY"));
  }

  if (command == "HST") {
    Serial.println(F("LE:RE:RES"));
    i_count = 10;
    while (command == "HST") {
      String conf_line = readLine();
      if (conf_line != "") {
        int pos = conf_line.indexOf(":");
        int left_edge = conf_line.substring(0, pos).toInt();
        int pos2 = conf_line.indexOf(":", pos+1);
        int right_edge = conf_line.substring(pos+1, pos2).toInt();
        const int resolution = conf_line.substring(pos2+1).toInt();
        Serial.println(String(left_edge) + ":" + String(right_edge) + ":" + String(resolution));
        long histogram[resolution][3];
        int interval = (right_edge - left_edge) / resolution - 1;
        int remainder = (right_edge - left_edge) % resolution;
        for (int bin = 0; bin < resolution; bin++) {
          if (bin == 0) {
            histogram[bin][0] = left_edge;
            histogram[bin][1] = histogram[bin][0] + interval;
            histogram[bin][2] = 0;
          } else if (bin == resolution - 1) {
            histogram[bin][0] = histogram[bin-1][1] + 1;
            histogram[bin][1] = right_edge;
            histogram[bin][2] = 0;
          } else {
            histogram[bin][0] = histogram[bin-1][1] + 1;
            histogram[bin][1] = histogram[bin][0] + interval;
            histogram[bin][2] = 0;
          }
        }
        int mid;
        int l_pos;
        int r_pos;
        digitalWrite(tr_pwr_pin, HIGH);
        while (command != "STP") {
          reading = denoisedRead(&signal_pin, &ir_pwr_pin);
          l_pos = 0;
          r_pos = resolution - 1;
          while (l_pos <= r_pos) {
            mid = l_pos + (r_pos - l_pos) / 2;
            if (reading >= histogram[mid][0] && reading <= histogram[mid][1]) {
              histogram[mid][2]++;
              break;
            }
            if (reading > histogram[mid][1]) {
              l_pos = mid + 1;
            } else {
              r_pos = mid - 1;
            }
          }
          getCommand();
          delay(1);
        }
        digitalWrite(tr_pwr_pin, LOW);
        for (int bin = 0; bin < resolution; bin++) {
          Serial.print(histogram[bin][0]);
          Serial.print(":");
          Serial.print(histogram[bin][1]);
          Serial.print(":");
          Serial.print(histogram[bin][2]);
          if (bin != resolution - 1) {
            Serial.print(";");
          }
        }
        Serial.println();
      } else {
        if (i_count < 1) {
          Serial.println(F("NaN:NaN:NaN"));
          command = "";
          break;
        }
        delay(5000 / i_count);
        i_count--;
      }
    }
    Serial.println(F("RDY"));
  }

  if (command == "STRTH") {
    digitalWrite(tr_pwr_pin, HIGH);
    detected = false;
    detection_threshold_count = 0;
    no_detection_threshold_count = 0;
    last_loop = 0;
    while (command != "STP") {
      reading = denoisedRead(&signal_pin, &ir_pwr_pin);
      current_ms = millis();
      if (reading >= left_boundary && reading <= right_boundary) {
          if (detection_threshold_count < detection_threshold) {
            detection_threshold_count++;
          } else {
            no_detection_threshold_count = 0;
            if (detected == false) {
              detected = true;
              digitalWrite(led_pwr_pin, HIGH);
              Serial.println(F("DET"));
            }
          }
          //Serial.println(detection_threshold_count);
      } else {
          if (detected == true) {
            if (no_detection_threshold_count < no_detection_threshold) {
              no_detection_threshold_count++;
            } else {
              no_detection_threshold_count = 0;
              detection_threshold_count = 0;
              detected = false;
              digitalWrite(led_pwr_pin, LOW);
            }
          } else {
            detection_threshold_count = 0;
          }
      }
      delay(1);
      getCommand();
      if (current_ms - last_loop > 5000) {
        last_loop = current_ms;
        if (detected != true) {
          blinkLED(&led_pwr_pin, 1, 0);
        }
      }
    }
    digitalWrite(tr_pwr_pin, LOW);
    Serial.println(F("RDY"));
  }
  
  if (command == "STRTA") {
    digitalWrite(tr_pwr_pin, HIGH);
    detected = false;
    reading_sum = 0;
    average = 0;
    current_avg = 0;
    tmp_avg = 0;
    iteration = 1;
    new_avg_threshold_count = 0;
    calibrated = false;
    detection_threshold_count = 0;
    no_detection_threshold_count = 0;
    last_loop = 0;
    while (command != "STP") {
      reading = denoisedRead(&signal_pin, &ir_pwr_pin);
      current_ms = millis();
      if (calibrated == true) {
        if (reading <= (current_avg - lower_limit_distance)) {
          if (detection_threshold_count < detection_threshold) {
            detection_threshold_count++;
          } else {
            no_detection_threshold_count = 0;
            if (detected == false) {
              detected = true;
              Serial.println(F("DET"));
            }
          }
        } else {
          if (detected == true) {
            if (no_detection_threshold_count < no_detection_threshold) {
              no_detection_threshold_count++;
            } else {
              no_detection_threshold_count = 0;
              detection_threshold_count = 0;
              detected = false;
              blinkLED(&led_pwr_pin);
            }
          } else {
            detection_threshold_count = 0;
          }
        }
      }
      if (reading_sum < 2147482623 && iteration < 2147482623) {
        reading_sum = reading_sum + reading;
        average = reading_sum / iteration;
        iteration++;
        if (average == tmp_avg) {
          new_avg_threshold_count++;
        } else {
          new_avg_threshold_count = 0;
        }
      } else {
        new_avg_threshold_count = new_avg_threshold;
      }
      if (new_avg_threshold_count == new_avg_threshold) {
        current_avg = tmp_avg;
        reading_sum = 0;
        iteration = 1;
        if (calibrated == false) {
          calibrated = true;
          Serial.println(F("CAL"));
        }
        Serial.print(F("AVG:"));
        Serial.println(String(current_avg));
      }
      tmp_avg = average;
      delay(1);
      getCommand();
      if (current_ms - last_loop > 5000) {
        last_loop = current_ms;
        blinkLED(&led_pwr_pin, 1, 0);
      }
    }
    digitalWrite(tr_pwr_pin, LOW);
    Serial.println(F("RDY"));
  }
  delay(10);
}
