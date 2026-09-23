# Smart Glove Air-Mouse (ESP32-C3) 

Acest proiect transformă o mănușă obișnuită într-un mouse Bluetooth (Air-Mouse) folosind un microcontroller ESP32-C3 și un giroscop MPU6050. Nu necesită o suprafață plană — controlezi cursorul prin mișcarea încheieturii, direct din aer. Am ales să nu includ un demo video pentru a lăsa bucuria descoperirii celor care vor replica proiectul.

## Funcționalități
* **Air-Mouse:** Control fluid al cursorului folosind viteza de rotație a mâinii.
* **Click Stânga & Dreapta:** Butoane dedicate pe degetul arătător și mijlociu.
* **Recalibrare anti-Drift (Ambreiaj):** Buton dedicat pe degetul inelar.

### Ce este "Drift-ul" și cum îl rezolvă butonul inelar?
În lumea senzorilor inerțiali (cum este giroscopul MPU6050), "drift-ul" este un fenomen inerent. Pe măsură ce senzorul funcționează și se încălzește ușor, sau pur și simplu din cauza acumulării unor mici erori matematice în timp, centrul său "zero" se deplasează. Rezultatul vizual este că mouse-ul începe să alunece singur pe ecran într-o direcție, chiar dacă tu ții mâna perfect nemișcată. 

Pentru a combate acest lucru fără a fi nevoie să restartezi complet placa de la zero, butonul de pe degetul inelar acționează ca un sistem de recalibrare on-the-fly. La apăsare, sistemul citește rapid valorile actuale, ignoră vechiul centru de referință și setează un nou punct de repaus,eliminând instant deviația (preferabil când apăsați acel buton de recalibrare ,să țineți mână într-o poziție comodă din care veți dori sa o utilizați deoarece se va alege noul centru ). Este exact echivalentul ridicării unui mouse optic de pe mousepad pentru a-l re-centra atunci când ai rămas fără spațiu pe birou.

## Componente Necesare
* 1x Placă de dezvoltare ESP32-C3 SuperMini (HW-466AB)
* 1x Senzor Giroscop/Accelerometru MPU-6050
* 1x Modul încărcare TP4056 (Type-C)
* 1x Baterie Li-Po / Li-Ion (marcată BAT)
* 4x Butoane push (tactile) pentru degete (buton1, buton2, buton3, buton4)
* 1x Buton ON/OFF cu reținere
* 1x Modul Micro SD Card Mini TF Card Reader Module SPI (integrat pentru viitoare update-uri)
* Fire de conexiune și o mănușă confortabilă textilă.
### Atenție ,dacă doriți să vă faceti o pereche (adică și pe cealaltă mână) ,atunci trebuie sa dublați cantitatea pieselor de mai sus!

## Schema de Conectare
![Schemă mănușă](schita_manusa.png)

Sistemul este gândit pentru eficiență: bateria se conectează la pinii B+ și B- ai modulului TP4056. Tensiunea pleacă din OUT+ prin butonul de ON/OFF direct în pinul de 5V al ESP-ului, care reglează mai departe curentul optim. Senzorul MPU-6050 și modulul SD Card sunt alimentate la tensiunea corectă din pinul de 3.3V al ESP-ului. Butoanele folosesc logica internă de `INPUT_PULLUP` a plăcii, având un capăt conectat la pinii digitali alocați și celălalt capăt închis la o linie comună spre pinul G (Ground).

