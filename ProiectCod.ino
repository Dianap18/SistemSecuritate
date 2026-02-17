#include <Wire.h>
// #include <Adafruit_SSD1306.h>
#include "Adafruit_Keypad.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_Fingerprint.h>

const char* ssid = "HONOR Magic7 Pro";
const char* password = "Diana123";

String serverName = "http://10.100.69.242:3000";

#define LED_GREEN 4
#define LED_RED 2
#define BUZZER 15
#define RXD2 16
#define TXD2 17

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
// Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

HardwareSerial mySerial(2);
Adafruit_Fingerprint finger(&mySerial);

const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {14, 27, 12, 13};
byte colPins[COLS] = {32, 33, 25, 26};

Adafruit_Keypad customKeypad = Adafruit_Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

String pinInput = "";
int mode = 0;
int currentUserId = -1;
int globalFingerCounter = 100;

void showMessage(String text) {
  Serial.println("\n[ECRAN]: " + text);
  // display.clearDisplay();
  // display.setTextSize(1);
  // display.setTextColor(SSD1306_WHITE);
  // display.setCursor(0, 10);
  // display.println(text);
  // display.display();
}

void buzzerOK() {
  tone(BUZZER, 1200, 100);
  delay(100);
  tone(BUZZER, 1500, 100);
}

void buzzerBAD() {
  tone(BUZZER, 300, 300);
}

void accessGranted() {
  digitalWrite(LED_GREEN, HIGH);
  buzzerOK();
  showMessage("ACCES OK");
  delay(1500);
  digitalWrite(LED_GREEN, LOW);
}

void accessDenied() {
  digitalWrite(LED_RED, HIGH);
  buzzerBAD();
  showMessage("REFUZAT");
  delay(1500);
  digitalWrite(LED_RED, LOW);
}

int getFingerprintID() {
  // Cere senzorului sa faca o poza a degetului
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) return -1;

  // Il pune in memoria temporara a senzorului (Slotul 1)
  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return -1;

  // Cauta template-ul din Slotul 1 in toata memoria senzorului 
  p = finger.fingerFastSearch();
  
  if (p != FINGERPRINT_OK){
    return -1;
    accessDenied();
  } 

  // Daca a ajuns aici a gasit o potrivire
  return finger.fingerID;
}


bool enrollFingerInSensor(int id) {
  int p = -1;
  showMessage("Pune degetul...");

  // poze continuu pana detecteaza un deget
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
  }

  // Converteste prima poza 
  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) return false;

  showMessage("Ridica degetul...");
  delay(2000);

  p = 0;
  // ca sa nu treaca la pasul 2 cu aceeasi atingere, asteapta sa se ia degetul 
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }

  showMessage("Pune iar...");
  p = -1;

  // Asteapta din nou sa pui degetul
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
  }

  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) return false;

  // daca pozele sunt identice creeaza model
  p = finger.createModel();
  if (p != FINGERPRINT_OK) return false;

  // scrie modelul la adresa id
  p = finger.storeModel(id);

  if (p != FINGERPRINT_OK) return false;
  
  return true;
}

bool verifyOTP(String otp) {
  if(WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverName + "/verify-otp");
    http.addHeader("Content-Type", "application/json");
    Serial.println(">> Trimit OTP: " + otp);
    int code = http.POST("{\"otp\":\"" + otp + "\"}");
    if (code > 0 && http.getString().indexOf("true") > 0) {
       http.end();
       return true;
    }
    http.end();
  }
  return false;
}

bool setPIN(String pin) {
  if(WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverName + "/set-pin");
    http.addHeader("Content-Type", "application/json");
    Serial.println(">> Salvez PIN: " + pin);
    int code = http.POST("{\"pin\":\"" + pin + "\"}");
    if (code > 0) {
      String response = http.getString();
      Serial.println("Raspuns: " + response);
      if (response.indexOf("true") > 0) {
        int idx = response.indexOf("user_id\":");
        if (idx > 0) {
             currentUserId = response.substring(idx + 9).toInt();
             http.end();
             return true;
        }
      }
    }
    http.end();
  }
  return false;
}

bool authenticatePIN(String pin) {
  if(WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverName + "/auth");
    http.addHeader("Content-Type", "application/json");
    int code = http.POST("{\"pin\":\"" + pin + "\"}");
    if(code > 0 && http.getString().indexOf("true") > 0) {
        http.end();
        return true;
    }
    http.end();
  }
  return false;
}

