#include <SoftwareSerial.h>

// ===================== PMS + MQ135 SETUP =====================
SoftwareSerial pmsSerial(8, 9);     // PMS TX -> D8
const int MQ135_PIN = A3;

// Sensor values
int pm1_0 = 0;
int pm2_5 = 0;
int pm10  = 0;
int gasLevel = 0;

// Trend history
const int HIST_SIZE = 10;
int pm25Hist[HIST_SIZE];
int histIndex = 0;
bool histFilled = false;

// Exposure timer (continuous unhealthy seconds)
unsigned long unhealthySeconds = 0;

// ALERT state + repeat timing
bool alertActive = false;
unsigned long lastAlertPrintMs = 0;
const unsigned long ALERT_REPEAT_MS = 3000;

// Gas warm-up label (warnings still show; label only)
const unsigned long GAS_WARMUP_MS = 60000; // 60 seconds
unsigned long programStartMs = 0;

// Sample timing
unsigned long lastSampleMs = 0;
const unsigned long SAMPLE_INTERVAL_MS = 1000;

// ===================== MODE TOGGLE =====================
// false = Message Mode (Serial Monitor)
// true  = Plot Mode (Serial Plotter)
bool plotMode = false;

// ---------- helpers ----------
int imax(int a, int b) { return (a > b) ? a : b; }

// Read PMS5003 frame (sync to header 0x42 0x4D)
bool readPMS() {
  while (pmsSerial.available() >= 2) {
    if (pmsSerial.peek() == 0x42) {
      uint8_t header[2];
      pmsSerial.readBytes(header, 2);

      if (header[1] == 0x4D) {
        uint8_t buffer[30];
        if (pmsSerial.readBytes(buffer, 30) == 30) {
          // buffer[0] corresponds to full-frame byte 2
          int pm1_index  = 10 - 2;
          int pm25_index = 12 - 2;
          int pm10_index = 14 - 2;

          pm1_0 = (buffer[pm1_index] << 8)  | buffer[pm1_index + 1];
          pm2_5 = (buffer[pm25_index] << 8) | buffer[pm25_index + 1];
          pm10  = (buffer[pm10_index] << 8) | buffer[pm10_index + 1];
          return true;
        }
      }
    } else {
      pmsSerial.read(); // discard until 0x42
    }
  }
  return false;
}

// AQI conversion tables (EPA)
int pm25_to_aqi(float pm25) {
  struct BP { float c1, c2; int i1, i2; };
  const BP t[] = {
    { 0.0, 12.0,   0,  50 },
    { 12.1, 35.4, 51, 100 },
    { 35.5, 55.4, 101,150 },
    { 55.5,150.4, 151,200 },
    {150.5,250.4, 201,300 },
    {250.5,350.4, 301,400 },
    {350.5,500.0, 401,500 }
  };
  for (int k = 0; k < 7; k++) {
    if (pm25 >= t[k].c1 && pm25 <= t[k].c2) {
      float aqi = (t[k].i2 - t[k].i1) * (pm25 - t[k].c1) / (t[k].c2 - t[k].c1) + t[k].i1;
      return (int)aqi;
    }
  }
  return -1;
}

int pm10_to_aqi(float pm10v) {
  struct BP { float c1, c2; int i1, i2; };
  const BP t[] = {
    { 0, 54,    0, 50 },
    { 55,154,  51,100 },
    {155,254, 101,150 },
    {255,354, 151,200 },
    {355,424, 201,300 },
    {425,504, 301,400 },
    {505,604, 401,500 }
  };
  for (int k = 0; k < 7; k++) {
    if (pm10v >= t[k].c1 && pm10v <= t[k].c2) {
      float aqi = (t[k].i2 - t[k].i1) * (pm10v - t[k].c1) / (t[k].c2 - t[k].c1) + t[k].i1;
      return (int)aqi;
    }
  }
  return -1;
}

const char* aqiCategory(int aqi) {
  if (aqi <= 50)  return "Good";
  if (aqi <= 100) return "Moderate";
  if (aqi <= 150) return "Unhealthy_Sens";
  if (aqi <= 200) return "Unhealthy";
  if (aqi <= 300) return "Very_Unhealthy";
  return "Hazardous";
}

const char* gasCategory(int gas) {
  if (gas < 100) return "Excellent";
  if (gas < 120) return "Good";
  if (gas < 160) return "Moderate";
  if (gas < 220) return "Unhealthy_Sens";
  if (gas < 280) return "Unhealthy";
  if (gas < 360) return "Very_Unhealthy";
  return "Hazardous";
}

// Gas danger rule (start warning from Unhealthy_Sens or worse)
bool gasIsDangerous(int gas) {
  return (gas >= 220);
}

const char* dustType(int pm25, int pm10v) {
  if (pm25 <= 0) return (pm10v > 100) ? "Desert_Dust" : "Unknown";
  if (pm10v >= (2 * pm25) && pm10v > 80) return "Desert_Dust";
  if (pm25 >= 35 && pm10v < (2 * pm25))  return "Indoor_Pollution";
  if (pm25 >= 35 && pm10v >= 80)         return "Mixed_Severe";
  return "Normal";
}

void pushHistory(int val) {
  pm25Hist[histIndex] = val;
  histIndex++;
  if (histIndex >= HIST_SIZE) { histIndex = 0; histFilled = true; }
}

const char* pm25Trend(int latest) {
  if (!histFilled && histIndex < 3) return "WarmingUp";
  long sum = 0;
  int count = histFilled ? HIST_SIZE : histIndex;
  for (int i = 0; i < count; i++) sum += pm25Hist[i];
  float avg = (float)sum / (float)count;
  float threshold = 5.0;
  if (latest > avg + threshold) return "Rising";
  if (latest < avg - threshold) return "Falling";
  return "Stable";
}