## Instalare și Rulare
1. Instalează **Arduino IDE**.
2. Adaugă pachetul pentru plăci ESP32 din *Boards Manager* (Recomandat: folosește versiunea stabilă **2.0.17** pentru a evita conflictele la inițializarea funcțiilor Bluetooth).
3. Instalează biblioteca [ESP32-BLE-Mouse](https://github.com/T-vK/ESP32-BLE-Mouse) ca arhivă .ZIP.
4. Selectează placa **ESP32C3 Dev Module**, activează **USB CDC On Boot: Enabled** și **Flash Mode: DIO**.
5. Încarcă codul `manusa_mouse_ble.ino`.
6. Asociază „Manusa Stanga” din setările Bluetooth ale PC-ului tău. LED-ul albastru de pe placă (pinul 8) se va aprinde și va rămâne aprins când conexiunea este stabilă.

## Troubleshooting: Eroare de compilare BleMouse.cpp
În funcție de versiunea pachetului ESP32 instalat, este posibil să primești o eroare de compilare legată de funcția `BLEDevice::init` sau `setValue`. Aceasta apare din cauza unei incompatibilități în modul în care sunt citite string-urile de text.

Pentru a repara rapid această eroare:
1. Mergi în folderul unde Arduino salvează bibliotecile (ex: `C:\Users\[Nume_Utilizator]\Documents\Arduino\libraries\ESP32_BLE_Mouse\`).
2. Deschide fișierul `BleMouse.cpp` cu Notepad sau orice alt editor de text.
3. **În jurul liniei 143**, caută linia:
   `BLEDevice::init(bleMouseInstance->deviceName);`
   Modific-o adăugând `.c_str()` la final, astfel:
   `BLEDevice::init(bleMouseInstance->deviceName.c_str());`
4. **În jurul liniei 151**, caută linia:
   `bleMouseInstance->hid->manufacturer()->setValue(bleMouseInstance->deviceManufacturer);`
   Modific-o exact așa:
   `bleMouseInstance->hid->manufacturer()->setValue((uint8_t*)bleMouseInstance->deviceManufacturer.c_str(), bleMouseInstance->deviceManufacturer.length());`
5. Salvează fișierul (`Ctrl+S`), închide Notepad și recompilează codul în Arduino IDE. Eroarea va dispărea!

## 🎨 Design & Ergonomie
Modul în care asamblați mănușa (aspectul fizic, alegerea materialului și așezarea componentelor pe mână) rămâne strict la latitudinea fiecăruia! 

Nu există o regulă fixă. Puteți coase componentele pe o mănușă textilă subțire, le puteți prinde cu benzi velcro (scai) pentru a le da jos ușor, sau chiar puteți printa 3D niște suporți. Singurele recomandări practice sunt:
* **Fixați senzorul MPU-6050 ferm:** Acesta nu trebuie să se miște independent de mână, altfel cursorul va tremura.
* **Poziția butoanelor:** Deoarece butoanele vor fi apăsate folosind **degetul mare**, fixați-le pe laterala sau pe buricul celorlalte degete (arătător, mijlociu, inelar ,degetul mic) exact în locurile în care degetul mare ajunge cel mai natural. Testați mișcarea înainte de a le lipi definitiv, pentru a vă asigura că nu forțați încheietura.

Fiți creativi și adaptați proiectul exact pe dimensiunea și confortul vostru!

## Future Updates (În lucru)
Hardware-ul conține deja cititorul MicroSD. În viitorul apropiat, firmware-ul va primi multiple "moduri" de operare (selecția făcându-se prin apăsarea unei combinații de degete la pornire):
* **Modul Mouse & Tastatură:** Funcționalitatea actuală extinsă.
* **Modul Web Server:** Găzduirea unei pagini locale și streaming de fișiere/muzică găzduite pe cardul SD.
* **Modul Deauther:** Un modul cu scop educativ pentru testarea și analiza rețelelor WiFi.

## Galerie Foto
<img width="455" height="526" alt="schita manusa" src="https://github.com/user-attachments/assets/c5f3fb49-c250-4c35-8a8e-6d68ce91ff49" />
<img width="1200" height="1600" alt="p2" src="https://github.com/user-attachments/assets/cfcf953c-24a9-44f7-b5d9-2e96a98588e8" />
<img width="1200" height="1600" alt="p1" src="https://github.com/user-attachments/assets/08919e4f-a507-49ac-9e1a-4278df24be28" />
