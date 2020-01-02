#include <MIDIUSB.h>
#include <PitchToNote.h>
#include "QuickPin.h"

#define SENDSERIALDEBUG 0

#define FIRSTNOTE pitchC2
#define FIRSTSOLONOTE pitchC4

#define DEFAULTVEL 127

#define USEVELOCITYSENSING 1

#if USEVELOCITYSENSING
  #define MINVELMICROS 6500
  #define MAXVELMICROS 30000
  
  #define MINVELOUT 1
  #define MAXVELOUT 127
#endif

uint32_t pdsrSnaps[4];
unsigned long currentMicros;

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

inline void SendNoteOn(byte channel, byte pitch, byte velocity = DEFAULTVEL)
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

struct BasicNote
{
  bool state;
  bool prev;

  inline void Poll(byte channel, byte pitch)
  {
    if (state != prev)
    {
      prev = state;

      if (state)
        SendNoteOn(channel, pitch);
      else
        SendNoteOff(channel, pitch);
    }
  }
};

#if USEVELOCITYSENSING
const float VELOCITY_DIVISOR = (float)(MAXVELMICROS - MINVELMICROS) / (float)(MAXVELOUT - MINVELOUT);

inline byte CalculateVelocity(unsigned long deltaMicros)
{
  signed long n = MAXVELMICROS - deltaMicros;

  if (n < 0)
    return MINVELOUT;

  signed int vel = (unsigned int)((float)n / VELOCITY_DIVISOR) + MINVELOUT;

  return vel > MAXVELOUT ? MAXVELOUT : vel;
}

struct Note
{
  unsigned long lastUpTime;
  unsigned long lastDownTime;

  inline void Poll(byte channel, byte pitch)
  {
    if (lastUpTime && lastDownTime)
    {
      #if SENDSERIALDEBUG
        SerialUSB.print(lastUpTime);
        SerialUSB.print(" > ");
        SerialUSB.print(lastDownTime);
        SerialUSB.print(" : ");
      #endif

      if (lastDownTime < lastUpTime)
        SendNoteOff(channel, pitch);
      else
        SendNoteOn(channel, pitch, CalculateVelocity(lastDownTime - lastUpTime));

      lastUpTime = lastDownTime = 0;
    }
  }
};

#else
typedef BasicNote Note;
#endif

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


#define MANUALNOTECOUNT 61
#define SOLONOTECOUNT 37
#define PEDALNOTECOUNT 25

Note upperNotes[MANUALNOTECOUNT];
Note lowerNotes[MANUALNOTECOUNT];
Note soloNotes[SOLONOTECOUNT];
BasicNote pedalNotes[PEDALNOTECOUNT];

#if USEVELOCITYSENSING
#define READ(NOTE, UP, DOWN) if (!DOWN->ReadFromPDSRs(pdsrSnaps)) NOTE.lastDownTime = currentMicros; else if (!UP->ReadFromPDSRs(pdsrSnaps)) NOTE.lastUpTime = currentMicros;
#define NEEDSPOLL(NOTE) (NOTE.lastDownTime && NOTE.lastUpTime)
#else
#define READ(NOTE, UP, DOWN) if (!DOWN->ReadFromPDSRs(pdsrSnaps)) NOTE.state = true; else if (!UP->ReadFromPDSRs(pdsrSnaps)) NOTE.state = false;
#define NEEDSPOLL(NOTE) (NOTE.state != NOTE.prev)
#endif

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
    READ(lowerNotes[0], lowerUps[0], lowerDowns[0]);
    READ(upperNotes[0], upperUps[0], upperDowns[0]);

    pedalNotes[0].state = !pedalDowns[0]->ReadFromPDSRs(pdsrSnaps);

    READ(soloNotes[0], soloUps[0], soloDowns[0]);

    return;
  }

  //UPPER/LOWER
  int pitch;
  for (int i = 0; i < 5; ++i)
  {
    pitch = i * 12 + octavePitch;

    READ(lowerNotes[pitch], lowerUps[i], lowerDowns[i]);
    READ(upperNotes[pitch], upperUps[i], upperDowns[i]);
  }

  //SOLO
  for (int i = 0; i < 3; ++i)
  {
    READ(soloNotes[i * 12 + octavePitch], soloUps[i], soloDowns[i]);
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
    if (NEEDSPOLL(lowerNotes[i])) lowerNotes[i].Poll(LOWER, FIRSTNOTE + i);
    if (NEEDSPOLL(upperNotes[i])) upperNotes[i].Poll(UPPER, FIRSTNOTE + i);
  }

  for (int i = 0; i < SOLONOTECOUNT; ++i)
  {
    if (NEEDSPOLL(soloNotes[i])) soloNotes[i].Poll(SOLO, FIRSTSOLONOTE + i);
  }

  for (int i = 0; i < PEDALNOTECOUNT; ++i)
  {
    if (pedalNotes[i].state != pedalNotes[i].prev) pedalNotes[i].Poll(PEDAL, FIRSTNOTE + i);
  }

  MidiUSB.flush();
}
