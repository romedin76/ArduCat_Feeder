#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include "ESP8266TimerInterrupt.h"
#include <TimeLib.h>
#include <WidgetRTC.h>
#include <Servo.h>
#include <ArduinoOTA.h>

// PINOUTS
#define BUTTON_START D3
#define LED_RED D4
#define SERVO_MIX D0
#define RELAY_DISPENSE D1
#define SENSOR_FEED_LEVEL D2
#define SENSOR_TRIGGER D5 
#define SENSOR_ECHO D6
#define GATEWWAY_PIN D7

#define BLYNK_VIRTUAL_PIN_CLOCK V5

#define BLYNK_VIRTUAL_PIN_BUTTON V0
#define BLYNK_VIRTUAL_PIN_TEXT V1
#define BLYNK_VIRTUAL_PIN_LED_RED V2
#define BLYNK_VIRTUAL_PIN_LED_GREEN V3
#define BLYNK_VIRTUAL_PIN_TEXT_REMAINING V4

#define TIMER_INTERVAL_MICROSEGUNDOS 60 * 1000L * 1000L
#define TIMER_REMAINING_MILISEGUNDOS 15 * 1000L
#define BLYNK_PRINT Serial

// SOME CONSTANTS
float distance;  // distancia entre sensor y cuenco comida
const int FEEDER_FULL_DISTANCE = 12;
volatile boolean trigger = false;


WidgetLED BLYNK_LED_GREEN(BLYNK_VIRTUAL_PIN_LED_GREEN);
WidgetLED BLYNK_LED_RED(BLYNK_VIRTUAL_PIN_LED_RED);
WidgetTerminal Terminal(BLYNK_VIRTUAL_PIN_TEXT);

char auth[] = "qaEPRS2EDFgHXAM47Qaggf6vaOziBiwH";
char ssid[] = "Charkito";
char pass[] = "********";

// Init ESP8266 timer 0
ESP8266Timer ESPtimer;
BlynkTimer timer;
WidgetRTC rtc;
Servo miniservo;


BLYNK_CONNECTED() {
  // Synchronize time on connection
  rtc.begin();
}

BLYNK_WRITE(BLYNK_VIRTUAL_PIN_BUTTON) {
  int state = param.asInt();
    Serial.println("Boton APP pulsado");
    //estaria bien que si esta funcionando el dispensador, el boton esté deshabilitado
  //TODO poner el pin D7 a LOW cuando se pulse el boton de la app
  // D7  que estará conectado al pin BUTTON_START( D3 )
  if (state) {
     Serial.println("Boton APP HIGH");
   digitalWrite(GATEWWAY_PIN, HIGH); //trigger fisical interrupt
    digitalWrite(GATEWWAY_PIN, LOW);
  } else{
    Serial.println("Boton APP LOW");
    //digitalWrite(GATEWWAY_PIN, LOW);
  }
}
void secondsToHMS( const uint32_t seconds, uint16_t &h, uint8_t &m, uint8_t &s ){
    uint32_t t = seconds;
    s = t % 60;
    t = (t - s)/60;
    m = t % 60;
    t = (t - m)/60;
    h = t;
}
void reminingTime(){
  uint16_t hours;
  uint8_t minutes, seconds;
  //secondsToHMS(now , hours, minutes, seconds);
  String remainingTime = String(hours) + ":" + minutes + ":" + seconds;
  Serial.println("Remaining time is " + remainingTime);
  Blynk.virtualWrite(BLYNK_VIRTUAL_PIN_TEXT_REMAINING, remainingTime);
}

float getFeederDistance()
{
  digitalWrite(SENSOR_TRIGGER, HIGH);             // lanzamos un pulso para activar el sensor
  delayMicroseconds(10);                          // lo mantenemos en alto durante 10 milisegundos
  digitalWrite(SENSOR_TRIGGER, LOW);              // desactivamos el pin 7
  long tiempo = (pulseIn(SENSOR_ECHO, HIGH) / 2); // medimos el pulso de respuesta
  float distancia = float(tiempo * 0.0343);       // multiplicamos el tiempo de respuesta por velocidad del sonido
  Serial.println(distancia);                      //imprimimos la respuesta en el puerto serie
  delayMicroseconds(10);                                    // refrescamos la pantalla cada segundo
  return distancia;
}

