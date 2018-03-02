const int sen_pin = A0;
const int ir_pin = 11;
const int ext_led_pin = 10;
const int dip_pwr_pin = 9;

struct pdcm {
  int pin;
  char* code_on;
  char* code_off;
};

const pdcm dip_pins[] = {
  {7, "A", "E"},
  {6, "B", "F"},
  {5, "C", "G"},
  {4, "D", "H"}
};

int new_avg_threshold = 0;
int detection_threshold = 0;
int lower_limit_distance = 0;


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


void blinkLED(const int *led_pin, int times=1, int pause=200) {
  for (int bl = 0; bl < times; bl++) {
    digitalWrite(*led_pin, HIGH);
    delay(pause);
    digitalWrite(*led_pin, LOW);
    if (times > 1 and bl != times-1) {
      delay(pause);
    }
  }
}


int denoisedRead(const int *sen_pin, const int *ir_pin, int pause=2000) {
  int reading_ir, reading_no_ir;
  digitalWrite(*ir_pin, HIGH);
  delayMicroseconds(pause);
  reading_ir = 1023 - analogRead(*sen_pin);
  digitalWrite(*ir_pin, LOW);
  delayMicroseconds(pause);
  reading_no_ir = 1023 - analogRead(*sen_pin);
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
  pinMode(ir_pin, OUTPUT);
  pinMode(ext_led_pin, OUTPUT);
  pinMode(dip_pwr_pin, OUTPUT);
  setupDipPins(dip_pins, sizeof(dip_pins));

  // print start message
  delay(2000);
  hw_id = readDips(&dip_pwr_pin, dip_pins, sizeof(dip_pins));
  Serial.println(F("RDY"));
}


bool detected = false;
long reading_sum = 0;
long current_avg = 0;
long tmp_avg = 0;
long iteration = 1;
int new_avg_threshold_count = 1;
bool calibrated = false;
int detection_threshold_count = 1;
String command = "";

void getCommand() {
  String line = readLine();
  if (line != "") {
    command = line;
  }
}

void loop() {
  getCommand();

  if (command == "ID") {
    Serial.println(hw_id);
    command = "";
    Serial.println(F("RDY"));
  }
  
  if (command == "CONF") {
    Serial.println(F("NAT:DT:LLD"));
    int i_count = 10;
    int interval = 5000 / i_count;
    while (command == "CONF") {
      String conf_line = readLine();
      if (conf_line != "") {
        int pos = conf_line.indexOf(":");
        new_avg_threshold = conf_line.substring(0, pos).toInt();
        int pos2 = conf_line.indexOf(":", pos+1);
        detection_threshold = conf_line.substring(pos+1, pos2).toInt();
        lower_limit_distance = conf_line.substring(pos2+1).toInt();
        Serial.println(String(new_avg_threshold) + ":" + String(detection_threshold) + ":" + String(lower_limit_distance));
        command = "";
      } else {
        if (i_count < 1) {
          Serial.println(String(new_avg_threshold) + ":" + String(detection_threshold) + ":" + String(lower_limit_distance));
          command = "";
          break;
        }
        delay(interval);
        i_count--;
      }
    }
    Serial.println(F("RDY"));
  }
  
  if (command == "MR") {
    while (command != "STP") {
      getCommand();
      Serial.println(analogRead(A0));
      delay(100);
    }
    Serial.println(F("RDY"));
  }
  
  if (command == "STRT") {
    while (command != "STP") {
      int reading = denoisedRead(&sen_pin, &ir_pin, 1800);
      if (calibrated == true) {
        if (reading < (current_avg - lower_limit_distance)) {
          if (detection_threshold_count <= detection_threshold) {
            detection_threshold_count++;
          }
          if (detection_threshold_count == detection_threshold) {
            if (detected == false) {
              detected = true;
              Serial.println(F("DET"));
            }
          }
        } else {
          detection_threshold_count = 1;
          if (detected == true) {
            detected = false;
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
          blinkLED(&ext_led_pin);
        }
      }
      tmp_avg = average;
      iteration++;
      delay(1);
      getCommand();
    }
    Serial.println(F("RDY"));
  }
  delay(10);
}
