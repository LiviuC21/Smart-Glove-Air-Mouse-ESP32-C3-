#include <Wire.h>
#include <BleMouse.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SD.h>
#include <SPI.h>

// ---------------- CONFIG PINI GENERALI ----------------
const int pinSDA = 5;
const int pinSCL = 6;
const int MPU_ADDR = 0x68;

const int butonAratator = 4;
const int butonMijlociu = 3;
const int butonInelar = 1;
const int butonDegetMic = 2; // <-- SCHIMBA cu pinul real unde ai legat butonul de pe degetul mic

const int ledPin = 8;

// ---------------- CONFIG CARD SD (MOD 2) ----------------
const int SD_CS   = 7;  // <-- SCHIMBA cu pinul CS real al modulului SD
const int SD_SCK  = 10; // <-- SCHIMBA cu pinul SCK real
const int SD_MISO = 20; // <-- SCHIMBA cu pinul MISO real
const int SD_MOSI = 21; // <-- SCHIMBA cu pinul MOSI real

const char* AP_SSID     = "Manusa-Stanga";
const char* AP_PASSWORD = "manusa1234"; // minim 8 caractere

WebServer server(80);
File uploadFile;

// ---------------- MODURI ----------------
enum Mod {
  MOD_MOUSE = 0,
  MOD_SERVER_WEB = 1,
  MOD_MODUL3 = 2,
  NR_MODURI = 3
};

Preferences prefs;
int modCurent = MOD_MOUSE;

// ==================================================================
// ============  PARTEA COMUNA: MPU6050 (folosita de mod 1) ========
// ==================================================================
BleMouse bleMouse("Manusa Stanga", "Liviu", 100);

float offsetGx = 0, offsetGy = 0, offsetGz = 0;
bool lastConnectedState = false;

unsigned long inelarApasatDeLa = 0;
bool inelarTinutPentruRecalibrare = false;
const unsigned long TIMP_RECALIBRARE = 2000;

const int PRAG_STATIONAR = 250;
const float VITEZA_ADAPTARE = 0.0008;

void citesteGyroRaw(int16_t &gx, int16_t &gy, int16_t &gz) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x43);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)6, true);

  if (Wire.available() >= 6) {
    gx = (Wire.read() << 8) | Wire.read();
    gy = (Wire.read() << 8) | Wire.read();
    gz = (Wire.read() << 8) | Wire.read();
  }
}

void calibreaza() {
  long sumGx = 0, sumGy = 0, sumGz = 0;
  const int NR_SAMPLES = 300;

  for (int i = 0; i < NR_SAMPLES; i++) {
    digitalWrite(ledPin, (i / 5) % 2);

    int16_t gx = 0, gy = 0, gz = 0;
    citesteGyroRaw(gx, gy, gz);
    sumGx += gx;
    sumGy += gy;
    sumGz += gz;

    delay(10);
    yield();
  }

  offsetGx = sumGx / (float)NR_SAMPLES;
  offsetGy = sumGy / (float)NR_SAMPLES;
  offsetGz = sumGz / (float)NR_SAMPLES;

  digitalWrite(ledPin, bleMouse.isConnected() ? LOW : HIGH);
}

// ==================================================================
// ======================  MOD 1: MOUSE  ============================
// ==================================================================
void setupModMouse() {
  pinMode(ledPin, OUTPUT); // Aici suntem in siguranta, il setam ca LED
  
  Wire.begin(pinSDA, pinSCL);
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission();

  pinMode(butonAratator, INPUT_PULLUP);
  pinMode(butonMijlociu, INPUT_PULLUP);
  pinMode(butonInelar, INPUT_PULLUP);

  bleMouse.begin();
  delay(2500);
  calibreaza();
}

