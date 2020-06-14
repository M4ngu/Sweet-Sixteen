/*
   16n Faderbank Firmware
   (c) 2017,2018 by Brian Crabtree, Sean Hellfritsch, Tom Armitage, and Brendon Cassidy
   MIT License
*/

/*
   NOTES:
   - Hardware MIDI is on pin 1
   - You **must** also compile this with Tools->USB type set to MIDI or MIDI/Serial (for debugging)
   - You also should overclock to 120MHz to make it as snappy as possible
*/

/*
   ALL configuration should take place in config.h.
   You can disable/enable flags, and configure  MIDI channels in there.
*/
// mod for pitchbend & boot-up delay by Mangu Díaz
// Tesseract Modular  
// www.tesseractmodular.com
// experimental version for Sweet Sixteen mkII and 'GESS' (Gate Expander for Sweet Sixteen)

#include "config.h"
#include <i2c_t3.h>
#include <MIDI.h>
#include <ResponsiveAnalogRead.h>
#include <CD74HC4067.h>
#include "TxHelper.h"

#ifdef GESS // only necessary for the GESS & midi note thing

#include <EEPROM.h>
// gate in, panel led and function button pins:
const int  G[8] = { 3, 0, 22, 20, 21, 23, 4, 2 };
const int  Led_ = 12;
const int  Butt = 15;
// variables for the gate inputs
bool volatile _G[8] ; // state of the gate input
bool old_G[8];
bool nNote[8];
bool nVelocity[8];
byte _nNote[8];
byte _nVelocity[8];
byte _nChannel[8];
// variables for the functon button
bool _Butt ;
bool old_Butt = HIGH;
// loop helpers
int ii, k;
// preset management
int nAddress = 0; // for the memory address
int nPreset;
bool loadActive = false;
bool saveActive = false;
#endif

MIDI_CREATE_DEFAULT_INSTANCE();

// loop helpers
int i, temp;

// midi write helpers
int q, shiftyTemp, notShiftyTemp;

// the storage of the values; current is in the main loop; last value is for midi output
int volatile currentValue[channelCount];
int lastMidiValue[channelCount];

// memory of the last unshifted value
int lastValue[channelCount];

#ifdef MASTER

// the i2c message buffer we are sending
uint8_t messageBuffer[4];

// temporary values
uint16_t valueTemp;
uint8_t device = 0;
uint8_t port = 0;

#endif

// the thing that smartly smooths the input
ResponsiveAnalogRead *analog[channelCount];

// mux config
CD74HC4067 mux(8, 7, 6, 5);
#ifdef REV
const int muxMapping[16] = {8, 9, 10, 11, 12, 13, 14, 15, 7, 6, 5, 4, 3, 2, 1, 0};
#else
const int muxMapping[16] = {0, 1, 2, 3, 4, 5, 6, 7, 15, 14, 13, 12, 11, 10, 9, 8};
#endif

// MIDI timers
IntervalTimer midiWriteTimer;
IntervalTimer midiReadTimer;
int midiInterval = 1000; // 1ms
bool shouldDoMidiRead = false;
bool shouldDoMidiWrite = false;

// helper values for i2c reading and future expansion
int activeInput = 0;
int activeMode = 0;

