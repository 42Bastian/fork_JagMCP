#include "jag/Eeprom.hpp"

namespace jag
{

void Eeprom::clock()
{
  if ( mStatus == Status::Idle )
  {
    // the command, a start bit, two of opcode and six of address, decoded as its last bit comes
    std::uint32_t const irhi = ( mIr >> 3 ) & 0x3f;
    std::uint32_t const next = ( ( mIr & 0x1f ) << 1 ) | ( mDi ? 1u : 0u );
    mIr = ( ( mIr << 1 ) | ( mDi ? 1u : 0u ) ) & 0x1fff;
    if ( !( irhi & 0x10 ) )
      return;
    std::uint32_t const op = ( irhi >> 2 ) & 3;
    if ( op == 2 ) // READ: the word fetched, a dummy 0 first
    {
      mDr = mWords[next & 0x3f];
      mDout = false;
      mStatus = Status::Read;
    }
    else if ( ( irhi & 0xf ) == 3 ) // EWEN
      mWriteEnable = true;
    else if ( op == 3 || ( irhi & 0xf ) == 2 ) // ERASE, ERAL
    {
      if ( mWriteEnable )
        write();
    }
    else if ( op == 1 || ( irhi & 0xf ) == 1 ) // WRITE, WRAL: the data follows
    {
      if ( mWriteEnable )
        mStatus = Status::Data;
    }
    else if ( ( irhi & 0xf ) == 0 ) // EWDS
      mWriteEnable = false;
  }
  else if ( mStatus == Status::Data )
  {
    mDr = static_cast<std::uint16_t>( std::uint32_t{ mDr } << 1 | ( mDi ? 1u : 0u ) );
    if ( ++mCount == 16 )
    {
      mCount = 0;
      write();
    }
  }
  else
  {
    mDout = ( mDr & 0x8000 ) != 0;
    mDr = static_cast<std::uint16_t>( std::uint32_t{ mDr } << 1 );
  }
}

// WR_BEGIN to WR_END: the command, its last address bit shifted in since, picks what is written
void Eeprom::write()
{
  std::uint32_t const irhi = ( mIr >> 3 ) & 0x3f;
  std::uint32_t const address = mIr & 0x3f;
  if ( ( irhi >> 3 & 3 ) == 3 ) // ERASE
    mWords[address] = 0xffff;
  else if ( ( irhi >> 3 & 3 ) == 1 ) // WRITE
    mWords[address] = mDr;
  else if ( ( irhi >> 1 & 0xf ) == 2 ) // ERAL
    mWords.fill( 0xffff );
  else if ( ( irhi >> 1 & 0xf ) == 1 ) // WRAL
    mWords.fill( mDr );
  mDout = true;
  mStatus = Status::Idle;
}

void Eeprom::deselect()
{
  mStatus = Status::Idle;
  mCount = 0;
  mIr = 0;
  mDr = 0;
  mDout = true;
}

} // namespace jag