void loopModMouse() {
  bool isConnected = bleMouse.isConnected();

  if (isConnected != lastConnectedState) {
    digitalWrite(ledPin, isConnected ? LOW : HIGH);
    lastConnectedState = isConnected;
  }

  if (digitalRead(butonInelar) == LOW) {
    if (inelarApasatDeLa == 0) {
      inelarApasatDeLa = millis();
    } else if (!inelarTinutPentruRecalibrare && (millis() - inelarApasatDeLa > TIMP_RECALIBRARE)) {
      inelarTinutPentruRecalibrare = true;
      calibreaza();
    }
  } else {
    inelarApasatDeLa = 0;
    inelarTinutPentruRecalibrare = false;
  }

  if (isConnected) {
    int16_t gxRaw = 0, gyRaw = 0, gzRaw = 0;
    citesteGyroRaw(gxRaw, gyRaw, gzRaw);

    int gx = gxRaw - (int)offsetGx;
    int gy = gyRaw - (int)offsetGy;
    int gz = gzRaw - (int)offsetGz;

    if (abs(gx) < PRAG_STATIONAR) offsetGx += gx * VITEZA_ADAPTARE;
    if (abs(gy) < PRAG_STATIONAR) offsetGy += gy * VITEZA_ADAPTARE;
    if (abs(gz) < PRAG_STATIONAR) offsetGz += gz * VITEZA_ADAPTARE;

    if (abs(gx) < 180) gx = 0;
    if (abs(gy) < 180) gy = 0;
    if (abs(gz) < 180) gz = 0;

    int divizorViteza = 500;
    int moveX = -gz / divizorViteza;
    int moveY = gx / divizorViteza;

    if (moveX != 0) moveX = moveX * 1.3;

    if (digitalRead(butonInelar) == HIGH) {
      if (moveX != 0 || moveY != 0) {
        bleMouse.move(moveX, moveY);
      }
    }

    if (digitalRead(butonAratator) == LOW) {
      if (!bleMouse.isPressed(MOUSE_LEFT)) bleMouse.press(MOUSE_LEFT);
    } else {
      if (bleMouse.isPressed(MOUSE_LEFT)) bleMouse.release(MOUSE_LEFT);
    }

    if (digitalRead(butonMijlociu) == LOW) {
      if (!bleMouse.isPressed(MOUSE_RIGHT)) bleMouse.press(MOUSE_RIGHT);
    } else {
      if (bleMouse.isPressed(MOUSE_RIGHT)) bleMouse.release(MOUSE_RIGHT);
    }

    delay(4);
  } else {
    delay(10);
  }
}

// ==================================================================
// ===================  MOD 2: SERVER WEB + SD  ======================
// ==================================================================
String tipMime(String nume) {
  if (nume.endsWith(".mp3")) return "audio/mpeg";
  if (nume.endsWith(".wav")) return "audio/wav";
  if (nume.endsWith(".txt")) return "text/plain";
  return "application/octet-stream";
}

String listeazaFisiere() {
  String html = "<ul>";
  File root = SD.open("/");
  File f = root.openNextFile();
  while (f) {
    if (!f.isDirectory()) {
      String nume = String(f.name());
      html += "<li>" + nume + " (" + String(f.size() / 1024) + " KB) ";
      html += "<a href=\"/download?file=" + nume + "\">Descarca</a> | ";
      html += "<a href=\"/play?file=" + nume + "\">Asculta</a></li>";
    }
    f = root.openNextFile();
  }
  html += "</ul>";
  return html;
}

void handleRoot() {
  String pagina = "<html><body><h2>Manusa - Fisiere SD</h2>";
  pagina += "<form method='POST' action='/upload' enctype='multipart/form-data'>";
  pagina += "<input type='file' name='fisier'> <input type='submit' value='Incarca'>";
  pagina += "</form><hr>";
  pagina += listeazaFisiere();
  pagina += "</body></html>";
  server.send(200, "text/html", pagina);
}

void handleUpload() {
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String nume = "/" + upload.filename;
    uploadFile = SD.open(nume, FILE_WRITE);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) uploadFile.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) uploadFile.close();
  }
}

