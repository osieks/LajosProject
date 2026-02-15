#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ThreeWire.h>
//#include <RtcDS1302.h>

// =======================
// WiFi i OTA
// =======================
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>

// do wysyłania danych do serwera
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

const char* serverName = "http://ubuntudziezok.ddns.net:5000/api/bpm"; // pełny adres
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 60000; // 60 sekund
const char* ssid = "Osiek";
const char* password = "OsiekRulz123";

// =======================
// OLED CONFIGURATION
// =======================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define I2C_SDA 14
#define I2C_SCL 12

// =======================
// RTC CONFIGURATION (DS1302)
// =======================
//ThreeWire myWire(4, 5, 0); // IO, SCLK, CE
//RtcDS1302<ThreeWire> Rtc(myWire);

// =======================
// PROJECT INFO
// =======================
String projectVersion = "LajosProject_0.5";

// =======================
// ODDECHY – detekcja oddechu z automatyczną regulacją progów
// =======================
#define SAMPLE_INTERVAL_MS 10      // próbkowanie co 10 ms (100 Hz)
#define BREATH_BUFFER_SIZE 50       // liczba ostatnich oddechów do obliczania średniej

// Progi – będą aktualizowane automatycznie
int HYSTERESIS_HIGH = 500;   // wartości początkowe
int HYSTERESIS_LOW = 450;

// Zmienne do detekcji oddechu
enum BreathState { WAITING_FOR_PEAK, WAITING_FOR_VALLEY };
BreathState breathState = WAITING_FOR_PEAK;

unsigned long lastSampleTime = 0;
int sensorValue = 0;                // ostatni odczyt z czujnika
int oddechy = 0;                    // wyświetlane oddechy na minutę

// Bufor czasów oddechów (w milisekundach)
unsigned long breathTimes[BREATH_BUFFER_SIZE];
int breathTimeIndex = 0;
int breathTimeCount = 0;

// Zmienne do automatycznej regulacji progów
int currentMax = 0;
int currentMin = 1023; // ADC 10-bit (0-1023)
int lastMinuteMax = 500;
int lastMinuteMin = 400;
unsigned long lastMinuteReset = 0;
#define MINUTE_MS 60000

// =======================
// FUNKCJE POMOCNICZE
// =======================
#define countof(a) (sizeof(a) / sizeof(a[0]))

/*
String printDateTime(const RtcDateTime& dt) {
    char datestring[20];
    snprintf_P(datestring, countof(datestring),
               PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
               dt.Month(), dt.Day(), dt.Year(),
               dt.Hour(), dt.Minute(), dt.Second());
    return datestring;
}
*/
// =======================
// Funkcja wysyłająca dane
// =======================
int httpResult = 0;
void sendBPMToServer(int bpm) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected");
        return;
    }

    WiFiClient client;
    HTTPClient http;

    http.begin(client, serverName);
    http.addHeader("Content-Type", "application/json");

    // Tworzymy JSON
    String jsonPayload = "{\"bpm\":" + String(bpm) + "}";

    int httpResponseCode = http.POST(jsonPayload);
   
    if (httpResponseCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String response = http.getString();
        Serial.println(response);
    } else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
    }
    httpResult = httpResponseCode;
    http.end();
}
// =======================
// OBSŁUGA ODDECHÓW
// =======================
void addBreathTime() {
    unsigned long now = millis();
    breathTimes[breathTimeIndex] = now;
    breathTimeIndex = (breathTimeIndex + 1) % BREATH_BUFFER_SIZE;
    if (breathTimeCount < BREATH_BUFFER_SIZE) breathTimeCount++;
    
    Serial.print("breathTimeCount: ");
    Serial.println(breathTimeCount);
}

float getBreathsPerMinute() {
    if (breathTimeCount < 2) return 0;
    
    unsigned long sum = 0;
    int count = 0;
    // Obliczamy średni odstęp między kolejnymi oddechami w buforze
    for (int i = 0; i < breathTimeCount - 1; i++) {
        int idx1 = (breathTimeIndex - 1 - i + BREATH_BUFFER_SIZE) % BREATH_BUFFER_SIZE;
        int idx2 = (breathTimeIndex - 2 - i + BREATH_BUFFER_SIZE) % BREATH_BUFFER_SIZE;
        sum += (breathTimes[idx1] - breathTimes[idx2]);
        count++;
    }
    
    if (count == 0) return 0;
    unsigned long avgInterval = sum / count;
    if (avgInterval == 0) return 0;
    return 60000.0 / avgInterval;
}

