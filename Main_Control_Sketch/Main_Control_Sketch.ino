
/*
    Prepreg Oven Main Control Sketch

    This sketch controls the oven using a PID loop with the output connected to
    an AC heater circuit. 
    
    Buttons are used to control the settings on an LCD and 2 thermocouples are
    used for feedback. A microSD card provides a convenient way to load
    temperature profiles with linearly interpolated values.

    See README for temperature profile structure.

    Author: JJ D

    UVic Rocketry
    October 2022
*/

#include <Wire.h>
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
#define HEATER_CTRL 3

// Thermocouple breakout boards
#define THERMOCOUPLE_1 A0
#define THERMOCOUPLE_2 A1

#define START_STOP_BTN 4
#define SELECT_BTN 5


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

    // 14 char description of profile. First chars in file.
    char description[15];

    // Location in temps and times array
    byte index = 0;

    // Set by profile on SD card
    byte temps[20] = {0};
    unsigned int times[20] = {0};
};

// ***** GLOBALS ***** //

// Set by START_STOP_BTN
bool ovenStarted = false;
bool ovenFinished = false;

// PID loop
// TODO tune these PID values. 
#define kP 1.0
#define kI 1.0
#define kD 1.0
#define intThresh 10   // Prevents "integral windup". See set_PID_heater()
#define PIDScale 0.1   // Scales PID value to roughly 0-100
float prevError = 0;   // Error of the previous PID calculation
float integral  = 0;   // Accumulation of integral values in PID
long  timerDt   = 0;   // Tracks millis() for PID dt value

// Value of millis() when the next temp in the profile was set
unsigned long millisStageStarted = 0;

unsigned long lastMillis = millis();

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
        ovenDone(F("SD Mount Failed"));
    }

    
    // Read first profile to show its description w/o a button press
    readSDProfile();

    // Temperature profile selection
    configureOven();
}

void loop()
{
    // Check for force shutdown/restart
    checkButtons();

    // Update display at 1Hz
    if(millis() - lastMillis > 1000)
    {
        updateDisplay(
            30,
            interpolateTemp(),
            sdTempProfile.temps[sdTempProfile.index],
            sdTempProfile.times[sdTempProfile.index] - (millis() - millisStageStarted)/1000,
            getTotalTimeRemaining()
            );

        lastMillis = millis();
    }

    // profile array contains 20 values max. Finished if end is reached
    if(sdTempProfile.index >= 20)
        ovenDone("Cycle Complete!");

    // Move to next stage in profile if needed
    if((millis() - millisStageStarted)/1000 > sdTempProfile.times[sdTempProfile.index])
    {
        sdTempProfile.index++;
        millisStageStarted = millis();
    }

    setHeaterPowerPID(measureTemp(), interpolateTemp(), millis() - timerDt);
    timerDt = millis();
}

