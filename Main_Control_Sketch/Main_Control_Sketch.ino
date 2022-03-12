
/*
    Prepreg Oven Main Control Sketch

    This sketch controls the oven through PWM control of an AC heater circuit.
    Buttons are used to control the settings on an LCD and
    2 thermocouples are used for feedback. A microSD card provides a convenient
    way to load temperature profiles with linearly interpolated values.

    SD card structure is as follows:
        - One file PER profile, named in ascending numerical order.
            ie: 0.txt 1.txt, 2.txt, 3.txt....

        - Each file contains up to 40 numbers seperated by commas alternating
            temp,time,temp,time...temp,F where temp is target temperature in C
            and time is seconds until the next temperature.
            
            The line must be terminated by the letter "F". 
            
            Fewer than 40 values can be used, but F must always end the line.
            
            ie: 20,2000,50,10000,100,10000,F
        

    Author: JJ D

    UVic Rocketry
    October 2022
*/

#include <Wire.h>

// Make sure to adjust the contrast with the little pot on the back
// if the text isn't very visible
#include <LiquidCrystal_I2C.h>

// https://github.com/fabianoriccardi/dimmable-light
#include <dimmable_light.h>

/* 
    SD Card library that is extra small to save ram.
    Note that the library's chip select pin by default is 10.
*/
#include <PetitFS.h>
#include <PetitSerial.h>

// ***** PINS ***** //

// Dimmer breakout board
#define HEATER_INT  2 // Interrupt capable
#define HEATER_CTRL 4

// Thermocouple breakout boards
#define THERMOCOUPLE_1 A0
#define THERMOCOUPLE_2 A1

#define START_STOP_BTN 3
#define SELECT_BTN 6


// ***** OBJECTS ***** //

// Heater dimmer object
DimmableLight heater(HEATER_CTRL);

// Instantiate our LCD
LiquidCrystal_I2C lcd(0x3F, 20, 4);

// MicroSD card
PetitSerial PS;
#define Serial PS
FATFS fs;



// Temperature profile read from SD card.
struct tempProfile
{
    // Name of the file on the SD card
    byte fileNumber = 0;

    // A profile with n temps has n-1 stages (segment between temps)
    byte currentStage = 0;

    byte temps[20];
    unsigned int times[20];
};

// ***** GLOBALS ***** //

// Set by START_STOP_BTN
bool ovenStarted = false;
bool ovenFinished = false;

String displayStatus = "";

// Value of millis() when the next temp in the profile was set
long millisStageStarted = 0;

long lastMillis = millis();

tempProfile sdTempProfile;

// Allows reset of Arduino through software
void(* resetFunc) (void) = 0;

void setup()
{
    Serial.begin(9600);

    pinMode(START_STOP_BTN, INPUT_PULLUP);
    pinMode(SELECT_BTN, INPUT_PULLUP);

	// Initialize the display
    lcd.init();
    lcd.backlight();
    lcd.clear();

    // Set up the heater dimmer object
    DimmableLight::setSyncPin(HEATER_INT);
    DimmableLight::begin();

    // Init the SD card
    if(pf_mount(&fs))
    {
        lcd.setCursor(1,1);
        lcd.print(F(" SD mount failed!"));
        while(true)
            checkButtons();
    }

    configureOven();
}

void loop()
{
    // Update display 1Hz
    if(millis() - lastMillis > 1000)
    {
        updateDisplay(displayStatus, 20, 50, 60, 6000, 6600);
        lastMillis = millis();
    }

    checkButtons();
}

