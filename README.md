# ThePrepregOvenProject
Circuit diagram, Arduino code, etc. related to the Prepreg Oven Project: Uvic Rocketry's custom composite curing oven! The PPO is capable of reading temperature profiles from an SD card. Users can load up to 999 custom profiles with up to 20 target tempertures. Temperature values are linearly interpolated automatically.

## Interface
The oven has 2 buttons for controlling various functions:

**SELECT** Used for scrolling through temperature profiles.

**START/STOP** Confirms temperature profile and starts the oven. Long press to force shut down oven mid cycle. Long press again to reboot the oven.

## Starting the Oven
Upon first power, or after rebooting, the oven displays the profile selection screen. 

Pressing the SELECT button scrolls through the temperature profiles stored on the SD card. The numerical name of the file is displayed, and a short description of the profile appears below.

Once the desired profile is displayed, press START to begin the profile.

## Creating Temperature Profiles
Temperature profiles can be loaded on the SD card for custom cycles.

The format for a profile is a single line in the file as follows:
- Numerical filenames starting at 0. No file extension.
- Exactly 14 alphnumeric characters describing the profile. Pad with spaces if needed. Terminate with a `,`.
- Up to 40 `,` seperated non negative integer values alternating between temperature in C and time between temperatures in seconds. The maximum value of a time number is `65535` (~18h in seconds), and maximum temperature value is `200`.

For example, a profile called `0` containing the single line of text: `Sample        ,20,1800,50,5400,50,1800,80,7200,80,1800,120,1800,120,3600,20` produces the cycle in the image below.

![Sample Profile](/images/sampleProfile.png)