bool enrollFingerprint(int uId, int fId) {
  if(WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverName + "/enroll-fingerprint");
    http.addHeader("Content-Type", "application/json");
    int code = http.POST("{\"user_id\":" + String(uId) + ", \"fingerprint_id\":" + String(fId) + "}");
    if(code > 0 && http.getString().indexOf("true") > 0) {
        http.end();
        return true;
    }
    http.end();
  }
  return false;
}

bool authenticateFingerprint(int fId) {
  if(WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = serverName + "/auth";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    String payload = "{\"fingerprint_id\":" + String(fId) + "}";
    int code = http.POST(payload);
    if(code > 0 && http.getString().indexOf("true") > 0) {
        http.end();
        return true;
    }
    http.end();
  }
  return false;
}

void resetToIdle() {
  pinInput = "";
  mode = 0;
  showMessage("ASTEPTARE... (B=Inrol, #=PIN, A=Ampr)");
}

void handleKeyInput(char key) {
  tone(BUZZER, 2000, 20);
  Serial.print("Tasta: ");
  Serial.println(key);
  if (key == '*') {
    resetToIdle();
    return;
  }
  if (mode == 0) {
    if (key == 'B') {
        mode = 3;
        pinInput = "";
        showMessage("Introdu Cod OTP:");
        return;
    }
    if (key == '#') {
        mode = 1;
        pinInput = "";
        showMessage("Introdu PIN:");
        return;
    }
    if (key == 'A') {
       showMessage("Pune degetul...");
       long startTime = millis();
       int id = -1;
       while (millis() - startTime < 3000) {
          id = getFingerprintID();
          if (id != -1) break;
          delay(50);
       }
       if (id != -1) {
         if (authenticateFingerprint(id)) accessGranted();
         else accessDenied();
       } else {
         showMessage("Necunoscut");
         accessDenied();
       }
       resetToIdle();
       return;
    }
  }
  else if (mode == 1 && key >= '0' && key <= '9') {
    pinInput += key;
    Serial.println("PIN Input: " + pinInput);
    if (pinInput.length() == 4) {
      if (authenticatePIN(pinInput)) accessGranted();
      else accessDenied();
      resetToIdle();
    }
  }
  else if (mode == 3 && key >= '0' && key <= '9') {
    pinInput += key;
    Serial.println("OTP Input: " + pinInput);
    if (pinInput.length() == 4) {
      if (verifyOTP(pinInput)) {
        mode = 4;
        pinInput = "";
        buzzerOK();
        showMessage("PIN Nou:");
        return;
      } else {
        accessDenied();
        resetToIdle();
      }
    }
  }
  else if (mode == 4 && key >= '0' && key <= '9') {
    pinInput += key;
    Serial.println("PIN NOU Input: " + pinInput);
    if (pinInput.length() == 4) {
      showMessage("Se salveaza...");
      if (setPIN(pinInput)) {
        showMessage("Vrei Amprenta? A=DA / B=NU");
        mode = 5;
        return;
      } else {
        showMessage("Eroare Salvare!");
        delay(2000);
        resetToIdle();
      }
    }
  }
  else if (mode == 5) {
      if (key == 'A') {
        bool ok = enrollFingerInSensor(currentUserId);
        if (ok) {
            showMessage("Salvat Senzor! Trimit...");
            if (enrollFingerprint(currentUserId, currentUserId)) accessGranted();
            else showMessage("Eroare Server");
        } else {
            showMessage("Eroare Senzor");
        }
        resetToIdle();
      }
      else {
         showMessage("Fara Amprenta. OK!");
         delay(1000);
         resetToIdle();
      }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n--- START SISTEM ---");
  
  // Wire.begin(21, 22);
  // if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
  //   display.begin(SSD1306_SWITCHCAPVCC, 0x3D);
  // }
  // display.clearDisplay();
  // display.display();

  mySerial.begin(57600, SERIAL_8N1, 16, 17);
  finger.begin(57600);
  delay(500);
  if (finger.verifyPassword()) {
    Serial.println("Senzor Amprenta GASIT!");
  } else {
    Serial.println("EROARE: Senzor Amprenta NU a fost gasit!");
  }
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi OK!");
  customKeypad.begin();
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  resetToIdle();
}

void loop() {
  customKeypad.tick();
  while(customKeypad.available()){
    keypadEvent e = customKeypad.read();
    if(e.bit.EVENT == KEY_JUST_PRESSED) handleKeyInput((char)e.bit.KEY);
  }
  delay(10);
}