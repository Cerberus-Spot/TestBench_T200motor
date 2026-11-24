/*
  Propulseur T200 + HC-SR04 — enregistrement de distance
  Arduino Uno + Blue Robotics Basic ESC

  START : arme le moteur + capteur, envoie les mesures en CSV sur le port série
  STOP  : coupe tout immédiatement (interruption)
  Le moteur s'éloigne du capteur : d1 = 1re mesure valide après START, arrêt à d2 = d1 + 50 cm.

  Branchement :
    ESC signal (fil blanc) -> D9     ESC GND (fil noir) -> GND Arduino
    HC-SR04 VCC -> 5V   GND -> GND   Trig -> D7   Echo -> D8
    Bouton START : D2 <-> GND        Bouton STOP : D3 <-> GND
    (pull-up internes, pas besoin de résistances)
*/

#include <Servo.h>

// ===== Broches =====
const uint8_t PIN_ESC   = 9;
const uint8_t PIN_TRIG  = 7;
const uint8_t PIN_ECHO  = 8;
const uint8_t PIN_START = 2;
const uint8_t PIN_STOP  = 3;   // D3 = interruption INT1 sur le Uno

// ===== Réglages =====
const float    ECART_CM             = 70.0;  // d2 = d1 + ECART_CM (d1 = 1re mesure valide après START)
const int      PWM_NEUTRE           = 1500;  // µs, moteur arrêté
const int      PWM_MARCHE           = 1650;  // µs, 1500-1900 = avant, 1100-1500 = arrière
const uint16_t PERIODE_MS           = 10;    // délai min. entre 2 mesures du HC-SR04
const uint8_t  LECTURES_CONFIRM     = 3;     // nb de lectures consécutives pour confirmer B
const unsigned long TIMEOUT_ECHO_US = 30000UL; // ~5 m max

Servo esc;
volatile bool stopDemande = false;
bool enMarche = false;
unsigned long t0 = 0;
unsigned long derniereMesure = 0;
uint8_t compteurCible = 0;
float d1 = -1.0;  // fixée à la 1re mesure valide après START
float d2 = -1.0;  // d1 + ECART_CM

void isrStop() {
  stopDemande = true;
}

float lireDistanceCm() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  unsigned long duree = pulseIn(PIN_ECHO, HIGH, TIMEOUT_ECHO_US);
  if (duree == 0) return -1.0;   // pas d'écho
  return duree / 58.0;
}

void arreter(const char* raison) {
  esc.writeMicroseconds(PWM_NEUTRE);
  if (enMarche) {
    enMarche = false;
    Serial.print(F("#STOP,"));
    Serial.println(raison);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_START, INPUT_PULLUP);
  pinMode(PIN_STOP, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_STOP), isrStop, FALLING);

  // Armement de l'ESC : il doit recevoir le signal neutre quelques secondes
  esc.attach(PIN_ESC);
  esc.writeMicroseconds(PWM_NEUTRE);
  Serial.println(F("#INFO,Armement de l'ESC..."));
  delay(7000);
  stopDemande = false;
  Serial.println(F("#PRET"));
}

void loop() {
  // --- STOP (priorité absolue) ---
  if (stopDemande) {
    stopDemande = false;
    arreter("bouton");
  }

  // --- START ---
  if (!enMarche && digitalRead(PIN_START) == LOW && digitalRead(PIN_STOP) == HIGH) {
    delay(30);  // anti-rebond
    if (digitalRead(PIN_START) == LOW) {
      stopDemande = false;
      enMarche = true;
      compteurCible = 0;
      d1 = -1.0;
      d2 = -1.0;
      t0 = millis();
      derniereMesure = 0;
      Serial.println(F("#START"));
      Serial.println(F("temps_ms,distance_cm,pwm_us"));
      esc.writeMicroseconds(PWM_MARCHE);
    }
  }

  // --- Mesure + enregistrement ---
  if (enMarche && millis() - derniereMesure >= PERIODE_MS) {
    derniereMesure = millis();
    float d = lireDistanceCm();
    if (stopDemande) return;  // STOP pendant la mesure : traité au prochain tour

    Serial.print(millis() - t0);
    Serial.print(',');
    if (d < 0) Serial.print(F("NaN"));
    else       Serial.print(d, 1);
    Serial.print(',');
    Serial.println(PWM_MARCHE);

    if (d1 < 0 && d > 0) {   // 1re mesure valide = d1
      d1 = d;
      d2 = d1 + ECART_CM;
      Serial.print(F("#INFO,d1="));
      Serial.print(d1, 1);
      Serial.print(F(",d2="));
      Serial.println(d2, 1);
    }

    bool atteinte = (d1 > 0) && (d > 0) && (d >= d2);
    compteurCible = atteinte ? compteurCible + 1 : 0;
    if (compteurCible >= LECTURES_CONFIRM) {
      arreter("distance_d2");
    }
  }
}