void configureOven()
{
    lcd.clear();

    while(!ovenStarted)
    {
        checkButtons();

        lcd.setCursor(0,0);

        lcd.print(F(" Welcome to OvenOS!"));

        lcd.setCursor(0,1);
        lcd.print(F(" Selected cycle: "));
        lcd.print(sdTempProfile.fileNumber);

        lcd.setCursor(0,3);
        lcd.print("   Press Start...");
    }

    lcd.clear();
    lcd.setCursor(0,1);

    // Memesssss
    switch (millis() % 24)
    {
    case 0:
        lcd.print(F("Hydrate or Die-drate"));
        break;

    case 1:
        lcd.print(F("     Mech Monkee"));
        lcd.setCursor(0,2);
        lcd.print(F("      Certified"));
        break;
        
    case 2:
        lcd.print(F("   Solidworks has"));
        lcd.setCursor(0,2);
        lcd.print(F("encountered an error"));
        break;

    case 3:
        lcd.print(F("  Has MULE-1 been"));
        lcd.setCursor(0,2);
        lcd.print(F("   hotfired yet??"));
        break;

    case 4:
        lcd.print(F("   Gucci to Gochi"));
        break;

    case 5:
        lcd.print(F("To the Katz-Mobile!"));
        break;

    case 6:
        lcd.print(F("Wrangling Pixies..."));
        break;

    case 7:
        lcd.print(F("   I hope Xenia-1"));
        lcd.setCursor(0,2);
        lcd.print(F("   didn't crash"));
        break;

    case 8:
        lcd.print(F("  Hello it's your"));
        lcd.setCursor(0,2);
        lcd.print(F("   overlord Kris"));
        break;

    case 9:
        lcd.print(F("   This oven fits"));
        lcd.setCursor(0,2);
        lcd.print(F("  one Devon inside"));
        break;

    case 10:
        lcd.print(F("   I use VIM btw"));
        break;

    case 11:
        lcd.print(F("   Also great for"));
        lcd.setCursor(0,2);
        lcd.print(F("   baking bread!"));
        break;

    case 12:
        lcd.setCursor(0,0);
        lcd.print(F("  3 C White Flour"));
        lcd.setCursor(0,1);
        lcd.print(F("   3/4 C Starter"));
        lcd.setCursor(0,2);
        lcd.print(F("   1 1/4 C Water"));
        lcd.setCursor(0,3);
        lcd.print(F("   1/2 TBSP Salt"));
        break;

    case 13:
        lcd.print(F(" Liberate the HAAS!"));
        break;

    case 14:
        lcd.print(F("   Post and plate"));
        lcd.setCursor(0,2);
        lcd.print(F("      babyyyyy"));
        break;

    case 15:
        lcd.print(F("    Snitches get"));
        lcd.setCursor(0,2);
        lcd.print(F("      Stitches"));
        break;

    case 16:
        lcd.print(F("  Migrating to the"));
        lcd.setCursor(0,2);
        lcd.print(F("   new server..."));
        break;

    case 17:
        lcd.print(F(" Migrating from the"));
        lcd.setCursor(0,2);
        lcd.print(F("   new server..."));
        break;

    case 18:
        lcd.print(F("Invest in UVR Coin!"));
        break;

    case 19:
        lcd.setCursor(0,3);
        lcd.print(F("    Bottom Text"));
        break;

    case 20:
        lcd.setCursor(0,0);
        lcd.print(F("   o   Ideal"));
        lcd.setCursor(0,1);
        lcd.print(F("   |   Electrical"));
        lcd.setCursor(0,2);
        lcd.print(F("  _|_  Engineer"));
        lcd.setCursor(0,3);
        lcd.print(F("   ^"));
        break;

    case 21:
        lcd.print(F("   Sponsored by"));
        lcd.setCursor(0,2);
        lcd.print(F("    Aliexpress"));
        break;

    case 22:
        lcd.print(F("JunCAD is Converging"));
        break;

    case 23:
        lcd.print(F("  Same hairea tho"));
        break;

    case 24:
        lcd.print(F(" In thrust we trust"));
        break;

    default:
        break;
    }

    delay(2000);
    lcd.clear();
    delay(1000);

    // This jumps to void loop()
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
        |Temp: 118/120 -> 150|
        |Stat: Ramping       |
        |27m left in stage   |
        |3.7h remaining      |

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
    lcd.print("Temp: " + String(realTemp) + "/" + String(setTemp) + " -> "
                + String(finalStageTemp));

    lcd.setCursor(0,1);
    lcd.print("Stat: " + status);
    
    lcd.setCursor(0,2);
    lcd.print(nextTempStr + " left in stage");
    
    lcd.setCursor(0,3);
    lcd.print(totalRemainingStr + " remaining");
}

void checkButtons()
{
    // Select button scrolls through temperature profiles on SD card
    if(digitalRead(SELECT_BTN) == LOW && !ovenStarted)
        scrollSDProfile();

    // Start button
    if(digitalRead(START_STOP_BTN) == LOW)
        ovenStarted = true;

    // 3s long press to shut down or reset oven
    int timePressed = 0;
    while (digitalRead(START_STOP_BTN) == LOW && timePressed <= 3000)
    {
        delay(100);
        timePressed += 100;
    }

    if(timePressed >= 3000 && ovenFinished)
        resetFunc();
    if(timePressed >= 3000 && ovenStarted)
        ovenDone(F("Force Shutdown"));
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

void scrollSDProfile()
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

void ovenDone(String status)
{

    ovenFinished = true;
    lcd.clear();

    while(true)
    {
        // Update display 1Hz
        if(millis() - lastMillis > 1000)
        {
            // Status message explaining shutdown reason
            lcd.setCursor(0,0);
            lcd.print(status);

            // Let user know if oven is cooled off yet
            lcd.setCursor(0,1);
            lcd.print(F("Temperature:       "));
            lcd.setCursor(14, 1);
            lcd.print(String(measureTemp()));

            lcd.setCursor(0,3);
            lcd.print(F("Hold Start to reset"));
            lastMillis = millis();
        }

        // Check for reset signal
        checkButtons();
    }
}