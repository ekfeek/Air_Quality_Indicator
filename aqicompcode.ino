#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define MQ135_PIN A3   // <-- Add your MQ-135 analog pin here

int gasLevel = 0;
String quality = "";

void setup() {
  Serial.begin(9600);
  dht.begin();

  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found!");
    for (;;);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
}

void loop() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read DHT!");
    return;
  }

  // Read MQ135
  gasLevel = analogRead(MQ135_PIN);

  // Air quality condition
  if (gasLevel < 150) {
    quality = "GOOD!";
  } 
  else if (gasLevel < 200) {
    quality = "Poor!";
  } 
  else if (gasLevel < 300) {
    quality = "Very Bad!";
  } 
  else if (gasLevel < 500) {
    quality = "Toxic!";
  } 
  else {
    quality = "Hazardous!"; 
  }

  // --- Print to Serial ---
  Serial.print("Temp: ");
  Serial.print(t);
  Serial.print(" C | Hum: ");
  Serial.print(h);
  Serial.print(" % | Gas: ");
  Serial.print(gasLevel);
  Serial.print(" | ");
  Serial.println(quality);

  // --- Print to OLED ---
  display.clearDisplay();

  display.setCursor(0, 0);
  display.setTextSize(2);
  display.print("T:");
  display.print(t);
  display.print("C");

  display.setCursor(0, 22);
  display.print("H:");
  display.print(h);
  display.print("%");

  display.setCursor(0, 44);
  display.setTextSize(1);
  display.print("Gas: ");
  display.print(gasLevel);
  display.print("  ");
  display.print(quality);

  display.display();

  delay(1000);
}

