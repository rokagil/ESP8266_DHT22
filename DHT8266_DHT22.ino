#include <ESP8266WiFi.h>

//https://github.com/tzapu/WiFiManager
#include <WiFiManager.h>
// WiFi.beginSmartConfig() could be an alternative

//https://github.com/knolleary/pubsubclient
#include <PubSubClient.h>

//https://github.com/adafruit/DHT-sensor-library
#include <DHT.h>
#define DHTPIN 2
#define DHTTYPE DHT22   // DHT 11
DHT dht(DHTPIN, DHTTYPE);


uint8_t MAC_array[6];
char MAC_char[18];

void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
}

WiFiClient wifiClient;
PubSubClient client(wifiClient);

char msg[50];
char hum[6];
char temp[6];

void setup()
{
  Serial.begin(115200);

  WiFiManager wifiManager;
  //reset settings - for testing
  //wifiManager.resetSettings();

  wifiManager.setTimeout(180); //so if it restarts and router is not yet online, it keeps rebooting and retrying to connect
  if (!wifiManager.autoConnect("AutoConnectAP")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  IPAddress server(192, 168, 28, 107);
  //client.setServer(WiFi.gatewayIP(), 1883);
  client.setServer(server, 1883);
  client.setCallback(callback);

  Serial.println();

  Serial.println(WiFi.macAddress());
  Serial.println(WiFi.hostname());

  Serial.println("Status\tHumidity (%)\tTemperature (C)\t(F)");
  
}

void loop()
{
  delay(2000);

  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  Serial.print(humidity, 1);
  Serial.print("\t\t");
  Serial.println(temperature, 1);

  if (!client.connected()) {
    Serial.println(client.state());
    client.connect(WiFi.hostname().c_str());
  }
  client.loop();

  dtostrf(humidity, 3, 1, hum);
  dtostrf(temperature, 3, 1, temp);

  sprintf (msg, "%s,%s", hum, temp);
  client.publish("outTopic", msg);
}



