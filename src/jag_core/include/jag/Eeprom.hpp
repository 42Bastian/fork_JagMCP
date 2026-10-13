#pragma once

#include <array>
#include <cstdint>

namespace jag
{

// The cartridge's serial EEPROM, a 93C46 of 64 words of 16 bits, on Jerry's general purpose chip
// selects (jaguar.v): an access to GPIO0 ($F14800) is a pulse of its clock SK, its rising edge as
// the access ends, a write's bit 0 the data DI it shifts in; an access to GPIO1 ($F15000) drops its
// chip select CS for its length; its data out DO is bit 0 of JOYSTICK. Its commands: READ, WRITE,
// ERASE, ERAL, WRAL, EWEN, EWDS; a write or an erase is done at once (the core's finishes in a few
// cycles of sys_clk, DO low meanwhile, long before a program can poll it). The size is not
// detected (auto_eeprom clear).
//
// Ports Jaguar_MiSTer rtl/eeprom_93c46_x16.v, read at 43a761d.
class Eeprom
{
public:
  Eeprom()
  {
    mWords.fill( 0xffff ); // erased
  }

  // DI as a write to GPIO0 drives it.
  void data( bool di )
  {
    mDi = di;
  }

  // SK rises: the end of an access to GPIO0.
  void clock();

  // CS drops: an access to GPIO1.
  void deselect();

  // DO.
  bool out() const
  {
    return mDout;
  }

  std::array<std::uint16_t, 64> const& words() const
  {
    return mWords;
  }

private:
  enum class Status : std::uint8_t
  {
    Idle,
    Data,
    Read
  };

  void write();

  std::array<std::uint16_t, 64> mWords{};
  Status mStatus = Status::Idle;
  bool mDi = false;
  bool mDout = true;
  bool mWriteEnable = false;
  std::uint32_t mCount = 0;
  std::uint32_t mIr = 0; // 13 bits
  std::uint16_t mDr = 0;
};

} // namespace jag
