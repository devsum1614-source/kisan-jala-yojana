/*
 * Kisan Jala Yojana (किसान जल योजना)
 * IoT-Based Smart Borewell Motor Remote Controller
 * 
 * Hardware:
 *  - Microcontroller: ESP32 DevKit v1
 *  - Cellular Modem: SIMCom A7670C-LASS (4G Cat-1 with Audio Codec)
 *  - Switching: 5V Optocoupler Relay Module
 *  - Current Sensor: SCT-013 (Dry-Run Protection)
 * 
 * Department of Computer Science and Engineering
 * Karavali Institute of Technology, Mangalore
 * Academic Year: 2025-26
 */

#include <Arduino.h>
#include <HardwareSerial.h>
#include <Preferences.h>

// Pin Definitions
#define MODEM_TX_PIN   16    // ESP32 RX2 <- Modem TX
#define MODEM_RX_PIN   17    // ESP32 TX2 -> Modem RX
#define MODEM_RESET_PIN 4    // Modem Reset
#define MODEM_PWRKEY_PIN 5   // Modem Power Key
#define RELAY_PIN      25    // Motor Starter Optocoupler Relay Trigger
#define CT_SENSOR_PIN  34    // ADC pin for SCT-013 current transformer

// Communication Baud Rates
#define DEBUG_BAUD     115200
#define MODEM_BAUD     115200

// Serial Interface to Modem
HardwareSerial SerialAT(2);
Preferences preferences;

// State Variables
bool isMotorRunning = false;
unsigned long motorStartTime = 0;
unsigned long motorDurationMs = 0; // 0 = continuous until stopped

// Whitelisted phone numbers (Can be updated dynamically via NVS)
const char* defaultAdminNumber = "+919876543210";

// Function Declarations
void sendATCommand(const String &cmd, unsigned long timeoutMs = 2000);
String readModemResponse(unsigned long timeoutMs = 2000);
bool isCallerWhitelisted(const String &callerNumber);
void startMotor(unsigned long runDurationMinutes = 0);
void stopMotor();
void checkDryRunCondition();
void handleDTMFCommand(char digit, const String &callerNumber);
void sendSMSConfirmation(const String &phoneNumber, const String &message);

void setup() {
  Serial.begin(DEBUG_BAUD);
  delay(1000);
  Serial.println(F("\n=========================================="));
  Serial.println(F("   KISAN JALA YOJANA FIELD CONTROLLER     "));
  Serial.println(F("   Department of CSE, KIT Mangalore       "));
  Serial.println(F("=========================================="));

  // Initialize GPIOs
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Relay OFF by default

  pinMode(MODEM_RESET_PIN, OUTPUT);
  pinMode(MODEM_PWRKEY_PIN, OUTPUT);
  digitalWrite(MODEM_RESET_PIN, HIGH);
  digitalWrite(MODEM_PWRKEY_PIN, HIGH);

  // Initialize Preferences (NVS storage for whitelisting)
  preferences.begin("kisan_jala", false);
  if (!preferences.isKey("admin_phone")) {
    preferences.putString("admin_phone", defaultAdminNumber);
    Serial.println(F("[NVS] Stored default admin phone number"));
  }

  // Initialize UART to SIMCom A7670C-LASS Modem
  SerialAT.begin(MODEM_BAUD, SERIAL_8N1, MODEM_TX_PIN, MODEM_RX_PIN);
  delay(2000);

  Serial.println(F("[Modem] Initializing AT commands..."));
  sendATCommand("AT");                        // Basic sync
  sendATCommand("ATE0");                      // Echo OFF
  sendATCommand("AT+CLIP=1");                 // Enable Caller ID display
  sendATCommand("AT+DDET=1");                 // Enable DTMF tone detection
  sendATCommand("AT+CMGF=1");                 // Set SMS text mode
  sendATCommand("AT+CSCS=\"GSM\"");           // Standard character set

  Serial.println(F("[System] Ready and waiting for incoming calls/commands."));
}

void loop() {
  // Check for auto-timed shutoff
  if (isMotorRunning && motorDurationMs > 0) {
    if (millis() - motorStartTime >= motorDurationMs) {
      Serial.println(F("[Timer] Run duration elapsed. Auto stopping motor."));
      stopMotor();
      String admin = preferences.getString("admin_phone", defaultAdminNumber);
      sendSMSConfirmation(admin, "Kisan Jala Yojana: Scheduled irrigation session complete. Motor turned OFF.");
    }
  }

  // Monitor Dry-Run Current Protection if motor is ON
  if (isMotorRunning) {
    checkDryRunCondition();
  }

  // Read incoming unsolicited result codes (URC) from modem
  if (SerialAT.available()) {
    String response = SerialAT.readStringUntil('\n');
    response.trim();

    if (response.length() > 0) {
      Serial.print(F("[Modem Event] "));
      Serial.println(response);

      // Handle Incoming Call
      if (response.indexOf("RING") >= 0 || response.indexOf("+CLIP:") >= 0) {
        // Extract caller number
        int clipIndex = response.indexOf("+CLIP:");
        if (clipIndex >= 0) {
          int firstQuote = response.indexOf("\"", clipIndex);
          int secondQuote = response.indexOf("\"", firstQuote + 1);
          if (firstQuote != -1 && secondQuote != -1) {
            String incomingNumber = response.substring(firstQuote + 1, secondQuote);
            Serial.print(F("[Incoming Call] Caller ID: "));
            Serial.println(incomingNumber);

            if (isCallerWhitelisted(incomingNumber)) {
              Serial.println(F("[Security] Caller verified. Answering call..."));
              sendATCommand("ATA"); // Answer call
            } else {
              Serial.println(F("[Security] Unauthorized caller! Rejecting..."));
              sendATCommand("ATH"); // Hang up
            }
          }
        }
      }

      // Handle DTMF Keypad Tones (+DTMF: <digit>)
      if (response.indexOf("+DTMF:") >= 0) {
        int dtmfIndex = response.indexOf("+DTMF:");
        char digit = response.charAt(dtmfIndex + 7);
        Serial.print(F("[DTMF Tone Detected]: Key '"));
        Serial.print(digit);
        Serial.println(F("'"));
        
        String admin = preferences.getString("admin_phone", defaultAdminNumber);
        handleDTMFCommand(digit, admin);
      }
    }
  }
}

