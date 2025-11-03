#include "WebServer.h"
#include "WiFi.h"
#include "DHT.h"
#include "SPI.h"
#include "time.h"

#define HTTP_PORT 80
#define WIFI_RETRY_DELAY 1000

#define PHOTORESISTOR_INPUT_PIN 34
#define DHT_INPUT_PIN 21
#define DHT_TYPE DHT11
#define SHIFT_REGISTER_DATA_PIN 18
#define SHIFT_REGISTER_LATCH_PIN 19
#define SHIFT_REGISTER_OUTPUT_CLOCK_PIN 23

#define FIRST_DIGIT_OUTPUT_PIN 13
#define SECOND_DIGIT_OUTPUT_PIN 14
#define THIRD_DIGIT_OUTPUT_PIN 32
#define FOURTH_DIGIT_OUTPUT_PIN 33

#define RELAY_CONTROL_PIN 25

#define FAN_TEMPERATURE_THRESHOLD 25

#define TIMER_FREQ 1000000
#define TIMER_ALARM 2000000

// Segment bit masks for the display
#define SEG_A 0b01111111
#define SEG_B 0b10111111
#define SEG_C 0b11111101
#define SEG_D 0b11110111
#define SEG_E 0b11101111
#define SEG_F 0b11011111
#define SEG_G 0b11111110
#define SEG_DOT 0b11111011

// Numbers for the segment display
#define DNUMBER_0 (SEG_A & SEG_B & SEG_C & SEG_D & SEG_E & SEG_F)
#define DNUMBER_1 (SEG_B & SEG_C)
#define DNUMBER_2 (SEG_A & SEG_B & SEG_G & SEG_E & SEG_D)
#define DNUMBER_3 (SEG_A & SEG_B & SEG_C & SEG_D & SEG_G)
#define DNUMBER_4 (SEG_B & SEG_C & SEG_F & SEG_G)
#define DNUMBER_5 (SEG_A & SEG_F & SEG_G & SEG_C & SEG_D)
#define DNUMBER_6 (SEG_A & SEG_F & SEG_E & SEG_D & SEG_C & SEG_G)
#define DNUMBER_7 (SEG_A & SEG_B & SEG_C)
#define DNUMBER_8 (SEG_A & SEG_B & SEG_C & SEG_D & SEG_E & SEG_F & SEG_G)
#define DNUMBER_9 (SEG_A & SEG_B & SEG_C & SEG_D & SEG_F & SEG_G)

uint8_t numbers[10] = {
  DNUMBER_0, DNUMBER_1, DNUMBER_2, DNUMBER_3, DNUMBER_4,
  DNUMBER_5, DNUMBER_6, DNUMBER_7, DNUMBER_8, DNUMBER_9
};

// Wifi creds
const char* ssid = "ssid";
const char* password = "password";

// Global vaiables
WebServer server(HTTP_PORT);
DHT dht(DHT_INPUT_PIN, DHT_TYPE);

float temperature = 0;
float humidity = 0;
uint32_t photores_val = 0;
uint8_t display_digits[4] = {0, 0, 0, 0};
bool relay_on_temp = false;
bool relay_on_client = false;
volatile bool measure_dht = true;
hw_timer_t *my_timer = NULL;

// Function declarations
void update_time_display();
void display_task(void *parameter);
void display_number(uint8_t number, uint8_t segment);
String createHTML();
void IRAM_ATTR timer_to_measure_dht();

void setup() {
  Serial.begin(9600);

  // add a timer interupt every 2 seconds to mearuse dht11
  // use 0th timer, 
  my_timer = timerBegin(TIMER_FREQ);
  timerAttachInterrupt(my_timer, &timer_to_measure_dht);
  timerAlarm(my_timer, TIMER_ALARM, true, 0);
  timerStart(my_timer);

  pinMode(SHIFT_REGISTER_DATA_PIN, OUTPUT);
  pinMode(SHIFT_REGISTER_OUTPUT_CLOCK_PIN, OUTPUT);
  pinMode(SHIFT_REGISTER_LATCH_PIN, OUTPUT);
  pinMode(FIRST_DIGIT_OUTPUT_PIN, OUTPUT);
  pinMode(SECOND_DIGIT_OUTPUT_PIN, OUTPUT);
  pinMode(THIRD_DIGIT_OUTPUT_PIN, OUTPUT);
  pinMode(FOURTH_DIGIT_OUTPUT_PIN, OUTPUT);
  pinMode(PHOTORESISTOR_INPUT_PIN, INPUT);
  pinMode(RELAY_CONTROL_PIN, OUTPUT);

  dht.begin();

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(WIFI_RETRY_DELAY);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.println(WiFi.localIP());

  // Sync time from NTP
  configTime(0, 0, "pool.ntp.org");
  // Lithuania timezone (EET/EEST)
  setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/4", 1);
  tzset();

  Serial.println("Fetching time...");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nTime synchronized!");

  // Create the display thread
  xTaskCreatePinnedToCore(
    display_task,       // function
    "DisplayTask",      // name
    2048,               // stack size
    NULL,               // parameter
    1,                  // priority
    NULL,               // handle
    0                   // core (0 or 1)
  );

  // Setup web server
  server.on("/", []() {
    server.send(200, "text/html", createHTML());
  });

  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });

  server.on("/relay/on", []() {
    relay_on_client = true;
    server.send(200, "text/html", createHTML());
  });

  server.on("/relay/off", []() {
    relay_on_client = false;
    server.send(200, "text/html", createHTML());
  });

  server.begin();
  Serial.println("HTTP server started");
}

