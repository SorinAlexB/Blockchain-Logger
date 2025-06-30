#include <WiFiS3.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <DHT.h>
#include <SHA256.h>

char ssid[] = "Sorin Iphone";
char pass[] = "12345678";
WiFiServer server(80);
int status = WL_IDLE_STATUS;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 10800, 60000);

#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

const int MAX_BLOCKS = 10;
struct Block {
  int index;
  String timestamp;
  float temperature;
  float humidity;
  String previousHash;
  String hash;
};

Block blockchain[MAX_BLOCKS];
int blockCount = 0;
unsigned long lastBlockTime = 0;

String computeHash(String input) {
  SHA256 sha256;
  sha256.reset();
  sha256.update((const uint8_t*)input.c_str(), input.length());
  uint8_t result[32];
  sha256.finalize(result, sizeof(result));
  String hashStr = "";
  for (int i = 0; i < 32; i++) {
    if (result[i] < 16) hashStr += "0";
    hashStr += String(result[i], HEX);
  }
  return hashStr;
}

void createGenesisBlock() {
  Block genesis;
  genesis.index = 0;
  genesis.timestamp = timeClient.getFormattedTime();
  genesis.temperature = 0.0;
  genesis.humidity = 0.0;
  genesis.previousHash = "0";
  genesis.hash = computeHash("0" + genesis.timestamp + "0.0" + "0.0" + "0");
  blockchain[0] = genesis;
  blockCount = 1;
}

void createNewBlock() {
  if (blockCount >= MAX_BLOCKS) return;
  
  delay(2000);
  
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }
  
  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.print(" %\t");
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" °C");
  
  Block prev = blockchain[blockCount - 1];
  Block newBlock;
  newBlock.index = blockCount;
  newBlock.timestamp = timeClient.getFormattedTime();
  newBlock.temperature = temperature;
  newBlock.humidity = humidity;
  newBlock.previousHash = prev.hash;
  
  String hashInput = String(newBlock.index) + newBlock.timestamp + String(temperature) + String(humidity) + newBlock.previousHash;
  newBlock.hash = computeHash(hashInput);
  
  blockchain[blockCount] = newBlock;
  blockCount++;
  
  Serial.println("New block created");
}

void printBlockchain(WiFiClient client) {
  for (int i = 0; i < blockCount; i++) {
    client.println("Block #" + String(blockchain[i].index));
    client.println("Time: " + blockchain[i].timestamp);
    client.println("Temperature: " + String(blockchain[i].temperature) + " °C");
    client.println("Humidity: " + String(blockchain[i].humidity) + " %");
    client.println("Prev Hash: " + blockchain[i].previousHash);
    client.println("Hash: " + blockchain[i].hash);
    client.println("------------------------------");
  }
}

void setup() {
  Serial.begin(9600);
  
  dht.begin();
  Serial.println("DHT initialized");
  
  while (status != WL_CONNECTED) {
    Serial.print("Connected at ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
    delay(5000);
  }
  Serial.println("Connected");
  Serial.println(WiFi.localIP());
  
  timeClient.begin();
  while (!timeClient.update()) timeClient.forceUpdate();
  
  server.begin();
  createGenesisBlock();
  lastBlockTime = millis();
  
  Serial.println("System started");
}

void loop() {
  timeClient.update();
  
  if (millis() - lastBlockTime >= 60000) {
    Serial.println("Creating new block...");
    createNewBlock();
    lastBlockTime = millis();
  }
  
  WiFiClient client = server.available();
  if (client) {
    while (client.connected()) {
      if (client.available()) {
        String req = client.readStringUntil('\r');
        client.read();
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/plain");
        client.println("Connection: close");
        client.println();
        
        printBlockchain(client);
        break;
      }
    }
    delay(1);
    client.stop();
  }
}