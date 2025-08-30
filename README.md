# WatchmanPongV2

Requires TV.io library
https://github.com/nootropicdesign/arduino-tvout

See Hardware Mods directory for the modifications you need to make to the Watchman.

I recommend an FD-10A model becase the IF chip is surface mounted and easy to remove.

An FD-2A works also but the IF chip is throughhole and the pin locations to video and 
audio are on the opposite side of the chip (easy to find with some poking around). 
You could just brute force cut the chip and RF tuner out with snips... 

Arduino Nano Pinouts:

pin A3 paddle - with 10k pullup

pin 10 - start/quit

d11 - audio out 
d9 to 1k resistor to composite in
d7 to 470 ohm to composite in

