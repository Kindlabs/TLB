#include <WiFiManager.h>          // WiFiManager for WiFi configuration
#include "HT_SSD1306Wire.h"       // Heltec OLED display library
#include "logo.h"                 // Include the logo file
#include "Arduino.h"

// Define OLED display object
SSD1306Wire factory_display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

// Define AP credentials
#define AP_SSID "TempLogger"       // Access Point SSID
#define AP_PASSWORD "12345678"     // Access Point Password

WiFiManager wm;                   // WiFiManager instance
WebServer server(80);             // Web server on port 80

// Battery measurement settings
#define VBAT_Read 1               // GPIO1 for VBAT
#define ADC_CTRL 37               // GPIO37 for ADC control
#define ADC_CTRL_LOGIC HIGH       // Set to LOW for V3.1, HIGH for V3.2
#define ADC_READ_STABILIZE 50     // Stabilization delay in ms

float readBatLevel() {
  pinMode(ADC_CTRL, OUTPUT);
  digitalWrite(ADC_CTRL, ADC_CTRL_LOGIC); // Use appropriate logic for your board
  delay(ADC_READ_STABILIZE);

  int analogValue = analogRead(VBAT_Read);
  Serial.print("Raw ADC Value: ");
  Serial.println(analogValue);

  if (analogValue == 0) {
    Serial.println("Error: ADC reading is zero. Check connections or ADC_CTRL logic.");
    return 0.0;
  }

  // Resistor values: R1 = 390kΩ, R2 = 100kΩ
  // Scaling factor: (R1 + R2) / R2 = 4.9
  float voltage = (analogValue * 3.3 / 4095.0) * ((390.0 + 100.0) / 100.0);
  return voltage;
}

void displayBatteryInfo() {
  float voltage = readBatLevel();
  factory_display.setFont(ArialMT_Plain_10);
  factory_display.setTextAlignment(TEXT_ALIGN_LEFT);
  factory_display.drawString(0, 40, "Battery: " + String(voltage, 2) + " V");
}

void updateDisplay(const String &status, const String &ip = "") {
  factory_display.clear();
  factory_display.setFont(ArialMT_Plain_10);

  // Display status
  factory_display.drawString(0, 0, "Status: " + status);

  // Display IP address
  if (!ip.isEmpty()) {
    factory_display.drawString(0, 20, "IP: " + ip);
  }

  // Display battery information
  displayBatteryInfo();

  factory_display.display();
}

void handleRoot() {
  server.send(200, "text/html",
              "<html><body>"
              "<h1>ESP32 WebUI</h1>"
              "<p>Status: Online</p>"
              "<p>Battery Voltage: " + String(readBatLevel(), 2) + " V</p>"
              "<a href='/reconfig'>Reconfigure WiFi</a>"
              "</body></html>");
}

void handleReconfigure() {
  server.send(200, "text/html", "<html><body><h1>Restarting WiFiManager...</h1></body></html>");
  delay(1000);
  wm.startConfigPortal(AP_SSID, AP_PASSWORD); // Relaunch WiFiManager
}

void configModeCallback(WiFiManager *wm) {
  String ssid = wm->getConfigPortalSSID();
  updateDisplay("AP Mode", "SSID: " + ssid);
  Serial.println("AP Mode Started. Connect to configure:");
  Serial.println(ssid);
}

void setup() {
  Serial.begin(115200);

  // Enable Vext power for OLED
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
  delay(100);

  // Reset OLED
  pinMode(RST_OLED, OUTPUT);
  digitalWrite(RST_OLED, LOW);
  delay(50);
  digitalWrite(RST_OLED, HIGH);

  // Initialize OLED display
  factory_display.init();
  factory_display.clear();

  // Display the logo on boot
  factory_display.drawXbm(0, 0, logo_width, logo_height, epd_bitmap_logo);
  factory_display.display();
  delay(2000); // Show the logo for 2 seconds

  // Initialize WiFiManager
  wm.setAPCallback(configModeCallback);
  if (!wm.autoConnect(AP_SSID, AP_PASSWORD)) {
    updateDisplay("WiFi Failed", "Restarting...");
    Serial.println("Failed to connect to WiFi. Restarting...");
    ESP.restart();
  }

  // Display WiFi connection status
  String ip = WiFi.localIP().toString();
  updateDisplay("Online", ip);
  Serial.println("WiFi Connected! IP Address: " + ip);

  // Initialize web server
  server.on("/", handleRoot);           // Main WebUI
  server.on("/reconfig", handleReconfigure); // Reconfigure WiFi via WiFiManager
  server.begin();
  Serial.println("WebUI server started.");
}

void loop() {
  server.handleClient(); // Handle web server requests
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 5000) { // Update display every 5 seconds
    lastUpdate = millis();
    updateDisplay("Online", WiFi.localIP().toString());
  }
}
