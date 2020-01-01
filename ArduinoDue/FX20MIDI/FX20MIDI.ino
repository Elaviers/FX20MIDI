#include <MIDIUSB.h>
#include <PitchToNote.h>
#include "QuickPin.h"

#define SENDSERIALDEBUG 0

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

bool pitchStates[4][120];
bool prevPitchStates[4][120];

enum Channel
{
  PEDAL = 0,
  LOWER = 1,
  UPPER = 2,
  SOLO = 3
};

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

  //CL
  if (octavePitch == 0)
  {
    if (!lowerDowns[0]->ReadFromPDSRs(pdsrSnaps))       pitchStates[LOWER][pitchC1] = true;
    else if (!lowerUps[0]->ReadFromPDSRs(pdsrSnaps))    pitchStates[LOWER][pitchC1] = false;
  
    if (!upperDowns[0]->ReadFromPDSRs(pdsrSnaps))       pitchStates[UPPER][pitchC1] = true;
    else if (!lowerUps[0]->ReadFromPDSRs(pdsrSnaps))  pitchStates[UPPER][pitchC1] = false;

    pitchStates[PEDAL][pitchC1] = !pedalDowns[0]->ReadFromPDSRs(pdsrSnaps);

    if (!soloDowns[0]->ReadFromPDSRs(pdsrSnaps))        pitchStates[SOLO][pitchC3] = true;
    else if (!soloUps[0]->ReadFromPDSRs(pdsrSnaps))     pitchStates[SOLO][pitchC3] = false;

    return;
  }
  
//UPPER/LOWER
  int basePitch = pitchC1 + octavePitch;
  int pitch;
  for (int i = 0; i < 5; ++i)
  {
    pitch = basePitch + i*12;
    
    if (!lowerDowns[i]->ReadFromPDSRs(pdsrSnaps))     pitchStates[LOWER][pitch] = true;
    else if (!lowerUps[i]->ReadFromPDSRs(pdsrSnaps))  pitchStates[LOWER][pitch] = false;

    if (!upperDowns[i]->ReadFromPDSRs(pdsrSnaps))     pitchStates[UPPER][pitch] = true;
    else if (!upperUps[i]->ReadFromPDSRs(pdsrSnaps))  pitchStates[UPPER][pitch] = false;
  }

//PEDAL
  pitchStates[PEDAL][basePitch] =       !pedalDowns[0]->ReadFromPDSRs(pdsrSnaps);
  pitchStates[PEDAL][basePitch + 12] =  !pedalDowns[1]->ReadFromPDSRs(pdsrSnaps);

//SOLO
  basePitch = pitchC3 + octavePitch;
  for (int i = 0; i < 3; ++i)
  {
    pitch = basePitch + i*12;
    
    if (!soloDowns[i]->ReadFromPDSRs(pdsrSnaps))    pitchStates[SOLO][pitch] = true;
    else if (!soloUps[i]->ReadFromPDSRs(pdsrSnaps)) pitchStates[SOLO][pitch] = false;
  }
}

void loop() {
  noInterrupts();
  for (int i = 0; i <= 12; ++i)
  {
    scanPitch(i);
  }
  interrupts();

  while (MidiUSB.available()) { MidiUSB.read(); }

  for (int channel = 0; channel < 4; ++channel)
  {
    for (int pitch = pitchC1; pitch <= pitchC6; ++pitch)
    {
      const bool& state = pitchStates[channel][pitch];
      bool& prev = prevPitchStates[channel][pitch];
      
      if (state != prev)
      {
        prev = state;
  
        if (state)
        {
          //Note on
          MidiUSB.sendMIDI({0x09, 0x90 | channel, pitch, 127});
          delayMicroseconds(300);

#if SENDSERIALDEBUG
          SerialUSB.print(channel);
          SerialUSB.print('-');
          SerialUSB.print(pitch);
          SerialUSB.println("->ON");
#endif
        }
        else
        {
          //Note off
          MidiUSB.sendMIDI({0x08, 0x80 | channel, pitch, 127});
          delayMicroseconds(300);

#if SENDSERIALDEBUG
          SerialUSB.print(channel);
          SerialUSB.print('-');
          SerialUSB.print(pitch);
          SerialUSB.println("->OFF");
#endif
        }
      }
    }
  }

  delayMicroseconds(20);

  MidiUSB.flush();
}
