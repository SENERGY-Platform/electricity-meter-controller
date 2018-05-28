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


void setupDipPins(const pdcm *pins, int arr_size) {
  for (int i = 0; i < arr_size / sizeof(pdcm); i++) {
    pinMode(pins[i].pin, INPUT);
  }
}


String hw_id = "";

void readDips(const int *pwr, pdcm *pins, int arr_size) {
  digitalWrite(*pwr, HIGH);
  delay(10);
  for (int i = 0; i < arr_size / sizeof(pdcm); i++) {
     int state = digitalRead(pins[i].pin);
     if (state == 1) {
        hw_id += pins[i].code_on;
     } else {
        hw_id += pins[i].code_off;
     }
  }
  digitalWrite(*pwr, LOW);
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
  readDips(&dip_pwr_pin, dip_pins, sizeof(dip_pins));
  blinkLED(&led_pwr_pin, 1, 500);
  delay(500);
  Serial.println(F("RDY"));
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


String command = "";

void getCommand() {
  String line = readLine();
  if (line != "") {
    command = line;
    blinkLED(&led_pwr_pin);
  }
}


void manualRead() {
  digitalWrite(tr_pwr_pin, HIGH);
  while (command != "STP") {
    getCommand();
    Serial.println(denoisedRead(&signal_pin, &ir_pwr_pin));
    delay(100);
  }
  digitalWrite(tr_pwr_pin, LOW);
}


char mode = 'H';
int conf_a = 0;
int conf_b = 0;
int detection_threshold = 0;
int no_detection_threshold = 0;

void configure() {
  Serial.println(F("M:CA:CB:DT:NDT"));
  int i_count = 10;
  while (command == "CONF") {
    String conf_line = readLine();
    if (conf_line != "") {
      int pos = conf_line.indexOf(":");
      mode = conf_line.substring(0, pos).toInt();
      int pos2 = conf_line.indexOf(":", pos+1);
      conf_a = conf_line.substring(pos+1, pos2).toInt();
      int pos3 = conf_line.indexOf(":", pos2+1);
      conf_b = conf_line.substring(pos2+1, pos3).toInt();
      int pos4 = conf_line.indexOf(":", pos3+1);
      detection_threshold = conf_line.substring(pos3+1, pos4).toInt();
      no_detection_threshold = conf_line.substring(pos4+1).toInt();
      Serial.println(String(mode) + ":" + String(conf_a) + ":" + String(conf_b) + ":" + String(detection_threshold) + ":" + String(no_detection_threshold));
      command = "";
    } else {
      if (i_count < 1) {
        Serial.println(String(mode) + ":" + String(conf_a) + ":" + String(conf_b) + ":" + String(detection_threshold) + ":" + String(no_detection_threshold));
        command = "";
        break;
      }
      delay(5000 / i_count);
      i_count--;
    }
  }
}


int reading;

void findEdges() {
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
}


void buildHistogram() {
  Serial.println(F("LE:RE:RES"));
  int i_count = 10;
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
}


//left_boundary = conf_a
//right_boundary = conf_b

void intervalDetection() {
  digitalWrite(tr_pwr_pin, HIGH);
  bool detected = false;
  int detection_threshold_count = 0;
  int no_detection_threshold_count = 0;
  long current_ms;
  long last_loop = 0;
  while (command != "STP") {
    reading = denoisedRead(&signal_pin, &ir_pwr_pin);
    current_ms = millis();
    if (reading >= conf_a && reading <= conf_b) {
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
}


//new_avg_threshold = conf_a
//lower_limit_distance = conf_b

void averageDetection() {
  digitalWrite(tr_pwr_pin, HIGH);
  bool detected = false;
  int detection_threshold_count = 0;
  int no_detection_threshold_count = 0;
  bool calibrated = false;
  long reading_sum = 0;
  long average = 0;
  long current_avg = 0;
  long tmp_avg = 0;
  long new_avg_threshold_count = 0;
  long iteration = 1;
  long current_ms;
  long last_loop = 0;
  while (command != "STP") {
    reading = denoisedRead(&signal_pin, &ir_pwr_pin);
    current_ms = millis();
    if (calibrated == true) {
      if (reading <= (current_avg - conf_b)) {
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
      new_avg_threshold_count = conf_a;
    }
    if (new_avg_threshold_count == conf_a) {
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
}


void loop() {
  getCommand();

  if (command == "ID") {
    Serial.println(hw_id);
    command = "";
    Serial.println(F("RDY"));
  }

  if (command == "CONF") {
    configure();
    Serial.println(F("RDY"));
  }
  
  if (command == "MR") {
    manualRead();
    Serial.println(F("RDY"));
  }

  if (command == "FE") {
    findEdges();
    Serial.println(F("RDY"));
  }

  if (command == "HST") {
    buildHistogram();
    Serial.println(F("RDY"));
  }

  if (command == "STRT") {
    switch (mode) {
      case 'H':
        intervalDetection();
        break;
      case 'A':
        averageDetection();
        break;
    }
    Serial.println(F("RDY"));
  }

  delay(10);
}