void handleDTMFCommand(char digit, const String &callerNumber) {
  switch (digit) {
    case '1':
      Serial.println(F("[Command] Start Motor"));
      startMotor(0);
      sendSMSConfirmation(callerNumber, "Kisan Jala Yojana: Motor turned ON successfully.");
      break;

    case '0':
      Serial.println(F("[Command] Stop Motor"));
      stopMotor();
      sendSMSConfirmation(callerNumber, "Kisan Jala Yojana: Motor turned OFF successfully.");
      break;

    case '2':
      Serial.println(F("[Command] Timer Mode - 30 Minutes"));
      startMotor(30);
      sendSMSConfirmation(callerNumber, "Kisan Jala Yojana: Motor turned ON for 30 Minutes.");
      break;

    case '3':
      Serial.println(F("[Command] Timer Mode - 60 Minutes"));
      startMotor(60);
      sendSMSConfirmation(callerNumber, "Kisan Jala Yojana: Motor turned ON for 60 Minutes.");
      break;

    case '9':
      Serial.println(F("[Command] Query Status"));
      {
        String statusMsg = "Kisan Jala Yojana Status: ";
        statusMsg += isMotorRunning ? "Motor is RUNNING." : "Motor is STOPPED.";
        sendSMSConfirmation(callerNumber, statusMsg);
      }
      break;

    case '#':
      Serial.println(F("[Command] Hang Up Call"));
      sendATCommand("ATH");
      break;

    default:
      Serial.print(F("[Command] Unhandled key: "));
      Serial.println(digit);
      break;
  }
}

void startMotor(unsigned long runDurationMinutes) {
  digitalWrite(RELAY_PIN, HIGH);
  isMotorRunning = true;
  motorStartTime = millis();
  motorDurationMs = runDurationMinutes * 60UL * 1000UL;
  Serial.print(F("[Motor Action] Relay energized. Mode: "));
  if (runDurationMinutes > 0) {
    Serial.print(runDurationMinutes);
    Serial.println(F(" min timer."));
  } else {
    Serial.println(F("Continuous until manual stop."));
  }
}

void stopMotor() {
  digitalWrite(RELAY_PIN, LOW);
  isMotorRunning = false;
  motorDurationMs = 0;
  Serial.println(F("[Motor Action] Relay de-energized. Motor STOPPED."));
}

void checkDryRunCondition() {
  // Read AC Current Sensor (SCT-013 via burden resistor and offset circuit)
  int rawADC = analogRead(CT_SENSOR_PIN);
  float voltage = (rawADC / 4095.0) * 3.3;
  // If sensor detects current below threshold while pump is energized:
  // Under typical 5HP load, normal current is ~7-10A; dry-run drops to < 3A.
  static unsigned long underCurrentStart = 0;
  bool isUnderCurrent = (voltage < 0.2); // threshold logic calibrated for burden circuit

  if (isUnderCurrent) {
    if (underCurrentStart == 0) {
      underCurrentStart = millis();
    } else if (millis() - underCurrentStart > 5000) { // Sustained for 5 seconds
      Serial.println(F("[ALERT] DRY-RUN DETECTED! Tripping motor immediately to prevent burnout!"));
      stopMotor();
      underCurrentStart = 0;
      String admin = preferences.getString("admin_phone", defaultAdminNumber);
      sendSMSConfirmation(admin, "CRITICAL ALERT: Kisan Jala Yojana shut OFF motor. Dry-run detected (no water flow)!");
    }
  } else {
    underCurrentStart = 0;
  }
}

bool isCallerWhitelisted(const String &callerNumber) {
  String admin = preferences.getString("admin_phone", defaultAdminNumber);
  // Match last 10 digits to accommodate country codes (+91 / 0)
  if (callerNumber.length() >= 10 && admin.length() >= 10) {
    String subIncoming = callerNumber.substring(callerNumber.length() - 10);
    String subAdmin = admin.substring(admin.length() - 10);
    return subIncoming.equals(subAdmin);
  }
  return false;
}

void sendATCommand(const String &cmd, unsigned long timeoutMs) {
  Serial.print(F("[AT TX] "));
  Serial.println(cmd);
  SerialAT.println(cmd);
  String resp = readModemResponse(timeoutMs);
  if (resp.length() > 0) {
    Serial.print(F("[AT RX] "));
    Serial.println(resp);
  }
}

String readModemResponse(unsigned long timeoutMs) {
  unsigned long start = millis();
  String resp = "";
  while (millis() - start < timeoutMs) {
    while (SerialAT.available()) {
      char c = SerialAT.read();
      resp += c;
    }
  }
  resp.trim();
  return resp;
}

void sendSMSConfirmation(const String &phoneNumber, const String &message) {
  Serial.print(F("[SMS Sending to "));
  Serial.print(phoneNumber);
  Serial.println(F("]..."));

  SerialAT.print("AT+CMGS=\"");
  SerialAT.print(phoneNumber);
  SerialAT.println("\"");
  delay(200);
  SerialAT.print(message);
  delay(100);
  SerialAT.write(26); // ASCII 26 = Ctrl+Z to send SMS
  delay(2000);
  Serial.println(F("[SMS] Dispatched."));
}