String createHTML() {
  String str = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><meta http-equiv=\"refresh\" content=\"2\">";
  str += "<style>body{font-family:Arial;text-align:center;color:#444;}";
  str += ".title{font-size:30px;margin-top:40px;}";
  str += ".data{font-size:20px;margin:10px;}</style></head><body>";
  str += "<h1 class=\"title\">ROOM STATUS</h1>";
  str += "<div class=\"data\">Temperature: " + String(temperature, 1) + "C</div>";
  str += "<div class=\"data\">Humidity: " + String(humidity, 1) + "%</div>";
  str += "<div class=\"data\">Light: " + String(photores_val) + "</div>";

  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char buf[16];
    strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
    str += "<div class=\"data\">Time: " + String(buf) + "</div>";
  }

  if (relay_on_temp || relay_on_client) {
    str += "<div class=\"data\">Relay: ON</div>";
    str += "<a href=\"/relay/off\"><button style=\"padding:10px 20px;font-size:16px;\">Turn OFF</button></a>";
  } else {
    str += "<div class=\"data\">Relay: OFF</div>";
    str += "<a href=\"/relay/on\"><button style=\"padding:10px 20px;font-size:16px;\">Turn ON</button></a>";
  }

  str += "</body></html>";
  return str;
}

void update_time_display() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to get time");
    return;
  }

  int hour = timeinfo.tm_hour;
  int minute = timeinfo.tm_min;

  display_digits[0] = hour / 10;
  display_digits[1] = hour % 10;
  display_digits[2] = minute / 10;
  display_digits[3] = minute % 10;
}

void display_task(void *parameter) {
  while (true) {
    display_number(numbers[display_digits[0]], FIRST_DIGIT_OUTPUT_PIN);
    display_number(numbers[display_digits[1]] & SEG_DOT, SECOND_DIGIT_OUTPUT_PIN);
    display_number(numbers[display_digits[2]], THIRD_DIGIT_OUTPUT_PIN);
    display_number(numbers[display_digits[3]], FOURTH_DIGIT_OUTPUT_PIN);
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

void display_number(uint8_t number, uint8_t segment) {
  digitalWrite(SHIFT_REGISTER_LATCH_PIN, LOW);
  shiftOut(SHIFT_REGISTER_DATA_PIN, SHIFT_REGISTER_OUTPUT_CLOCK_PIN, LSBFIRST, number);
  digitalWrite(SHIFT_REGISTER_LATCH_PIN, HIGH);

  digitalWrite(segment, HIGH);
  delay(2);
  digitalWrite(segment, LOW);
}

// --- Main loop ---
void loop() {
  server.handleClient();

  // Read sensors
  photores_val = analogRead(PHOTORESISTOR_INPUT_PIN);
  if(measure_dht) {
    humidity = dht.readHumidity();
    temperature = dht.readTemperature();
    measure_dht = false;
  }

  if(temperature >= FAN_TEMPERATURE_THRESHOLD) {
    relay_on_temp = true;
  }
  else {
    relay_on_temp = false;
  }

  if(relay_on_temp || relay_on_client) {
    digitalWrite(RELAY_CONTROL_PIN, HIGH);
  }
  else {
    digitalWrite(RELAY_CONTROL_PIN, LOW);
  }

  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 1000) {
    lastUpdate = millis();
    update_time_display();
  }
}

void IRAM_ATTR timer_to_measure_dht() {
  measure_dht = true;
}