void printBigAlert(int AQI,
                   unsigned long unhealthySeconds,
                   bool dustDanger,
                   bool gasDanger,
                   const char* gasCat,
                   bool gasWarmupActive) {
  Serial.println();
  Serial.println("#####################################################");
  Serial.println("###               ⚠️  WARNING  ⚠️                   ###");
  Serial.print  ("### Reason: ");
  if (dustDanger && gasDanger) Serial.println("DUST + GAS");
  else if (dustDanger)         Serial.println("DUST (AQI sustained)");
  else if (gasDanger)          Serial.println("GAS (immediate)");
  else                         Serial.println("N/A");

  Serial.print("### AQI: "); Serial.print(AQI);
  Serial.print(" | DustExposure: "); Serial.print(unhealthySeconds / 60);
  Serial.println(" min");

  Serial.print("### GasCat: "); Serial.print(gasCat);
  if (gasWarmupActive) Serial.print("  (WARMUP)");
  Serial.println();

  Serial.println("### ACTION: Ventilate / reduce exposure              ###");
  Serial.println("#####################################################");
  Serial.println();
}

// ===================== MODE HANDLER =====================
void handleModeToggle() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == 'p' || c == 'P') {
      // Switch to plot mode
      plotMode = true;
      // Print a short confirmation (will appear once; after that, numbers only)
      Serial.println("Switched to PLOT MODE. Open Serial Plotter now.");
      Serial.println("Plot columns: PM2.5 PM10 AQI GAS");
    }
    if (c == 'm' || c == 'M') {
      // Switch to message mode
      plotMode = false;
      Serial.println("Switched to MESSAGE MODE. Text + alerts enabled.");
    }
  }
}

void setup() {
  Serial.begin(9600);
  pmsSerial.begin(9600);
  programStartMs = millis();

  Serial.println("Air Quality Monitor (NO OLED)");
  Serial.println("Type 'p' then Enter -> Plot Mode (Serial Plotter)");
  Serial.println("Type 'm' then Enter -> Message Mode (Serial Monitor)");
  Serial.println();
}

void loop() {
  handleModeToggle(); // check for 'p' or 'm' anytime

  unsigned long now = millis();
  if (now - lastSampleMs < SAMPLE_INTERVAL_MS) return;
  lastSampleMs = now;

  gasLevel = analogRead(MQ135_PIN);
  bool pmsOK = readPMS();

  // If PMS missing:
  if (!pmsOK) {
    if (plotMode) {
      // Plotter needs numeric output only
      Serial.print(-1); Serial.print(" ");
      Serial.print(-1); Serial.print(" ");
      Serial.print(-1); Serial.print(" ");
      Serial.println(gasLevel);
    } else {
      Serial.print("PMS:NoData  Gas:"); Serial.print(gasLevel);
      Serial.print("  GasCat:"); Serial.println(gasCategory(gasLevel));
    }
    return;
  }

  int aqi25 = pm25_to_aqi((float)pm2_5);
  int aqi10 = pm10_to_aqi((float)pm10);
  int AQI = imax(aqi25, aqi10);

  pushHistory(pm2_5);

  const char* trend  = pm25Trend(pm2_5);
  const char* dust   = dustType(pm2_5, pm10);
  const char* gasCat = gasCategory(gasLevel);
  const char* aqiCat = aqiCategory(AQI);

  // Dust danger: AQI >= 151 for 10 minutes
  bool dustDanger = false;
  if (AQI >= 151) {
    unhealthySeconds++;
    if (unhealthySeconds >= 600) dustDanger = true;
  } else {
    unhealthySeconds = 0;
    dustDanger = false;
  }

  bool gasWarmupActive = (now - programStartMs) < GAS_WARMUP_MS;
  bool gasDanger = gasIsDangerous(gasLevel);

  alertActive = dustDanger || gasDanger;

  // ===================== PLOT MODE =====================
  if (plotMode) {
    // NUMBERS ONLY (4 columns): PM2.5 PM10 AQI Gas
    Serial.print(pm2_5);  Serial.print(" ");
    Serial.print(pm10);   Serial.print(" ");
    Serial.print(AQI);    Serial.print(" ");
    Serial.println(gasLevel);
    return;
  }

  // ===================== MESSAGE MODE =====================
  if (alertActive) Serial.println("!!! SYSTEM STATE: ALERT !!!");
  else             Serial.println("System State: Normal");

  Serial.println("----- AIR REPORT -----");
  Serial.print("PM1.0: "); Serial.print(pm1_0);
  Serial.print("  PM2.5: "); Serial.print(pm2_5);
  Serial.print("  PM10: "); Serial.println(pm10);

  Serial.print("AQI: "); Serial.print(AQI);
  Serial.print(" ("); Serial.print(aqiCat); Serial.println(")");

  Serial.print("DustType: "); Serial.print(dust);
  Serial.print("  Trend: "); Serial.println(trend);

  Serial.print("GasRaw: "); Serial.print(gasLevel);
  Serial.print("  GasCat: "); Serial.print(gasCat);
  if (gasWarmupActive) Serial.print(" [WARMUP]");
  Serial.println();

  if (gasDanger) {
    Serial.println(">>> GAS WARNING: Unhealthy gas level detected!");
  }
  if (dustDanger) {
    Serial.println(">>> DUST WARNING: AQI unhealthy for 10+ minutes!");
  }

  if (alertActive && (now - lastAlertPrintMs >= ALERT_REPEAT_MS)) {
    lastAlertPrintMs = now;
    printBigAlert(AQI, unhealthySeconds, dustDanger, gasDanger, gasCat, gasWarmupActive);
  }

  Serial.println();
}