void checkBreath(int value) {
    switch (breathState) {
        case WAITING_FOR_PEAK:
            if (value >= HYSTERESIS_HIGH) {
                Serial.println("PEAK");
                breathState = WAITING_FOR_VALLEY;
            }
            break;
        case WAITING_FOR_VALLEY:
            if (value <= HYSTERESIS_LOW) {
                Serial.println("VALLEY");
                // Wykryto pełny cykl oddechu
                Serial.println("Wykryto pełny cykl oddechu");
                addBreathTime();
                breathState = WAITING_FOR_PEAK;
            }
            break;
    }
}

// =======================
// WYŚWIETLANIE NA OLED
// =======================
void updateDisplay() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);

    display.println(projectVersion);
    display.print("sensor: ");
    display.println(sensorValue);
    display.print("oddechy: ");
    display.print(oddechy);
    display.println(" / min");
    
    unsigned long currentMillis = millis();
    unsigned long totalSeconds = currentMillis / 1000;
    unsigned long seconds = totalSeconds % 60;
    unsigned long minutes = (totalSeconds / 60) % 60;
    unsigned long hours = (totalSeconds / 3600) % 24;
    unsigned long days = totalSeconds / 86400;
    
    if (days > 0) {
      display.print(days);
      display.print("d ");
    }
    if (hours > 0) {
      display.print(hours);
      display.print("h ");
    }
    if (minutes > 0) {
      display.print(minutes);
      display.print("m ");
    }
      display.print(seconds);
      display.print("s ");
    display.println();
    
    if (httpResult > 0) {
        display.print("HTTP OK: ");
        display.println(httpResult);
    } else {
        display.print("HTTP ERROR: ");
        display.println(httpResult);
    }

    // Pobierz aktualny czas z RTC
    //RtcDateTime now = Rtc.GetDateTime();
    //display.print(printDateTime(now));
    display.print("HIGH:");
    display.print(HYSTERESIS_HIGH);
    display.print("| LOW:");
    display.println(HYSTERESIS_LOW);
    
    display.print("WiFi:");
    display.println(ssid);
    
    // Wyświetl status WiFi (opcjonalnie)
    display.setCursor(0, 56);
    if (WiFi.status() == WL_CONNECTED) {
        display.print("IP: ");
        display.print(WiFi.localIP()); 
    } else {
        display.print("WiFi: ...");
    }

    display.display();
}

// =======================
// SETUP
// =======================
void setup() {
    Serial.begin(115200);
    Serial.println();
    Serial.println("Start systemu...");

    // ---- Połączenie z WiFi i OTA ----
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.print("Łączenie z WiFi");
    while (WiFi.waitForConnectResult() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }
    Serial.println();
    Serial.print("Połączono, adres IP: ");
    Serial.println(WiFi.localIP());

    // Konfiguracja OTA]
    ArduinoOTA.setHostname("LajosProject-esp");
    ArduinoOTA.setPassword("home"); // opcjonalne hasło
    ArduinoOTA.setPort(8266); // add this before ArduinoOTA.begin()

    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "sketch";
        } else { // U_SPIFFS
            type = "filesystem";
        }
        Serial.println("Start aktualizacji OTA: " + type);
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("\nKoniec OTA");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Postęp: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Błąd OTA [%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

    ArduinoOTA.begin();
    Serial.println("OTA gotowe");

    /*
    // ---- RTC INIT ----
    Serial.print("compiled: ");
    Serial.print(__DATE__);
    Serial.println(__TIME__);

    Rtc.Begin();
    RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
    if (!Rtc.IsDateTimeValid()) {
        Serial.println("RTC lost confidence in the DateTime!");
        Rtc.SetDateTime(compiled);
    }
    if (Rtc.GetIsWriteProtected()) {
        Serial.println("RTC was write protected, enabling writing now");
        Rtc.SetIsWriteProtected(false);
    }
    if (!Rtc.GetIsRunning()) {
        Serial.println("RTC was not actively running, starting now");
        Rtc.SetIsRunning(true);
    }
    RtcDateTime now = Rtc.GetDateTime();
    if (now < compiled) {
        Serial.println("RTC is older than compile time! (Updating DateTime)");
        Rtc.SetDateTime(compiled);
    } else if (now > compiled) {
        Serial.println("RTC is newer than compile time. (this is expected)");
    } else {
        Serial.println("RTC is the same as compile time! (not expected but all is fine)");
    }
    */
    // ---- OLED INIT ----
    Wire.begin(I2C_SDA, I2C_SCL);
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 initialization failed"));
        while (true);
    }
    display.setRotation(2); // obrót o 180°
    display.clearDisplay();
    display.display();

    // Wyświetl informacje startowe
    updateDisplay();

    Serial.println("Setup completed.");
}