/*
   The function that sets up the application
*/
void setup()
{

#ifdef bootDelay // whait some time before boot-up
  delay(bootDelay);
#endif

#ifdef GESS
  for (i = 0; i < 8; i++)
  {
    pinMode (G[i], INPUT_PULLUP);
    _nChannel[i] = i + 1;
    old_G[i] = HIGH;
    nNote[i] = true;
    nVelocity[i] = true;
  }
  pinMode (Butt, INPUT_PULLUP);
  pinMode (Led_, OUTPUT);
  // boot up pattern
  for ( i = 0; i < 3; i++)
  {
    digitalWrite(Led_, HIGH);
    delay(100);
    digitalWrite(Led_, LOW);
    delay(100);
  }
#endif

#ifdef DEBUG
  while (!Serial)
    ;
  Serial.print("16n Firmware Debug Mode\n");
#endif

  // initialize the TX Helper
#ifdef V125
  TxHelper::UseWire1(true);
#else
  TxHelper::UseWire1(false);
#endif
  TxHelper::SetPorts(16);
  TxHelper::SetModes(4);

  // set read resolution to teensy's 13 usable bits
  analogReadResolution(13);

  // initialize the value storage
  for (i = 0; i < channelCount; i++)
  {
    // analog[i] = new ResponsiveAnalogRead(0, false);

    analog[i] = new ResponsiveAnalogRead(0, true, .0001);
    analog[i]->setAnalogResolution(1 << 13);

    // ResponsiveAnalogRead is designed for 10-bit ADCs
    // meanining its threshold defaults to 4. Let's bump that for
    // our 13-bit adc by setting it to 4 << (13-10)
    analog[i]->setActivityThreshold(32);

    currentValue[i] = 0;
    lastMidiValue[i] = 0;
    //#ifdef MASTER
    lastValue[i] = 0;
    //#endif
  }

  // i2c using the default I2C pins on a Teensy 3.2
#ifdef MASTER

#ifdef DEBUG
  Serial.println("Enabling i2c in MASTER mode");
#endif

#ifdef V125
  Wire1.begin(I2C_MASTER, I2C_ADDRESS, I2C_PINS_29_30, I2C_PULLUP_EXT, 400000);
#else
  Wire.begin(I2C_MASTER, I2C_ADDRESS, I2C_PINS_18_19, I2C_PULLUP_EXT, 400000);
#endif

#else
  // non-master mode

#ifdef DEBUG
  Serial.println("Enabling i2c enabled in SLAVE mode");
#endif

#ifdef V125
  Wire1.begin(I2C_SLAVE, I2C_ADDRESS, I2C_PINS_29_30, I2C_PULLUP_EXT, 400000);
  Wire1.onReceive(i2cWrite);
  Wire1.onRequest(i2cReadRequest);
#else
  Wire.begin(I2C_SLAVE, I2C_ADDRESS, I2C_PINS_18_19, I2C_PULLUP_EXT, 400000);
  Wire.onReceive(i2cWrite);
  Wire.onRequest(i2cReadRequest);
#endif

#endif

  // turn on the MIDI party
  MIDI.begin();
  midiWriteTimer.begin(writeMidi, midiInterval);
  midiReadTimer.begin(readMidi, midiInterval);

#ifdef LED
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);
#endif
}

/*
   The main read loop that goes through all of the sliders
*/
void loop()
{
  // read loop using the i counter
  for (i = 0; i < channelCount; i++)
  {
#ifdef V125
    temp = analogRead(ports[i]); // mux goes into A0
#else
    // set mux to appropriate channel
    mux.channel(muxMapping[i]);

    // read the value
    temp = analogRead(0); // mux goes into A0
#endif

    // put the value into the smoother
    analog[i]->update(temp);

    // read from the smoother, constrain (to account for tolerances), and map it
    temp = analog[i]->getValue();

#ifdef FLIP
    temp = 8191 - temp; // MAXFADER - temp; 
#endif

    temp = constrain(temp, MINFADER, MAXFADER);

    temp = map(temp, MINFADER, MAXFADER, 0, 16383);

    // map and update the value
    currentValue[i] = temp;

#ifdef GESS
    // read the gate inputs
    if (i < 8)
    {
      _G[i] = digitalRead(G[i]);
    }
#endif

  }

  if (shouldDoMidiRead)
  {
    doMidiRead();
    noInterrupts();
    shouldDoMidiRead = false;
    interrupts();
  }

  if (shouldDoMidiWrite)
  {
    doMidiWrite();
    noInterrupts();
    shouldDoMidiWrite = false;
    interrupts();
  }
}

/*
   Tiny function called via interrupt
   (it's important to catch inbound MIDI messages even if we do nothing with
   them.)
*/
void readMidi()
{
  shouldDoMidiRead = true;
}

/*
   Function called when shouldDoMidiRead flag is HIGH
*/

void doMidiRead()
{
  MIDI.read();
  usbMIDI.read();
}

/*
   Tiny function called via interrupt
*/
void writeMidi()
{
  shouldDoMidiWrite = true;
}

