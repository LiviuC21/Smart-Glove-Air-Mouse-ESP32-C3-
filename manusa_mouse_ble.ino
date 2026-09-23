#include <Wire.h>
#include <BleMouse.h>

BleMouse bleMouse("Manusa Stanga", "Liviu", 100);

const int pinSDA = 5;
const int pinSCL = 6;
const int MPU_ADDR = 0x68;

const int butonAratator = 4;
const int butonMijlociu = 3;
const int butonInelar = 1; // Ambreiaj / pauza + tinut apasat = recalibrare

// Pinul pentru LED-ul integrat
const int ledPin = 8;

float offsetGx = 0;
float offsetGy = 0;
float offsetGz = 0;

bool lastConnectedState = false;

// --- pentru recalibrare manuala (tinut apasat butonul inelar) ---
unsigned long inelarApasatDeLa = 0;
bool inelarTinutPentruRecalibrare = false;
const unsigned long TIMP_RECALIBRARE = 2000; // 2 secunde tinut apasat

// --- pentru auto-corectie continua a drift-ului ---
const int PRAG_STATIONAR = 250; // sub aceasta valoare (raw, dupa offset) consideram mana "aproape stationara"
const float VITEZA_ADAPTARE = 0.0008; // cat de repede se ajusteaza offsetul (mic = lent si sigur)

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
  // Semnalizam vizual ca incepe calibrarea: LED clipeste rapid
  long sumGx = 0, sumGy = 0, sumGz = 0;
  const int NR_SAMPLES = 300;

  for (int i = 0; i < NR_SAMPLES; i++) {
    digitalWrite(ledPin, (i / 5) % 2); // clipeste rapid cat calibreaza

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

  // LED se stinge/aprinde stabil dupa calibrare, in functie de starea conexiunii
  digitalWrite(ledPin, bleMouse.isConnected() ? LOW : HIGH);
}

void setup() {
  Serial.begin(115200);
  Wire.begin(pinSDA, pinSCL);

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission();

  pinMode(butonAratator, INPUT_PULLUP);
  pinMode(butonMijlociu, INPUT_PULLUP);
  pinMode(butonInelar, INPUT_PULLUP);

  bleMouse.begin();

  // Lasam 2.5s inainte sa inceapa efectiv calibrarea, ca sa apuci sa pui mana jos
  delay(2500);

  calibreaza();
}

void loop() {
  bool isConnected = bleMouse.isConnected();

  if (isConnected != lastConnectedState) {
    digitalWrite(ledPin, isConnected ? LOW : HIGH);
    lastConnectedState = isConnected;
  }

  // --- Recalibrare manuala: tine apasat butonul inelar 2 secunde ---
  if (digitalRead(butonInelar) == LOW) {
    if (inelarApasatDeLa == 0) {
      inelarApasatDeLa = millis();
    } else if (!inelarTinutPentruRecalibrare && (millis() - inelarApasatDeLa > TIMP_RECALIBRARE)) {
      inelarTinutPentruRecalibrare = true;
      calibreaza(); // recalibreaza pe loc (tine mana nemiscata cand faci asta!)
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

    // --- Auto-corectie continua a drift-ului termic ---
    // Daca semnalul e mic (mana aproape nemiscata), ajustam usor offsetul
    // ca sa "urmarim" bias-ul care se schimba cu temperatura cipului.
    if (abs(gx) < PRAG_STATIONAR) offsetGx += gx * VITEZA_ADAPTARE;
    if (abs(gy) < PRAG_STATIONAR) offsetGy += gy * VITEZA_ADAPTARE;
    if (abs(gz) < PRAG_STATIONAR) offsetGz += gz * VITEZA_ADAPTARE;

    // Zona moarta pentru miscarea efectiva a cursorului
    if (abs(gx) < 180) gx = 0;
    if (abs(gy) < 180) gy = 0;
    if (abs(gz) < 180) gz = 0;

    int divizorViteza = 500;

    int moveX = -gz / divizorViteza;
    int moveY = gx / divizorViteza;

    if (moveX != 0) {
      moveX = moveX * 1.3;
    }

    if (digitalRead(butonInelar) == HIGH) {
      if (moveX != 0 || moveY != 0) {
        bleMouse.move(moveX, moveY);
      }
    }

    // Click Stanga
    if (digitalRead(butonAratator) == LOW) {
      if (!bleMouse.isPressed(MOUSE_LEFT)) bleMouse.press(MOUSE_LEFT);
    } else {
      if (bleMouse.isPressed(MOUSE_LEFT)) bleMouse.release(MOUSE_LEFT);
    }

    // Click Dreapta
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
