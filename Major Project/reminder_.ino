#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <WebServer.h>

// Configuration
const char* ssid = "Bindu";
const char* password = "12345678";

String botToken = "8316264884:AAE0MLg3lemAZEbQ8_3yRmJ8ajgTWR6Vo3A";
String chatID   = "1459675191";

// LCD and RTC setup
LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS3231 rtc;

// Pins
const int buzzerPin        = 13;
const int confirmButtonPin = 25;
const int editButtonPin    = 26; 

const int buzzerChannel = 0;

// Medicine struct
struct Medicine {
  String name;
  String type;
  int hour;
  int minute;
  bool taken;
};

Medicine medicines[10];
int medCount = 3;

WebServer server(80);

//Telegram
void sendTelegramMessage(String message) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://api.telegram.org/bot" + botToken +
                 "/sendMessage?chat_id=" + chatID + "&text=" + message;
    http.begin(url);
    int code = http.GET();
    if (code > 0) Serial.println("? Telegram message sent");
    else {
      Serial.print("Telegram failed, code: ");
      Serial.println(code);
    }
    http.end();
  } else {
    Serial.println("WiFi not connected - cannot send Telegram");
  }
}

// Daily Report
void sendDailyReport() {
  String report = "📋 Daily Medicine Report\n\n";
  for (int i = 0; i < medCount; i++) {
    report += String(i+1) + ". " + medicines[i].name + " (" + medicines[i].type + ") - ";
    report += (medicines[i].taken ? " Taken" : " Missed");
    report += "\n";
  }
  sendTelegramMessage(report);
}

// WiFi
void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("🔄Connecting to WiFi");
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    delay(500);
    Serial.print(".");
    tries++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n WiFi Connected!");
    Serial.print("📡 IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n Failed to connect to WiFi");
  }
}

//Medicines
void setupMedicines() {
  medicines[0] = {"Paracetamol", "Morning", 8, 00, false};
  medicines[1] = {"VitaminC",   "Afternoon", 14, 00, false};
  medicines[2] = {"Antibiotic", "Evening", 19, 00, false};
  medCount = 3;
}