void handleUploadDone() {
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleDownload() {
  if (!server.hasArg("file")) { server.send(400, "text/plain", "Lipseste parametrul file"); return; }
  String nume = "/" + server.arg("file");
  if (!SD.exists(nume)) { server.send(404, "text/plain", "Fisier inexistent"); return; }
  File f = SD.open(nume, FILE_READ);
  server.sendHeader("Content-Disposition", "attachment; filename=" + server.arg("file"));
  server.streamFile(f, tipMime(nume));
  f.close();
}

void handlePlay() {
  if (!server.hasArg("file")) { server.send(400, "text/plain", "Lipseste parametrul file"); return; }
  String nume = "/" + server.arg("file");
  if (!SD.exists(nume)) { server.send(404, "text/plain", "Fisier inexistent"); return; }
  File f = SD.open(nume, FILE_READ);
  server.streamFile(f, tipMime(nume));
  f.close();
}

void setupModServerWeb() {
  // Aici NU facem pinMode(ledPin, OUTPUT) ca sa nu blocam placa.
  // Lasam placa sa pregateasca pinul 8 strict pentru SD_MISO.
  
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  if (!SD.begin(SD_CS)) {
    // Doar daca SD-ul da eroare fatala si nu porneste, fortam pinul sa clipeasca
    pinMode(ledPin, OUTPUT);
    while (true) {
      digitalWrite(ledPin, LOW);
      delay(100);
      digitalWrite(ledPin, HIGH);
      delay(100);
    }
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/upload", HTTP_POST, handleUploadDone, handleUpload);
  server.on("/download", HTTP_GET, handleDownload);
  server.on("/play", HTTP_GET, handlePlay);
  server.begin();
  
  // Am scos complet aprinderea led-ului de la finalul functiei.
}

void loopModServerWeb() {
  server.handleClient();
}

// ==================================================================
// =====================  MOD 3: LOCUL TAU  ==========================
// ==================================================================
void setupModul3() {
  // <<< AICI PUI CODUL TAU DE INITIALIZARE PENTRU MODUL 3 >>>
}

void loopModul3() {
  // <<< AICI PUI CODUL TAU CARE RULEAZA CONTINUU IN MODUL 3 >>>
}

// ==================================================================
// ==============  SCHIMBARE MOD (buton deget mic) ==================
// ==================================================================
bool lastButonDegetMic = HIGH;
unsigned long lastDebounceDegetMic = 0;

void semnalizeazaMod(int mod) {
  // Fortam pinul sa redevina LED, ignorand cardul SD pentru moment
  pinMode(ledPin, OUTPUT);
  
  for (int i = 0; i <= mod; i++) {
    digitalWrite(ledPin, LOW);  // Aprinde LED-ul
    delay(200);
    digitalWrite(ledPin, HIGH); // Stinge LED-ul
    delay(200);
  }
  delay(500);
}

void verificaSchimbareMod() {
  // Dacă butonul este apăsat
  if (digitalRead(butonDegetMic) == LOW) {
    
    // Așteptăm 50ms și verificăm din nou (pentru a ignora zgomotul electric/apăsările false)
    delay(50); 
    
    if (digitalRead(butonDegetMic) == LOW) {
      // Trecem la următorul mod
      modCurent = (modCurent + 1) % NR_MODURI;
      
      // Salvăm noul mod în memorie
      prefs.putInt("mod", modCurent);
      
      // Semnalizăm din LED
      semnalizeazaMod(modCurent);
      
      // Dăm restart plăcii ca să pornească curat în noul mod
      ESP.restart(); 
    }
  }
}

// ==================================================================
// ========================  SETUP / LOOP  ===========================
// ==================================================================
void setup() {
  Serial.begin(115200);

  pinMode(ledPin, OUTPUT);
  pinMode(butonDegetMic, INPUT_PULLUP);

  prefs.begin("manusa", false);
  modCurent = prefs.getInt("mod", MOD_MOUSE);

  semnalizeazaMod(modCurent);

  switch (modCurent) {
    case MOD_MOUSE:      setupModMouse(); break;
    case MOD_SERVER_WEB: setupModServerWeb(); break;
    case MOD_MODUL3:     setupModul3(); break;
  }
}

void loop() {
  verificaSchimbareMod();

  switch (modCurent) {
    case MOD_MOUSE:      loopModMouse(); break;
    case MOD_SERVER_WEB: loopModServerWeb(); break;
    case MOD_MODUL3:     loopModul3(); break;
  }
}
