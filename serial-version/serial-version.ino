const int signal_pin = A0;
const int ir_pwr_pin = 6;
const int dip_pwr_pin = 9;
const int tr_pwr_pin = 10;
const int led_pwr_pin = 11;
const char sw_version[] = "2.11.6";

struct pdcm {
  int pin;
  char code_on;
  char code_off;
};

const pdcm dip_pins[] = {
  {13, '1', '6'},
  {12, '2', '7'},
  {5, '3', '8'},
  {7, '4', '9'},
  {8, '5', '0'}
};


void setupDipPins(const pdcm *pins, int arr_size) {
  for (int i = 0; i < arr_size / sizeof(pdcm); i++) {
    pinMode(pins[i].pin, INPUT);
  }
}


char hw_id[6] = "_ERR_";

void readDips(const int *pwr, pdcm *pins, int arr_size) {
  digitalWrite(*pwr, HIGH);
  delay(1);
  for (int i = 0; i < arr_size / sizeof(pdcm); i++) {
     int state = digitalRead(pins[i].pin);
     if (state == 1) {
        hw_id[i] = pins[i].code_on;
     } else {
        hw_id[i] = pins[i].code_off;
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
  delay(500);
  blinkLED(&led_pwr_pin, 1, 500);
  Serial.print(F("FERRARIS-SENSOR:V"));
  Serial.print(sw_version);
  Serial.print(":");
  Serial.println(hw_id);
  delay(100);
  Serial.println(F("RDY"));
}

float snap;

float snapCurve(float val) {
  snap = (1 - 1 / (val + 1)) * 2;
  if (snap > 1) {
    return 1;
  }
  return snap;
}

int reading_ir;
int reading_no_ambient;
float rema = 0.0;

int noAmbientSmoothedRead(const int *signal_pin, const int *ir_pwr_pin, int pause=1800, float multi=0.02) {
  digitalWrite(*ir_pwr_pin, HIGH);
  delayMicroseconds(pause);
  reading_ir = analogRead(*signal_pin);
  digitalWrite(*ir_pwr_pin, LOW);
  delayMicroseconds(pause);
  reading_no_ambient = reading_ir - analogRead(*signal_pin);
  rema = rema + (reading_no_ambient - rema) * snapCurve(abs(reading_no_ambient - rema) * multi);
  /* debug
  Serial.print(reading_no_ambient); // reading
  Serial.print(",");
  Serial.print(static_cast<int>(floor(rema))); // rema reading
  Serial.print(",");
  Serial.println(reading_ir - reading_no_ambient); // ambient
  */
  return static_cast<int>(floor(rema));
}

String line;
bool endline;
int i_count;
int interval;

String readLine(int timeout = 3000) {
  if (Serial.available() > 0) {
    line = "";
    endline = false;
    i_count = 10;
    interval = timeout / i_count;
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
  return "";
}


String command = "";

void getCommand() {
  command = readLine();
  if (command != "") {
    blinkLED(&led_pwr_pin);
  }
}


void manualRead(int pause=100) {
  digitalWrite(tr_pwr_pin, HIGH);
  while (command != "STP") {
    getCommand();
    Serial.println(noAmbientSmoothedRead(&signal_pin, &ir_pwr_pin));
    delay(pause);
  }
  digitalWrite(tr_pwr_pin, LOW);
}


char mode = 'I';
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
      mode = conf_line.substring(0, pos).charAt(0);
      int pos2 = conf_line.indexOf(":", pos+1);
      conf_a = conf_line.substring(pos+1, pos2).toInt();
      int pos3 = conf_line.indexOf(":", pos2+1);
      conf_b = conf_line.substring(pos2+1, pos3).toInt();
      int pos4 = conf_line.indexOf(":", pos3+1);
      detection_threshold = conf_line.substring(pos3+1, pos4).toInt();
      no_detection_threshold = conf_line.substring(pos4+1).toInt();
      Serial.println(String(mode) + ":" + String(conf_a) + ":" + String(conf_b) + ":" + String(detection_threshold) + ":" + String(no_detection_threshold));
      break;
    } else {
      if (i_count < 1) {
        Serial.println(String(mode) + ":" + String(conf_a) + ":" + String(conf_b) + ":" + String(detection_threshold) + ":" + String(no_detection_threshold));
        break;
      }
      delay(5000 / i_count);
      i_count--;
    }
  }
}


int reading;

void findBoundaries() {
  digitalWrite(tr_pwr_pin, HIGH);
  int left_edge = -100;
  int right_edge = 0;
  while (command != "STP") {
    getCommand();
    reading = noAmbientSmoothedRead(&signal_pin, &ir_pwr_pin);
    if (left_edge == -100) {
      left_edge = reading;
    }
    if (reading >= 0 && reading < left_edge) {
      left_edge = reading;
    }
    if (reading > right_edge) {
      right_edge = reading;
    }
  }
  digitalWrite(tr_pwr_pin, LOW);
  Serial.print(left_edge);
  Serial.print(F(":"));
  Serial.println(right_edge);
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
        getCommand();
        reading = noAmbientSmoothedRead(&signal_pin, &ir_pwr_pin);
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
  unsigned long current_ms;
  unsigned long last_loop = 0;
  while (command != "STP") {
    current_ms = millis();
    reading = noAmbientSmoothedRead(&signal_pin, &ir_pwr_pin);
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
    getCommand();
    if (current_ms - last_loop > 5000) {
      last_loop = current_ms;
      if (detected != true) {
        blinkLED(&led_pwr_pin, 1, 0);
      }
    }
    /* debug
    Serial.print("det_t:");
    Serial.print(detection_threshold_count);
    Serial.print(",");
    Serial.print("no_det_t:");
    Serial.println(no_detection_threshold_count);
    */
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
  unsigned long current_ms;
  unsigned long last_loop = 0;
  while (command != "STP") {
    current_ms = millis();
    reading = noAmbientSmoothedRead(&signal_pin, &ir_pwr_pin);
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

  if (command == "CONF") {
    configure();
    Serial.println(F("RDY"));
  }
  
  if (command == "MR") {
    manualRead();
    Serial.println(F("RDY"));
  }

  if (command == "FB") {
    findBoundaries();
    Serial.println(F("RDY"));
  }

  if (command == "HST") {
    buildHistogram();
    Serial.println(F("RDY"));
  }

  if (command == "STRT") {
    switch (mode) {
      case 'I':
        intervalDetection();
        break;
      case 'A':
        averageDetection();
        break;
    }
    Serial.println(F("RDY"));
  }

  command = "";
  delay(10);
}
