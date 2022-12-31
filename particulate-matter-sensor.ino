// SDS011
#include <SoftwareSerial.h>
#include <Sds011.h>

// WiFi
#include <WiFi.h>

// NTP
#include "time.h"

//MQTT
#include <PubSubClient.h> //MQTT

unsigned long timer;

// WiFi Config
const char *ssid = "******************";
const char *password = "******************";

// NTP Config
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

// MQTT Config
const char *mqttServer = "192.168.3.5";
const char *mqttClientName = "ParticleSensor";
const char *mqttTopicPm25= "/backyard/particles/pm25";
const char *mqttTopicPm10 = "/backyard/particles/pm10";

//SDS011 config
const byte D7 = 21;
const byte D8 = 22;


#define SDS_PIN_RX D7
#define SDS_PIN_TX D8
//int8_t SDS_PIN_RX D7 //21;
//int8_t SDS_PIN_TX D8 // 22;


HardwareSerial& serialSDS(Serial2);
Sds011Async< HardwareSerial > sds011(serialSDS);

// The example stops the sensor for 210s, then runs it for 30s, then repeats.
// At tablesizes 20 and below, the tables get filled during duty cycle
// and then measurement completes.
// At tablesizes above 20, the tables do not get completely filled
// during the 30s total runtime, and the rampup / 4 timeout trips,
// thus completing the measurement at whatever number of data points
// were recorded in the tables.
constexpr int pm_tablesize = 20;
int pm25_table[pm_tablesize];
int pm10_table[pm_tablesize];

bool is_SDS_running = true;

//Error LED
#define ERROR_LED 2 //2 = onboard LED

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Method definitions
void connectWiFi();

void connectMqtt();

void getNtpTime();

void sendSdsData(float, float);


void setup() {
  Serial.begin(115200);
  pinMode(ERROR_LED, OUTPUT);
  digitalWrite(ERROR_LED, LOW);

  timer = millis();
  
  connectWiFi(); // Connect WiFi
  //getNtpTime(); // Get Time
  connectMqtt(); //Start MQTT


  serialSDS.begin(9600, SERIAL_8N1, SDS_PIN_RX, SDS_PIN_TX);
  delay(100);


  start_SDS();
  Serial.print("SDS011 is running = ");
  Serial.println(is_SDS_running);

  String firmware_version;
  uint16_t device_id;
  if (!sds011.device_info(firmware_version, device_id)) {
      Serial.println("Sds011::firmware_version() failed");
  }
  else
  {
      Serial.print("Sds011 firmware version: ");
      Serial.println(firmware_version);
      Serial.print("Sds011 device id: ");
      Serial.println(device_id, 16);
  }

  Sds011::Report_mode report_mode;
  if (!sds011.get_data_reporting_mode(report_mode)) {
      Serial.println("Sds011::get_data_reporting_mode() failed");
  }
  if (Sds011::REPORT_ACTIVE != report_mode) {
      Serial.println("Turning on Sds011::REPORT_ACTIVE reporting mode");
      if (!sds011.set_data_reporting_mode(Sds011::REPORT_ACTIVE)) {
          Serial.println("Sds011::set_data_reporting_mode(Sds011::REPORT_ACTIVE) failed");
      }
  } 

}

void loop() {
  // Per manufacturer specification, place the sensor in standby to prolong service life.
  // At an user-determined interval (here 210s down plus 30s duty = 4m), run the sensor for 30s.
  // Quick response time is given as 10s by the manufacturer, thus the library drops the
  // measurements obtained during the first 10s of each run.

  constexpr uint32_t down_s = 210;

  stop_SDS();
  Serial.print("stopped SDS011 (is running = ");
  Serial.print(is_SDS_running);
  Serial.println(")");

  uint32_t deadline = millis() + down_s * 1000;
  while (static_cast<int32_t>(deadline - millis()) > 0) {
      delay(1000);
      Serial.println(static_cast<int32_t>(deadline - millis()) / 1000);
      sds011.perform_work();
  }

  constexpr uint32_t duty_s = 30;

  start_SDS();
  Serial.print("started SDS011 (is running = ");
  Serial.print(is_SDS_running);
  Serial.println(")");

  sds011.on_query_data_auto_completed([](int n) {
      Serial.println("Begin Handling SDS011 query data");
      int pm25;
      int pm10;
      Serial.print("n = "); Serial.println(n);
      if (sds011.filter_data(n, pm25_table, pm10_table, pm25, pm10) &&
          !isnan(pm10) && !isnan(pm25)) {
          Serial.print("PM10: ");
          Serial.println(float(pm10) / 10);
          Serial.print("PM2.5: ");
          Serial.println(float(pm25) / 10);
      }
      Serial.println("End Handling SDS011 query data");
      });

  if (!sds011.query_data_auto_async(pm_tablesize, pm25_table, pm10_table)) {
      Serial.println("measurement capture start failed");
  }

  deadline = millis() + duty_s * 1000;
  while (static_cast<int32_t>(deadline - millis()) > 0) {
      delay(1000);
      Serial.println(static_cast<int32_t>(deadline - millis()) / 1000);
      sds011.perform_work();
  }
}


//Connect to WiFi. Without WiFi no time
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(ERROR_LED, HIGH);
    Serial.print(".");
    delay(150);
    digitalWrite(ERROR_LED, LOW);
    delay(150);
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  digitalWrite(ERROR_LED, LOW);
}

void connectMqtt() {
  mqttClient.setServer(mqttServer, 1883);
  mqttClient.setKeepAlive(90);
  if (mqttClient.connect(mqttClientName)) {
    Serial.println("MQTT connected");
  } else {
    Serial.print("Error while connecting to MQTT");
    digitalWrite(ERROR_LED, HIGH);
    delay(3000);
    digitalWrite(ERROR_LED, LOW);
  }
  
}

void getNtpTime() {
  Serial.println("Get NTP Time");
  Serial.println(ntpServer);
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void start_SDS() {
    Serial.println("Start wakeup SDS011");

    if (sds011.set_sleep(false)) { is_SDS_running = true; }

    Serial.println("End wakeup SDS011");
}

void stop_SDS() {
    Serial.println("Start sleep SDS011");

    if (sds011.set_sleep(true)) { is_SDS_running = false; }

    Serial.println("End sleep SDS011");
}

void sendSdsData(float pm25, float pm10) {
    Serial.println("Start converting SDS011 Data");

    char pm25String[10];
    sprintf(pm25String, "%lf", pm25);

    char pm10String[10];
    sprintf(pm10String, "%lf", pm10);

    Serial.println("Start pushing Data");

    if (!mqttClient.connected()) {
        mqttClient.connect(mqttClientName);
    }

    mqttClient.publish(mqttTopicPm25, pm25String);
    mqttClient.publish(mqttTopicPm10, pm10String);

}
