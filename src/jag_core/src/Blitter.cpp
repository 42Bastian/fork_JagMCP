// Tom's blitter: Jaguar_MiSTer rtl/Tom/blit.v and the modules under it, read at 43a761d.

#include "jag/Blitter.hpp"

#include <cstring>
#include <optional>
#include <type_traits>

namespace jag
{

namespace
{

constexpr bool bit( std::uint32_t v, std::uint32_t n )
{
  return ( v >> n ) & 1u;
}

std::uint32_t rotl8( std::uint32_t v, std::uint32_t n )
{
  n &= 7;
  return ( ( v << n ) | ( v >> ( ( 8 - n ) & 7 ) ) ) & 0xffu;
}

std::uint32_t reverse8( std::uint32_t v )
{
  std::uint32_t r = 0;
  for ( std::uint32_t i = 0; i < 8; ++i )
    r |= ( ( v >> i ) & 1u ) << ( 7 - i );
  return r;
}

// srcshift.v: the source phrase aligned with the destination, from the two last read (srcd1 the
// newer, srcd2 the older), by srcshift bits; big_pix shifts the other way.
void sourceShift( std::uint64_t srcd1,
                  std::uint64_t srcd2,
                  std::uint32_t srcshift,
                  bool bigPix,
                  std::uint32_t& lo,
                  std::uint32_t& hi )
{
  // shsrc[127:0] = {srcd1, srcd2}; oneb[127:64] the 64 bit window it is rotated to
  std::uint32_t const n = ( srcshift >> 3 ) & 7;
  std::uint32_t const r = bigPix ? 64 + 8 * n : 64 - 8 * n; // a right rotation of shsrc
  std::uint64_t window = 0;
  if ( r == 64 )
    window = srcd1;
  else if ( r < 64 )
    window = ( srcd2 >> r ) | ( srcd1 << ( 64 - r ) );
  else if ( r < 128 )
    window = ( srcd1 >> ( r - 64 ) ) | ( srcd2 << ( 128 - r ) );
  std::uint32_t const shift = bigPix ? ( 8 - ( srcshift & 7 ) ) & 7 : srcshift & 7;
  std::uint32_t const oneo = rotl8( static_cast<std::uint32_t>( window ) & 0xffu, shift );
  lo = ( static_cast<std::uint32_t>( window ) & 0xffffff00u ) | oneo;
  hi = static_cast<std::uint32_t>( window >> 32 );
}

// add16sat.v
std::uint32_t add16sat( std::uint32_t a, std::uint32_t b, bool cin, bool sat, bool eightbit, bool hicinh, bool& co )
{
  std::uint32_t const c = cin ? 1u : 0u;
  std::uint32_t const s0 = ( a & 0xff ) + ( b & 0xff ) + c;
  std::uint32_t const s1 = ( a & 0xffff ) + ( b & 0xffff ) + c;
  std::uint32_t const s2 = ( ( a >> 8 ) & 0xff ) + ( ( b >> 8 ) & 0xff );
  std::uint32_t const s3 = ( ( a >> 12 ) & 0xf ) + ( ( b >> 12 ) & 0xf );
  bool const carry0 = bit( s0, 8 ), carry1 = bit( s1, 16 ), carry2 = bit( s2, 8 ), carry3 = bit( s3, 4 );
  co = hicinh ? carry3 : eightbit ? carry2 : carry1;
  bool const btop = eightbit ? bit( b, 7 ) : bit( b, 15 );
  bool const ctop = eightbit ? carry0 : co;
  bool const saturate = sat && ( btop != ctop );
  bool const hisaturate = !eightbit && saturate;
  std::uint32_t const q1 = s1 & 0xffff, q2 = ( s2 & 0xff ) << 8, q3 = ( s3 & 0xf ) << 12;
  std::uint32_t r = saturate ? ( ctop ? 0xffu : 0u ) : s0 & 0xff;
  if ( hisaturate )
    r |= ctop ? 0xff00u : 0u;
  else
  {
    r |= hicinh ? q3 : eightbit ? q2 & 0xf000 : q1 & 0xf000;
    r |= eightbit ? q2 & 0x0f00 : q1 & 0x0f00;
  }
  return r;
}

} // namespace

// The combinational logic of a cycle: what the flip-flops take at its end, and what it drives.
struct Blitter::Comb
{
  // blitgpu.v
  bool cmdld, countld, stopld, a1ptrld, a1fracld, a2ptrld;
  std::uint32_t intld, zedld, srcd1ld, srcz1ld, srcz2ld, patdld;
  // the loops
  bool step, stepP1, instart, indone, inner0, readreq, writereq, blitack, readAck;
  bool idlei, sreadxi, szreadxi, sreadi, szreadi, dreadi, dzreadi, dwritei, dzwritei, aticki0;
  bool srcaAddi, gena2i, oldoutside, denat1, denat3, dpipe1t0, zpipe1t0, srcdreadt, dnrt5;
  bool srcdpend, srcdxpend, srczpend, srczxpend, dstdpend, dstzpend;
  bool a1updatei, a1fupdatei, a2updatei, a1Addi, a2Addi, addaregi, srczread;
  std::uint32_t addaseli, addbseli, dstartb, dendb, shfti, cnti, address;
  bool uflowt, icntena, nowrite;
  // the pointers
  std::uint32_t dataX, dataY;
  bool cxt0, cyt0;
  // the data path
  std::uint32_t srcd1i[4], srcz1i[4], srcz2i[4];
  std::uint64_t patdi;
  std::uint32_t srczp[2];
  bool zpipe1, dpipe1;
  std::uint32_t co;
  std::uint32_t bcompselt;
  bool bcompbitpt;
  std::uint64_t wdata;
  bool collidea;
};

void Blitter::latch( Inputs const& in )
{
  if ( mRepeat.valid && in == mRepeat.in )
  {
    mLatched = false;
    return;
  }
  mLatched = takeLatches( in );
  if ( mLatched )
    mRepeat.valid = false;
}

// The registers the GPU writes (the ldp1q latches of address.v and data.v), and the data the loop
// latches (dstd, dstz, srcd2, patd), at tlw.
bool Blitter::takeLatches( Inputs const& in )
{
  bool changed = false;
  auto const put = [&changed]( auto& field, auto value )
  {
    auto const v = static_cast<std::remove_reference_t<decltype( field )>>( value );
    if ( field != v )
    {
      field = v;
      changed = true;
    }
  };
  std::uint32_t const idx = ( in.address >> 2 ) & 7;
  std::uint32_t const grp = ( in.address >> 5 ) & 7;
  auto const sel = [&]( std::uint32_t g, std::uint32_t k )
  { return in.memw && grp == g && idx == k && ( g < 2 || !in.back ); };
  std::uint32_t const din = in.din;
  std::uint32_t const lo16 = din & 0xffff, hi16 = din >> 16;
  if ( sel( 0, 0 ) )
    put( mA1Base, ( din >> 3 ) & 0x1fffff );
  if ( sel( 0, 1 ) )
    put( mA1Flags, din & 0x1fffff );
  if ( sel( 0, 2 ) )
  {
    put( mA1WinX, din & 0x7fff );
    put( mA1WinY, ( din >> 16 ) & 0x7fff );
  }
  if ( sel( 0, 4 ) )
  {
    put( mA1StepX, lo16 );
    put( mA1StepY, hi16 );
  }
  if ( sel( 0, 5 ) )
  {
    put( mA1StepfX, lo16 );
    put( mA1StepfY, hi16 );
  }
  if ( sel( 0, 7 ) )
  {
    put( mA1IncX, lo16 );
    put( mA1IncY, hi16 );
  }
  if ( sel( 1, 0 ) )
  {
    put( mA1IncfX, lo16 );
    put( mA1IncfY, hi16 );
  }
  if ( sel( 1, 1 ) )
    put( mA2Base, ( din >> 3 ) & 0x1fffff );
  if ( sel( 1, 2 ) )
    put( mA2Flags, din & 0x1fffff );
  if ( sel( 1, 3 ) )
  {
    put( mA2MaskX, lo16 );
    put( mA2MaskY, hi16 );
  }
  if ( sel( 1, 5 ) )
  {
    put( mA2StepX, lo16 );
    put( mA2StepY, hi16 );
  }
  if ( sel( 3, 4 ) )
    put( mIinc, din );
  if ( sel( 3, 5 ) )
    put( mZinc, din );

  // the loop's reads: the data bus while it has or asks for the bus, else what the GPU writes
  // (local_mux)
  bool const atick1 = bit( mAtick, 1 );
  bool const active = mWactive || mRactive;
  bool const breqt = mOidle && !active;
  bool const breq = !( breqt || mStopped );
  bool const readAck = mWaitack && in.ack;
  bool const busOwn = in.back || breq;
  std::uint32_t const load0 = busOwn ? static_cast<std::uint32_t>( in.data ) : din;
  std::uint32_t const load1 = busOwn ? static_cast<std::uint32_t>( in.data >> 32 ) : din;
  std::uint32_t const dstdldg = ( sel( 2, 2 ) ? 1u : 0u ) | ( sel( 2, 3 ) ? 2u : 0u );
  std::uint32_t const dstzldg = ( sel( 2, 4 ) ? 1u : 0u ) | ( sel( 2, 5 ) ? 2u : 0u );
  bool const dstdread = mDstdpend && readAck;
  bool const dstzread = mDstzpend && readAck;
  std::uint32_t const dstdld = dstdread ? 3u : dstdldg;
  std::uint32_t const dstzld = dstzread ? 3u : dstzldg;
  if ( bit( dstdld, 0 ) )
    put( mDstd[0], load0 );
  if ( bit( dstdld, 1 ) )
    put( mDstd[1], load1 );
  if ( bit( dstzld, 0 ) )
    put( mDstz[0], load0 );
  if ( bit( dstzld, 1 ) )
    put( mDstz[1], load1 );
  bool const srcdreadt = ( mSrcdxpend || mSrcdpend ) && readAck;
  bool const srcdread = ( mSrcshade && mSrcdreadd ) || srcdreadt;
  if ( srcdread )
  {
    put( mSrcd2[0], mSd1[0] | mSd1[1] << 16 );
    put( mSrcd2[1], mSd1[2] | mSd1[3] << 16 );
  }
  bool const dpipe0 = ( atick1 && mDwrite ) || !mGourd;
  if ( dpipe0 )
  {
    put( mPatd[0], static_cast<std::uint32_t>( mPdu ) );
    put( mPatd[1], static_cast<std::uint32_t>( mPdu >> 32 ) );
  }
  return changed;
}

Blitter::Outputs Blitter::outputs() const
{
  if ( mRepeat.valid )
    return mRepeat.out;
  Outputs o;
  bool const active = mWactive || mRactive;
  bool const breqt = mOidle && !active;
  o.breq = ( !( breqt || mStopped || mBushi ) ? 1u : 0u ) | ( !( breqt || mStopped || !mBushi ) ? 2u : 0u );
  o.mreq = active;
  o.read = mRactive;
  o.address = mBlita;

  std::uint32_t const a1addx = ( mA1Flags >> 16 ) & 3, a2addx = ( mA2Flags >> 16 ) & 3;
  std::uint32_t const pixsize = mDsta2 ? ( mA2Flags >> 3 ) & 7 : ( mA1Flags >> 3 ) & 7;
  bool const phraseMode = ( mDsta2 && a2addx == 0 ) || ( !mDsta2 && a1addx == 0 );
  bool const destCycle1 = mDread1 || mDzread1 || mDwrite1 || mDzwrite1;
  bool const pcsela2 = mDsta2 == destCycle1;
  bool const phraseCycle = ( pcsela2 && a2addx == 0 ) || ( !pcsela2 && a1addx == 0 );
  bool const fontread = ( mSread1 || mSreadx1 ) && mBcompen;
  bool const wt0 = !( ( bit( pixsize, 2 ) && !fontread ) || phraseCycle );
  bool const wt1 = pixsize == 4 && !phraseCycle && !fontread;
  bool const wt2 = pixsize == 5 && !phraseCycle && !fontread;
  std::uint32_t const wt = ( wt0 ? 1u : 0u ) | ( wt1 ? 2u : 0u ) | ( wt2 ? 4u : 0u ) | ( phraseCycle ? 8u : 0u );
  std::uint32_t const de = mDendp >> 3, ds = mDstartp >> 3;
  std::uint32_t const pwidth = ( ( de - ds ) & 7 ) | ( de == 0 && ds == 0 ? 8u : 0u );
  o.width = phraseCycle && mWactive ? pwidth : wt;
  o.justify = !( !fontread && phraseMode );
  return o;
}

std::uint32_t Blitter::read( std::uint32_t address, bool back ) const
{
  std::uint32_t const a = ( address >> 2 ) & 31;
  bool const a2mask = bit( mA2Flags, 15 );
  std::uint32_t const a2x = a2mask ? mA2Xr & mA2MaskX : mA2Xr;
  std::uint32_t const a2y = a2mask ? mA2Yr & mA2MaskY : mA2Yr;
  switch ( a )
  {
  case 0x01: // A1 pointer, read at $F02204 (Tom's bug 10)
    return mA1Y << 16 | mA1Xt;
  case 0x06: // A1 fractional pointer
    return mA1FracY << 16 | mA1FracX;
  case 0x0b: // A2 pointer, read at $F0222C (bug 10)
    return a2y << 16 | a2x;
  case 0x0e:
  {
    // the status: idle, stopped, the inner loop's states, the outer loop's, the inner count
    bool const active = mWactive || mRactive;
    bool const breqt = mOidle && !active;
    bool const breq = !( breqt || mStopped );
    bool const blitIdle = !( breq || back );
    return ( blitIdle ? 1u : 0u ) | ( mStopped ? 2u : 0u ) | ( mIdle ? 4u : 0u ) | ( mSreadx ? 8u : 0u ) |
           ( mSzreadx ? 0x10u : 0u ) | ( mSread ? 0x20u : 0u ) | ( mSzread ? 0x40u : 0u ) | ( mDread ? 0x80u : 0u ) |
           ( mDzread ? 0x100u : 0u ) | ( mDwrite ? 0x200u : 0u ) | ( mDzwrite ? 0x400u : 0u ) |
           ( mOidle ? 0x800u : 0u ) | ( mInner ? 0x1000u : 0u ) | ( mOa1fupdate ? 0x2000u : 0u ) |
           ( mOa1update ? 0x4000u : 0u ) | ( mOa2update ? 0x8000u : 0u ) | mIcount << 16;
  }
  default:
    return 0;
  }
}

Blitter::Comb Blitter::evaluate( Inputs const& in ) const
{
  Comb c{};
  std::uint32_t const din = in.din;

  // --- blitgpu.v: the GPU's write decoded -------------------------------------------------------
  std::uint32_t const idx = ( in.address >> 2 ) & 7;
  std::uint32_t const grp = ( in.address >> 5 ) & 7;
  auto const sel = [&]( std::uint32_t g, std::uint32_t k )
  { return in.memw && grp == g && idx == k && ( g < 2 || !in.back ); };
  bool const a1ptrldg = sel( 0, 3 ), a1fracldg = sel( 0, 6 ), a2ptrldg = sel( 1, 4 );
  c.cmdld = sel( 1, 6 );
  c.countld = sel( 1, 7 );
  c.stopld = sel( 3, 6 );
  std::uint32_t const srcd1ldg = ( sel( 2, 0 ) ? 1u : 0u ) | ( sel( 2, 1 ) ? 2u : 0u );
  std::uint32_t const srcz1ldg = ( sel( 2, 6 ) ? 1u : 0u ) | ( sel( 2, 7 ) ? 2u : 0u );
  c.srcz2ld = ( sel( 3, 0 ) ? 1u : 0u ) | ( sel( 3, 1 ) ? 2u : 0u );
  std::uint32_t const patdldg = ( sel( 3, 2 ) ? 1u : 0u ) | ( sel( 3, 3 ) ? 2u : 0u );
  c.intld =
      ( sel( 3, 7 ) ? 1u : 0u ) | ( sel( 4, 0 ) ? 2u : 0u ) | ( sel( 4, 1 ) ? 4u : 0u ) | ( sel( 4, 2 ) ? 8u : 0u );
  c.zedld =
      ( sel( 4, 3 ) ? 1u : 0u ) | ( sel( 4, 4 ) ? 2u : 0u ) | ( sel( 4, 5 ) ? 4u : 0u ) | ( sel( 4, 6 ) ? 8u : 0u );

  // --- address.v's fields -----------------------------------------------------------------------
  std::uint32_t const a1pitch = mA1Flags & 3, a2pitch = mA2Flags & 3;
  std::uint32_t const a1pixsize = ( mA1Flags >> 3 ) & 7, a2pixsize = ( mA2Flags >> 3 ) & 7;
  std::uint32_t const a1zoffset = ( mA1Flags >> 6 ) & 3, a2zoffset = ( mA2Flags >> 6 ) & 3;
  std::uint32_t const a1width = ( mA1Flags >> 9 ) & 63, a2width = ( mA2Flags >> 9 ) & 63;
  bool const a2mask = bit( mA2Flags, 15 );
  std::uint32_t const a1addx = ( mA1Flags >> 16 ) & 3, a2addx = ( mA2Flags >> 16 ) & 3;
  bool const a1addy = bit( mA1Flags, 18 ), a2addy = bit( mA2Flags, 18 );
  bool const a1xsign = bit( mA1Flags, 19 ), a2xsign = bit( mA2Flags, 19 );
  bool const a1ysign = bit( mA1Flags, 20 ), a2ysign = bit( mA2Flags, 20 );
  std::uint32_t const a1x = mA1Xt, a1y = mA1Y;
  std::uint32_t const a2x = a2mask ? mA2Xr & mA2MaskX : mA2Xr;
  std::uint32_t const a2y = a2mask ? mA2Yr & mA2MaskY : mA2Yr;

  // --- the mode -----------------------------------------------------------------------------------
  bool const dsta2 = mDsta2;
  std::uint32_t const pixsize = dsta2 ? a2pixsize : a1pixsize;
  bool const phraseMode = ( dsta2 && a2addx == 0 ) || ( !dsta2 && a1addx == 0 );
  bool const pixel8 = pixsize == 3, pixel16 = pixsize == 4, pixel32 = pixsize == 5;
  std::uint32_t const dstxp = dsta2 ? a2x : a1x;
  bool const atick0 = bit( mAtick, 0 ), atick1 = bit( mAtick, 1 );

  // --- mcontrol.v, outer.v: the bus and the loops' control ---------------------------------------
  bool const active = mWactive || mRactive;
  c.blitack = in.ack && in.back;
  bool const memready = c.blitack && active;
  bool const mready = memready || !active;
  c.readAck = mWaitack && in.ack;
  bool const outer0 = mOcount == 0;
  bool const breqt = mOidle && !active;
  bool const breqAny = !( breqt || mStopped );
  bool const inhibent = !( mZmode != 0 || mBcompen || mDcompen );
  bool const inhiben = !( inhibent || phraseMode || mBkgwren );

  // --- inner.v: the reads under way, the step ---------------------------------------------------
  bool const ra = c.readAck;
  bool const srcdxackN = !ra;
  bool const srczxackN = !ra || mSrcdxpend;
  bool const srcdackN = !ra || mSrcdxpend || mSrczxpend;
  bool const srczackN = !ra || mSrcdpend || mSrcdxpend || mSrczxpend;
  bool const dstdackN = !ra || mSrcdpend || mSrczpend || mSrcdxpend || mSrczxpend;
  bool const dstzackN = !ra || mDstdpend || mSrcdpend || mSrczpend || mSrcdxpend || mSrczxpend;
  c.dnrt5 = ( mDzread1 && dstzackN ) || ( mDread1 && !mDstenz && dstdackN ) ||
            ( mSzread1 && !mDsten && !mDstenz && srczackN ) ||
            ( mSread1 && !mSrcenz && !mDsten && !mDstenz && srcdackN ) ||
            ( mSreadx1 && !mSrcenz && !mSrcen && !mDsten && !mDstenz && srcdxackN );
  bool const dnreadyd = ( mSrcshade && mGourz ) || inhiben;
  bool const dnreadyN = dnreadyd ? !mDnrt6 : !c.dnrt5;
  bool const aready = !atick0;
  bool const steptN = !( aready && mready && dnreadyN && !mIdle );
  c.inner0 = mIcount == 0 || mUnderflow;
  bool const inner0 = c.inner0;
  // indone: the inner states are one hot, so that idle excludes the writes it takes the step of
  bool const step0 = !steptN;
  c.indone = ( mDzwrite && step0 && inner0 ) || ( mDwrite && step0 && !mDstwrz && inner0 );
  bool const indone = c.indone;
  bool const innert0 = !( mOidle && mGo );
  bool const innert2 = !( indone && mInner && !outer0 && !mUpda1f && !mUpda1 && !mUpda2 );
  bool const innert3 = !( mOa1update && !mUpda2 );
  bool const innert4 = !mOa2update;
  c.instart = !( innert0 && innert2 && innert3 && innert4 );
  bool const startup = c.instart || mStupl;
  bool const istept1 = !( ( mDwrite1 || mDzwrite1 ) && !mready );
  bool const istepN = !( mIdle && startup && istept1 );
  c.step = !( steptN && istepN );
  bool const step = c.step;
  c.stepP1 = !( !( aready && mready && dnreadyN ) && istepN );

  // the next inner state
  c.idlei = ( mIdle && !step ) || ( mDzwrite && step && inner0 ) || ( mDwrite && step && !mDstwrz && inner0 );
  c.sreadxi = ( mIdle && step && mSrcenx ) || ( mSreadx && !step );
  c.szreadxi = ( mSreadx && step && mSrcenz ) || ( mSzreadx && !step );
  c.sreadi = ( mSzreadx && step ) || ( mSreadx && step && !mSrcenz && mSrcen ) ||
             ( mIdle && step && !mSrcenx && mSrcen ) || ( mDzwrite && step && !inner0 && mSrcen ) ||
             ( mDwrite && step && !mDstwrz && !inner0 && mSrcen ) || ( mSread && !step );
  c.szreadi = ( mSread && step && mSrcenz ) || ( mSzread && !step );
  c.dreadi = ( mSzread && step && mDsten ) || ( mSread && step && !mSrcenz && mDsten ) ||
             ( mSreadx && step && !mSrcenz && !mSrcen && mDsten ) ||
             ( mIdle && step && !mSrcenx && !mSrcen && mDsten ) ||
             ( mDzwrite && step && !inner0 && !mSrcen && mDsten ) ||
             ( mDwrite && step && !mDstwrz && !inner0 && !mSrcen && mDsten ) || ( mDread && !step );
  c.dzreadi = ( mDread && step && mDstenz ) || ( mSzread && step && !mDsten && mDstenz ) ||
              ( mSread && step && !mSrcenz && !mDsten && mDstenz ) ||
              ( mSreadx && step && !mSrcenz && !mSrcen && !mDsten && mDstenz ) ||
              ( mIdle && step && !mSrcenx && !mSrcen && !mDsten && mDstenz ) ||
              ( mDzwrite && step && !inner0 && !mSrcen && !mDsten && mDstenz ) ||
              ( mDwrite && step && !mDstwrz && !inner0 && !mSrcen && !mDsten && mDstenz ) || ( mDzread && !step );
  c.dwritei = ( mDzread && step ) || ( mDread && step && !mDstenz ) || ( mSzread && step && !mDsten && !mDstenz ) ||
              ( mSread && step && !mSrcenz && !mDsten && !mDstenz ) ||
              ( mSreadx && step && !mSrcenz && !mSrcen && !mDsten && !mDstenz ) ||
              ( mIdle && step && !mSrcenx && !mSrcen && !mDsten && !mDstenz ) ||
              ( mDzwrite && step && !inner0 && !mSrcen && !mDsten && !mDstenz ) ||
              ( mDwrite && step && !mDstwrz && !inner0 && !mSrcen && !mDsten && !mDstenz ) || ( mDwrite && !step );
  c.dzwritei = ( mDzwrite && !step ) || ( mDwrite && step && mDstwrz );
  c.aticki0 = ( mAtick == 0 && step && !indone ) || ( atick1 && step && !indone );
  bool const aticki0 = c.aticki0;

  // outer.v: the pointers' updates at the end of an inner loop
  c.a1fupdatei = mInner && indone && !outer0 && mUpda1f;
  c.a1updatei = mOa1fupdate || ( mInner && indone && !outer0 && !mUpda1f && mUpda1 );
  c.a2updatei = ( mOa1update && mUpda2 ) || ( mInner && indone && !outer0 && !mUpda1f && !mUpda1 && mUpda2 );

  // inner.v: the address generator's source and destination
  c.srcaAddi = c.szreadxi || c.szreadi || ( c.sreadxi && !mSrcenz ) || ( c.sreadi && !mSrcenz );
  bool const dstaAddi = c.dzwritei || ( c.dwritei && !mDstwrz );
  bool const gensrc = c.sreadxi || c.szreadxi || c.sreadi || c.szreadi;
  bool const gendst = c.dreadi || c.dzreadi || c.dwritei || c.dzwritei;
  c.gena2i = ( gensrc && !dsta2 ) || ( gendst && dsta2 );
  bool const zaddr = mSzreadx || mSzread || mDzread || mDzwrite;
  bool const apipe = atick1;

  // --- dcontrol.v -----------------------------------------------------------------------------------
  bool const atickboth = mAtick != 0;
  bool const shadeadd = mDwrite && mSrcshade;
  std::uint32_t const daddasel = ( ( mDwrite && mGourd && atick1 ) || ( mDzwrite && mGourz && atick0 ) ? 1u : 0u ) |
                                 ( mDzwrite && mGourz && atickboth ? 2u : 0u ) |
                                 ( mGourd || mGourz || shadeadd ? 4u : 0u );
  std::uint32_t const daddbsel =
      ( ( mDwrite && mGourd && atick1 ) || ( mDzwrite && mGourz && atick1 ) || shadeadd ? 1u : 0u ) |
      ( mDzwrite && mGourz && atickboth ? 2u : 0u ) |
      ( ( mDwrite && mGourd && atickboth ) || ( mDzwrite && mGourz && atickboth ) || shadeadd ? 4u : 0u );
  bool const dm0t1 = mTopben == mTopnen;
  bool const daddmode0 = ( mDzwrite && mGourz && atick1 ) || !dm0t1 || ( mDwrite && mGourd && atick1 && dm0t1 ) ||
                         ( !mGourd && !mGourz && dm0t1 ) || ( shadeadd && dm0t1 );
  bool const daddmode1 =
      ( mDwrite && mGourd && atick1 && !mTopben ) || ( !mGourd && !mGourz && !mTopben ) || ( shadeadd && !mTopben );
  bool const daddmode2 = ( !mGourd && !mGourz ) || shadeadd;
  bool const patfadd = mDwrite && mGourd && atick0;
  bool const patdadd = mDwrite && mGourd && atick1;
  bool const srcz1add = mDzwrite && mGourz && atick1;
  bool const srcz2add = mDzwrite && mGourz && atick0;
  bool const srcshadd = mSrcdreadd && mSrcshade;
  bool const daddqSel = patfadd || patdadd || srcz1add || srcz2add || srcshadd;
  bool const dataSel0 = mDzwrite1d || !( mPatdsel || mAdddsel );
  bool const dataSel1 = mDzwrite1d || mAdddsel;

  // inner.v: the reads requested and taken
  c.readreq = ( mSreadx || mSzreadx || mSread || mSzread || mDread || mDzread ) && step;
  c.srcdreadt = ( mSrcdxpend || mSrcdpend ) && ra;
  bool const srcdread = ( mSrcshade && mSrcdreadd ) || c.srcdreadt;
  c.srczread = ( mSrczpend || mSrczxpend ) && ra;
  c.srcd1ld = srcdread || patfadd ? 3u : srcd1ldg;
  c.srcz1ld = c.srczread || srcz1add ? 3u : srcz1ldg;
  c.patdld = patdadd ? 3u : patdldg;
  c.srcdpend = ( c.readreq && mSread ) || ( mSrcdpend && srcdackN );
  c.srcdxpend = ( c.readreq && mSreadx ) || ( mSrcdxpend && srcdxackN );
  c.srczpend = ( c.readreq && mSzread ) || ( mSrczpend && srczackN );
  c.srczxpend = ( c.readreq && mSzreadx ) || ( mSrczxpend && srczxackN );
  c.dstdpend = ( c.readreq && mDread ) || ( mDstdpend && dstdackN );
  c.dstzpend = ( c.readreq && mDzread ) || ( mDstzpend && dstzackN );

  // --- acontrol.v: the address adder's operands ------------------------------------------------------
  auto const xp = []( std::uint32_t ps )
  {
    bool const p0 = bit( ps, 0 ), p1 = bit( ps, 1 ), p2 = bit( ps, 2 );
    bool const xp1 = !( p0 || p1 ) || ( p0 && p1 && !p2 );
    bool const xp2 = !( p0 && p1 ) && !p2;
    return ( p0 ? 1u : 0u ) | ( xp1 ? 2u : 0u ) | ( xp2 ? 4u : 0u );
  };
  std::uint32_t const a1xp = xp( a1pixsize ), a2xp = xp( a2pixsize );
  auto const xconst = []( std::uint32_t p, std::uint32_t addx )
  { return ( ( bit( addx, 0 ) ? 0u : p ) | ( bit( addx, 1 ) ? 7u : 0u ) ) & 7u; };
  std::uint32_t const addaXconst = mA2Add ? xconst( a2xp, a2addx ) : xconst( a1xp, a1addx );
  bool const addaYconst = a1addy;
  c.a1Addi = dsta2 ? c.srcaAddi : dstaAddi;
  c.a2Addi = dsta2 ? dstaAddi : c.srcaAddi;
  c.addaseli = ( c.a1fupdatei || ( c.a1Addi && aticki0 && a1addx == 3 ) ? 1u : 0u ) |
               ( c.a1Addi && a1addx == 3 ? 2u : 0u ) | ( c.a2updatei ? 4u : 0u );
  c.addaregi = ( c.a1Addi && a1addx == 3 ) || ( c.a2Addi && a2addx == 3 ) || c.a1updatei || c.a1fupdatei || c.a2updatei;
  c.addbseli =
      ( c.a2updatei || c.a2Addi ? 1u : 0u ) | ( c.a1fupdatei || ( c.a1Addi && aticki0 && a1addx == 3 ) ? 2u : 0u );
  std::uint32_t const modx = mA2Add ? ( mA2Add && a2addx == 0 ? a2xp : 0u ) : ( mA1Add && a1addx == 0 ? a1xp : 0u );
  bool const addqsel = mA1Add || mA2Add || mAa1update || mAa1fupdate || mAa2update;
  bool const subaX = ( mA1Add && a1addx == 1 && a1xsign ) || ( mA2Add && a2addx == 1 && a2xsign );
  bool const subaY = ( mA1Add && a1addy && a1ysign ) || ( mA2Add && a2addy && a2ysign );
  bool const a1ptrldi = mAa1update || ( atick1 && mA1Add );
  bool const a1fracldi = mAa1fupdate || ( atick0 && mA1Add && a1addx == 3 );
  bool const a2ptrldi = mAa2update || ( atick1 && mA2Add );
  c.a1ptrld = a1ptrldi || a1ptrldg;
  c.a1fracld = a1fracldi || a1fracldg;
  c.a2ptrld = a2ptrldi || a2ptrldg;

  // --- address.v: the adder (addamux.v, addradd.v) ---------------------------------------------------
  std::uint32_t addarX = 0, addarY = 0;
  if ( bit( mAddasel, 2 ) )
  {
    addarX = mA2StepX;
    addarY = mA2StepY;
  }
  else
    switch ( mAddasel & 3 )
    {
    case 0:
      addarX = mA1StepX;
      addarY = mA1StepY;
      break;
    case 1:
      addarX = mA1StepfX;
      addarY = mA1StepfY;
      break;
    case 2:
      addarX = mA1IncX;
      addarY = mA1IncY;
      break;
    default:
      addarX = mA1IncfX;
      addarY = mA1IncfY;
      break;
    }
  std::uint32_t const addacX = ( 1u << addaXconst ) & 0x7fu;
  std::uint32_t const addacY = addaYconst ? 1u : 0u;
  std::uint32_t const addaX = ( subaX ? 0xffffu : 0u ) ^ ( mAddareg ? addarX : addacX );
  std::uint32_t const addaY = ( subaY ? 0xffffu : 0u ) ^ ( mAddareg ? addarY : addacY );
  std::uint32_t addbX = 0, addbY = 0;
  switch ( mAddbsel & 3 )
  {
  case 1:
    addbX = a2x;
    addbY = a2y;
    break;
  case 2:
    addbX = mA1FracX;
    addbY = mA1FracY;
    break;
  default:
    addbX = a1x;
    addbY = a1y;
    break;
  }
  std::uint32_t const sumX = addaX + addbX + ( mCxt1 != subaX ? 1u : 0u );
  std::uint32_t const sumY = addaY + addbY + ( mCyt1 != subaY ? 1u : 0u );
  std::uint32_t addqX = sumX & 0xffff;
  if ( modx <= 6 )
    addqX &= ~( ( 1u << modx ) - 1u );
  std::uint32_t const addqY = sumY & 0xffff;
  c.cxt0 = bit( sumX, 16 ) && c.a1fracld;
  c.cyt0 = bit( sumY, 16 ) && c.a1fracld;
  c.dataX = addqsel ? addqX : din & 0xffff;
  c.dataY = addqsel ? addqY : din >> 16;

  // --- addrgen.v: the address of the pixel --------------------------------------------------------
  {
    bool const g = mGena2;
    std::uint32_t const x = g ? a2x : a1x;
    std::uint32_t const y = ( g ? a2y : a1y ) & 0xfff;
    std::uint32_t const width = g ? a2width : a1width;
    std::uint32_t const ps = g ? a2pixsize : a1pixsize;
    std::uint32_t const pitch = g ? a2pitch : a1pitch;
    std::uint32_t const base = g ? mA2Base : mA1Base;
    std::uint32_t const zoffset = g ? a2zoffset : a1zoffset;
    std::uint32_t const ym1 = bit( width, 1 ) ? y : 0u;
    std::uint32_t const ym2 = bit( width, 0 ) ? y : 0u;
    std::uint64_t const ytm = ( ym2 + ( ym1 << 1 ) + ( y << 2 ) ) & 0x7fff;
    std::uint64_t const yaT = ( ytm << ( 9 + ( width >> 2 ) ) ) & ( ( std::uint64_t{ 1 } << 35 ) - 1 );
    std::uint32_t const ya = ( width >> 4 ) == 3 ? 0u : static_cast<std::uint32_t>( yaT >> 11 ) & 0xffffff;
    std::uint32_t const pa = ( ya + x ) & 0x1ffffff;
    std::uint32_t const pst = ( ps >> 1 ) == 3 ? ps & 5 : ps;
    std::uint32_t const pixa = static_cast<std::uint32_t>( ( static_cast<std::uint64_t>( pa ) << pst ) & 0x7ffffff );
    bool const pt0 = pitch == 1, pt1 = pitch == 2;
    std::uint32_t const pt = ( pt0 ? 1u : 0u ) | ( pt1 ? 2u : 0u );
    std::uint32_t const phradr = ( pixa >> 6 ) << pt & 0x1fffff;
    std::uint32_t const shup = pitch == 3 ? ( pixa >> 6 ) & 0xfffff : 0u;
    std::uint32_t const za = zaddr ? zoffset : 0u;
    std::uint32_t const addr = ( ( shup << 1 ) + base + phradr + za ) & 0x1fffff;
    std::uint32_t const addrgen = addr << 3 | ( ( pixa >> 3 ) & 7 );
    c.address = apipe ? addrgen : mAddressi;

    // acontrol.v: the first and last bits written of the phrase
    std::uint32_t const pixaLow = pixa & 7;
    bool const smask3 = bit( dstxp, 0 ) && pixel8;
    bool const smask4 = ( bit( dstxp, 1 ) && pixel8 ) || ( bit( dstxp, 0 ) && pixel16 );
    bool const smask5 =
        ( bit( dstxp, 2 ) && pixel8 ) || ( bit( dstxp, 1 ) && pixel16 ) || ( bit( dstxp, 0 ) && pixel32 );
    std::uint32_t const smask = ( smask3 ? 1u : 0u ) | ( smask4 ? 2u : 0u ) | ( smask5 ? 4u : 0u );
    std::uint32_t const dstarta = phraseMode ? smask << 3 : pixaLow;
    c.dstartb = atick1 ? dstarta : mDstartbl;
    std::uint32_t const pseq = ( dstxp ^ mA1WinX ) & 0x7ffe;
    bool const rmpt0 = bit( pseq, 1 ) && pixsize == 5;
    bool const rmpt1 = bit( pseq, 2 ) && ( pixsize >> 1 ) == 2;
    bool const penden = !( rmpt0 || rmpt1 || ( pseq >> 3 ) != 0 );
    bool const wmb3 = bit( mA1WinX, 0 ) && pixel8;
    bool const wmb4 = ( bit( mA1WinX, 1 ) && pixel8 ) || ( bit( mA1WinX, 0 ) && pixel16 );
    bool const wmb5 =
        ( bit( mA1WinX, 2 ) && pixel8 ) || ( bit( mA1WinX, 1 ) && pixel16 ) || ( bit( mA1WinX, 0 ) && pixel32 );
    std::uint32_t const windowMask = penden ? ( wmb3 ? 1u : 0u ) | ( wmb4 ? 2u : 0u ) | ( wmb5 ? 4u : 0u ) : 0u;
    bool const imb3 = bit( mIcount, 0 ) && pixel8;
    bool const imb4 = ( bit( mIcount, 1 ) && pixel8 ) || ( bit( mIcount, 0 ) && pixel16 );
    bool const imb5 =
        ( bit( mIcount, 2 ) && pixel8 ) || ( bit( mIcount, 1 ) && pixel16 ) || ( bit( mIcount, 0 ) && pixel32 );
    std::uint32_t const innerMask = inner0 ? ( imb3 ? 1u : 0u ) | ( imb4 ? 2u : 0u ) | ( imb5 ? 4u : 0u ) : 0u;
    std::uint32_t const wm = windowMask | ( windowMask == 0 ? 8u : 0u );
    std::uint32_t const im = innerMask | ( innerMask == 0 ? 8u : 0u );
    std::uint32_t const emask = wm > im ? innerMask : windowMask;
    std::uint32_t const pm = pixsize <= 5 ? 1u << pixsize : 0u;
    std::uint32_t const pma = ( pm + pixaLow ) & 63;
    std::uint32_t const denda = phraseMode ? emask << 3 : pma;
    c.dendb = atick1 ? denda : mDendbl;
  }

  // the source's alignment
  {
    std::uint32_t const srcxp = ( dsta2 ? a1x : a2x ) & 63;
    std::uint32_t const shftt = ( dstxp - srcxp ) & 63;
    std::uint32_t const pst = ( pixsize >> 1 ) == 3 ? pixsize & 5 : pixsize;
    std::uint32_t shftv = ( shftt << pst ) & 63;
    if ( bit( pixsize, 1 ) )
      shftv &= ~2u;
    bool const pobb0 = ( pixel8 || pixel16 || pixel32 ) && bit( dstxp, 0 );
    bool const pobb1 = ( pixel8 || pixel16 ) && bit( dstxp, 1 );
    bool const pobb2 = pixel8 && bit( dstxp, 2 );
    std::uint32_t const pobb = ( pobb0 ? 1u : 0u ) | ( pobb1 ? 2u : 0u ) | ( pobb2 ? 4u : 0u );
    bool const pobbsel = phraseMode && mBcompen;
    bool const loshenN = !( mSrcen || pobbsel );
    std::uint32_t const loshd = pobbsel ? pobb : shftv & 7;
    std::uint32_t const lo = loshenN ? 0u : ( mSshftld ? loshd : mSrcshift & 7 );
    bool const hishenN = !( mSrcen && phraseMode );
    std::uint32_t const hi = hishenN ? 0u : ( mSshftld ? shftv >> 3 : mSrcshift >> 3 );
    c.shfti = lo | hi << 3;
  }

  // --- inner_cnt.v -------------------------------------------------------------------------------------
  {
    std::uint32_t const cntval = c.countld ? din & 0xffff : mCntvall;
    bool const inct0 = bit( dstxp, 0 );
    bool const inct1 = bit( dstxp, 0 ) != bit( dstxp, 1 );
    bool const incc1 = !bit( dstxp, 0 ) && !bit( dstxp, 1 );
    bool const inct2 = !bit( dstxp, 2 ) != incc1;
    bool const incN0 = phraseMode && !inct0;
    bool const inc1t4 = !( !( ( pixel8 || pixel16 ) && inct1 ) && !( pixel32 && !inct0 ) );
    bool const incN1 = !( phraseMode && inc1t4 );
    bool const inc2t2 = ( pixel8 && inct2 ) || ( pixel16 && !inct0 && !inct1 );
    bool const incN2 = !( phraseMode && inc2t2 );
    bool const incN3 = !( phraseMode && pixel8 && !inct0 && !inct1 && !inct2 );
    std::uint32_t const inc = ( incN0 ? 0u : 1u ) | ( incN1 ? 0u : 2u ) | ( incN2 ? 0u : 4u ) | ( incN3 ? 0u : 8u );
    std::uint32_t const count = ( mIcount - inc ) & 0xffff;
    c.icntena = atick0 && mDwrite;
    bool const cntisel1 = c.instart || mCntlden;
    c.cnti = cntisel1 ? cntval : ( c.icntena ? count : mIcount );
    c.uflowt = bit( count, 15 ) && !bit( mIcount, 15 );
  }

  // --- data.v ----------------------------------------------------------------------------------------
  std::uint64_t const srcd1 = static_cast<std::uint64_t>( mSd1[2] | mSd1[3] << 16 ) << 32 | ( mSd1[0] | mSd1[1] << 16 );
  std::uint64_t const srcd2 = static_cast<std::uint64_t>( mSrcd2[1] ) << 32 | mSrcd2[0];
  std::uint32_t srcdlo = 0, srcdhi = 0;
  sourceShift( srcd1, srcd2, mSrcshift, BIG_PIX, srcdlo, srcdhi );
  std::uint32_t const srcd8 = srcdlo & 0xff;
  std::uint32_t const srcz10 = mSz1[0] | mSz1[1] << 16, srcz11 = mSz1[2] | mSz1[3] << 16;
  std::uint32_t const srcz20 = mSz2[0] | mSz2[1] << 16, srcz21 = mSz2[2] | mSz2[3] << 16;
  std::uint32_t srcz0 = 0, srcz1 = 0;
  switch ( ( mSrcshift >> 4 ) & 3 )
  {
  case 0:
    srcz0 = srcz10;
    srcz1 = srcz11;
    break;
  case 1:
    srcz0 = ( srcz11 & 0xffff ) << 16 | srcz10 >> 16;
    srcz1 = ( srcz20 & 0xffff ) << 16 | srcz11 >> 16;
    break;
  case 2:
    srcz0 = srcz11;
    srcz1 = srcz20;
    break;
  default:
    srcz0 = ( srcz20 & 0xffff ) << 16 | srcz11 >> 16;
    srcz1 = ( srcz21 & 0xffff ) << 16 | srcz20 >> 16;
    break;
  }
  bool const zpipe0 = ( atick0 && mDzwrite ) || !mGourz;
  c.srczp[0] = zpipe0 ? srcz0 : mSrczpt[0];
  c.srczp[1] = zpipe0 ? srcz1 : mSrczpt[1];
  // zedcomp.v
  std::uint32_t zcomp = 0;
  for ( std::uint32_t k = 0; k < 4 && mZmode != 0; ++k ) // none without a mode
  {
    std::uint32_t const s = ( c.srczp[k >> 1] >> ( 16 * ( k & 1 ) ) ) & 0xffff;
    std::uint32_t const d = ( mDstz[k >> 1] >> ( 16 * ( k & 1 ) ) ) & 0xffff;
    bool const z = ( bit( mZmode, 0 ) && s < d ) || ( bit( mZmode, 1 ) && s == d ) || ( bit( mZmode, 2 ) && s > d );
    zcomp |= z ? 1u << k : 0u;
  }
  // datacomp
  std::uint32_t dcomp = 0;
  for ( std::uint32_t k = 0; k < 8 && mDcompen; ++k ) // read only with DCOMPEN
  {
    std::uint32_t const half = k >> 2, sh = 8 * ( k & 3 );
    std::uint32_t const p = ( mPatd[half] >> sh ) & 0xff;
    std::uint32_t const other = mCmpdst ? mDstd[half] : ( half ? srcdhi : srcdlo );
    dcomp |= p == ( ( other >> sh ) & 0xff ) ? 1u << k : 0u;
  }
  // lfu
  std::uint32_t const f = mLfuFunc;
  auto const lfu = [f]( std::uint32_t s, std::uint32_t d )
  {
    return ( bit( f, 3 ) ? s & d : 0u ) | ( bit( f, 2 ) ? s & ~d : 0u ) | ( bit( f, 1 ) ? ~s & d : 0u ) |
           ( bit( f, 0 ) ? ~s & ~d : 0u );
  };
  std::uint32_t const lfu0 = lfu( srcdlo, mDstd[0] ), lfu1 = lfu( srcdhi, mDstd[1] );
  // daddamux.v, daddbmux.v, addarray.v
  std::uint32_t const patdu0 = static_cast<std::uint32_t>( mPdu ), patdu1 = static_cast<std::uint32_t>( mPdu >> 32 );
  std::uint32_t selLo = 0, selHi = 0;
  switch ( daddasel & 3 )
  {
  case 0:
    selLo = srcdlo;
    selHi = srcdhi;
    break;
  case 1:
    selLo = patdu0;
    selHi = patdu1;
    break;
  case 2:
    selLo = srcz10;
    selHi = srcz11;
    break;
  default:
    selLo = srcz20;
    selHi = srcz21;
    break;
  }
  // the adders: their sums are read with Gouraud shading, Z, source shading or ADDDSEL, their carries
  // (carryq) only with Gouraud shading or Z, whose flags a write of B_CMD may set for the next cycle
  bool const adding = mGourd || mGourz || mSrcshade || mAdddsel || c.cmdld;
  std::uint32_t const addaLo = bit( daddasel, 2 ) ? selLo : mDstd[0];
  std::uint32_t const addaHi = bit( daddasel, 2 ) ? selHi : mDstd[1];
  std::uint32_t word = 0;
  switch ( daddbsel & 3 )
  {
  case 0:
    word = mIinc & 0xffff;
    break;
  case 1:
    word = mIinc >> 16;
    break;
  case 2:
    word = mZinc & 0xffff;
    break;
  default:
    word = mZinc >> 16;
    break;
  }
  std::uint32_t const adda[4] = { addaLo & 0xffff, addaLo >> 16, addaHi & 0xffff, addaHi >> 16 };
  std::uint32_t const addb[4] = { bit( daddbsel, 2 ) ? word : srcdlo & 0xffff,
                                  bit( daddbsel, 2 ) ? word : srcdlo >> 16,
                                  bit( daddbsel, 2 ) ? word : srcdhi & 0xffff,
                                  bit( daddbsel, 2 ) ? word : srcdhi >> 16 };
  bool const cinsel = ( daddmode0 || daddmode1 ) && !daddmode2;
  bool const sat = daddmode0 || daddmode1;
  bool const hicinh = daddmode0 && daddmode1;
  std::uint32_t addq[4] = {};
  c.co = adding ? 0u : mCarryq;
  for ( std::uint32_t k = 0; k < 4 && adding; ++k )
  {
    bool co = false;
    addq[k] = add16sat( adda[k], addb[k], cinsel && bit( mCarryq, k ), sat, daddmode1, hicinh, co );
    c.co |= co ? 1u << k : 0u;
  }
  // local_mux: what the loop loads
  bool const busOwn = in.back || breqAny;
  std::uint32_t const load0 = busOwn ? static_cast<std::uint32_t>( in.data ) : din;
  std::uint32_t const load1 = busOwn ? static_cast<std::uint32_t>( in.data >> 32 ) : din;
  std::uint32_t const local0 = daddqSel ? addq[1] << 16 | addq[0] : load0;
  std::uint32_t const local1 = daddqSel ? addq[3] << 16 | addq[2] : load1;
  std::uint32_t const localLane[4] = { local0 & 0xffff, local0 >> 16, local1 & 0xffff, local1 >> 16 };
  std::uint32_t const dinlo = din & 0xffff, dinhi = din >> 16;
  bool const loading = c.intld != 0 || c.srcd1ld != 0 || c.srcz1ld != 0 || c.zedld != 0 || c.srcz2ld != 0 || srcz2add ||
                       c.srczread; // else each keeps what it holds
  for ( std::uint32_t k = 0; k < 4; ++k )
  {
    if ( !loading )
    {
      c.srcd1i[k] = mSd1[k];
      c.srcz1i[k] = mSz1[k];
      c.srcz2i[k] = mSz2[k];
      continue;
    }
    bool const s1 = bit( c.srcd1ld, k >> 1 );
    c.srcd1i[k] = bit( c.intld, k ) ? ( s1 ? 0u : dinlo ) : ( s1 ? localLane[k] : mSd1[k] );
    bool const z1 = bit( c.srcz1ld, k >> 1 );
    c.srcz1i[k] = bit( c.zedld, k ) ? ( z1 ? 0u : dinhi ) : ( z1 ? localLane[k] : mSz1[k] );
    bool const sel1 = bit( c.srcz2ld, k >> 1 ) || srcz2add || bit( c.zedld, k );
    bool const sel0 = c.srczread || bit( c.zedld, k );
    c.srcz2i[k] = sel1 ? ( sel0 ? dinlo : localLane[k] ) : ( sel0 ? mSz1[k] : mSz2[k] );
  }
  {
    auto const byte = []( std::uint64_t v, std::uint32_t n )
    { return static_cast<std::uint32_t>( v >> ( 8 * n ) ) & 0xff; };
    std::uint64_t const ld = static_cast<std::uint64_t>( local1 ) << 32 | local0;
    std::uint32_t const di = ( din >> 16 ) & 0xff;
    std::uint64_t p = 0;
    for ( std::uint32_t n = 0; n < 8 && ( c.patdld != 0 || c.intld != 0 ); ++n )
    {
      bool const ldn = bit( c.patdld, n >> 2 );
      std::uint32_t v = ldn ? byte( ld, n ) : byte( mPdu, n );
      if ( ( n & 1 ) == 0 && bit( c.intld, n >> 1 ) )
        v = ldn ? 0u : di;
      p |= static_cast<std::uint64_t>( v ) << ( 8 * n );
    }
    c.patdi = c.patdld != 0 || c.intld != 0 ? p : mPdu; // else it keeps what it holds
  }
  c.zpipe1 = mZpipe1t1 || !mGourz;
  c.dpipe1 = mDpipe1t1 || !mGourd;

  // --- comp_ctrl.v -------------------------------------------------------------------------------------
  c.bcompselt = ( mIcount & 7 ) ^ ( BIG_PIX ? 7u : 0u );
  bool const bcompbit = bit( srcd8, 7 - c.bcompselt );
  c.bcompbitpt = bit( srcd8, 7 - mBcompsel );
  bool const cond0 = mBcompen && !bcompbit && !phraseMode;
  bool const cond1 = mDcompen && bit( dcomp, 0 ) && !phraseMode && pixel8;
  bool const cond2 = mDcompen && bit( dcomp, 0 ) && bit( dcomp, 1 ) && !phraseMode && pixel16;
  bool const cond3 = bit( zcomp, 0 ) && !phraseMode && pixel16;
  c.nowrite = ( cond0 || cond1 || cond2 || cond3 ) && !mBkgwren;
  bool const winhibit = ( mBcompen && !mBcompbitp && !phraseMode ) || cond1 || cond2 || cond3;
  // the bytes written and the phrase: read only as the blitter drives the bus (data_ena)
  std::uint32_t dbinhN = 0;
  if ( mDataEna )
  {
    bool const p2 = bit( pixsize, 2 );
    for ( std::uint32_t k = 0; k < 8; k += 2 )
    {
      // a pair of bytes: the Z or 16 bit compare of the pair, then each byte's own
      bool const zp = p2 && bit( zcomp, k >> 1 );
      bool const dp = p2 && bit( dcomp, k ) && bit( dcomp, k + 1 ) && mDcompen;
      for ( std::uint32_t j = k; j <= k + 1; ++j )
      {
        bool const bs = !bit( srcd8, j ) && mBcompen;
        bool const db = !p2 && bit( dcomp, j ) && mDcompen;
        bool const inh = zp || dp || bs || db;
        bool const n = j < 4 ? !( ( inh && phraseMode ) || winhibit ) : !( inh && phraseMode );
        dbinhN |= n ? 1u << j : 0u;
      }
    }
  }

  // --- inner.v: the write ----------------------------------------------------------------------------
  bool const oldoutld = mSrcaAdd && atick1;
  bool a1Outside = false;
  {
    bool const xgeq = ( a1x & 0x7fff ) >= mA1WinX;
    bool const ygeq = ( a1y & 0x7fff ) >= mA1WinY;
    a1Outside = bit( a1x, 15 ) || xgeq || bit( a1y, 15 ) || ygeq;
  }
  c.oldoutside = oldoutld ? a1Outside : mOldoutsidel;
  bool const outside = dsta2 ? c.oldoutside : a1Outside;
  bool const clipN = !( mDisoA1 && outside );
  c.writereq = ( mDwrite || mDzwrite ) && step && !c.nowrite && clipN;
  c.denat1 = ( mDwrite || mDzwrite ) && !c.nowrite && clipN;
  c.denat3 = c.blitack && mWactive && mDenat2;
  c.dpipe1t0 = c.writereq && mDwrite;
  c.zpipe1t0 = c.writereq && mDzwrite;
  c.collidea = c.nowrite && mStopen && mDwrite1;

  // --- data_mux.v: the phrase written ------------------------------------------------------------------
  c.wdata = 0;
  if ( mDataEna )
  {
    bool const edisN = mDend != 0;
    std::uint32_t const eCoarseN = edisN ? ~( 1u << ( mDend >> 3 ) ) & 0xffu : 0xffu;
    std::uint32_t const eFineN = edisN && ( mDend >> 3 ) == 0 ? ~( 1u << ( mDend & 7 ) ) & 0xffu : 0xffu;
    std::uint32_t const sCoarse = 1u << ( mDstart >> 3 );
    std::uint32_t const sFine = ( mDstart >> 3 ) == 0 ? 1u << ( mDstart & 7 ) : 0u;
    std::uint32_t maskt = sFine & 1u;
    for ( std::uint32_t k = 1; k < 8; ++k )
      if ( ( bit( maskt, k - 1 ) || bit( sFine, k ) ) && bit( eFineN, k ) )
        maskt |= 1u << k;
    bool prev = bit( sCoarse, 0 ) && bit( eCoarseN, 0 );
    for ( std::uint32_t k = 8; k < 15; ++k )
    {
      bool const m = ( prev || bit( sCoarse, k - 7 ) ) && bit( eCoarseN, k - 7 );
      maskt |= m ? 1u << k : 0u;
      prev = m;
    }
    bool const mirBit = !phraseMode && BIG_PIX;
    bool const mirByte = phraseMode && BIG_PIX;
    std::uint32_t masku = 0;
    if ( mirByte )
    {
      masku = mirBit ? 0u : ( bit( maskt, 14 ) ? 0xffu : 0u );
      masku |= bit( maskt, 0 ) ? 1u << 14 : 0u;
      for ( std::uint32_t k = 8; k < 14; ++k )
        masku |= bit( maskt, k ) ? 1u << ( 21 - k ) : 0u;
    }
    else
      masku = ( mirBit ? reverse8( maskt & 0xff ) : maskt & 0xff ) | ( maskt & 0x7f00u );
    std::uint32_t const mask =
        ( bit( dbinhN, 0 ) ? masku & 0xffu : 0u ) | ( ( dbinhN & 0xfeu ) << 7 & masku & 0x7f00u );
    std::uint32_t ddatlo = 0, ddathi = 0;
    if ( dataSel1 )
    {
      ddatlo = dataSel0 ? 0u : addq[1] << 16 | addq[0];
      ddathi = dataSel0 ? 0u : addq[3] << 16 | addq[2];
    }
    else
    {
      ddatlo = dataSel0 ? lfu0 : mPatdo[0];
      ddathi = dataSel0 ? lfu1 : mPatdo[1];
    }
    bool const zedSel = dataSel0 && dataSel1;
    std::uint64_t const src = zedSel ? static_cast<std::uint64_t>( mSrczo[1] ) << 32 | mSrczo[0]
                                     : static_cast<std::uint64_t>( ddathi ) << 32 | ddatlo;
    std::uint64_t const dst = zedSel ? static_cast<std::uint64_t>( mDstz[1] ) << 32 | mDstz[0]
                                     : static_cast<std::uint64_t>( mDstd[1] ) << 32 | mDstd[0];
    std::uint64_t m = mask & 0xffu;
    for ( std::uint32_t k = 8; k < 15; ++k )
      if ( bit( mask, k ) )
        m |= std::uint64_t{ 0xff } << ( 8 * ( k - 7 ) );
    c.wdata = ( src & m ) | ( dst & ~m );
  }
  return c;
}

std::uint64_t Blitter::clock( Inputs const& in )
{
  // settled needs the cycle to change nothing: a blitter asking for the bus, waiting for an
  // acknowledge or with a latch taken before it cannot be, as the same state would ask again
  if ( mRepeat.valid && in == mRepeat.in )
    return mRepeat.wdata;
  mRepeat.valid = false;
  bool const couldSettle = outputs().breq == 0 && !mWaitack && !mLatched;
  std::optional<Blitter> before;
  if ( couldSettle )
    before.emplace( *this );
  // a cycle with the inputs of the one before may change nothing, and the next then need not be worked
  // out: the state as its bytes, which differ if a register does
  static_assert( std::is_trivially_copyable_v<Blitter> );
  bool const couldRepeat = in == mRepeat.in && !mLatched;
  alignas( Blitter ) unsigned char snapshot[sizeof( Blitter )];
  if ( couldRepeat )
    std::memcpy( snapshot, static_cast<void const*>( this ), sizeof( Blitter ) );
  bool const wasDrvReset = mDrvReset, wasDzwrite1 = mDzwrite1, wasSettled = mSettled, wasInterrupt = mInterrupt;
  Comb const c = evaluate( in );
  std::uint32_t const din = in.din;
  bool const active = mWactive || mRactive;

  // state.v, inner.v, outer.v, acontrol.v, dcontrol.v: the command
  if ( c.cmdld )
  {
    mZmode = ( din >> 18 ) & 7;
    mLfuFunc = ( din >> 21 ) & 15;
    mCmpdst = bit( din, 25 );
    mBcompen = bit( din, 26 );
    mDcompen = bit( din, 27 );
    mBkgwren = bit( din, 28 );
    mSrcshade = bit( din, 30 );
    mSrcen = bit( din, 0 );
    mSrcenz = bit( din, 1 );
    mSrcenx = bit( din, 2 );
    mDsten = bit( din, 3 );
    mDstenz = bit( din, 4 );
    mDstwrz = bit( din, 5 );
    mDisoA1 = bit( din, 6 );
    mUpda1f = bit( din, 8 );
    mUpda1 = bit( din, 9 );
    mUpda2 = bit( din, 10 );
    mBushi = bit( din, 29 );
    mDsta2 = bit( din, 11 );
    mGourd = bit( din, 12 );
    mGourz = bit( din, 13 );
    mTopben = bit( din, 14 );
    mTopnen = bit( din, 15 );
    mPatdsel = bit( din, 16 );
    mAdddsel = bit( din, 17 );
  }

  // outer.v
  bool const outer0 = mOcount == 0;
  bool const oidle = ( mOidle && !mGo ) || ( mInner && outer0 && c.indone );
  bool const innerNext = ( mOidle && mGo ) || ( mInner && !c.indone ) ||
                         ( c.indone && mInner && !outer0 && !mUpda1f && !mUpda1 && !mUpda2 ) ||
                         ( mOa1update && !mUpda2 ) || mOa2update;
  bool const idledt = mOidle && !active;
  mInterrupt = !mIdled && idledt;
  mGo = c.cmdld && !bit( din, 7 );
  mOidle = oidle;
  mInner = innerNext;
  mOa1fupdate = c.a1fupdatei;
  mOa1update = c.a1updatei;
  mOa2update = c.a2updatei;
  mSshftld = c.instart;
  mOcount = c.countld ? din >> 16 : ( c.instart ? ( mOcount - 1 ) & 0xffff : mOcount );
  mIdled = idledt;

  // inner.v
  bool const stepP1 = c.stepP1;
  if ( stepP1 )
  {
    mSreadx1 = mSreadx;
    mSread1 = mSread;
    mSzread1 = mSzread;
    mDread1 = mDread;
    mDzread1 = mDzread;
    mDenat2 = c.denat1;
    mDwrite1 = mDwrite;
    mDzwrite1 = mDzwrite;
  }
  mStupl = c.instart || ( mIdle && mStupl );
  mIdle = c.idlei;
  mSreadx = c.sreadxi;
  mSzreadx = c.szreadxi;
  mSread = c.sreadi;
  mSzread = c.szreadi;
  mDread = c.dreadi;
  mDzread = c.dzreadi;
  mDwrite = c.dwritei;
  mDzwrite = c.dzwritei;
  mDnrt6 = c.dnrt5;
  mAtick = ( c.aticki0 ? 1u : 0u ) | ( bit( mAtick, 0 ) ? 2u : 0u );
  mDpipe1t1 = c.dpipe1t0;
  mZpipe1t1 = c.zpipe1t0;
  mOldoutsidel = c.oldoutside;
  mSrcaAdd = c.srcaAddi;
  mGena2 = c.gena2i;
  mSrcdpend = c.srcdpend;
  mSrcdxpend = c.srcdxpend;
  mSrczpend = c.srczpend;
  mSrczxpend = c.srczxpend;
  mDstdpend = c.dstdpend;
  mDstzpend = c.dstzpend;
  mSrcdreadd = c.srcdreadt;
  mDataEna = c.denat3;

  // inner_cnt.v
  mCntvall = c.countld ? din & 0xffff : mCntvall;
  mIcount = c.cnti;
  mCntlden = c.countld;
  if ( c.icntena )
    mUnderflow = c.uflowt;

  // mcontrol.v
  bool const ractive =
      ( !mRactive && c.readreq ) || ( mRactive && !c.blitack ) || ( mRactive && c.blitack && c.readreq );
  bool const wactive =
      ( !mWactive && c.writereq ) || ( mWactive && !c.blitack ) || ( mWactive && c.blitack && c.writereq );
  if ( c.step )
    mBlita = c.address;
  mWaitack = ( mRactive && c.blitack ) || ( mWaitack && !in.ack );
  mRactive = ractive;
  mWactive = wactive;

  // acontrol.v
  mAa1update = c.a1updatei;
  mAa1fupdate = c.a1fupdatei;
  mAa2update = c.a2updatei;
  mA1Add = c.a1Addi;
  mA2Add = c.a2Addi;
  mAddasel = c.addaseli;
  mAddareg = c.addaregi;
  mAddbsel = c.addbseli;
  mDstart = mDstartp;
  mDend = mDendp;
  if ( c.step )
  {
    mDstartp = c.dstartb;
    mDendp = c.dendb;
  }
  mDstartbl = c.dstartb;
  mDendbl = c.dendb;
  mSrcshift = c.shfti;

  // dcontrol.v
  mDzwrite1d = wasDzwrite1;

  // comp_ctrl.v
  if ( c.step )
    mBcompsel = c.bcompselt;
  mBcompbitp = c.bcompbitpt;

  // blitstop.v
  {
    bool const resume = c.stopld && bit( din, 0 );
    bool const collAbort = c.stopld && bit( din, 1 );
    if ( c.stopld )
      mStopen = bit( din, 2 );
    bool const collide = c.collidea && !mCollideb;
    bool const bidle = !( mStopped || mDrvReset );
    mCollideb = c.collidea;
    bool const stopped = ( bidle && collide ) || ( mStopped && !resume && !collAbort );
    mDrvReset = mStopped && collAbort;
    mStopped = stopped;
  }

  // address.v
  if ( c.a1ptrld )
  {
    mA1Xt = c.dataX;
    mA1Y = c.dataY;
  }
  if ( c.a1fracld )
  {
    mA1FracX = c.dataX;
    mA1FracY = c.dataY;
  }
  if ( c.a2ptrld )
  {
    mA2Xr = c.dataX;
    mA2Yr = c.dataY;
  }
  mCxt1 = c.cxt0;
  mCyt1 = c.cyt0;
  mAddressi = c.address;

  // data.v
  for ( std::uint32_t k = 0; k < 4; ++k )
  {
    mSd1[k] = c.srcd1i[k];
    mSz1[k] = c.srcz1i[k];
    mSz2[k] = c.srcz2i[k];
  }
  mPdu = c.patdi;
  if ( c.dpipe1 )
  {
    mPatdo[0] = mPatd[0];
    mPatdo[1] = mPatd[1];
  }
  mSrczpt[0] = c.srczp[0];
  mSrczpt[1] = c.srczp[1];
  if ( c.zpipe1 )
  {
    mSrczo[0] = c.srczp[0];
    mSrczo[1] = c.srczp[1];
  }
  mCarryq = c.co;

  // drv_reset (blitstop.v reset_n): the loops and the memory interface reset while it holds, and as
  // it rises
  if ( mDrvReset || wasDrvReset )
  {
    mIdle = true;
    mSreadx = mSzreadx = mSread = mSzread = mDread = mDzread = mDwrite = mDzwrite = false;
    mAtick = 0;
    mStupl = false;
    mSreadx1 = mSread1 = mSzread1 = mDread1 = mDzread1 = mDenat2 = false;
    mSrcdpend = mSrcdxpend = mSrczpend = mSrczxpend = mDstdpend = mDstzpend = false;
    mOidle = true;
    mInner = mOa1fupdate = mOa1update = mOa2update = false;
    mIdled = true;
    mRactive = mWactive = mWaitack = false;
    mCarryq = 0;
  }

  // settled: the cycle changed nothing, and nothing asks for the bus or waits for it
  bool same = false;
  if ( before )
  {
    Blitter now = *this;
    now.mSettled = wasSettled;
    now.mInterrupt = wasInterrupt;
    same = now == *before;
  }
  mSettled = same && !mLatched && !in.memw && outputs().breq == 0 && !mWaitack && !mInterrupt;
  bool const repeat = couldRepeat && std::memcmp( snapshot, static_cast<void const*>( this ), sizeof( Blitter ) ) == 0;
  mRepeat.in = in;
  mRepeat.wdata = c.wdata;
  if ( repeat )
    mRepeat.out = outputs();
  mRepeat.valid = repeat;
  return c.wdata;
}

} // namespace jag