/*
   The function that writes changes in slider positions out the midi ports
   Called when shouldDoMidiWrite flag is HIGH
*/
void doMidiWrite()
{
  // write loop using the q counter (
  // (can't use i or temp cuz this might interrupt the reads)
  for (q = 0; q < channelCount; q++)
  {
    notShiftyTemp = currentValue[q];

    // shift for MIDI precision (0-127)
    shiftyTemp = notShiftyTemp >> 7;

    // if there was a change in the midi value
    if (( shiftyTemp != lastMidiValue[q]) || ((( usb_ccs[q] == 128 ) || ( trs_ccs[q] == 128 )) && ( notShiftyTemp != lastValue[q] )))
    {
      if ( usb_ccs[q] < 128 )
      { // send the message over USB
        usbMIDI.sendControlChange(usb_ccs[q], shiftyTemp, usb_channels[q]);
      }
      else
      {
        usbMIDI.sendPitchBend((notShiftyTemp - 8192), usb_channels[q]);
      }

#ifdef GESS
      if ((( q < 8 ) && (nNote[q] == false)) || (( q > 7 ) && (nVelocity[q - 8] == false)))
      {
#endif

        if ( trs_ccs[q] < 128 )
        { // send the message over physical MIDI
          MIDI.sendControlChange(trs_ccs[q], shiftyTemp, trs_channels[q]);
        }
        else
        {
          MIDI.sendPitchBend((notShiftyTemp - 8192), trs_channels[q]);
        }

#ifdef GESS
      }
#endif

      // store the shifted value for future comparison
      lastMidiValue[q] = shiftyTemp;

#ifdef DEBUG
      Serial.printf("MIDI[%d]: %d\n", q, shiftyTemp);
#endif
    }

#ifdef GESS
    // Midi Note
    if ((q > 7) && (_G[q - 8] != old_G[q - 8])) // only if the gate input has changed we do this:
    {
      k = ( q - 8) ;
      if ( _G[k] == LOW ) //note ON
      {
        if (nNote[k] == true) // taking the note value from the upper row of faders instead of a fixed value
        {
          _nNote[k] = lastMidiValue[k];
        }
        if (nVelocity[k] == true) // taking the midi velocity from the lower row instead instead of a fixed value
        {
          _nVelocity[k] = lastMidiValue[q];
        }
        MIDI.sendNoteOn(_nNote[k], _nVelocity[k], _nChannel[k] ); // at the moment channels are 1 to 8 preassigned
      }
      else //Note Off
      {
        MIDI.sendNoteOff(_nNote[k], _nVelocity[k], _nChannel[k] );
      }
      old_G[k] = _G[k];
    }  //end of midi note code
#endif

    // we send out to all three supported i2c slave devices
    // keeps the firmware simple :)

    if (notShiftyTemp != lastValue[q])
    {
#ifdef MASTER
#ifdef DEBUG
      Serial.printf("i2c Master[%d]: %d\n", q, notShiftyTemp);
#endif

      // for 4 output devices
      port = q % 4;
      device = q / 4;

      // TXo
      sendi2c(0x60, device, 0x11, port, notShiftyTemp);

      // ER-301
      sendi2c(0x31, 0, 0x11, q, notShiftyTemp);

      // ANSIBLE
      sendi2c(0x20, device << 1, 0x06, port, notShiftyTemp);
#endif
      lastValue[q] = notShiftyTemp;
    }
  } // end of channels loop

#ifdef GESS
  // button read to load & save presets etc:
  _Butt = digitalRead(Butt);

  if (_Butt != old_Butt) // change in the button
  {
    if (_Butt == true )
    {
      // button depressed, switch the load preset mode
      saveActive = false;
      loadActive = !loadActive;
      digitalWrite(Led_, loadActive);
    }
    else
    {
      saveActive = true;
    }
    old_Butt = _Butt;
  }

  if ( saveActive ==  true )  // save preset if any of the GESS buttons/triggers is active
  {
    for (ii = 0; ii < 8; ii++)
    {
      if (_G[ii] == LOW)
      {
        savePreset(ii);
        saveActive = false;
        loadActive = true; // when the button is released it will become false ;-)
        digitalWrite(Led_, HIGH);
      }
    }
  }

  if (( loadActive == true ) && ( _Butt == HIGH ))
  {
    for (ii = 0; ii < 8; ii++)
    {
      if (_G[ii] == LOW)
      {
        loadPreset(ii);
        loadActive = false;
        digitalWrite(Led_, LOW);
      }
    }
  }
  // end of the preset management
#endif


} // end of doMidiWrite

#ifdef MASTER

