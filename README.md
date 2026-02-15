# Breath Monitor – system zdalnego monitorowania oddechu

System składa się z urządzenia pomiarowego (ESP8266 z czujnikiem analogowym) oraz serwera WWW zbierającego dane i wyświetlającego je na wykresie. Urządzenie wykrywa cykle oddechowe, oblicza BPM (breaths per minute) i wysyła dane co minutę na serwer przez Wi-Fi. Serwer udostępnia panel z wykresem i tabelą ostatnich pomiarów.

## Funkcje

- Detekcja oddechu na podstawie sygnału analogowego z czujnika (np. piezoelektrycznego, na pas).
- Automatyczna kalibracja progów detekcji (co minutę na podstawie rzeczywistych minimów i maksimów).
- Obliczanie BPM w czasie rzeczywistym.
- Wysyłanie danych na serwer przez HTTP POST (co minutę).
- Aktualizacje Over‑the‑Air (OTA) – bezprzewodowe wgrywanie nowego firmware.
- Serwer w Dockerze (Flask + Gunicorn) zbierający dane i udostępniający stronę WWW.
- Wizualizacja danych za pomocą Chart.js (wykres liniowy) i tabeli.

## Architektura
ESP8266 (z czujnikiem) ──WiFi──› Serwer Ubuntu (Oracle Cloud)
│
├─ Kontener Docker z Flask
└─ Port 5000 (HTTP)


## Wymagania

### Sprzęt
- ESP8266 (NodeMCU, Wemos D1 mini itp.)
- Czujnik analogowy (np. pas oddechowy z wyjściem analogowym)
- Opcjonalnie: wyświetlacz OLED SSD1306 (128×64, I2C)
- Opcjonalnie: zegar RTC DS1302

### Oprogramowanie
- Arduino IDE z zainstalowanymi bibliotekami:
  - `ESP8266WiFi`, `ArduinoOTA`
  - `Adafruit GFX`, `Adafruit SSD1306`
  - `ThreeWire`, `RtcDS1302` (opcjonalnie)
- Python 3.8+ na serwerze (lub Docker)
- Docker i Docker Compose (opcjonalnie, ale zalecane)

## Konfiguracja urządzenia ESP8266

### 1. Przygotowanie środowiska Arduino
Zainstaluj wymagane biblioteki przez Menedżera bibliotek:
- `Adafruit GFX Library`
- `Adafruit SSD1306`
- `RtcDS1302` (jeśli używasz RTC)
- `ESP8266WiFi` i `ArduinoOTA` (dostępne w pakiecie ESP8266)

### 2. Konfiguracja w kodzie
Otwórz plik `breath_monitor.ino` i dostosuj początkowe ustawienia:

```cpp
// Sieć Wi-Fi
const char* ssid = "Twoja_Siec";
const char* password = "Twoje_Haslo";

// Adres serwera (bez http://)
const char* serverName = "http://twoja-domena.ddns.net:5000/api/bpm";

// Piny I2C dla OLED
#define I2C_SDA 14   // D5
#define I2C_SCL 12   // D6

// Piny RTC (opcjonalnie)
ThreeWire myWire(4, 5, 0); // IO, SCLK, CE
