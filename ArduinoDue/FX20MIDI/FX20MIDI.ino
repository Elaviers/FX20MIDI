/*
   NOTE: COMPILE WITH THE OPTIMISATION SWITCH SET TO O1!!!
   IF YOU DON'T, YOU'll DEFINETLY REALISE WHY YOU SHOULD!
*/

#define SENDSERIALDEBUG 0

#define FIRSTNOTE pitchC2
#define FIRSTSOLONOTE pitchC4

#define DEFAULTVEL 127

#define USEVELOCITYSENSING 0

#if USEVELOCITYSENSING
#define MINVELMICROS 6500
#define MAXVELMICROS 30000

#define MINVELOUT 1
#define MAXVELOUT 127
#endif

#define CRES_MIN 80
#define CRES_MAX 825
#define CRES_MIN_DIFF 20 
#define CRES_AVG_SZ 10
#define CRES_MIDI_CHANNEL 0 //MIDI Channel
#define CRES_MIDI_CONTROLLER 11 //Expression Controller
#define CRES_PIN A0

///////////
#include <MIDIUSB.h>
#include <PitchToNote.h>
#include "QuickPin.h"
#include "Note.h"

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

enum Channel : byte
{
  PEDAL = 0,
  LOWER = 1,
  UPPER = 2,
  SOLO = 3
};

#if USEVELOCITYSENSING
#define NOTE_READ(NOTE, UP, DOWN)                                           \
  if (!DOWN->ReadFromPDSRs(pdsrSnaps)) NOTE.lastDownTime = currentMicros;   \
  else if (!UP->ReadFromPDSRs(pdsrSnaps)) NOTE.lastUpTime = currentMicros;  

#define NOTE_NEEDS_POLL(NOTE) (NOTE.lastDownTime && NOTE.lastUpTime)
#else
#define NOTE_READ(NOTE, UP, DOWN)                                           \
  if (!DOWN->ReadFromPDSRs(pdsrSnaps)) NOTE.state = true;                   \
  else if (!UP->ReadFromPDSRs(pdsrSnaps)) NOTE.state = false;
  
#define NOTE_NEEDS_POLL(NOTE) (NOTE.state != NOTE.prev)
#endif


#define MANUALNOTECOUNT 61
#define SOLONOTECOUNT 37
#define PEDALNOTECOUNT 25

Note upperNotes[MANUALNOTECOUNT];
Note lowerNotes[MANUALNOTECOUNT];
Note soloNotes[SOLONOTECOUNT];
BasicNote pedalNotes[PEDALNOTECOUNT];

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
  volatile uint32_t& pdsr = pitches[octavePitch]->GetPIO()->PIO_PDSR;
  uint32_t mask = pitches[octavePitch]->GetMask();

  while ((pdsr & mask) == 0) {}
  while (pdsr & mask) {}

  asm volatile(
    "NOP\nNOP\nNOP\nNOP\nNOP\nNOP\nNOP\nNOP\n" //95.23ns
    "NOP\nNOP\nNOP\nNOP\nNOP\n"                //154.762ns
  );

  asm("MOV %0, %1\n" : "=r" (pdsrSnaps[0]) : "r" (PIOA->PIO_PDSR));
  asm("MOV %0, %1\n" : "=r" (pdsrSnaps[1]) : "r" (PIOB->PIO_PDSR));
  asm("MOV %0, %1\n" : "=r" (pdsrSnaps[2]) : "r" (PIOC->PIO_PDSR));
  asm("MOV %0, %1\n" : "=r" (pdsrSnaps[3]) : "r" (PIOD->PIO_PDSR));

  //PEDAL
  pedalNotes[octavePitch].state =       !pedalDowns[0]->ReadFromPDSRs(pdsrSnaps);
  pedalNotes[12 + octavePitch].state =  !pedalDowns[1]->ReadFromPDSRs(pdsrSnaps);

  //CL
  if (octavePitch == 0)
  {
    NOTE_READ(lowerNotes[0], lowerUps[0], lowerDowns[0]);
    NOTE_READ(upperNotes[0], upperUps[0], upperDowns[0]);

    pedalNotes[0].state = !pedalDowns[0]->ReadFromPDSRs(pdsrSnaps);

    NOTE_READ(soloNotes[0], soloUps[0], soloDowns[0]);

    return;
  }

  //UPPER/LOWER
  int pitch;
  for (int i = 0; i < 5; ++i)
  {
    pitch = i * 12 + octavePitch;

    NOTE_READ(lowerNotes[pitch], lowerUps[i], lowerDowns[i]);
    NOTE_READ(upperNotes[pitch], upperUps[i], upperDowns[i]);
  }

  //SOLO
  for (int i = 0; i < 3; ++i)
  {
    NOTE_READ(soloNotes[i * 12 + octavePitch], soloUps[i], soloDowns[i]);
  }
}

void updateCres()
{
  constexpr float CRES_FLT_DIFF = (float)CRES_MIN_DIFF / (float)(CRES_MAX - CRES_MIN);
  
  static float avgVals[CRES_AVG_SZ];
  static int avgIdx = 0;
  static float cresAvg = 0.f;

  const int cresVal = analogRead(CRES_PIN);
  const float cresFlt = cresVal > CRES_MIN ? min((float)(cresVal - CRES_MIN) / (float)(CRES_MAX - CRES_MIN), 1.f) : 0.f;
  avgVals[avgIdx] = cresFlt / (float)CRES_AVG_SZ;

  cresAvg += avgVals[avgIdx];

  {
    static float lastSentAvg = -1.f;
    if (lastSentAvg < 0.f || fabs(cresAvg - lastSentAvg) > CRES_FLT_DIFF)
    {
      lastSentAvg = cresAvg;
      MidiUSB.sendMIDI({0x0B, 0xB0 | CRES_MIDI_CHANNEL, CRES_MIDI_CONTROLLER, (uint8_t)(cresAvg * 127.f)});
      delayMicroseconds(300);
    }
  }

  int nextIdx = (avgIdx + 1) % CRES_AVG_SZ;
  cresAvg -= avgVals[nextIdx];
  avgIdx = nextIdx;
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
    if (NOTE_NEEDS_POLL(lowerNotes[i])) lowerNotes[i].Poll(LOWER, FIRSTNOTE + i);
    if (NOTE_NEEDS_POLL(upperNotes[i])) upperNotes[i].Poll(UPPER, FIRSTNOTE + i);
  }

  for (int i = 0; i < SOLONOTECOUNT; ++i)
  {
    if (NOTE_NEEDS_POLL(soloNotes[i])) soloNotes[i].Poll(SOLO, FIRSTSOLONOTE + i);
  }

  for (int i = 0; i < PEDALNOTECOUNT; ++i)
  {
    if (pedalNotes[i].state != pedalNotes[i].prev) pedalNotes[i].Poll(PEDAL, FIRSTNOTE + i);
  }

  updateCres();

  MidiUSB.flush();
}