// Web server
void handleRoot() {
  String html = "<!doctype html><html><head><meta name='viewport' content='width=device-width'/><title>Medicine Schedule</title></head><body>";
  html += "<h2>Medicine Schedule</h2>";
  html += "<form method='POST' action='/add'>Name: <input name='name' required> Hour: <input name='hour' type='number' min='0' max='23' required> Min: <input name='minute' type='number' min='0' max='59' required> <button type='submit'>Add</button></form><hr>";
  html += "<table border='1' style='width:100%;'><tr><th>#</th><th>Name</th><th>Type</th><th>Time</th><th>Status</th><th>Action</th></tr>";
  for (int i = 0; i < medCount; i++) {
    html += "<tr><td>" + String(i+1) + "</td><td>" + medicines[i].name + "</td><td>" + medicines[i].type + "</td><td>";
    if (medicines[i].hour < 10) html += "0";
    html += String(medicines[i].hour) + ":";
    if (medicines[i].minute < 10) html += "0";
    html += String(medicines[i].minute) + "</td><td>" + (medicines[i].taken ? "Taken" : "Pending") + "</td>";
    html += "<td><a href='/delete?id=" + String(i) + "'>Delete</a></td></tr>";
  }
  html += "</table><p>Press edit button: Short press = add med in 1 min, Long press = clear all.</p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleAdd() {
  if (!server.hasArg("name") || !server.hasArg("hour") || !server.hasArg("minute")) {
    server.send(400, "text/plain", "Missing parameters");
    return;
  }
  if (medCount >= 10) {
    server.send(400, "text/plain", "Medicine list full");
    return;
  }
  String name = server.arg("name");
  int hour = server.arg("hour").toInt();
  int minute = server.arg("minute").toInt();
  medicines[medCount++] = {name, "Custom", hour, minute, false};
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleDelete() {
  if (!server.hasArg("id")) {
    server.send(400, "text/plain", "Missing id");
    return;
  }
  int id = server.arg("id").toInt();
  if (id < 0 || id >= medCount) {
    server.send(400, "text/plain", "Invalid id");
    return;
  }
  for (int i = id; i < medCount - 1; i++) medicines[i] = medicines[i+1];
  medCount--;
  server.sendHeader("Location", "/");
  server.send(303);
}

void setup() {
  Serial.begin(115200);

  
  ledcSetup(buzzerChannel, 2000, 8); 
  ledcAttachPin(buzzerPin, buzzerChannel);

  pinMode(confirmButtonPin, INPUT_PULLUP);
  pinMode(editButtonPin, INPUT_PULLUP);

  // LCD
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Smart Medicine");
  lcd.setCursor(0, 1);
  lcd.print("Reminder System");
  delay(2000);
  lcd.clear();

  // RTC
  Wire.begin();
  if (!rtc.begin()) {
    Serial.println("RTC not found!");
    while (1) delay(10);
  }
   rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // uncomment once

  setupMedicines();

  // WiFi + server
  connectToWiFi();
  server.on("/", HTTP_GET, handleRoot);
  server.on("/add", HTTP_POST, handleAdd);
  server.on("/delete", HTTP_GET, handleDelete);
  server.begin();
  Serial.println("🌐 Web server started");
}

// ------------------ Main loop ------------------
void loop() {
  DateTime now = rtc.now();

  // Display time
  lcd.setCursor(0, 0);
  char timestr[17];
  sprintf(timestr, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  lcd.print(timestr);
  lcd.print("    ");

  // Next medicine
  int nextIndex = -1, bestDiff = 24*3600;
  int nowSec = now.hour()*3600 + now.minute()*60 + now.second();
  for (int i = 0; i < medCount; i++) {
    int remSec = medicines[i].hour*3600 + medicines[i].minute*60;
    int diff = remSec - nowSec;
    if (diff < 0) diff += 24*3600;
    if (diff < bestDiff) { bestDiff = diff; nextIndex = i; }
  }
  lcd.setCursor(0, 1);
  if (nextIndex >= 0) {
    String name = medicines[nextIndex].name;
    if (name.length() > 16) name = name.substring(0, 16);
    lcd.print(name);
    for (int i = 0; i < (16 - name.length()); i++) lcd.print(" ");
  } else {
    lcd.print("No Meds Scheduled ");
  }

  // Medicine reminders
  for (int i = 0; i < medCount; i++) {
    if (now.hour() == medicines[i].hour && now.minute() == medicines[i].minute && !medicines[i].taken) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Take: ");
      lcd.print(medicines[i].name.substring(0,9));
      lcd.setCursor(0, 1);
      lcd.print("Press Btn to confirm");

      unsigned long start = millis();
      bool confirmed = false;
      while (millis() - start < 30000UL) {
        ledcWriteTone(buzzerChannel, 1000);
        delay(400);
        ledcWriteTone(buzzerChannel, 0);
        delay(400);
        if (digitalRead(confirmButtonPin) == LOW) {
          confirmed = true;
          delay(50);
          while (digitalRead(confirmButtonPin) == LOW) delay(10);
          break;
        }
      }

      if (confirmed) {
        medicines[i].taken = true;
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Taken:");
        lcd.setCursor(0,1);
        lcd.print(medicines[i].name.substring(0,16));
      } else {
        medicines[i].taken = true;
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Missed:");
        lcd.setCursor(0,1);
        lcd.print(medicines[i].name.substring(0,16));
        sendTelegramMessage("Missed medicine: " + medicines[i].name + " (" + medicines[i].type + ")");
      }
      delay(2000);
      lcd.clear();
    }

    if (now.hour() == medicines[i].hour && now.minute() == (medicines[i].minute + 1)) {
      medicines[i].taken = false;
    }
  }

  // Edit button
  static unsigned long editPressStart = 0;
  static bool editPressed = false;
  int editState = digitalRead(editButtonPin);
  if (editState == LOW && !editPressed) {
    editPressed = true;
    editPressStart = millis();
  }
  if (editState == HIGH && editPressed) {
    unsigned long held = millis() - editPressStart;
    editPressed = false;
    if (held < 2000) {
      if (medCount < 10) {
        DateTime t = rtc.now();
        int newMin = (t.minute() + 1) % 60;
        int newHour = t.hour() + (t.minute() == 59 ? 1 : 0);
        if (newHour >= 24) newHour -= 24;
        medicines[medCount++] = {"NewMed", "Custom", newHour, newMin, false};
        Serial.println("➕ Added NewMed (1 min ahead)");
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Added NewMed");
        delay(1200);
        lcd.clear();
      } else {
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("List Full");
        delay(1000);
        lcd.clear();
      }
    } else {
      medCount = 0;
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("All Cleared");
      delay(1200);
      lcd.clear();
    }
  }

  // Daily report 
const int reportHour = 19;     
const int reportMinute = 00;   

static bool reportSentToday = false;

if (now.hour() == reportHour && now.minute() == reportMinute) {
  if (!reportSentToday) {
    sendDailyReport();
    reportSentToday = true;
  }
} 
else if (now.hour() == 0 && now.minute() == 0) {
  // Reset for new day
  reportSentToday = false;
  for (int i = 0; i < medCount; i++) {
    medicines[i].taken = false;  // reset status
  }
}

  server.handleClient();
  delay(300);
}
