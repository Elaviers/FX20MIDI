#include <MIDIUSB.h>
#include <PitchToNote.h>
#include "QuickPin.h"

#define SENDSERIALDEBUG 1

#define FIRSTNOTE pitchC2
#define FIRSTSOLONOTE pitchC4

#define MANUALNOTECOUNT 61
#define SOLONOTECOUNT 37
#define PEDALNOTECOUNT 25

#define MINVELMICROS 6500
#define MAXVELMICROS 30000

#define MINVELOUT 1
#define MAXVELOUT 127

uint32_t pdsrSnaps[4];

//                            CL          CS          D           DS          E           F           FS          G           GS          A           AS          B           C
const Pin* pitches[13] =    { &dPins[22], &dPins[23], &dPins[24], &dPins[25], &dPins[26], &dPins[27], &dPins[28], &dPins[29], &dPins[30], &dPins[31], &dPins[32], &dPins[33], &dPins[34] };

//                            U0           U1         U2          U3          U4
const Pin* lowerUps[5] =    { &dPins[39], &dPins[38], &dPins[37], &dPins[36], &dPins[35] };

//                            D0           D1         D2          D3          D4
const Pin* lowerDowns[5] =  { &dPins[40], &dPins[41], &dPins[42], &dPins[43], &dPins[44] };

//                            U0           U1         U2          U3          U4
const Pin* upperUps[5] =    { &dPins[49], &dPins[48], &dPins[47], &dPins[46], &dPins[45] };

//                            D0           D1         D2          D3          D4
const Pin* upperDowns[5] =  { &dPins[50], &dPins[51], &dPins[52], &dPins[53], &dPins[2] };

//                            U2          U3          U4
const Pin* soloUps[3] =     { &dPins[5], &dPins[4], &dPins[3] };

//                            D2          D3          D4
const Pin* soloDowns[3] =   { &dPins[6], &dPins[7], &dPins[8] };

//                            D0          D1
const Pin* pedalDowns[2] =  { &dPins[9],  &dPins[10] };

enum Channel
{
  PEDAL = 0,
  LOWER = 1,
  UPPER = 2,
  SOLO = 3
};

struct Note
{
  unsigned long lastUpTime;
  unsigned long lastDownTime;
};

struct BasicNote
{
  bool state;
  bool prev;
};

Note upperNotes[MANUALNOTECOUNT];
Note lowerNotes[MANUALNOTECOUNT];
Note soloNotes[SOLONOTECOUNT];
BasicNote pedalNotes[PEDALNOTECOUNT];

unsigned long currentMicros;

void setup() {
  QUICKPIN::Setup();

  for (int i = 0; i < 13; ++i)
    pitches[i]->UseInputMode();

  for (int i = 0; i < 5; ++i)
  {
    lowerUps[i]->UseInputMode();
    lowerDowns[i]->UseInputMode();
    upperUps[i]->UseInputMode();
    upperDowns[i]->UseInputMode();
  }

  for (int i = 0; i < 3; ++i)
  {
    soloUps[i]->UseInputMode();
    soloDowns[i]->UseInputMode();
  }

  for (int i = 0; i < 2; ++i)
    pedalDowns[i]->UseInputMode();
}

