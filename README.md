# Arduino project for a sound level detector that triggers a visual alarm when things get too loud!

This project monitors a voltage source hooked to a sound level circuit, and triggers an alarm if that level is over a threshold for a given percent of a specified sampling period.

The project includes an LCD display which shows the current sound level, and is used to configure the various parameters.  Additionally the project sends IR commands when the alarm is triggered to activate an external alarm.

## Installation

The project depends on these libraries which you have to install your libraries folder for this sketch to compile:

+ Bounce: http://www.arduino.cc/playground/Code/Bounce
+ IRremote: https://github.com/shirriff/Arduino-IRremote
+ Debug: https://github.com/zippy/arduino_debug

## Operation

The arduino will always be in one of four modes:

### Normal Mode
This is the monitoring mode where the arduino is monitoring the sound voltage for and will trigger the alarm.  In this mode the LCD displays the current sound level.

### Alarm Mode
In this mode the arduino is simply waiting for the "Alarm Reset" period to go by before switching back to the Normal mode.  When this mode is first triggered it will send the out the IR codes to turn on the alarm, and when this mode completes it will send out the IR code to turn off the alarm.  You can also press the button to switch away from the alarm mode back to the normal mode.

### Setup Mode
When you hold down the button for more the a second while in Normal Mode, the arduino goes into setup mode.  In Setup mode you can cycle through the following parameters that can be configured by rotating the setup-pot.  The LCD will show the current value of the given parameter, to set a given parameter press the button.

+ "Level-M" is for manual configuration of the sound level that triggers the alarm.  (Use Level-A to auto-configure the level value).
+ "Period" is the number of seconds that arduino samples sound.  Rotate the setup-pot to set the value.
+ "Alarm Reset" is the number of seconds after which the alarm should be reset when activated.  Rotate the setup pot to set the value.
+ "Percent" is the percentage of time during the period that the sound must be above the threshold to trigger the alarm.  Rotate the setup-pot to set the value.
+ "Level-A" is for auto configuration of the sound level that triggers the alarm.  When auto-configuring the level, simply press the button when the level of noise is present which should trigger the alarm.
+ "Auto Range" is a boolean as to whether auto ranging is turned on or off.  When auto-ranging is on, the actual maximum and minimum voltage levels sensed by the system while in Normal Mode will adjust the range used in the level meter
+ "Calibrate Range" allows you to calibrate the voltage range that marks the top and bottom of the level meter based on actual sound inputs.

### Get Param Mode
In this mode (which is a sub-mode of the Setup mode) the Arduino continually check the resistance of the setup-pot and uses it to set the value of the current setup parameter.

### Autolevel Mode
In this mode (which is a sub-mode of the Setup mode) the Arduino monitors the sound level, but uses it to set the threshold.  What you should do is create the level of noise that you want to be the threshold value for the alarm.  In auto-level the LCD will display the sound level, plus the threshold marker which keeps getting bumped up as the sound level increases.  Press the button to save the current threshold marker.

### Calibrate Range Mode
In this mode (which is a sub-mode of the Setup mode) the Arduino monitors the sound level, but uses it to set the upper and lower sound values used to display the level meter.