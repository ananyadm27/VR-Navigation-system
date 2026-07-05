#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_VL53L0X.h>

const char* ssid = "Ananya";
const char* password = "ananya2005";

WebServer server(80);

const int MPU = 0x68;
const uint8_t TOF_XSHUT = 25;
const uint8_t TOF_ADDR = 0x30;

const int OBSTACLE_STOP_MM = 250;

Adafruit_VL53L0X tof;
bool tofReady = false;

float accX = 0;
float accY = 0;
float accZ = 0;

int distanceValue = -1;
String recommendation = "forward";

void readAveragedMpu(float &x, float &y, float &z) {
  const int samples = 16;
  long sumX = 0;
  long sumY = 0;
  long sumZ = 0;

  for (int i = 0; i < samples; i++) {
    Wire.beginTransmission(MPU);
    Wire.write(0x3B);
    Wire.endTransmission(false);

    Wire.requestFrom(MPU, 6, true);
    if (Wire.available() < 6) {
      continue;
    }

    int16_t rawX = (Wire.read() << 8) | Wire.read();
    int16_t rawY = (Wire.read() << 8) | Wire.read();
    int16_t rawZ = (Wire.read() << 8) | Wire.read();

    sumX += rawX;
    sumY += rawY;
    sumZ += rawZ;

    delayMicroseconds(800);
  }

  x = (float)sumX / samples;
  y = (float)sumY / samples;
  z = (float)sumZ / samples;
}

int readDistanceMm() {
  if (!tofReady) {
    return -1;
  }

  VL53L0X_RangingMeasurementData_t measure;
  tof.rangingTest(&measure, false);

  if (measure.RangeStatus == 4) {
    return -1;
  }

  return (int)measure.RangeMilliMeter;
}

String getRecommendation(int distanceMm) {
  if (distanceMm > 0 && distanceMm < OBSTACLE_STOP_MM) {
    return "stop";
  }

  return "forward";
}

void handleRoot() {
  server.send(200, "text/plain", "ESP32 Sensor Server Running");
}

void handleData() {
  readAveragedMpu(accX, accY, accZ);
  distanceValue = readDistanceMm();
  recommendation = getRecommendation(distanceValue);

  String json = "{";
  json += "\"x\":" + String(accX, 2) + ",";
  json += "\"y\":" + String(accY, 2) + ",";
  json += "\"z\":" + String(accZ, 2) + ",";
  json += "\"distance\":" + String(distanceValue) + ",";
  json += "\"frontDistance\":" + String(distanceValue) + ",";
  json += "\"leftDistance\":-1,";
  json += "\"rightDistance\":-1,";
  json += "\"backDistance\":-1,";
  json += "\"recommendation\":\"" + recommendation + "\"";
  json += "}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);

  Wire.begin(21, 22);
  Wire.setClock(400000);

  pinMode(TOF_XSHUT, OUTPUT);
  digitalWrite(TOF_XSHUT, LOW);
  delay(20);

  Wire.beginTransmission(MPU);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);

  Wire.beginTransmission(MPU);
  Wire.write(0x1C);
  Wire.write(0x00);
  Wire.endTransmission(true);

  Wire.beginTransmission(MPU);
  Wire.write(0x1B);
  Wire.write(0x00);
  Wire.endTransmission(true);

  Serial.println("MPU6050 Ready");

  digitalWrite(TOF_XSHUT, HIGH);
  delay(10);

  if (tof.begin()) {
    tof.setAddress(TOF_ADDR);
    tofReady = true;
    Serial.println("VL53L0X Ready");
  } else {
    tofReady = false;
    Serial.println("VL53L0X NOT FOUND");
  }

  WiFi.begin(ssid, password);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
  Serial.println("Server Started");
}

void loop() {
  server.handleClient();
}