void scanPitch(int octavePitch)
{
  pitches[octavePitch]->WaitForFallingEdge();

  pdsrSnaps[0] = PIOA->PIO_PDSR;
  pdsrSnaps[1] = PIOB->PIO_PDSR;
  pdsrSnaps[2] = PIOC->PIO_PDSR;
  pdsrSnaps[3] = PIOD->PIO_PDSR;

  //PEDAL
  pedalNotes[octavePitch].state =       !pedalDowns[0]->ReadFromPDSRs(pdsrSnaps);
  pedalNotes[12 + octavePitch].state =  !pedalDowns[1]->ReadFromPDSRs(pdsrSnaps);

  //CL
  if (octavePitch == 0)
  {
    if (!lowerDowns[0]->ReadFromPDSRs(pdsrSnaps))       lowerNotes[0].lastDownTime = t;
    else if (!lowerUps[0]->ReadFromPDSRs(pdsrSnaps))    lowerNotes[0].lastUpTime = t;

    if (!upperDowns[0]->ReadFromPDSRs(pdsrSnaps))       upperNotes[0].lastDownTime = t;
    else if (!upperUps[0]->ReadFromPDSRs(pdsrSnaps))    upperNotes[0].lastUpTime = t;

    pedalNotes[0].state = !pedalDowns[0]->ReadFromPDSRs(pdsrSnaps);

    if (!soloDowns[0]->ReadFromPDSRs(pdsrSnaps))        soloNotes[0].lastDownTime = t;
    else if (!soloUps[0]->ReadFromPDSRs(pdsrSnaps))     soloNotes[0].lastUpTime = t;

    return;
  }

  //UPPER/LOWER
  int pitch;
  for (int i = 0; i < 5; ++i)
  {
    pitch = i * 12 + octavePitch;

    if (!lowerDowns[i]->ReadFromPDSRs(pdsrSnaps))     lowerNotes[pitch].lastDownTime = t;
    else if (!lowerUps[i]->ReadFromPDSRs(pdsrSnaps))  lowerNotes[pitch].lastUpTime = t;

    if (!upperDowns[i]->ReadFromPDSRs(pdsrSnaps))     upperNotes[pitch].lastDownTime = t;
    else if (!upperUps[i]->ReadFromPDSRs(pdsrSnaps))  upperNotes[pitch].lastUpTime = t;
  }

  //SOLO
  for (int i = 0; i < 3; ++i)
  {
    if (!soloDowns[i]->ReadFromPDSRs(pdsrSnaps))    soloNotes[i * 12 + octavePitch].lastDownTime = t;
    else if (!soloUps[i]->ReadFromPDSRs(pdsrSnaps)) soloNotes[i * 12 + octavePitch].lastUpTime = t;
  }
}

inline void SendNoteOn(byte channel, byte pitch, byte velocity = 127)
{
  MidiUSB.sendMIDI({0x09, 0x90 | channel, pitch, velocity});
  delayMicroseconds(300);

#if SENDSERIALDEBUG
  SerialUSB.print(channel);
  SerialUSB.print('-');
  SerialUSB.print(pitch);
  SerialUSB.println("->ON");
#endif
}

inline void SendNoteOff(byte channel, byte pitch, byte velocity = 127)
{
  MidiUSB.sendMIDI({0x08, 0x80 | channel, pitch, velocity});
  delayMicroseconds(300);

#if SENDSERIALDEBUG
  SerialUSB.print(channel);
  SerialUSB.print('-');
  SerialUSB.print(pitch);
  SerialUSB.println("->OFF");
#endif
}

const float VELOCITY_DIVISOR = (float)(MAXVELMICROS - MINVELMICROS) / (float)(MAXVELOUT - MINVELOUT);

inline byte CalculateVelocity(unsigned long deltaMicros)
{
  signed long n = MAXVELMICROS - deltaMicros;

  if (n < 0)
    return MINVELOUT;

  signed int vel = (unsigned int)((float)n / VELOCITY_DIVISOR) + MINVELOUT;
  
  return vel > MAXVELOUT ? MAXVELOUT : vel;
}

inline void ScanNote(Note& note, byte channel, byte pitch)
{
  if (note.lastUpTime && note.lastDownTime)
  {
    #if SENDSERIALDEBUG
    SerialUSB.print(note.lastUpTime);
    SerialUSB.print(" > ");
    SerialUSB.print(note.lastDownTime);
    SerialUSB.print(" : ");
    #endif
    
    if (note.lastDownTime < note.lastUpTime)
      SendNoteOff(channel, pitch);
    else
      SendNoteOn(channel, pitch, CalculateVelocity(note.lastDownTime - note.lastUpTime));

    note.lastUpTime = note.lastDownTime = 0;
  }
}

void loop() {
  noInterrupts();

  currentMicros = micros(); 
  
  for (int i = 0; i <= 12; ++i)
  {
    scanPitch(i);
  }
  interrupts();

  while (MidiUSB.available()) {
    MidiUSB.read();
  }

  for (int i = 0; i < MANUALNOTECOUNT; ++i)
  {
    ScanNote(lowerNotes[i], LOWER, FIRSTNOTE + i);
    ScanNote(upperNotes[i], UPPER, FIRSTNOTE + i);
  }

  for (int i = 0; i < SOLONOTECOUNT; ++i)
  {
    ScanNote(soloNotes[i], SOLO, FIRSTSOLONOTE + i);
  }

  for (int i = 0; i < PEDALNOTECOUNT; ++i)
  {
    if (pedalNotes[i].state != pedalNotes[i].prev)
    {
      BasicNote& note = pedalNotes[i];
      note.prev = note.state;

      if (note.state)
        SendNoteOn(PEDAL, FIRSTNOTE + i);
      else
        SendNoteOff(PEDAL, FIRSTNOTE + i);
    }
  }

  delayMicroseconds(20);

  MidiUSB.flush();
}
