# WatchmanPongV2

Demo video: https://youtube.com/shorts/m0Smps4umFc?si=Mw8ZHat44lLpZ-JR

**Requires TV.io library**  
https://github.com/nootropicdesign/arduino-tvout

## Hardware Mods:
**See Hardware Mods directory for reference photos.**

• Pull the IF chip off the board.  
• Pull the RF modulator out and jump a wire from the RF pin to GND (reference photo arduino.jpeg bottom right corner you will see my jumper).  
• Cut traces on the PCB to the pins on the tuner pot and VHF/UHF chip so it is no longer making contacts to the components on the board.  
• Jump a wire from the pot to GND (see photo bodgeWires.jpeg where it says "jump")  
• Run the bodge wires to the arduino.  

I recommend an FD-10A model because the IF chip is surface mounted and easy to remove.

An FD-2A works also but the IF chip is throughhole and the pin locations to video and audio are on the opposite side of the chip (easy to find with some poking around). 

You could just brute force cut the chip and RF tuner out with snips... 

## Arduino Nano Pinouts:

**Board type:** Arduino Nano  
**Processor:** ATmega328P (Old Bootloader)

- **pin A3** - paddle (with 10k pullup)
- **pin 10** - start/quit  
- **pin D11** - audio out  
- **pin D9** - to 1k resistor to composite in  
- **pin D7** - to 470 ohm to composite in