void configureOven()
{
    lcd.clear();

    while(!ovenStarted)
    {
        checkButtons();

        lcd.setCursor(0,0);
        lcd.print(F("Selected cycle: "));
        lcd.print(sdTempProfile.fileNumber);

        lcd.setCursor(0,1);
        lcd.print(F("Desc: "));
        lcd.print(sdTempProfile.description);

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

    // Set start of first stage
    millisStageStarted = millis();

    // This jumps to void loop()
}

void updateDisplay(byte realTemp, byte setTemp, byte finalStageTemp,
                    unsigned int secondsToNextTemp,
                    unsigned long totalSecondsRemaining)
{
    /*
        Display is 4 lines, 20 char each

        // All in degrees science (C)
        realTemp = 118
        setTemp = 120
        finalStageTemp = 150

        secondsToNextTemp = 1620 (27 minutes)
        totalSecondsRemaining = 13320 (3.7 hours)
        _____________________
        |Real 118  Target 120|
        |Final stage temp 150|
        |27m left in stage   |
        |3.7h remaining      |

    */

    lcd.clear();

    // Format seconds to hours or minutes

    // totalSecondsRemaining
    String totalRemainingStr = "";

    if(totalSecondsRemaining > 3600)
        totalRemainingStr = String(float(totalSecondsRemaining)/3600.0, 1) + "h";
    else if(totalSecondsRemaining > 60)
        totalRemainingStr = String(totalSecondsRemaining / 60) + "m";
    else
        totalRemainingStr = "<1m";

    // secondsToNextTemp
    String nextTempStr = "";

    if(secondsToNextTemp > 3600)
        nextTempStr = String(float(secondsToNextTemp)/3600.0, 1) + "h";
    else if(totalSecondsRemaining > 60)
        nextTempStr = String(secondsToNextTemp / 60) + "m";
    else
        nextTempStr = "<1m";

    // Printing to the lcd line by line
    lcd.setCursor(0,0);
    lcd.print(F("Real "));
    lcd.print(realTemp);
    lcd.setCursor(10,0);
    lcd.print(F("Target "));
    lcd.print(setTemp);

    lcd.setCursor(0,1);
    lcd.print(F("Final stage temp "));
    lcd.print(finalStageTemp);

    lcd.setCursor(0,2);
    lcd.print(nextTempStr);
    lcd.print(F(" left in stage"));
    
    lcd.setCursor(0,3);
    lcd.print(totalRemainingStr);
    lcd.print(F(" remaining"));
}

void checkButtons()
{
    // Select button scrolls through temperature profiles on SD card
    if(digitalRead(SELECT_BTN) == LOW && !ovenStarted)
    {
        sdTempProfile.fileNumber++;
        readSDProfile();
    }

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

void readSDProfile()
{
    // IF THERE ARE ANY BUGS IN THE CODE THEY ARE PROBABLY HERE

    // Clear the profile arrays
    for(int i = 0; i < 20; i++)
    {
        sdTempProfile.temps[i] = 0;
        sdTempProfile.times[i] = 0;
    }

    // Convert fileNumber to char array
    char fileName[4] = "   ";
    itoa(sdTempProfile.fileNumber, fileName, 10);

    // Loop back and open 0 if no higher file exists
    if(pf_open(fileName))
    {
        sdTempProfile.fileNumber = 0;
        pf_open("0");
    }

    // Update sdTempProfile with the contents of the file

    // Read the description (first 14 chars)
    UINT bytesRead;
    pf_read(sdTempProfile.description, 14, &bytesRead);

    // Move past the comma after description
    pf_lseek(fs.fptr + 1);


    // Next read csv into sdTempProfile.

    // 5 char is enough to store max value of unsigned int
    char charsNum[6];
    
    // Index in charsNum 
    byte charIndex = 0;

    // Index in temps/times array in sdTempProfile
    byte profileIndex = 0;

    // true means temp, false means time
    bool isTemp = true;     

    // bytesRead = 0 when EOF is reached
    while(bytesRead > 0) 
    {
        pf_read(&charsNum[charIndex], 1, &bytesRead);

        // Comma or EOF reached
        if(bytesRead < 1 || !isDigit(charsNum[charIndex]))
        {
            charsNum[charIndex] = '\0';
            String strNum = String(charsNum);

            // Convert and store value in profile
            // This breaks if profile contains too large of numbers
            if(isTemp){
                sdTempProfile.temps[profileIndex] = strNum.toInt();
                isTemp = false;
            }
            else
            {
                sdTempProfile.times[profileIndex] = strNum.toInt();
                profileIndex++;
                isTemp = true;
            }

            // Prepare to get next number string
            charIndex = 0;
        }
        else
            charIndex++;
    }

    // Debounce
    delay(250);
}

byte interpolateTemp()
{
    // Linearly interpolates values when temperature profile ramps
    // (y) = y1 + [(x-x1) × (dy)]/ (dx) where y is interpolated temp

    byte i = sdTempProfile.index;

    float dx = sdTempProfile.times[i+1] - sdTempProfile.times[i];
    float dy = sdTempProfile.temps[i+1] - sdTempProfile.temps[i];

    return sdTempProfile.temps[i] + ( (float((millisStageStarted - millis()))*dy) / dx );
}

unsigned long getTotalTimeRemaining()
{
    unsigned long sum = 0;

    // This is guaranteed to add at least the last value in times to sum
    for(byte i = sdTempProfile.index; i < 20; i++)
        sum += sdTempProfile.times[i];

    // Subtract mid-stage time elapsed.
    sum -= (millis() - millisStageStarted)/1000;
    Serial.println(sum);
    return sum;
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

void setHeaterPowerPID(byte realTemp, byte targetTemp, float dt)
{

    float error = targetTemp - realTemp;

    // We only include integral when the oven is already
    // within intThresh degrees of the tempSet to prevent it from dominating
    // This behaviour is called "integral windup"
    if(abs(error) < intThresh)
        integral += error*dt / 1000; // ms->s
    else
        integral = 0;

    float derivative = (error - prevError) / dt;
    float pwr = (error*kP) + (integral*kI) + (derivative*kD);

    pwr *= PIDScale;

    // The circuit is not rated for the full power of the heater
    // so we limit the values to 0-100 rather than 0-255
    if(pwr > 100)
        pwr = 100;
    else if(pwr < 0)
        pwr = 0;

    heater.setBrightness(pwr);

    prevError = error;
}

// Status 20 chars max
void ovenDone(String status)
{
    heater.setBrightness(0);
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