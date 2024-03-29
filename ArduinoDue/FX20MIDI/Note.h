inline void SendNoteOn(byte channel, byte pitch, byte velocity = DEFAULTVEL) noexcept
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

inline void SendNoteOff(byte channel, byte pitch, byte velocity = 127) noexcept
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

  void Poll(byte channel, byte pitch) noexcept
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
constexpr float VELOCITY_DIVISOR = (float)(MAXVELMICROS - MINVELMICROS) / (float)(MAXVELOUT - MINVELOUT);

constexpr byte CalculateVelocity(unsigned long deltaMicros) noexcept
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

  void Poll(byte channel, byte pitch) noexcept
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
