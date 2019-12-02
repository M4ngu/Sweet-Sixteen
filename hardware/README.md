If you want to order pcbs yourself make sure you have a valid replacement for the faders, the ones I use may have a weird pinout. You can also edit the board in order to use your desired fader model.

The voltage of the faders (when there's nothing plugged in) could be easy modified by replacing the IC4 for another LDO and changing the value of the summing resistors in the MCP6004 circuits, in order to get 3,3v (or close) from the LDO once passes throught the op-amp.

The formula is pretty simple: 

Vin * (RFB / RIN) = 3,3 

In this case the IC4 LDO is an 8V regulator, so: 

8v * (33k / 80,6k) = 3,27v

![MCP6004 circuit](/mcp6004.png)

Keep in mind that if you change the 33k feedback resistor for other value, the summing resistors for the voltage reference must be also adapted with the same criteria: 

5v * (33k / 49,9k) = 3,3v
