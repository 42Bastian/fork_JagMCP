#include "jag/ObjectProcessor.hpp"

#include <algorithm>

namespace jag
{

namespace
{

constexpr bool bit( std::uint64_t v, unsigned n )
{
  return ( v >> n ) & 1u;
}

constexpr std::uint64_t field( std::uint64_t v, unsigned lo, unsigned bits )
{
  return ( v >> lo ) & ( ( 1ull << bits ) - 1 );
}

// lbuf.v: a field of the pixel in the buffer plus the signed one written (sadd), saturated
std::uint32_t saturatingAdd( std::uint32_t old, std::uint32_t add, unsigned lo, unsigned bits )
{
  std::uint32_t const mask = ( 1u << bits ) - 1;
  std::uint32_t const a = ( old >> lo ) & mask;
  std::uint32_t const b = ( add >> lo ) & mask;
  std::uint32_t const sum = a + b;
  bool const carry = ( sum >> bits ) & 1u;
  bool const negative = ( b >> ( bits - 1 ) ) & 1u;
  std::uint32_t const out = carry != negative ? ( carry ? mask : 0u ) : sum & mask;
  return out << lo;
}

} // namespace

namespace
{

// evaluate()'s profiles: none fixed (AnyState), or a state of the steady pixel stream (q4, the
// pixel sequencer writing, smq2, or taking the next phrase, smq0) of an object of a depth, scaled or
// not, with no header written back, no RMW and no line starting: most of a busy line's cycles
struct AnyState
{
  static constexpr bool FIXED = false;
  static constexpr std::uint32_t SMQ = 0;
  static constexpr std::uint32_t DEPTH = 0;
  static constexpr bool SCALED = false;
};

template <std::uint32_t S, std::uint32_t D, bool SC>
struct Stream
{
  static constexpr bool FIXED = true;
  static constexpr std::uint32_t SMQ = S;
  static constexpr std::uint32_t DEPTH = D;
  static constexpr bool SCALED = SC;
};

template <std::uint32_t S, bool SC, typename F>
void withDepth( std::uint32_t depth, F&& f )
{
  switch ( depth )
  {
  case 0:
    return f( Stream<S, 0, SC>{} );
  case 1:
    return f( Stream<S, 1, SC>{} );
  case 2:
    return f( Stream<S, 2, SC>{} );
  case 3:
    return f( Stream<S, 3, SC>{} );
  case 4:
    return f( Stream<S, 4, SC>{} );
  default:
    return f( Stream<S, 5, SC>{} );
  }
}

template <std::uint32_t S, typename F>
void withKind( std::uint32_t depth, bool scaled, F&& f )
{
  if ( scaled )
    withDepth<S, true>( depth, f );
  else
    withDepth<S, false>( depth, f );
}

} // namespace

void ObjectProcessor::evaluate( Inputs const& in, Comb& c ) const
{
  withProfile( [&]( auto p ) { evaluateT<decltype( p )>( in, c ); } );
}

template <typename F>
void ObjectProcessor::withProfile( F&& f ) const
{
  if ( mQ == 1u << 4 && mWq == 1u && !mRmw && !mRmw1 && !mStart1 && !mOverrun && mType == ( mScaledi ? 1u : 0u ) &&
       mDepth <= 5 )
  {
    if ( mSmq == 1u )
      return withKind<1u>( mDepth, mScaledi, f );
    if ( mSmq == 4u )
      return withKind<4u>( mDepth, mScaledi, f );
  }
  f( AnyState{} );
}

template <typename P>
void ObjectProcessor::evaluateT( Inputs const& in, Comb& c ) const
{
  // the registers the profile fixes: constants there, so that the compiler folds what they steer
  std::uint32_t const pQ = P::FIXED ? 1u << 4 : mQ;
  std::uint32_t const pType = P::FIXED ? ( P::SCALED ? 1u : 0u ) : mType;
  std::uint32_t const pDepth = P::FIXED ? P::DEPTH : mDepth;
  bool const pScaledi = P::FIXED ? P::SCALED : mScaledi;
  std::uint32_t const pWq = P::FIXED ? 1u : mWq;
  std::uint32_t const pSmq = P::FIXED ? P::SMQ : mSmq;
  bool const pRmw = P::FIXED ? false : mRmw;
  bool const pRmw1 = P::FIXED ? false : mRmw1;
  bool const pStart1 = P::FIXED ? false : mStart1;
  bool const pOverrun = P::FIXED ? false : mOverrun;
  bool const q0 = bit( pQ, 0 ), q1 = bit( pQ, 1 ), q2 = bit( pQ, 2 ), q3 = bit( pQ, 3 ), q4 = bit( pQ, 4 ),
             q5 = bit( pQ, 5 ), q7 = bit( pQ, 7 ), q8 = bit( pQ, 8 ), q9 = bit( pQ, 9 );
  c.bitob = pType == 0;
  c.scaled = pType == 1;
  c.grpob = pType == 2;
  c.branchob = pType == 3;
  c.stopob = pType == 4;
  std::uint32_t const vc = in.vc & 0x7ffu;
  bool const vgy = vc > mYpos;
  bool const vly = vc < mYpos;
  bool const vey = vc == mYpos;
  bool const geq = !vly;
  bool const y7ff = mYpos == 0x7ffu;
  bool const remnz = mRem != 0;
  bool const bit0 = bit( mIwidth, 0 ) && c.scaled;
  bool const iwidthz = !bit0 && field( mIwidth, 1, 9 ) == 0;
  bool const iwidthnz = !iwidthz;
  c.obmack = in.obback && in.mack;
  bool const notobmack = !c.obmack;
  bool const sstart = pStart1 && !mStart2;
  c.startover = pOverrun || sstart;

  // wbk.v's outputs
  c.wbkdone = bit( pWq, 0 );
  c.heightnz = mNewheight != 0;
  bool const intremz = field( mNewrem, 5, 4 ) == 0 || bit( mNewrem, 8 );

  // obdata.v: the pixels of the phrase (pa picks them, d3 to d7), the CLUT's addresses (pra, prb)
  bool const scaledO = pScaledi; // its own latch of the type
  bool const notscaledO = !scaledO;
  bool const mode1 = pDepth == 0, mode2 = pDepth == 1, mode4 = pDepth == 2, mode8 = pDepth == 3, mode16 = pDepth == 4,
             mode24 = pDepth == 5;
  c.phys = mode16 || mode24;
  bool const modew = c.phys;
  bool const hilo = in.hilo;
  c.pa = ( mPad & 1u ) | ( ( hilo ? ~mPad : mPad ) & 0x3eu );
  std::uint64_t const d3 = bit( c.pa, 5 ) ? mD2 >> 32 : mD2 & 0xffffffffu;
  std::uint64_t const d4 = bit( c.pa, 4 ) ? ( d3 >> 16 ) & 0xffffu : d3 & 0xffffu;
  std::uint64_t const d5 = bit( c.pa, 3 ) ? ( d4 >> 8 ) & 0xffu : d4 & 0xffu;
  std::uint64_t const d6 = bit( c.pa, 2 ) ? ( d5 >> 4 ) & 0xfu : d5 & 0xfu;
  std::uint64_t const d7 = bit( c.pa, 1 ) ? ( d6 >> 2 ) & 3u : d6 & 3u;
  // the CLUT's addresses, read in the next cycle only at a depth that is not physical; the depth
  // changes as obld[1] latches a header's second phrase
  if ( !( mode16 || mode24 ) || ( bit( mLatchQ, 1 ) && in.mack ) )
  {
    std::uint32_t const index = mIndex;
    auto const pick = [&]( bool m1, bool m2, bool m4, bool m8 )
    { return ( m1 && mode1 ) || ( m2 && mode2 ) || ( m4 && mode4 ) || ( m8 && mode8 ); };
    c.pra = ( pick( bit( d7, 0 ), bit( d6, 0 ), bit( d5, 0 ), bit( d4, 0 ) ) ? 1u : 0u ) |
            ( pick( bit( index, 1 ), bit( d6, 1 ), bit( d5, 1 ), bit( d4, 1 ) ) ? 2u : 0u ) |
            ( pick( bit( index, 2 ), bit( index, 2 ), bit( d5, 2 ), bit( d4, 2 ) ) ? 4u : 0u ) |
            ( pick( bit( index, 3 ), bit( index, 3 ), bit( d5, 3 ), bit( d4, 3 ) ) ? 8u : 0u ) |
            ( mode8 ? static_cast<std::uint32_t>( d4 & 0xf0u ) : index & 0xf0u );
    c.prb = ( pick( bit( d7, 1 ), bit( d6, 2 ), bit( d5, 4 ), bit( d4, 8 ) ) ? 1u : 0u ) |
            ( pick( bit( index, 1 ), bit( d6, 3 ), bit( d5, 5 ), bit( d4, 9 ) ) ? 2u : 0u ) |
            ( pick( bit( index, 2 ), bit( index, 2 ), bit( d5, 6 ), bit( d4, 10 ) ) ? 4u : 0u ) |
            ( pick( bit( index, 3 ), bit( index, 3 ), bit( d5, 7 ), bit( d4, 11 ) ) ? 8u : 0u ) |
            ( mode8 ? static_cast<std::uint32_t>( ( d4 >> 8 ) & 0xf0u ) : index & 0xf0u );
  }
  else
  {
    c.pra = mPaaq;
    c.prb = mPabq;
  }
  // d8 of physical pixels, in the order HILO gives (the CLUT's words are taken in the edge)
  std::uint32_t const lo16 = static_cast<std::uint32_t>( d3 & 0xffffu ), hi16 = static_cast<std::uint32_t>( d3 >> 16 );
  c.d9next = hilo ? ( lo16 << 16 | hi16 ) : static_cast<std::uint32_t>( d3 );

  bool const smq0 = bit( pSmq, 0 ), smq1 = bit( pSmq, 1 ), smq2 = bit( pSmq, 2 );
  bool const notfull = !mFull;
  c.notremgte2 = field( mXrem, 6, 3 ) == 0;
  c.remgte2 = !c.notremgte2;
  c.remgte1 = field( mXrem, 5, 4 ) != 0;
  bool const p2done = bit( c.pa, 0 ) || notscaledO;
  c.phdone = p2done && ( ( field( mPad, 1, 5 ) == 31 && mode1 ) || ( field( mPad, 2, 4 ) == 15 && mode2 ) ||
                         ( field( mPad, 3, 3 ) == 7 && mode4 ) || ( field( mPad, 4, 2 ) == 3 && mode8 ) ||
                         ( bit( mPad, 5 ) && modew ) );
  // the line buffer address and whether it is off the line
  std::uint32_t const a = mLbwad;
  bool const c5 = bit( a, 4 ) || bit( a, 5 );
  bool const c7 = !( c5 && bit( a, 6 ) && bit( a, 7 ) );
  bool const c8 = !( c7 && !bit( a, 8 ) );
  bool const c9 = !( c8 && bit( a, 9 ) );
  bool const c11 = !( c9 && !bit( a, 10 ) && !bit( a, 11 ) );
  c.left = bit( a, 11 );
  c.right = c11 && !bit( a, 11 );
  c.obdOnscreen = !( c.left || c.right );
  c.offscreen = mReflected ? c.left : c.right;
  bool const off = c.offscreen;
  bool const rmw = pRmw;
  c.nextphrase = ( smq0 && mFull ) || ( smq2 && scaledO && c.notremgte2 && c.phdone && mFull ) ||
                 ( smq2 && notscaledO && c.phdone && mFull );
  c.nextx = ( smq2 && scaledO && c.remgte1 ) || ( smq2 && notscaledO );
  c.nextbits = ( smq2 && scaledO && c.notremgte2 ) || ( smq2 && notscaledO );
  bool const smd0 = ( smq0 && notfull ) || ( smq2 && scaledO && c.notremgte2 && c.phdone && notfull ) ||
                    ( smq2 && notscaledO && c.phdone && notfull ) || off;
  bool const smd1 = !off && ( ( smq0 && mFull && rmw ) || ( smq2 && mFull && rmw ) || ( smq2 && !c.phdone && rmw ) ||
                              ( smq2 && scaledO && c.remgte2 && rmw ) );
  bool const smd2 = !off && ( ( smq0 && mFull && !rmw ) || ( smq2 && mFull && !rmw ) || ( smq2 && !c.phdone && !rmw ) ||
                              smq1 || ( smq2 && scaledO && c.remgte2 && !rmw ) );
  c.smd = ( smd0 ? 1u : 0u ) | ( smd1 ? 2u : 0u ) | ( smd2 ? 4u : 0u );
  c.lbwrite = smq2;
  c.obdone = smq0 && notfull;
  c.obdready = !( ( mFull || ( bit( mLatchQ, 3 ) && in.mack ) ) && !c.nextphrase && !smq0 );
  // transparency of the pair of pixels
  bool const transa = ( mode1 && !bit( d7, 0 ) ) || ( mode2 && ( d6 & 3u ) == 0 ) || ( mode4 && ( d5 & 0xfu ) == 0 ) ||
                      ( mode8 && ( d4 & 0xffu ) == 0 ) || ( mode16 && ( d3 & 0xffffu ) == 0 );
  bool const transb = ( mode1 && !bit( d7, 1 ) ) || ( mode2 && ( d6 & 0xcu ) == 0 ) ||
                      ( mode4 && ( d5 & 0xf0u ) == 0 ) || ( mode8 && ( d4 & 0xff00u ) == 0 ) ||
                      ( mode16 && ( d3 & 0xffff0000u ) == 0 );
  bool const nottransa = !( transa && mTransen );
  bool const nottransb = !( transb && mTransen );
  bool const nntransa = hilo ? nottransb : nottransa;
  bool const nntransb = hilo ? nottransa : nottransb;
  c.lbwea1 = c.lbwrite && nntransa;
  c.lbweb1 = c.lbwrite && nntransb;
  c.lbwea4 = c.phys ? c.lbwea1 : mLbwea2;
  c.lbweb4 = c.phys ? c.lbweb1 : mLbweb2;
  bool const lbweb5 = pRmw1 ? mLbweb52 : mLbweb51;
  bool const del1 = bit( mLbwa, 0 ) != mReflected;
  bool const delayed = del1 && notscaledO;
  bool const lbweb6 = delayed ? lbweb5 : c.lbweb4;
  bool const pa20 = c.phys ? bit( c.pa, 0 ) : mPa10;
  bool const oddeven = bit( a, 0 ) != pa20;
  c.pswapd = scaledO ? oddeven : bit( a, 0 );
  bool const lbwea7 = c.pswapd ? lbweb6 : c.lbwea4;
  bool const lbweb7 = c.pswapd ? c.lbwea4 : lbweb6;
  bool const nextxx = c.phys ? c.nextx : mNextx1;
  c.lbwea8 =
      ( lbwea7 && notscaledO && c.obdOnscreen ) || ( lbwea7 && scaledO && c.obdOnscreen && !bit( a, 0 ) && nextxx );
  c.lbweb8 =
      ( lbweb7 && notscaledO && c.obdOnscreen ) || ( lbweb7 && scaledO && c.obdOnscreen && bit( a, 0 ) && nextxx );
  c.lbend = smd2 || c.lbwrite || mLbw != 0;
  // the pixels written: d9's halves, the second delayed a pixel where the pair straddles a word
  std::uint32_t const db = delayed ? mDa : mD9 >> 16;
  c.lbwdOut = mPswap ? ( db | ( mD9 & 0xffffu ) << 16 ) : ( ( mD9 & 0xffffu ) | db << 16 );
  c.onscreen = !c.offscreen;

  // ob.v next state
  bool const bitnotinrange = !( geq && c.heightnz );
  bool const scalednotinrange = !( geq && ( c.heightnz || remnz ) );
  bool const scaledinrange = !scalednotinrange;
  std::uint32_t const cc = mNewheight & 7u;
  bool const cctrue = ( cc == 0 && vey ) || ( cc == 0 && y7ff ) || ( cc == 1 && vly ) || ( cc == 2 && vgy ) ||
                      ( cc == 3 && mObf ) || ( cc == 4 && in.hcb10 );
  bool const onscr = c.onscreen;
  bool const offscr = c.offscreen;
  c.d[0] = ( q0 && !c.startover ) || ( q3 && !c.bitob && !c.scaled && !c.grpob && !c.branchob );
  c.d[1] = ( q0 && c.startover ) || ( q1 && notobmack ) || ( q3 && c.bitob && bitnotinrange ) ||
           ( q3 && c.branchob && cctrue ) || ( q7 && in.obfw ) || ( q9 && scalednotinrange ) ||
           ( q4 && iwidthz && c.wbkdone && c.obmack && c.bitob ) ||
           ( q4 && offscr && c.wbkdone && c.obmack && c.bitob ) || ( q5 && c.obmack ) ||
           ( q3 && c.branchob && !cctrue );
  c.d[2] = ( q1 && c.obmack ) || ( q2 && notobmack ) || ( q2 && !c.obdone );
  c.d[3] = ( q2 && c.obdone && c.obmack ) || ( q3 && c.bitob && geq && c.heightnz && notobmack ) ||
           ( q3 && c.scaled && notobmack );
  c.d[4] = ( q3 && c.bitob && geq && c.heightnz && c.obmack ) || ( q4 && onscr && iwidthnz ) ||
           ( q4 && iwidthz && c.wbkdone && notobmack ) || ( q4 && iwidthz && !c.wbkdone ) ||
           ( q9 && scaledinrange && c.obmack ) || ( q4 && offscr && c.wbkdone && notobmack ) ||
           ( q4 && offscr && !c.wbkdone );
  c.d[5] = ( q4 && iwidthz && c.wbkdone && c.obmack && c.scaled ) || ( q5 && notobmack ) ||
           ( q4 && offscr && c.wbkdone && c.obmack && c.scaled );
  c.d[6] = false;
  c.d[7] = ( q3 && c.grpob ) || ( q7 && !in.obfw );
  c.d[8] = ( q3 && c.scaled && c.obmack ) || ( q8 && notobmack );
  c.d[9] = ( q8 && c.obmack ) || ( q9 && scaledinrange && notobmack );
  c.pcld = q0 && c.startover;
  bool const hold = !mRelease;
  c.obbreq = q1 || q5 || q8 || q9 || ( q3 && c.bitob ) || ( q3 && c.branchob ) || ( q3 && c.scaled ) ||
             ( q2 && ( c.obdone || hold ) ) || ( q4 && onscr && iwidthnz && ( c.obdready || hold ) ) ||
             ( q4 && offscr ) || ( q4 && iwidthz );
  bool const omrq3 = !( q4 && iwidthz && c.wbkdone );
  bool const omrq6 = !( q4 && offscr && c.wbkdone );
  c.obmreq = q1 || q5 || ( q3 && c.bitob && geq && c.heightnz ) || ( q4 && onscr && iwidthnz && c.obdready ) ||
             !omrq3 || ( q9 && scaledinrange ) || ( q2 && c.obdone ) || !omrq6 || ( q3 && c.scaled );
  c.obldd0 = q1 && c.obmack;
  c.obldd1 = q2 && c.obdone && c.obmack;
  c.obldd2 = q3 && c.scaled && c.obmack;
  c.pc1en = c.obldd1;
  c.pc2en = c.obldd2 || ( q5 && c.obmack );
  c.dmaen = ( q3 && c.bitob && geq && c.heightnz && c.obmack ) ||
            ( q4 && onscr && iwidthnz && c.obdready && c.obmack ) || ( q9 && scaledinrange && c.obmack );
  c.datasel = ( q3 && c.bitob ) || q9 || ( q4 && onscr && iwidthnz );
  c.pclink = ( q3 && c.bitob && bitnotinrange ) || ( q3 && c.branchob && cctrue ) || ( q9 && scalednotinrange ) ||
             ( q4 && iwidthz && c.wbkdone && c.obmack && c.bitob ) ||
             ( q4 && offscr && c.wbkdone && c.obmack && c.bitob ) || ( q5 && c.obmack );
  c.pcinc = ( q3 && c.grpob ) || ( q3 && c.branchob && !cctrue );
  c.obrw = !q5 && omrq3 && omrq6;
  c.wbkstart = ( q3 && c.bitob && geq && c.heightnz && c.obmack ) || ( q9 && scaledinrange );
  c.wbken2d = q5 && c.obmack;
  c.wbken0d = !( c.wbken2d || in.ewr );
  c.xld = ( q3 && c.bitob && geq && c.heightnz && c.obmack ) || ( q3 && c.scaled && c.obmack );
  // the address: the pixel data's, or the list pointer's, the phrase within the object from the
  // load enables (olp1)
  std::uint32_t const olp1 = ( mOlp & 3u ) | ( c.pc1en ? 1u : 0u ) | ( c.pc2en ? 2u : 0u );
  if ( c.datasel )
    c.oa = mData & 0x1fffffu;
  else
    c.oa = ( olp1 & 3u ) | ( mOlp & 0x7fffcu ) | ( mOlpd & 0x180000u );
  for ( unsigned k = 0; k < 4; ++k )
    c.latch[k] = bit( mLatchQ, k ) && in.mack;

  // wbk.v next state
  bool const wq0 = bit( pWq, 0 ), wq1 = bit( pWq, 1 ), wq2 = bit( pWq, 2 );
  bool const heightz = !c.heightnz;
  bool const intremnz = !intremz;
  bool const wd0 = ( wq0 && !c.wbkstart ) || ( wq1 && !c.scaled ) || ( wq2 && heightz ) || ( wq2 && intremnz );
  bool const wd1 = wq0 && c.wbkstart;
  bool const wd2 = ( wq1 && c.scaled ) || ( wq2 && c.heightnz && intremz );
  c.wd = ( wd0 ? 1u : 0u ) | ( wd1 ? 2u : 0u ) | ( wd2 ? 4u : 0u );
  c.decheight = ( wq1 && !c.scaled ) || ( wq2 && c.heightnz && intremz );
  c.addnewdata = c.decheight;
  c.addrem = wq2 && c.heightnz && intremz;
  c.decrem = wq1 && c.scaled;
}

ObjectProcessor::Outputs ObjectProcessor::outputsOf( Comb const& c, std::uint32_t q, std::uint32_t ypos )
{
  Outputs o;
  o.obbreq = c.obbreq;
  o.mreq = c.obmreq;
  o.address = c.oa << 3;
  o.read = c.obrw;
  o.latch = c.latch[0] || c.latch[1] || c.latch[2] || c.latch[3];
  o.startref = bit( q, 3 ) && c.stopob;
  o.interrupt = bit( q, 3 ) && c.stopob && bit( ypos, 0 );
  o.gpuInterrupt = bit( q, 3 ) && c.grpob;
  return o;
}

ObjectProcessor::Outputs ObjectProcessor::outputs( Inputs const& in ) const
{
  if ( idleFast( in ) )
    return mIdleOutputs;
  evaluate( in, mComb );
  mCombValid = true;
  mCombIn = in;
  return outputsOf( mComb, mQ, mYpos );
}

std::uint32_t ObjectProcessor::gpuQuietCycles() const
{
  if ( mQ & 0xeu ) // q1 to q3
    return 0;
  if ( mQ != 1u << 4 ) // into q1 next at the earliest, then q2 and q3
    return 2;
  // q4, an object's pixel data: it leaves once IWIDTH has counted its phrases down (dmaen, one a
  // cycle at most; to 1 for an unscaled object, whose bit 0 is not looked at) or once the line
  // buffer's address is off the line (xld only in q3; two pixels a cycle at most, up or, reflected,
  // down): right of it from $2D0, left of it from $800
  std::uint32_t const iwidth = mType == 1 ? mIwidth : mIwidth >> 1 << 1;
  std::uint32_t const counted = mType == 1 ? iwidth : ( iwidth > 1 ? iwidth - 1 : 0u );
  std::uint32_t const a = mLbwad & 0xfffu;
  std::uint32_t off = 0;
  if ( mReflected )
    off = a >= 0x800u ? 0u : a + 1;
  else
    off = ( a >= 0x2d0u && a < 0x800u ) ? 0u : ( 0x2d0u - a ) & 0xfffu;
  return std::min( counted, ( off + 1 ) / 2 ) + 2;
}

std::uint64_t ObjectProcessor::writeData() const
{
  if ( mWbkeni2 )
    return static_cast<std::uint64_t>( mHscale ) | static_cast<std::uint64_t>( mVscale ) << 8 |
           static_cast<std::uint64_t>( mNewrem & 0xffu ) << 16;
  return static_cast<std::uint64_t>( mType ) | static_cast<std::uint64_t>( mYpos ) << 3 |
         static_cast<std::uint64_t>( mNewheight ) << 14 | static_cast<std::uint64_t>( mLink ) << 24 |
         static_cast<std::uint64_t>( mNewdata ) << 43;
}

void ObjectProcessor::writeOlp( bool high, std::uint16_t value )
{
  mCombValid = false;
  mIdleSettled = false;
  if ( high )
    mOlpd = ( mOlpd & 0x1fffu ) | ( value & 0xffu ) << 13;
  else
    mOlpd = ( mOlpd & ~0x1fffu ) | ( ( value >> 3 ) & 0x1fffu );
}

std::uint16_t ObjectProcessor::readOb( std::uint32_t n ) const
{
  switch ( n & 3u )
  {
  case 0:
    return static_cast<std::uint16_t>( mType | mYpos << 3 | ( mNewheight & 3u ) << 14 );
  case 1:
    return static_cast<std::uint16_t>( ( mNewheight >> 2 ) | ( mLink & 0xffu ) << 8 );
  case 2:
    return static_cast<std::uint16_t>( ( mLink >> 8 ) | ( mData & 0x1fu ) << 11 );
  default:
    return static_cast<std::uint16_t>( mData >> 5 );
  }
}

void ObjectProcessor::clock( Inputs const& in, std::uint64_t d )
{
  if ( idleFast( in ) )
  {
    // d9 follows the CLUT's bus, the data bus while the pixel path is idle; rmwd1 the word
    mD9 = ( mDepth == 4 || mDepth == 5 ) ? mIdleD9 : static_cast<std::uint32_t>( in.dout ) << 16 | in.dout;
    auto const& buffer = in.lbufa ? mBuffers.b : mBuffers.a;
    mRmwd1 = buffer[( mLbwa >> 1 ) & 0x1ffu];
    mLineWrites = false;
    mCombValid = false;
    mWbkeni0 = !in.ewr;
    return;
  }
  bool const wasIdle = idle() && !in.start && !in.obback;
  auto const before = wasIdle ? idleKey() : decltype( idleKey() ){};
  clockFull( in, d );
  mIdleSettled = wasIdle && idle() && !mLben && !mLineWrites && idleKey() == before;
  if ( mIdleSettled )
  {
    Comb c;
    evaluate( in, c );
    mIdleD9 = c.d9next;
    mIdleOutputs = outputsOf( c, mQ, mYpos );
  }
}

void ObjectProcessor::clockFull( Inputs const& in, std::uint64_t d )
{
  withProfile( [&]( auto p ) { clockFullT<decltype( p )>( in, d ); } );
}

template <typename P>
void ObjectProcessor::clockFullT( Inputs const& in, std::uint64_t d )
{
  // the registers the profile fixes, as they stand before the edge
  std::uint32_t const pQ = P::FIXED ? 1u << 4 : mQ;
  std::uint32_t const pDepth = P::FIXED ? P::DEPTH : mDepth;
  bool const pScaledi = P::FIXED ? P::SCALED : mScaledi;
  std::uint32_t const pSmq = P::FIXED ? P::SMQ : mSmq;
  bool const pRmw = P::FIXED ? false : mRmw;
  bool const pRmw1 = P::FIXED ? false : mRmw1;
  bool const pStart1 = P::FIXED ? false : mStart1;
  bool const pOverrun = P::FIXED ? false : mOverrun;
  Inputs const& was = mCombIn;
  bool const same = mCombValid && was.vc == in.vc && was.hcb10 == in.hcb10 && was.mack == in.mack &&
                    was.obback == in.obback && was.hilo == in.hilo && !was.obfw && !was.ewr;
  mCombValid = false;
  Comb& c = mComb;
  if ( !same )
    evaluate( in, c );
  else
  {
    // obfw and ewr, which outputs() was not given
    bool const q7 = bit( pQ, 7 );
    c.d[1] = c.d[1] || ( q7 && in.obfw );
    c.d[7] = ( bit( pQ, 3 ) && c.grpob ) || ( q7 && !in.obfw );
    c.wbken0d = !( c.wbken2d || in.ewr );
  }
  bool const obld0 = c.latch[0], obld1 = c.latch[1], obld2 = c.latch[2], obdlatch = c.latch[3];

  // the line buffer the video output does not read, written as the cycle ends (lbuf.v): the low
  // half (lbwe[0]) and the high half, the sum with what it holds for an RMW object
  mLineWrites = mLben;
  if ( mLben )
  {
    std::uint32_t value = c.lbwdOut;
    if ( pRmw1 )
      value = saturatingAdd( mRmwd1, value, 0, 8 ) | saturatingAdd( mRmwd1, value, 8, 4 ) |
              saturatingAdd( mRmwd1, value, 12, 4 ) | saturatingAdd( mRmwd1, value, 16, 8 ) |
              saturatingAdd( mRmwd1, value, 24, 4 ) | saturatingAdd( mRmwd1, value, 28, 4 );
    mLineWd = value;
  }
  if ( mLben && ( mLbwe & 3u ) )
  {
    auto& buffer = in.lbufa ? mBuffers.b : mBuffers.a;
    std::uint32_t& word = buffer[( mLbwa >> 1 ) & 0x1ffu];
    std::uint32_t const value = mLineWd;
    if ( mLbwe & 1u )
      word = ( word & 0xffff0000u ) | ( value & 0xffffu );
    if ( mLbwe & 2u )
      word = ( word & 0xffffu ) | ( value & 0xffff0000u );
  }
  {
    auto const& buffer = in.lbufa ? mBuffers.b : mBuffers.a;
    mRmwd1 = buffer[( mLbwa >> 1 ) & 0x1ffu];
  }

  // obdata.v, from the state before the edge
  bool const scaledO = pScaledi;
  bool const busy = !bit( pSmq, 0 );
  // the CLUT's buses: its RAM read as the cycle goes while the pixel path is busy, else the data
  // bus the CPU writes it with (pwda, pwdb)
  bool const cs = mBusy1;
  std::uint16_t const pda = cs ? mClut[mPaaq] : in.dout;
  std::uint16_t const pdb = cs ? mClut[mPabq] : in.dout;
  std::uint32_t const d9next = c.phys ? c.d9next
                                      : ( in.hilo ? ( static_cast<std::uint32_t>( pda ) << 16 | pdb )
                                                  : ( static_cast<std::uint32_t>( pdb ) << 16 | pda ) );
  bool const delpix = c.phys ? mNextbits1 : mNextbits2;
  bool const nextxx = c.phys ? c.nextx : mNextx1;
  std::uint32_t const combined = ( c.phys ? 32u : 0u ) | ( pDepth == 3 ? 16u : 0u ) | ( pDepth == 2 ? 8u : 0u ) |
                                 ( pDepth == 1 ? 4u : 0u ) | ( pDepth == 0 ? 2u : 0u );
  std::uint32_t const simplified = !scaledO ? combined : ( bit( c.pa, 0 ) ? combined - 1 : 1u );
  bool const empty = c.offscreen || c.nextphrase;
  std::uint32_t const sum = ( mXrem & 0xffu ) + mHscale;
  std::uint32_t const xs = c.nextbits ? sum : mXrem;
  std::uint32_t const diff = ( ( xs >> 5 ) + ( c.nextx ? 0xfu : 0u ) ) & 0xfu;
  // wbk.v
  std::uint32_t const rd = c.decrem ? 0x1e0u : mVscale;
  std::uint32_t const rs = ( mNewrem + rd ) & 0x1ffu;
  // ob.v
  bool const sstart = pStart1 && !mStart2;
  bool const overrun = ( sstart || pOverrun ) && !bit( pQ, 0 );
  std::uint32_t const link = mLink;
  std::uint32_t const skip = mSkip;

  // the edge
  if ( obdlatch )
  {
    mD1 = d;
    mScaledi = c.scaled;
    mRmw1 = pRmw;
  }
  if ( c.nextphrase )
    mD2 = mD1;
  if ( !obdlatch && empty )
    mFull = false;
  else if ( obdlatch && !empty )
    mFull = true;
  else if ( obdlatch && empty )
    mFull = !mFull;
  if ( c.nextphrase )
    mPad = mIp;
  else if ( c.nextbits )
    mPad = ( mPad + simplified ) & 0x3fu;
  if ( obld1 || c.nextphrase )
    mIp = obld1 ? static_cast<std::uint32_t>( field( d, 49, 6 ) ) : 0u;
  mPaaq = c.pra & 0xffu;
  mPabq = c.prb & 0xffu;
  mBusy1 = busy;
  if ( delpix )
    mDa = mD9 >> 16;
  mD9 = d9next;
  mNextbits2 = mNextbits1;
  mNextbits1 = c.nextbits;
  mNextx1 = c.nextx;
  mLbwa = mLbwad & 0x3ffu;
  if ( c.xld )
    mLbwad = static_cast<std::uint32_t>( d & 0xfffu );
  else if ( nextxx )
  {
    bool const up = !mReflected;
    if ( scaledO ) // a pixel at a time
      mLbwad = ( mLbwad + ( up ? 1u : 0xfffu ) ) & 0xfffu;
    else // two
      mLbwad = ( mLbwad + ( up ? 2u : 0xffeu ) ) & 0xfffu;
  }
  mLbwea2 = c.lbwea1;
  mLbweb2 = c.lbweb1;
  mLbweb52 = mLbweb51;
  mLbweb51 = c.lbweb4;
  mLbwe = ( c.lbwea8 ? 1u : 0u ) | ( c.lbweb8 ? 2u : 0u );
  mLbw = ( ( mLbw << 1 ) | ( c.lbwrite ? 1u : 0u ) ) & 7u;
  mLben = c.lbend;
  mPa10 = bit( c.pa, 0 );
  mPswap = c.pswapd;
  if ( obld2 )
    mXrem = static_cast<std::uint32_t>( d & 0xffu );
  else
    mXrem = ( xs & 0x1fu ) | ( diff << 5 );
  mSmq = c.smd;

  if ( obld2 || c.addrem || c.decrem )
    mNewrem = obld2 ? static_cast<std::uint32_t>( field( d, 16, 8 ) ) : rs;
  if ( obld0 )
    mNewheight = static_cast<std::uint32_t>( field( d, 14, 10 ) );
  else if ( c.decheight )
    mNewheight = ( mNewheight - 1 ) & 0x3ffu;
  if ( obld0 )
    mNewdata = static_cast<std::uint32_t>( field( d, 43, 21 ) );
  else if ( c.addnewdata )
    mNewdata = ( mNewdata + mDwidth ) & 0x1fffffu;
  mWq = c.wd;

  if ( c.pcld || c.pclink )
    mOlp = c.pclink ? link & 0x7ffffu : mOlpd & 0x7ffffu;
  else if ( c.pcinc )
    mOlp = ( mOlp + 1 ) & 0x7ffffu;
  if ( obld0 )
    mData = static_cast<std::uint32_t>( field( d, 43, 21 ) );
  else if ( mDmainc )
    mData = ( mData + skip ) & 0x1fffffu;
  mDmainc = c.dmaen;
  if ( obld1 )
    mIwidth = static_cast<std::uint32_t>( field( d, 28, 10 ) );
  else if ( c.dmaen )
    mIwidth = ( mIwidth - 1 ) & 0x3ffu;
  if ( obld0 )
  {
    mType = static_cast<std::uint32_t>( field( d, 0, 3 ) );
    mYpos = static_cast<std::uint32_t>( field( d, 3, 11 ) );
    mLink = static_cast<std::uint32_t>( field( d, 24, 19 ) );
  }
  if ( obld1 )
  {
    mDepth = static_cast<std::uint32_t>( field( d, 12, 3 ) );
    mSkip = static_cast<std::uint32_t>( field( d, 15, 3 ) );
    mDwidth = static_cast<std::uint32_t>( field( d, 18, 10 ) );
    mIndex = static_cast<std::uint32_t>( field( d, 38, 7 ) ) << 1;
    mReflected = bit( d, 45 );
    mRmw = bit( d, 46 );
    mTransen = bit( d, 47 );
    mRelease = bit( d, 48 );
  }
  if ( obld2 )
  {
    mHscale = static_cast<std::uint32_t>( field( d, 0, 8 ) );
    mVscale = static_cast<std::uint32_t>( field( d, 8, 8 ) );
    mRem = static_cast<std::uint32_t>( field( d, 21, 3 ) );
  }
  mQ = 0;
  for ( unsigned k = 0; k < 10; ++k )
    if ( c.d[k] )
      mQ |= 1u << k;
  mOverrun = overrun;
  mStart2 = pStart1;
  mStart1 = in.start;
  // ack_pipe: set by the latch's request, held until the acknowledge
  bool const latchd[4] = { c.obldd0, c.obldd1, c.obldd2, c.dmaen };
  std::uint32_t q = 0;
  for ( unsigned k = 0; k < 4; ++k )
    if ( latchd[k] || ( bit( mLatchQ, k ) && !in.mack ) )
      q |= 1u << k;
  mLatchQ = q;
  mWbkeni0 = c.wbken0d;
  mWbkeni2 = c.wbken2d;
}

} // namespace jag
