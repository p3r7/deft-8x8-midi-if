/* Create a "class compliant " USB to 8 MIDI IN and 8 MIDI OUT interface.

   Teensy 4.1

   MIDI receive (6N138 optocoupler) input circuit and series resistor

   You must select "Serial + MIDI" from the "Tools > USB Type" menu

   NB: can also use "Serial + MIDIx16" to get 16 virtual interfaces
   the selected interface is retrieved w/ `usbMIDI.getCable` and allows having different cnfs for each virtualInterface
   this is similar to midihub's beahviour

   This example code is in the public domain.
*/



// - IN1 is master keyboard -> omni
// - IN/OUT2 is MPC -> omni, except sysex
// - IN/OUT8 is computer -> omni
// TODO: expose 2nd USB port to all other devices (+ ideally as additional USB devices)

// device -> USB
// 1 OK
// 2 KO -> bug or interface dead?, most likely 2nd opt
// 3 OK
// 4 OK
// 5 OK
// 6 KO -> known issue, likely a buggy teensy
// 7 OK
// 8 OK


// ------------------------------------------------------------------------

#include <MIDI.h>
#include <USBHost_t36.h> // access to USB MIDI devices (plugged into 2nd USB port)


// ------------------------------------------------------------------------
// CONSTS

#define SERIAL_BAUDS 115200
#define LED_PIN 13

#define NB_MIDI 8


// ------------------------------------------------------------------------
// STATE - CONF

const uint8_t OMNI_ROUTE_CHANNEL = 16;

// to prevent self-echoing messages
boolean areInOutSameDev[] = { false, true, true, true,
                              true, true, true, true };

// only controllers can send notes, CC & PGM change to all devices
// sequencers also go in this category

// only librarian devices can receive sysex & CC from all devices
// this is to prevent spamming sysex on all interfaces

#define M_DEVICE      2
#define M_CONTROLLER  4
#define M_LIBRARIAN   8

uint8_t mode[] = { M_CONTROLLER, M_CONTROLLER, M_DEVICE, M_DEVICE,
                   M_DEVICE, M_DEVICE, M_DEVICE, M_CONTROLLER | M_LIBRARIAN};

#define CH_M_EXPOSED  0 // channel exposed by interface
#define CH_M_DEVICE   1 // channel device listens to

// this conf allows remapping sent/received channels for each port
// this allows binding a dedicated one to each device wo/ having to temper w/ their config
boolean doChannelRemap[] = { false, false, false, false,
                             false, false, false, false};
uint8_t channelRemap[][2] = { {1, 1}, {2, 1}, {3, 1}, {4, 1},
                              {5, 1}, {6, 1}, {7, 1}, {8, 1}};

// - IN1 is master keyboard -> omni
// - IN/OUT2 is MPC -> omni
// - 3/4/5/6/7 are vanilla
// - 8 is computer -> send sysex

boolean isController (int midi_id) {
  return (mode[midi_id] & M_CONTROLLER) > 0;
}

boolean isLibrarian (int midi_id) {
  return (mode[midi_id] & M_LIBRARIAN) > 0;
}

// ------------------------------------------------------------------------
// STATE (MEMORY)

// A variable to know how long the LED has been turned on
elapsedMillis ledOnMillis;


// ------------------------------------------------------------------------
// INIT

// Create the Serial MIDI ports
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI1);
MIDI_CREATE_INSTANCE(HardwareSerial, Serial2, MIDI2);
MIDI_CREATE_INSTANCE(HardwareSerial, Serial3, MIDI3);
MIDI_CREATE_INSTANCE(HardwareSerial, Serial4, MIDI4);
MIDI_CREATE_INSTANCE(HardwareSerial, Serial5, MIDI5);
MIDI_CREATE_INSTANCE(HardwareSerial, Serial6, MIDI6);
MIDI_CREATE_INSTANCE(HardwareSerial, Serial7, MIDI7);
MIDI_CREATE_INSTANCE(HardwareSerial, Serial8, MIDI8);


midi::MidiInterface<midi::SerialMIDI<HardwareSerial> > m[]={ MIDI1, MIDI2, MIDI3, MIDI4,
                                                             MIDI5, MIDI6, MIDI7, MIDI8 };

// Create the ports for USB devices plugged into Teensy's 2nd USB port (via hubs)
USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
USBHub hub3(myusb);
USBHub hub4(myusb);
MIDIDevice midi01(myusb);
MIDIDevice midi02(myusb);
MIDIDevice midi03(myusb);
MIDIDevice midi04(myusb);
MIDIDevice midi05(myusb);
MIDIDevice midi06(myusb);
MIDIDevice midi07(myusb);
MIDIDevice midi08(myusb);
MIDIDevice * midilist[8] = { &midi01, &midi02, &midi03, &midi04,
                             &midi05, &midi06, &midi07, &midi08 };


void setup() {
  Serial.begin(SERIAL_BAUDS);
  pinMode(LED_PIN, OUTPUT); // LED pin

  for (int i=0 ; i < NB_MIDI ; i++) {
    m[i].begin(MIDI_CHANNEL_OMNI);
    m[i].turnThruOff();
  }

  digitalWriteFast(LED_PIN, HIGH); // LED on
  delay(500);
  digitalWriteFast(LED_PIN, LOW);

  myusb.begin();
}


// ------------------------------------------------------------------------
// MAIN