boolean isHopperEmpty() {
  return !digitalRead(SENSOR_FEED_LEVEL);
}

void pushNotification(const String& msg) {
  Blynk.notify(msg);
}

void adviceHopperEmpty()
{
  digitalWrite(LED_RED, HIGH); // Activamos LED rojo
  BLYNK_LED_RED.on();
  Serial.println("Hopper Empty");
  Terminal.println("Hopper Empty");
}

void adviceHopperFull()
{
  digitalWrite(LED_RED, LOW); // Desactivamos LED rojo
  BLYNK_LED_RED.off();
  Serial.println("Hopper Full");
  Terminal.println("Hopper Full");
}

void printDispenseAmount(long totalTime) {
  double cantidadDispensada = totalTime * 0.05;
  Terminal.println("Gramos : " + String(cantidadDispensada));
  Terminal.println("-------------------------------");
}

void dispenseFeed()
{
  Serial.println("entro en dispense");
  long start = millis();
  miniservo.write(10);                // Desplazamos a la posición 10º
  delayMicroseconds(1000);                        // esperamos 1 segundo
  miniservo.write(160);               // Desplazamos a la posición 170º
  delayMicroseconds(1000);                        // esperamos 1 segundo
  digitalWrite(RELAY_DISPENSE, HIGH); // Activamos el pin 9 para arrancar el sinfín
  BLYNK_LED_GREEN.on();
  long totalTime = millis() - start;
  printDispenseAmount(totalTime);
}

void stopDispense()
{
  Serial.println("paro en dispense");
  digitalWrite(SERVO_MIX, LOW);      // dejamos el servo pequeño parado posición 10º
  digitalWrite(RELAY_DISPENSE, LOW); // ponemos el pin 9 a low para que pare
  BLYNK_LED_GREEN.off();
}

void ICACHE_RAM_ATTR startFeed(void) {
    if (isHopperEmpty()) {
      adviceHopperEmpty(); // Activamos led rojo
    }
    else {
      adviceHopperFull(); // Desactivamos led rojo

      while (getFeederDistance() < FEEDER_FULL_DISTANCE) {
        Serial.println("entro en while");
        dispenseFeed();
      }

      stopDispense();
    }
}
// Digital clock display of the time
void clockDisplay()
{
  String currentTime = String(hour()) + ":" + minute() + ":" + second();
  String currentDate = String(day()) + " " + month() + " " + year();
  Serial.print("Current time: ");
  Serial.print(currentTime);
  Serial.print(" ");
  Serial.print(currentDate);
  Serial.println();

  // Send time to the App
  Blynk.virtualWrite(BLYNK_VIRTUAL_PIN_CLOCK, currentTime);
}


void setup() {
  pinMode(SENSOR_TRIGGER, OUTPUT); // declaramos SENSOR_TRIGGER como salida (ultrasonidos)
  pinMode(SENSOR_ECHO, INPUT);     // declaramos input como entrada (ultrasonidos)
  pinMode(RELAY_DISPENSE, OUTPUT);

  pinMode(BUTTON_START, INPUT_PULLUP);
  pinMode(GATEWWAY_PIN, OUTPUT);
  pinMode(LED_RED, OUTPUT);

  Serial.begin(9600);
  Blynk.begin(auth, ssid, pass);
  ArduinoOTA.begin();
  ArduinoOTA.setHostname("catFeeder");

  Terminal.clear();
  miniservo.attach(SERVO_MIX); // iniciamos el servo pequeño con pin 10

  attachInterrupt(digitalPinToInterrupt(BUTTON_START), startFeed, HIGH);
  ESPtimer.attachInterruptInterval(TIMER_INTERVAL_MICROSEGUNDOS, startFeed);


  setSyncInterval(10 * 60); // Sync interval in seconds (10 minutes)
  // Display digital clock every 10 seconds
  timer.setInterval(TIMER_REMAINING_MILISEGUNDOS, clockDisplay);

}

void loop() {
  Blynk.run();
  ArduinoOTA.handle();
  timer.run();
}