// =======================
// LOOP
// =======================
void loop() {
    // Obsługa OTA – musi być wywoływana regularnie
    ArduinoOTA.handle();

    unsigned long now = millis();

    // 1. Próbkowanie czujnika i detekcja oddechu (co SAMPLE_INTERVAL_MS)
    if (now - lastSampleTime >= SAMPLE_INTERVAL_MS) {
        lastSampleTime = now;
        sensorValue = analogRead(0);   // odczyt z A0
        
        // Aktualizuj bieżące min/max dla automatycznej regulacji
        if (sensorValue > currentMax) currentMax = sensorValue;
        if (sensorValue < currentMin) currentMin = sensorValue;
        
        // Sprawdź oddech
        checkBreath(sensorValue);
    }

    // 2. Automatyczna regulacja progów co minutę
    if (now - lastMinuteReset >= MINUTE_MS) {
        lastMinuteMax = currentMax;
        lastMinuteMin = currentMin;
        
        // Ustaw nowe progi: szczyt - 15, dołek + 15
        HYSTERESIS_HIGH = lastMinuteMax - 15;
        HYSTERESIS_LOW = lastMinuteMin + 15;
        
        // Zabezpieczenia
        if (HYSTERESIS_HIGH < 0) HYSTERESIS_HIGH = 0;
        if (HYSTERESIS_LOW > 1023) HYSTERESIS_LOW = 1023;
        if (HYSTERESIS_HIGH <= HYSTERESIS_LOW) {
            // Jeśli progi się zrównały lub nachodzą, rozsuń je
            HYSTERESIS_HIGH = HYSTERESIS_LOW + 20;
            if (HYSTERESIS_HIGH > 1023) {
                HYSTERESIS_HIGH = 1023;
                HYSTERESIS_LOW = 1023 - 20;
            }
        }
        
        // Resetuj bieżące min/max na aktualną wartość
        currentMax = sensorValue;
        currentMin = sensorValue;
        
        lastMinuteReset = now;
        
        // Informacja na serial
        Serial.print("Auto-update progów: HIGH=");
        Serial.print(HYSTERESIS_HIGH);
        Serial.print(" LOW=");
        Serial.println(HYSTERESIS_LOW);

        // Tutaj robimy update do serwera:
        sendBPMToServer(oddechy);
    }

    // 3. Aktualizacja wyświetlacza (co 100 ms)
    static unsigned long lastDisplayUpdate = 0;
    if (now - lastDisplayUpdate >= 100) {
        lastDisplayUpdate = now;

        // Oblicz aktualne BPM na podstawie bufora czasów
        float bpm = getBreathsPerMinute();
        oddechy = (int)(bpm + 0.5); // zaokrąglenie

        // Odśwież OLED
        updateDisplay();
    }

    // 4. Heartbeat na Serial (co 500 ms)
    static unsigned long lastBeat = 0;
    if (now - lastBeat >= 500) {
        lastBeat = now;
        //RtcDateTime rtcNow = Rtc.GetDateTime();
        //Serial.print(printDateTime(rtcNow));
        Serial.print(" | sensor: ");
        Serial.print(sensorValue);
        Serial.print(" | BPM: ");
        Serial.print(oddechy);
        Serial.print(" | HIGH:");
        Serial.print(HYSTERESIS_HIGH);
        Serial.print(" LOW:");
        Serial.println(HYSTERESIS_LOW);
    }
}
