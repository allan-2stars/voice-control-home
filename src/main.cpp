#include <Arduino.h>
#include <Preferences.h>

/*
    3-Color Light Control System
    - ASR Pulse Mode
    - High Trigger Relay
    - 4 Independent Voice Commands
    - Non-blocking state machine
    - Power-loss memory using NVS
*/

// ================= Pin Definitions =================
#define ASR_ON_PIN     4
#define ASR_OFF_PIN    5
#define ASR_WHITE_PIN  6
#define ASR_MIX_PIN    7
#define RELAY_PIN      18

// ================= Timing Configuration =================
const unsigned long debounceInterval = 50;
const unsigned long powerCycleDelay  = 400;

// ================= Global State =================
Preferences preferences;

bool lightState = false;     // true = ON, false = OFF
int  currentMode = 0;        // 0=Yellow, 1=White, 2=Mix

bool cycling = false;
int  cyclesRemaining = 0;

unsigned long cycleStartTime  = 0;
unsigned long lastTriggerTime = 0;

bool lastOnState    = LOW;
bool lastOffState   = LOW;
bool lastWhiteState = LOW;
bool lastMixState   = LOW;

// ================= Function: Start Power Cycle =================
void startPowerCycle() {
    digitalWrite(RELAY_PIN, LOW);   // Power OFF
    cycleStartTime = millis();
    cycling = true;
}

// ================= Setup =================
void setup() {
    Serial.begin(115200);

    pinMode(ASR_ON_PIN, INPUT);
    pinMode(ASR_OFF_PIN, INPUT);
    pinMode(ASR_WHITE_PIN, INPUT);
    pinMode(ASR_MIX_PIN, INPUT);

    pinMode(RELAY_PIN, OUTPUT);

    preferences.begin("light", false);

    lightState  = preferences.getBool("power", false);
    currentMode = preferences.getInt("mode", 0);

    if (lightState) {
        digitalWrite(RELAY_PIN, HIGH);
        Serial.println("Restored: Light ON");
    } else {
        digitalWrite(RELAY_PIN, LOW);
        Serial.println("Restored: Light OFF");
    }

    Serial.print("Restored Mode: ");
    Serial.println(currentMode);
}

// ================= Main Loop =================
void loop() {
    unsigned long now = millis();

    // ===== Handle Power Cycle State Machine =====
    if (cycling) {
        if (now - cycleStartTime >= powerCycleDelay) {
            digitalWrite(RELAY_PIN, HIGH);

            currentMode = (currentMode + 1) % 3;
            preferences.putInt("mode", currentMode);

            cyclesRemaining--;

            if (cyclesRemaining > 0) {
                startPowerCycle();
            } else {
                cycling = false;
                Serial.print("Mode Updated To: ");
                Serial.println(currentMode);
            }
        }
        return;
    }

    // ===== Read Inputs =====
    bool onState    = digitalRead(ASR_ON_PIN);
    bool offState   = digitalRead(ASR_OFF_PIN);
    bool whiteState = digitalRead(ASR_WHITE_PIN);
    bool mixState   = digitalRead(ASR_MIX_PIN);

    // ===== ON (Yellow Default) =====
    if (onState == HIGH && lastOnState == LOW) {
        if (now - lastTriggerTime > debounceInterval) {
            lightState = true;
            currentMode = 0;

            digitalWrite(RELAY_PIN, HIGH);

            preferences.putBool("power", true);
            preferences.putInt("mode", currentMode);

            Serial.println("Light ON (Yellow)");
            lastTriggerTime = now;
        }
    }

    // ===== OFF =====
    if (offState == HIGH && lastOffState == LOW) {
        if (now - lastTriggerTime > debounceInterval) {
            lightState = false;
            digitalWrite(RELAY_PIN, LOW);

            preferences.putBool("power", false);

            Serial.println("Light OFF");
            lastTriggerTime = now;
        }
    }

    if (!lightState) {
        lastOnState    = onState;
        lastOffState   = offState;
        lastWhiteState = whiteState;
        lastMixState   = mixState;
        return;
    }

    // ===== WHITE Mode =====
    if (whiteState == HIGH && lastWhiteState == LOW) {
        if (now - lastTriggerTime > debounceInterval) {
            int diff = (1 - currentMode + 3) % 3;

            if (diff > 0) {
                cyclesRemaining = diff;
                startPowerCycle();
            }

            lastTriggerTime = now;
        }
    }

    // ===== MIX Mode =====
    if (mixState == HIGH && lastMixState == LOW) {
        if (now - lastTriggerTime > debounceInterval) {
            int diff = (2 - currentMode + 3) % 3;

            if (diff > 0) {
                cyclesRemaining = diff;
                startPowerCycle();
            }

            lastTriggerTime = now;
        }
    }

    lastOnState    = onState;
    lastOffState   = offState;
    lastWhiteState = whiteState;
    lastMixState   = mixState;
}
