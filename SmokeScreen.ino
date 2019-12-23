#include "EspMQTTClient.h"
#include "secrets.h"

//Pins
#define PITPIN 32
#define MEAT1PIN 33

// resistor definition for ET733 probes - maybe don't need to use a 1M resistor for ET733?
// #define PITRESISTOR 992500 

// resistor definition for 10k probes like Thermoworks
//#define PITRESISTOR 9870
#define PITRESISTOR 14750
#define MEATRESISTOR1 14890

#define MQTT_TOPIC "smokescreen"
#define OVERSAMPLE 8

// interval between control loops in milliseconds
#define CTRL_INTERVAL 500

// interval between automatic temperature updates via MQTT in milliseconds
#define REPORT_INTERVAL 10000

// Temp equation definitions for ET733 - hand calibrated
#define A_ET733 -0.0034058161844664142
#define B_ET733 0.0005973859341993963
#define C_ET733 -5.875837340570243e-7

// Temp equation definitions for Thermoworks probes - taken from HeaterMeter by CapnBry
//#define A_THERM 7.3431401e-4
//#define B_THERM 2.1574370e-4
//#define C_THERM 9.5156860e-8

// Temp equations for Thermoworks probes, hand calibrated
#define A_THERM 0.00047630463092729856
#define B_THERM 0.00024773576511516335
#define C_THERM -6.407966149699128e-9

// Network info setup
// Array of resistor pins; array is indexed -32, so pin 32 is array entry 0 (default is pit pin on 32), pin 33 is entry 1, etc.
const int pinresistor[2] = {PITRESISTOR, MEATRESISTOR1};

const char* ssid = WIFI_SSID;
const char* password = WIFI_PW;
const char* mqtt_server = MQTT_IP;
const char* mqtt_username = MQTT_USER;
const char* mqtt_password = MQTT_PW;
const char* mqtt_clientid = "smokescreen";
const short mqtt_port = 1883;

EspMQTTClient client(
    ssid,
    password,
    mqtt_server,
    mqtt_username,
    mqtt_password,
    mqtt_clientid,
    mqtt_port
);

unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;
unsigned long previousMillis = 0;
unsigned long reportMillis = 0;
const long interval = CTRL_INTERVAL;
const long report_interval = REPORT_INTERVAL;
float setpoint = 100.0;
float pid_p = 4;
float pid_i = 0.01;
float pid_d = 0.0;

void cmd_callback(const String &payload) {
  String tvar, out;
  Serial.print("Message arrived on SmokeScreen/commands ");
  Serial.print(payload);
  Serial.println();
  tvar = payload;
  tvar.remove(0, 8);
  if (payload.startsWith("settemp ")) {
    Serial.print("Setting temperature to ");
    Serial.println(payload);
    setpoint = tvar.toFloat(); // strip settemp command, leaving only temperature
  } else if (payload.startsWith("setpidP ")) {
    Serial.print("Setting PID P value to ");
    Serial.println(payload);
    pid_p = tvar.toFloat(); // strip setpidP command, leaving only P value
  } else if (payload.startsWith("setpidI ")) {
    Serial.print("Setting PID P value to ");
    Serial.println(payload);
    pid_i = tvar.toFloat(); // strip setpidI command, leaving only I value
  } else if (payload.startsWith("setpidD ")) {
    Serial.print("Setting PID P value to ");
    Serial.println(payload);
    pid_d = tvar.toFloat(); // strip setpidD command, leaving only D value
  } else if (payload.startsWith("getpidP ")) {
    Serial.print("PID P value is ");
    Serial.println(pid_p);
    out = "pidPval " + String(pid_p,4);
    client.publish("SmokeScreen/data", out);
  } else if (payload.startsWith("getpidI ")) {
    Serial.print("PID I value is ");
    Serial.println(pid_i);
    out = "pidIval " + String(pid_i,4);
    client.publish("SmokeScreen/data", out);
  } else if (payload.startsWith("getpidD ")) {
    Serial.print("PID D value is ");
    Serial.println(pid_d);
    out = "pidDval " + String(pid_d,4);
    client.publish("SmokeScreen/data", out);
  }
}