/*
   Sends an i2c command out to a slave when running in master mode
*/
void sendi2c(uint8_t model, uint8_t deviceIndex, uint8_t cmd, uint8_t devicePort, int value)
{

  valueTemp = (uint16_t)value;
  messageBuffer[2] = valueTemp >> 8;
  messageBuffer[3] = valueTemp & 0xff;

#ifdef V125
  Wire1.beginTransmission(model + deviceIndex);
  messageBuffer[0] = cmd;
  messageBuffer[1] = (uint8_t)devicePort;
  Wire1.write(messageBuffer, 4);
  Wire1.endTransmission();
#else
  Wire.beginTransmission(model + deviceIndex);
  messageBuffer[0] = cmd;
  messageBuffer[1] = (uint8_t)devicePort;
  Wire.write(messageBuffer, 4);
  Wire.endTransmission();
#endif
}

#else

/*
   The function that responds to a command from i2c.
   In the first version, this simply sets the port to be read from.
*/
void i2cWrite(size_t len)
{

#ifdef DEBUG
  Serial.printf("i2c Write (%d)\n", len);
#endif

  // parse the response
  TxResponse response = TxHelper::Parse(len);

  // true command our setting of the input for a read?
  if (len == 1)
  {

    // use a helper to decode the command
    TxIO io = TxHelper::DecodeIO(response.Command);

#ifdef DEBUG
    Serial.printf("Port: %d; Mode: %d [%d]\n", io.Port, io.Mode, response.Command);
#endif

    // this is the single byte that sets the active input
    activeInput = io.Port;
    activeMode = io.Mode;
  }
  else
  {
    // act on the command
    actOnCommand(response.Command, response.Output, response.Value);
  }
}

/*
   The function that responds to read requests over i2c.
   This uses the port from the write request to determine which slider to send.
*/
void i2cReadRequest()
{

#ifdef DEBUG
  Serial.print("i2c Read\n");
#endif

  // get and cast the value
  uint16_t shiftReady = 0;
  switch (activeMode)
  {
    case 1:
      shiftReady = (uint16_t)currentValue[activeInput];
      break;
    case 2:
      shiftReady = (uint16_t)currentValue[activeInput];
      break;
    default:
      shiftReady = (uint16_t)currentValue[activeInput];
      break;
  }

#ifdef DEBUG
  Serial.printf("delivering: %d; value: %d [%d]\n", activeInput, currentValue[activeInput], shiftReady);
#endif

  // send the puppy as a pair of bytes
#ifdef V125
  Wire1.write(shiftReady >> 8);
  Wire1.write(shiftReady & 255);
#else
  Wire.write(shiftReady >> 8);
  Wire.write(shiftReady & 255);
#endif
}

/*
   Future function if we add more i2c capabilities beyond reading values.
*/
void actOnCommand(byte cmd, byte out, int value) {}

#endif

#ifdef GESS
void loadPreset(int nPreset)  //read eeprom
{
  nAddress = ( nPreset * 16 );
  for ( ii = 0; ii < 8; ii++ ) //gates
  {
    _nNote[ii] = EEPROM.read(nAddress);
    if (_nNote[ii] == 0)
    {
      nNote[ii] = true;
    }
    else
    {
      nNote[ii] = false;
    }
    nAddress++ ;
    _nVelocity[ii] = EEPROM.read(nAddress);
    if (_nVelocity[ii] == 0)
    {
      nVelocity[ii] = true;
    }
    else
    {
      nVelocity[ii] = false;
    }
    nAddress++ ;
    //  _nChannel[ii] = EEPROM.read(nAddress);
    //   nAddress++ ;
  }
}
void savePreset(int nPreset)
{
  nAddress = ( nPreset * 16 );
  for ( ii = 0; ii < 8; ii++ ) //gates
  {

    if (lastMidiValue[ii] == 0)
    {
      nNote[ii] = true;
      _nNote[ii] = 0;
    }
    else
    {
      nNote[ii] = false;
    }

    EEPROM.write(nAddress, _nNote[ii]);
    nAddress++;

    if (lastMidiValue[ii + 8] == 0)
    {
      nVelocity[ii] = true;
      _nVelocity[ii] = 0;
    }
    else
    {
      nVelocity[ii] = false;
    }

    EEPROM.write(nAddress, _nVelocity[ii]);
    nAddress++;
    // midi channel setting are not saved because they're preassigned
    //    EEPROM.write(nAddress, _nChannel[ii]);
    //    nAddress++;
  }
}
#endif
