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
#define START_BTN 8


// Heater dimmer object
DimmableLight heater(HEATER_CTRL);

// Instantiate our LCD
LiquidCrystal_I2C lcd(0x3F, 20, 4);

// Encoder object will keep track of pulses from rotary knob
Encoder encoder(E_INT, E_REG);

// MicroSD card
SdFat sdCard;
SdFile sdProfilesFile;

void setup()
{
    Serial.begin(9600);

    pinMode(START_BTN, INPUT_PULLUP);
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

}

int getNextSDTempTime()
{
    return 0;
}

int interpolateTemp()
{
    return 0;
}

void scrollSDProfile(bool direction)
{

}

int measureTemp()
{
    return 0;
}

void setHeaterPowerPID(byte realTemp, byte targetTemp)
{

}

void ovenFinished()
{

}