/*
    Prepreg Oven Main Control Sketch

    This sketch controls the oven through PWM control of an AC heater circuit.
    Buttons and an encoder are used to control the settings on an LCD and
    2 thermocouples are used for feedback. A microSD card provides a convenient
    way to load temperature profiles with linearly interpolated values.

    Author: JJ D

    UVic Rocketry
    October 2021
*/

#include <Wire.h>

// https://www.pjrc.com/teensy/td_libs_Encoder.html
#include <Encoder.h>

// Make sure to adjust the contrast with the little pot on the back
// if the text isn't very visible
#include <LiquidCrystal_I2C.h>

// https://github.com/fabianoriccardi/dimmable-light
#include <dimmable_light.h>

// SD card that stores temperature profiles
#include <SdFat.h>

// ***** PINS ***** //

// Encoder
#define E_INT 3 // Interrupt capable
#define E_REG 5
#define E_BTN 6

// Dimmer breakout board
#define HEATER_INT  2 // Interrupt capable
#define HEATER_CTRL 7

// Thermocouple breakout boards
#define THERMOCOUPLE_1 A0
#define THERMOCOUPLE_2 A1

// SD card breakout board
#define SD_CHIP_SELECT 4

// Start btn
#define START_STOP_BTN 8


// ***** OBJECTS ***** //

// Heater dimmer object
DimmableLight heater(HEATER_CTRL);

// Instantiate our LCD
LiquidCrystal_I2C lcd(0x3F, 20, 4);

// Encoder object will keep track of pulses from rotary knob
Encoder encoder(E_INT, E_REG);

// MicroSD card
SdFat sdCard;
SdFile sdProfilesFile;

// Temperature profile read from SD card.
struct tempProfile
{
    // A profile with n temps has n-1 stages (segment between temps)
    byte currentStage = 0;

    byte temps[20];
    unsigned int times[20];
};

// ***** GLOBALS ***** //

// Set by START_STOP_BTN
bool ovenStarted = false;

byte previousEncoderVal = 0;

// Value of millis() when the next temp in the profile was set
long millisStageStarted = 0;

tempProfile sdTempProfile;

void setup()
{
    Serial.begin(9600);

    pinMode(START_STOP_BTN, INPUT_PULLUP);
    pinMode(E_BTN, INPUT_PULLUP);

	// Initialize the display
	lcd.init();
	lcd.backlight();

    // Set up the heater dimmer object
    DimmableLight::setSyncPin(HEATER_INT);
    DimmableLight::begin();
    
    // Start the SD card
    sdCard.begin(SD_CHIP_SELECT);
}

void loop()
{
    // update the display 

    // if !started:
        // scroll through temp profiles
        // check io: encoder/start button
    // else:
        // check true temp
        // read target temp from profile
        // interpolate current temp needed
        // set heater power
        // check IO: stop btn?

    // if time is up:
        // power off heater
        // display finished message
        // wait for start button press



    
}

// TODO max cycle length will be no longer than 18h with 
// totalTimeRemaining as a uint
void updateDisplay(String status, byte realTemp, byte setTemp, 
                   byte finalStageTemp, unsigned int secondsToNextTemp,
                   unsigned int totalSecondsRemaining)
{
    /*
        Display is 4 lines, 20 char each

        // All in degrees science (C)
        realTemp = 118
        setTemp = 120
        finalStageTemp = 150

        secondsToNextTemp = 1620 (27 minutes)
        totalSecondsRemaining = 13320 (3.7 hours)
        status = "Ramping", could be "Holding" or "Cooling" etc.
        _____________________
        |T: 118/120 -> 150   |
        |27m left in stage   |
        |3.7h remaining      |
        |Status: Ramping     |
    */

    lcd.clear();

    // Format seconds to hours or minutes

    // totalSecondsRemaining
    String totalRemainingStr = "";

    if(totalSecondsRemaining > 3600)
        totalRemainingStr = String(float(totalSecondsRemaining)/3600, 1) + "h";
    else
        totalRemainingStr = String(totalSecondsRemaining / 60) + "m";

    // secondsToNextTemp
    String nextTempStr = "";

    if(secondsToNextTemp > 3600)
        nextTempStr = String(float(secondsToNextTemp)/3600, 1) + "h";
    else
        nextTempStr = String(secondsToNextTemp / 60) + "m";

    // Printing to the lcd line by line
    lcd.setCursor(0,0);
    lcd.print("Curr T:" + String(realTemp) + "/" + String(setTemp) + " -> "
                + String(finalStageTemp));

    lcd.setCursor(0,1);
    lcd.print(nextTempStr + "m left in stage");
    
    lcd.setCursor(0,2);
    lcd.print(totalRemainingStr + "h remaining");

    lcd.setCursor(0,3);
    lcd.print("Status: " + status);
}

void checkButtons()
{
    // Scroll through temperature profiles
    if(encoder.read() > 0)
        scrollSDProfile(true);
    else if(encoder.read() < 0)
        scrollSDProfile(false);
    encoder.readAndReset();

    // Encoder clicked
    if(digitalRead(E_BTN) == LOW)
        selectSDProfile();

    // Start button
    if(digitalRead(START_STOP_BTN) == LOW && !ovenStarted)
        ovenStarted = true;

    // 3s long press to shut down oven
    int timePressed = 0;
    while (digitalRead(START_STOP_BTN) == LOW)
    {
        delay(10);
        timePressed += 10;
    }

    if(timePressed > 3000)
        ovenFinished(F("Manual shutdown"));
}

int getNextSDTempTime()
{
    return 0;
}

byte interpolateTemp()
{
    // Linearly interpolates values when temperature profile ramps
    // (y) = y1 + [(x-x1) × (dy)]/ (dx) where y is interpolated temp
    // TODO how bad is error due to integer truncation here?

    byte i = sdTempProfile.currentStage;

    unsigned int dx = sdTempProfile.times[i+1] - sdTempProfile.times[i];
    byte         dy = sdTempProfile.temps[i+1] - sdTempProfile.temps[i];

    return sdTempProfile.temps[i] + ( ((millisStageStarted - millis())*dy) / dx );
}

void scrollSDProfile(bool direction)
{

}

void selectSDProfile()
{

}

int measureTemp()
{
	// This is based on: adafruits ad8495 article
    
    #define AREF 4.5 // Nano is 4.5V 
    #define ADC_RESOLUTION 10 // Nano is 10 bit

	// Get the raw voltage pin on the thermocouple pin
	float voltage = 0.5*(analogRead(THERMOCOUPLE_2) + analogRead(THERMOCOUPLE_1))
                    * (AREF / (pow(2, ADC_RESOLUTION) - 1));

    // Convert the voltage to a value in C 
    // Adjust the x in (voltage-x) to tune the temperature.
    return (voltage - 1.59) / 0.005;
}

void setHeaterPowerPID(byte realTemp, byte targetTemp)
{

}

void ovenFinished(String status)
{

}