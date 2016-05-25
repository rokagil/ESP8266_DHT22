#include <FS.h>

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

//https://github.com/bblanchon/ArduinoJson
#include <ArduinoJson.h> 


#define TRIGGER_PIN 0

//define default values here
char mqtt_server[40] = "localhost";
char mqtt_port[6] = "1883";

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback ()
{
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

uint8_t MAC_array[6];
char MAC_char[18];

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}


WiFiClient wifiClient;
PubSubClient client(wifiClient);

long lastMsg = 0;


void resetSettings()
{
  SPIFFS.format();
  WiFiManager wifiManager;
  wifiManager.resetSettings();

  delay(3000);
  Serial.println("Restarting...");
  ESP.restart();
}

void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected()) 
  {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(WiFi.hostname().c_str()))
    {
      Serial.println("connected");
      client.publish("outTopic", "hello world");
      client.subscribe("someTopic");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup()
{
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  attachInterrupt(TRIGGER_PIN, resetSettings, FALLING);
  
  Serial.begin(115200);
  while (!Serial) {
    // wait serial port initialization
  }
  Serial.println("Starting...");

  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  Serial.println("Mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json"))
    {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile)
      {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success())
        {
          Serial.println("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
        }
        else
        {
          Serial.println("failed to load json config");
        }
      }
    }
  }
  else
  {
    Serial.println("failed to mount FS");
  }

  // The extra parameters to be configured (can be either global or just in the setup)
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 5);
  
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

 //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);

  // reset settings - for testing
  // wifiManager.resetSettings();

  // sets timeout until configuration portal gets turned off
  // useful to make it all retry or go to sleep in seconds
  wifiManager.setTimeout(180);

  // if it does not connect it starts an access point with the specified name
  // and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("AutoConnectAP")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  Serial.println("Connected...");

  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  IPAddress server;
  server.fromString(mqtt_server);
  client.setServer(server, atol(mqtt_port));
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

  if (!client.connected())
  {
    reconnect();
  }
  client.loop();

  long now = millis();
  if(now - lastMsg > 60 * 1000)
  {
    lastMsg = now;

    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();

    Serial.print(humidity, 1);
    Serial.print("\t\t");
    Serial.println(temperature, 1);

    StaticJsonBuffer<50> jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["humidity"] = humidity;
    json["temperature"] = temperature;
    char buffer[50];
    json.printTo(buffer, sizeof(buffer));
    client.publish("outTopic", buffer);
  }
}