void cmd_callback_pit(const String &payload) {
  Serial.print("Message arrived on SmokeScreen/commands/pit ");
  Serial.print(payload);
  Serial.println();
  if (payload.startsWith("gettemp")) {
    Serial.print("Getting pit temperature: ");
    Serial.println(payload);
    client.publish("SmokeScreen/data/pit", "temp " + String(get_temp(PITPIN),3));
  }
}

void cmd_callback_meat1(const String &payload) {
  Serial.print("Message arrived on SmokeScreen/commands/meat1 ");
  Serial.print(payload);
  Serial.println();
  if (payload.startsWith("gettemp ")) {
    Serial.print("Getting meat probe 1 temperature: ");
    Serial.println(payload);
    client.publish("SmokeScreen/data/meat1", "temp " + String(get_temp(MEAT1PIN),3));
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  client.enableDebuggingMessages();
  previousMillis = millis();
}


float adc2temp(int adcval) {

  float R, ln, TInv, temp;

  // coefficients for temp calc, ET733 probe
//  float A = A_ET733;
//  float B = B_ET733;
//  float C = C_ET733;

  // coefficients for temp calc, Thermoworks probe
  float A = A_THERM;
  float B = B_THERM;
  float C = C_THERM;

  R = PITRESISTOR * (4095.0 / adcval - 1);

  // temp calc
  ln = log(R);
  TInv = A + B*ln + C*pow(ln, 3);
  temp = 1.8*(1.0/TInv - 273.15) + 32.0;

  return temp;
}

void onConnectionEstablished() {
  // client.subscribe("SmokeScreen/commands", [](const String & payload) { cmd_callback(payload); });
  client.subscribe("SmokeScreen/commands", cmd_callback);
  client.subscribe("SmokeScreen/commands/pit", cmd_callback_pit);
  client.subscribe("SmokeScreen/commands/meat1", cmd_callback_meat1);
  client.publish("SmokeScreen/data", "smokescreen connected");
}

void loop() {
  unsigned long currentMillis = millis();
  
  while (currentMillis - previousMillis <= CTRL_INTERVAL) 
    {
      client.loop();
      currentMillis = millis();
    }
  previousMillis = currentMillis;

  // temp measurement code
  int pitadc, meatadc;
  float pittemp, meat1temp;
  char tempstring[20];

  pittemp = get_temp(PITPIN);
  meat1temp = get_temp(MEAT1PIN);

  if (currentMillis - reportMillis >= REPORT_INTERVAL) {
    Serial.println("REPORTING");
    reportMillis = currentMillis;
    client.publish("SmokeScreen/status/pittemp", String(pittemp,3));
    client.publish("SmokeScreen/status/meat1temp", String(meat1temp,3));
  }
//  delay(1000);
}

int getADC(int pinnum) {

  double pinSamples[OVERSAMPLE];
  double average;
  int i;

  // Collect oversampled number of samples
  for (i=0; i<OVERSAMPLE; i++) {
    pinSamples[i] = analogRead(pinnum);
    delay(10);
  }

// sum the raw oversampled samples
  average = 0;
  for (i=0; i<OVERSAMPLE; i++){
    average += pinSamples[i];
  }
  
// divide by number of oversamples to obtain higher accuracy result
  average /= OVERSAMPLE;
  return average;
}

float get_temp(int pinnum) {
  
  int pinadc;
  double pinvolt;
  float R, pintemp;
  
  // get oversampled ADC
  pinadc = getADC(pinnum);

  // temp calc
  pintemp = adc2temp(pinadc);

  // estimate raw voltage at ADC - only used for debug
  pinvolt = pinadc*3.3;
  pinvolt = pinvolt / 4096.0;
  // convert ADC output to resistance estimate - only needed for debug
  R = pinresistor[pinnum - 32] * (4095.0 / pinadc - 1);
  // serial output of ADC for debug
//  Serial.print("Pin ");
//  Serial.print(pinnum);
//  Serial.print(" ADC val: ");
//  Serial.println(pinadc);
//  Serial.print("Pin ");
//  Serial.print(pinnum);
//  Serial.print(" ADC voltage: ");
//  Serial.println(pinvolt);
//  Serial.print("Pin ");
//  Serial.print(pinnum);
//  Serial.print(" thermistor ohms: ");
//  Serial.println(R);
//  Serial.print("Pin ");
//  Serial.print(pinnum);
//  Serial.print ("estimated temp: ");
//  Serial.print(pintemp);
//  Serial.println(" F");
  return pintemp;
}
