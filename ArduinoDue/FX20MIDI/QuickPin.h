#define ALWAYSINLINE __attribute__((always_inline))

namespace QUICKPIN
{
  void Setup();
}

class Pin
{
  volatile Pio *_pio;
  uint32_t _mask;

  //0 = A, 1 = B, 2 = C, 3 = D
  int _portIndex;

  //The arduino pin this one represents
  int _pin;

public:
  constexpr Pin(volatile Pio *pio, uint32_t mask, int portIndex) noexcept : _pio(pio), _mask(mask), _portIndex(portIndex), _pin(-1) {}

  ALWAYSINLINE void UseInputMode() const noexcept { pinMode(_pin, INPUT);};

  ALWAYSINLINE constexpr bool ReadFromPDSRs(const uint32_t snapshot[4]) const noexcept { return snapshot[_portIndex] & _mask; }
  ALWAYSINLINE bool Read() const noexcept { return _pio->PIO_PDSR & _mask; }

  ALWAYSINLINE volatile constexpr Pio* GetPIO() const noexcept { return _pio; }

  ALWAYSINLINE constexpr uint32_t GetMask() const noexcept { return _mask; }

  friend void QUICKPIN::Setup();
};

#define _BV(bit) (1 << (bit))
#define PA(NUMBER) Pin(PIOA, _BV(NUMBER), 0)
#define PB(NUMBER) Pin(PIOB, _BV(NUMBER), 1)
#define PC(NUMBER) Pin(PIOC, _BV(NUMBER), 2)
#define PD(NUMBER) Pin(PIOD, _BV(NUMBER), 3)

Pin dPins[54] =
{
          //+0     +1       +2        +3       +4         +5         +6         +7       +8       +9
  /*0*/   PA(8),   PA(9),   PB(25),   PC(28),   PC(26),    PC(25),    PC(24),    PC(23),  PC(22),  PC(21),
  /*10*/  PC(29),  PD(7),   PD(8),    PB(27),   PD(4),     PD(5),     PA(13),    PA(12),  PA(11),  PA(10),
  /*20*/  PB(12),  PB(13),  PB(26),   PA(14),   PA(15),    PD(0),     PD(1),     PD(2),   PD(3),   PD(6),
  /*30*/  PD(9),   PA(7),   PD(10),   PC(1),    PC(2),     PC(3),     PC(4),     PC(5),   PC(6),   PC(7),
  /*40*/  PC(8),   PC(9),   PA(19),   PA(20),   PC(19),    PC(18),    PC(17),    PC(16),  PC(15),  PC(14),
  /*50*/  PC(13),  PC(12),  PB(21),   PB(14)  
};

namespace QUICKPIN
{
  void Setup()
  {
    for (int i = 0; i < 54; ++i)
      dPins[i]._pin = i;
  }
}
