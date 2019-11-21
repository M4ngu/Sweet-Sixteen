# Sweet-Sixteen
Based in the [16n faderbank][16n-faderbank/16n], the following changes were made in the hardware:

-The PCB layout is compretly new

-A power section has been added to work within the eurorack modular synth format, with diode protection and the option to power the Teensy with a 5V LDO or the eurorack 5V rail (switchable with a jumper)

![PCB](https://lh3.googleusercontent.com/Elq1ayZZGXsQK5p0A-S--crPwu4DdsH9hsDBSZvOMHxTNBKJrN2qklEanpVfKWG8FPIvnjy56ERZpbgHbj4bKpIKUn7xhZon6FvhDxSas5UfAamzbx2L=w472)

![Sweet Sixteen](20191105_181811.jpg)

[firmware](_16n_faderbank_firmware_Sweet/)

[hardware](hardware/)

There are 16 channels, which can be used as:

 -Midi controller

 -Manual CV generator (0 to 8v range)

 -Attenuator

 -CV to Midi CC

 -i2C controller

 -CV to i2C data

Features:

 -16 inputs
 
 -16 outputs
 
 -16 faders
 
 -16 bi-color leds to show what's going on in every channel
 
 -Midi trs out
 
 -Jumpers on the back to swap between 'Arturia/Novation'  and 'Korg/Makenoise' Midi trs standards
 
 -USB Midi out (connector type B, usb midi class compliant device)
 
 -4 switches to enable bi-polar CV to Midi/i2C conversion (one for every 4 channels)
 
 -i2C via mini jack on front panel and pin header on the back of the module.


## Credits
This is a derivative work of the [16n faderbank][16n-faderbank/16n]

Based on original work by [Brian Crabtree][tehn] and Sean Hellfritsch.  
Minijack MIDI, I2C circuitry and CV outputs by [Tom Armitage][infovore].  
Firmware by [Brian Crabtree][tehn], [Tom Armitage][infovore], and [Brendon Cassidy][bpcmusic].

## Licensing

Panels and electronic schematics/layouts/gerber files are licensed under
[Creative Commons Attribution Share-Alike 4.0][ccbysa].

Firmware is licensed under the [MIT License][mitlicense].

[linespost]: https://llllllll.co/t/sixteen-n-faderbank/3643
[tehn]: https://github.com/tehn
[bpcmusic]: https://github.com/bpcmusic
[infovore]: https://github.com/infovore
[octobom]: https://octopart.com/bom-tool/unJxkzvR
[ccbysa]: https://creativecommons.org/licenses/by-sa/4.0/
[mitlicense]: https://opensource.org/licenses/MIT
[16n-faderbank/16n]: https://github.com/16n-faderbank/16n
