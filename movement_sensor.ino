// Network connection
#include <ESP8266WiFi.h>
// Web requests
#include <WiFiClientSecureBearSSL.h>
#include <ESP8266HTTPClient.h>
// NTP time requests
#include <NTPClient.h>
#include <WiFiUdp.h>
// Fancy array
#include <Array.h>

const int TRIG_PIN = D5;
const int ECHO_PIN = D6;

const float SOUND_VELOCITY = 0.034;
const float MAX_DISTANCE = 400;
const float THRESHOLD = 30;

const String NETWORK_NAME = "tlu";
const String NETWORK_PASS = "";

const String URL = "https://script.google.com/a/macros/tlu.ee/s/AKfycbxU-fv_BnuK5BM8wpC9u7F2OHBR3PTN5mR_ZzvuaWvlmHQ5xZHwvCP1ndPmflfFFfrG6Q/exec";
const String GET_REQUEST_PARAM = "?ready=1";
const long GET_REQUEST_DELAY = 2500;
const long POST_REQUEST_DELAY = 60000;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

bool stopperActive = false;

float dist;
float avgDist;
uint timesMeasured = 0;
String payload;
ulong postRequestMillis = 0;

Array<ulong, 60> timestamps;

void setup() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(NETWORK_NAME.c_str(), NETWORK_PASS.c_str());

  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting to WiFi..");
    delay(500);
  }
  timeClient.begin();

}

float activateSensor() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH);
  return (duration * SOUND_VELOCITY / 2);
}

String sendRequest(String url, String type, String payload = "") {
  Serial.printf("Sending %s request..\n", type);

  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  client -> setInsecure();
  HTTPClient https;
  https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  
  String response = "Request failed";
  if (https.begin(*client, url)) {
    int httpCode = https.sendRequest(type.c_str(), payload);
    
    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        response = https.getString();
        Serial.printf("Response: %s\n", response);
      }
    }
  }
  https.end();
  return response;
}

void loop() {
  digitalWrite(LED_BUILTIN, stopperActive ? LOW : HIGH);
  if (stopperActive) {

    dist = min(MAX_DISTANCE, activateSensor());
    timesMeasured++;
    
    float multiAvg = (avgDist * (timesMeasured - 1));
    avgDist = (multiAvg + dist) / timesMeasured;
    
    if (abs(dist - avgDist) > THRESHOLD) {
      timeClient.update();
      time_t epoch = timeClient.getEpochTime();
      Serial.printf("%f %f\n", dist, avgDist);
      int size = timestamps.size();
      if (size == 0 || timestamps[size - 1] != epoch) {
        timestamps.push_back(epoch);
      } 
    }

    if ((millis() > POST_REQUEST_DELAY) && (millis() - POST_REQUEST_DELAY > postRequestMillis)) {
      payload = "{\"times\": [ ";
      for (int i = 0; i < timestamps.size(); i++) {
        ulong timestamp = timestamps[i];
        payload.concat(String(timestamp));
        payload.concat(",");
      }
      payload[payload.length() - 1] = ']';
      payload.concat("}");

      Serial.println(payload);

      String response = sendRequest(URL, "POST", payload);
      postRequestMillis = millis();
      timestamps.clear();

      if (response == "0") {
        stopperActive = false;
        return;
      }

    }

    delay(50);

  } else {
    String response = sendRequest(URL + GET_REQUEST_PARAM, "GET");
    if (response == "1") {
      stopperActive = true;
      postRequestMillis = millis();
      return;
    }
    delay(GET_REQUEST_DELAY);
  }
}