void loop() {
  bool activity = false;

  // serial midi ports
  for (int i=0 ; i < NB_MIDI ; i++) {
    if (m[i].read()) {
      byte type    = m[i].getType();
      byte channel = m[i].getChannel();
      byte data1   = m[i].getData1();
      byte data2   = m[i].getData2();

      if (type != midi::SystemExclusive) {
        midi::MidiType mtype = (midi::MidiType)type;
        byte mappedChannel = channel;

        usbMIDI.send(type, data1, data2, channel, 0);

        for (int o=0 ; o < NB_MIDI ; o++) {
          mappedChannel = channel;

          // don't self-echo
          if (o == i && areInOutSameDev[i]) {
            continue;
          }

          // only controllers can send to other devices
          // w/ exception of librarians that receive from any device
          if (! (isController(i) || isLibrarian(o))) {
            continue;
          }

          if (isController(i)) {
            if (isLibrarian(o)) {
              // -> be transparent
            } else {
              // -> filter/remap according to dest device (o)
              if (channel == OMNI_ROUTE_CHANNEL
                  || channel == channelRemap[o][CH_M_EXPOSED]) {
                mappedChannel = channelRemap[o][CH_M_DEVICE];
              } else {
                // filter out
                continue;
              }
            }
          } else {
            // -> remap according to source device (i)
            mappedChannel = channelRemap[i][CH_M_EXPOSED];
          }

          m[o].send(mtype, data1, data2, mappedChannel);
        }
      } else {
        unsigned int SysExLength = data1 + data2 * 256;
        usbMIDI.sendSysEx(SysExLength, m[i].getSysExArray(), true, 0);
        for (int o=0 ; o < NB_MIDI ; o++) {
          if (o == i && areInOutSameDev[i]) { // don't self-echo sysex
            continue;
          }

          // librarians can send sysed to any device and receive from all
          if (! (isLibrarian(i) || isLibrarian(o))) {
            continue;
          }

          m[o].sendSysEx(SysExLength, m[i].getSysExArray(), true);
        }
      }

      activity = true;
    }
  }

  // secondary USB port (acting as host)
  // could host up to 8 devices (connected through a USB hub)
  for (int port=0; port < 8; port++) {
    if (midilist[port]->read()) {
      uint8_t type =       midilist[port]->getType();
      uint8_t data1 =      midilist[port]->getData1();
      uint8_t data2 =      midilist[port]->getData2();
      uint8_t channel =    midilist[port]->getChannel();
      const uint8_t *sys = midilist[port]->getSysExArray();
      // sendToComputer(type, data1, data2, channel, sys, 8 + port);
      sendOmni(type, data1, data2, channel, sys, 0);

      activity = true;
    }
  }

  // main USB interface port
  if (usbMIDI.read()) {
    byte type    = usbMIDI.getType();
    byte channel = usbMIDI.getChannel();
    byte data1   = usbMIDI.getData1();
    byte data2   = usbMIDI.getData2();
    byte cable   = usbMIDI.getCable();

    if (type != usbMIDI.SystemExclusive) {
      midi::MidiType mtype = (midi::MidiType)type;
      byte mappedChannel = channel;

      for (int o=0 ; o < NB_MIDI ; o++) {
        mappedChannel = channel;

        if (isLibrarian(o)) {
          // -> be transparent
        } else {
          // -> filter/remap according to dest device (o)
          if (channel == OMNI_ROUTE_CHANNEL
              || channel == channelRemap[o][CH_M_EXPOSED]) {
            mappedChannel = channelRemap[o][CH_M_DEVICE];
          } else {
            // filter out
            continue;
          }
        }

        m[o].send(mtype, data1, data2, mappedChannel);
      }
    } else {
      unsigned int SysExLength = data1 + data2 * 256;

      for (int o=0 ; o < NB_MIDI ; o++) {
        m[o].sendSysEx(SysExLength, usbMIDI.getSysExArray(), true);
      }
    }

    activity = true;
  }

  // blink the LED when any activity has happened
  if (activity) {
    digitalWriteFast(13, HIGH); // LED on
    ledOnMillis = 0;
  }
  if (ledOnMillis > 15) {
    digitalWriteFast(13, LOW);  // LED off
  }

}



// ------------------------------------------------------------------------
// UTILS - MIDI

void sendToComputer(byte type, byte data1, byte data2, byte channel, const uint8_t *sysexarray, byte cable)
{
  if (type != midi::SystemExclusive) {
    usbMIDI.send(type, data1, data2, channel, cable);
  } else {
    unsigned int SysExLength = data1 + data2 * 256;
    usbMIDI.sendSysEx(SysExLength, sysexarray, true, cable);
  }
}

void sendOmni(byte type, byte data1, byte data2, byte channel, const uint8_t *sysexarray, byte cable)
{
  if (type != midi::SystemExclusive) {
    midi::MidiType mtype = (midi::MidiType)type;

    usbMIDI.send(type, data1, data2, channel, cable);

    for (int o=0 ; o < NB_MIDI ; o++) {
      m[o].send(mtype, data1, data2, channel);
    }

  } else {

    unsigned int SysExLength = data1 + data2 * 256;
    usbMIDI.sendSysEx(SysExLength, sysexarray, true, cable);
    MIDI8.sendSysEx(SysExLength, sysexarray, true);
  }

}
