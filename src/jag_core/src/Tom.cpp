#include "jag/Tom.hpp"

#include <algorithm>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <mutex>
#include <thread>

namespace jag
{

namespace
{

// word offsets from $F00000
constexpr std::uint32_t REG_HC = 0x04 / 2;
constexpr std::uint32_t REG_VC = 0x06 / 2;
constexpr std::uint32_t REG_VMODE = 0x28 / 2;
constexpr std::uint32_t REG_HP = 0x2e / 2;
constexpr std::uint32_t REG_HDE = 0x3c / 2;
constexpr std::uint32_t REG_VP = 0x3e / 2;
constexpr std::uint32_t REG_VI = 0x4e / 2;
constexpr std::uint32_t REG_PIT0 = 0x50 / 2;
constexpr std::uint32_t REG_PIT1 = 0x52 / 2;
constexpr std::uint32_t REG_INT1 = 0xe0 / 2;
constexpr std::uint32_t REG_INT2 = 0xe2 / 2;

constexpr std::uint32_t REG_BORD1 = 0x2a / 2;
constexpr std::uint32_t REG_BORD2 = 0x2c / 2;
constexpr std::uint32_t REG_OLP1 = 0x20 / 2;
constexpr std::uint32_t REG_OLP2 = 0x22 / 2;
constexpr std::uint32_t REG_OBF = 0x26 / 2;
constexpr std::uint32_t REG_HBB = 0x30 / 2;
constexpr std::uint32_t REG_HBE = 0x32 / 2;
constexpr std::uint32_t REG_HDB1 = 0x38 / 2;
constexpr std::uint32_t REG_HDB2 = 0x3a / 2;
constexpr std::uint32_t REG_VDB = 0x46 / 2;
constexpr std::uint32_t REG_VDE = 0x48 / 2;
constexpr std::uint32_t REG_BG = 0x58 / 2;
constexpr std::uint32_t REG_VBB = 0x40 / 2;
constexpr std::uint32_t REG_VBE = 0x42 / 2;

constexpr std::uint32_t VIDEN = 1;

// Tom's CRY colour tables: red, green, blue
constexpr std::uint8_t CRY_ROM[3][256] = {
#include "CryRom.inc"
};
constexpr std::size_t LINE_BYTES = 3 * 4096; // bytes of a line the collector keeps

} // namespace

// `n` cycles of the counters: HC's low bits count to HP and wrap (hpeq), toggling bit 10 and
// counting VC, whose low bits wrap the same way at VP (nextfield), toggling bit 11.
void Tom::stepCounters( std::uint64_t n )
{
  if ( !( mReg[REG_VMODE] & VIDEN ) || n == 0 )
    return;
  std::uint32_t const hp = mReg[REG_HP] & 0x3ffu;
  std::uint32_t const vp = mReg[REG_VP] & 0x7ffu;
  std::uint32_t const lo = mHc & 0x3ffu;
  std::uint64_t const toWrap = ( hp - lo ) & 0x3ffu; // cycles before the one HC equals HP in
  if ( n <= toWrap )
  {
    mHc = ( mHc & 0x400u ) | ( lo + static_cast<std::uint32_t>( n ) );
    return;
  }
  std::uint64_t const period = hp + 1ull;
  std::uint64_t const wraps = 1 + ( n - toWrap - 1 ) / period;
  std::uint64_t const after = ( n - toWrap - 1 ) % period; // cycles into the last half line
  mHc = ( ( mHc ^ ( ( wraps & 1 ) << 10 ) ) & 0x400u ) | static_cast<std::uint32_t>( after );
  // VC counts once a wrap
  std::uint32_t const vlo = mVc & 0x7ffu;
  std::uint64_t const toField = ( vp - vlo ) & 0x7ffu; // wraps before the one VC equals VP at
  if ( wraps <= toField )
  {
    mVc = ( mVc & 0x800u ) | ( vlo + static_cast<std::uint32_t>( wraps ) );
    return;
  }
  std::uint64_t const fields = 1 + ( wraps - toField - 1 ) / ( vp + 1ull );
  std::uint64_t const into = ( wraps - toField - 1 ) % ( vp + 1ull );
  mVc = ( ( mVc ^ ( ( fields & 1 ) << 11 ) ) & 0x800u ) | static_cast<std::uint32_t>( into );
}

// The counters and misc.v up to the start of `cycle`: the vertical interrupt latched (lvint) the
// cycle after vintd, its rise seen by vi1 a cycle later and taken into INT1's i[0] at the end of
// that cycle if enabled then (vip); the PIT's carry taken into i[3] at the end of its cycle (tip).
// A latch already set stays until acknowledged.
void Tom::advance( std::uint64_t cycle )
{
  if ( cycle <= mAt )
    return;
  if ( !mLvint )
  {
    std::uint64_t const vint = nextVint();
    if ( vint < cycle )
    {
      mLvint = true;
      mRise = vint + 1;
      mVip = true;
    }
  }
  // a request seen in INT1 from a cycle on sets ihandler in the next (intl, ihd)
  if ( mVip && mRise + 2 <= cycle )
  {
    mVip = false;
    if ( mIe & 1u )
    {
      mPending |= 1u;
      mIhandlerFrom = std::min( mIhandlerFrom, mRise + 3 );
      noteInterrupt( mRise + 2 );
    }
  }
  // Jerry's line seen high by di1 the cycle after it rises, taken into i[4] at the end of that
  // cycle if enabled (dip)
  if ( mJerry )
  {
    std::uint64_t const rise = mJerry->riseIn( mAt, cycle );
    if ( rise != UINT64_MAX && ( mIe & 16u ) && !( mPending & 16u ) )
    {
      mPending |= 16u;
      mIhandlerFrom = std::min( mIhandlerFrom, rise + 3 );
      noteInterrupt( rise + 2 );
    }
  }
  if ( ( mIe & 8u ) && !( mPending & 8u ) )
  {
    std::uint64_t const next = mPit.firstRequestFrom( mAt );
    if ( next < cycle )
    {
      mPending |= 8u;
      mIhandlerFrom = std::min( mIhandlerFrom, next + 2 );
      noteInterrupt( next + 1 );
    }
  }
  stepCounters( cycle - mAt );
  mAt = cycle;
}

std::uint64_t Tom::ihandlerSteady( std::uint64_t cycle, bool& value )
{
  if ( mIe == 0 && mIhandlerFrom == UINT64_MAX )
  {
    value = false;
    return UINT64_MAX;
  }
  advance( cycle );
  value = cycle >= mIhandlerFrom;
  if ( value ) // until INT2 is written with none pending
    return UINT64_MAX;
  std::uint64_t until = mIhandlerFrom;
  if ( mIe & 1u )
  {
    if ( mVip )
      until = std::min( until, mRise + 3 );
    else if ( !mLvint )
    {
      std::uint64_t const vint = nextVint();
      if ( vint != UINT64_MAX )
        until = std::min( until, vint + 4 );
    }
  }
  if ( mJerry && ( mIe & 16u ) && !( mPending & 16u ) )
  {
    std::uint64_t const rise = mJerry->nextRise( cycle );
    if ( rise != UINT64_MAX )
      until = std::min( until, rise + 3 );
  }
  if ( ( mIe & 8u ) && !( mPending & 8u ) )
  {
    std::uint64_t const next = mPit.firstRequestFrom( mAt );
    if ( next != UINT64_MAX )
      until = std::min( until, next + 2 );
  }
  return until;
}

// The first cycle from mAt on in whose start VC's low bits equal VI and HC equals HDE (vintd).
std::uint64_t Tom::nextVint()
{
  if ( mVintKnown && ( mVintAt == UINT64_MAX || mVintAt >= mAt ) )
    return mVintAt;
  mVintKnown = true;
  mVintAt = UINT64_MAX;
  if ( !( mReg[REG_VMODE] & VIDEN ) )
    return mVintAt;
  std::uint32_t const hp = mReg[REG_HP] & 0x3ffu;
  std::uint32_t const vp = mReg[REG_VP] & 0x7ffu;
  std::uint32_t const hde = mReg[REG_HDE] & 0x7ffu;
  std::uint32_t const vi = mReg[REG_VI] & 0x7ffu;
  std::uint32_t hc = mHc;
  std::uint32_t vc = mVc;
  std::uint64_t at = mAt;
  // a half line at a time, for two fields at most
  for ( std::uint32_t k = 0; k < 2 * 2048 + 2; ++k )
  {
    std::uint32_t const lo = hc & 0x3ffu;
    std::uint64_t const toWrap = ( hp - lo ) & 0x3ffu;
    if ( ( vc & 0x7ffu ) == vi && ( hc & 0x400u ) == ( hde & 0x400u ) &&
         ( ( ( hde & 0x3ffu ) - lo ) & 0x3ffu ) <= toWrap )
    {
      mVintAt = at + ( ( ( hde & 0x3ffu ) - lo ) & 0x3ffu );
      return mVintAt;
    }
    at += toWrap + 1;
    hc = ( hc ^ 0x400u ) & 0x400u;
    vc = ( vc & 0x7ffu ) == vp ? ( vc ^ 0x800u ) & 0x800u : ( vc & 0x800u ) | ( ( vc + 1 ) & 0x7ffu );
  }
  return mVintAt;
}

namespace
{

// A frame's hash: "<FNV-1a of its lines> <width>x<height>", each line its pixel count (two bytes)
// and its RGB bytes.
std::string frameHash( std::vector<std::vector<std::uint8_t>> const& lines )
{
  std::uint32_t hash = 0x811c9dc5;
  std::size_t width = 0;
  auto const mix = [&hash]( std::uint8_t byte )
  {
    hash ^= byte;
    hash *= 0x01000193;
  };
  for ( auto const& line : lines )
  {
    width = std::max( width, line.size() / 3 );
    std::uint32_t const n = static_cast<std::uint32_t>( line.size() / 3 );
    mix( static_cast<std::uint8_t>( n >> 8 ) );
    mix( static_cast<std::uint8_t>( n ) );
    for ( std::uint8_t const byte : line )
      mix( byte );
  }
  char text[48];
  std::snprintf( text, sizeof text, "%08x %zux%zu", hash, width, lines.size() );
  return text;
}

} // namespace

// The frames' hashes, worked out on a thread of their own, started with the first frame: a frame's
// lines are handed over as it ends, the hashes kept in the order the frames came.
class Tom::FrameHasher
{
public:
  ~FrameHasher()
  {
    {
      std::lock_guard const lock( mMutex );
      mStop = true;
    }
    mWake.notify_all();
    if ( mThread.joinable() )
      mThread.join();
  }

  void add( std::vector<std::vector<std::uint8_t>>&& lines )
  {
    {
      std::lock_guard const lock( mMutex );
      mJobs.push_back( std::move( lines ) );
    }
    if ( !mThread.joinable() )
      mThread = std::thread( [this] { run(); } );
    mWake.notify_all();
  }

  // Every frame handed over hashed.
  std::vector<std::string> const& hashes()
  {
    std::unique_lock lock( mMutex );
    mDone.wait( lock, [this] { return mJobs.empty() && !mBusy; } );
    return mHashes;
  }

private:
  void run()
  {
    std::unique_lock lock( mMutex );
    for ( ;; )
    {
      mWake.wait( lock, [this] { return mStop || !mJobs.empty(); } );
      if ( mJobs.empty() )
        return;
      std::vector<std::vector<std::uint8_t>> lines = std::move( mJobs.front() );
      mJobs.pop_front();
      mBusy = true;
      lock.unlock();
      std::string hash = frameHash( lines );
      lock.lock();
      mHashes.push_back( std::move( hash ) );
      mBusy = false;
      mDone.notify_all();
    }
  }

  std::mutex mMutex;
  std::condition_variable mWake;
  std::condition_variable mDone;
  std::deque<std::vector<std::vector<std::uint8_t>>> mJobs;
  std::vector<std::string> mHashes;
  bool mBusy = false;
  bool mStop = false;
  std::thread mThread; // last: started after the rest, joined first
};

Tom::Tom() : mHasher( std::make_unique<FrameHasher>() ) {}

Tom::~Tom() = default;

std::vector<std::string> const& Tom::frames() const
{
  return mHasher->hashes();
}

void Tom::finishFrame()
{
  mLines.resize( mLineCount );
  if ( mFrameSink )
    mFrameSink( mFrameContext, mLines );
  mHasher->add( std::move( mLines ) );
  mLines.clear();
}

// One cycle of the video output: what the MiSTer core's output shows in it (vid_ce, hblank_obuf,
// vblank_obuf, pix.v's last stage), then the clock edge: vid.v's blanking, display and pixel
// pacing, the line buffer read and cleared to the background, pix.v's and cryrgb.v's pipeline,
// the pixel enable's divider, the counters.
Tom::VideoConsts Tom::videoConsts() const
{
  VideoConsts k{};
  std::uint32_t const vm = mReg[REG_VMODE];
  k.vm = vm;
  k.cry16 = ( vm & 6u ) == 0;
  k.rgb24 = ( vm & 6u ) == 2;
  k.rg16 = ( vm & 6u ) == 4;
  k.rgb16 = ( vm & 6u ) == 6;
  k.viden = vm & VIDEN;
  k.ppn = ( vm >> 9 ) & 7u;
  k.longpix = !( k.cry16 || k.rgb16 );
  k.vsxp = !k.longpix || k.ppn != 0;
  k.incen = vm & 0x10u;
  k.varmod = vm & 0x100u;
  k.bgEnable = vm & 0x80u;
  k.hp = mReg[REG_HP] & 0x3ffu;
  k.vp = mReg[REG_VP] & 0x7ffu;
  k.hdb1 = mReg[REG_HDB1] & 0x7ffu;
  k.hdb2 = mReg[REG_HDB2] & 0x7ffu;
  k.hde = mReg[REG_HDE] & 0x7ffu;
  k.hbb = mReg[REG_HBB] & 0x7ffu;
  k.hbe = mReg[REG_HBE] & 0x7ffu;
  k.vbb = mReg[REG_VBB] & 0x7ffu;
  k.vbe = mReg[REG_VBE] & 0x7ffu;
  k.vdb = mReg[REG_VDB] & 0x7ffu;
  k.vde = mReg[REG_VDE] & 0x7ffu;
  return k;
}

inline void Tom::renderCycle( VideoConsts const& k )
{
  bool const cry16 = k.cry16, rgb24 = k.rgb24, rg16 = k.rg16, rgb16 = k.rgb16;
  bool const blank = mHblank || mVblank;

  // the collector (sim/sim_main.cpp)
  std::uint32_t pixel = mPd6;
  if ( rg16 ) // pix.v: the line buffer's high word as red and green, blue the display's state
  {
    std::uint32_t const pd4 = mPd1 >> 16;
    pixel = pd4 | ( ( mVactive ? 2u : 0u ) | ( blank ? 1u : 0u ) ) << 16;
  }
  if ( mPixDiv == 0 && !mHblankOut && !mVblankOut && mLine.size() < LINE_BYTES )
  {
    mLine.push_back( static_cast<std::uint8_t>( pixel ) );
    mLine.push_back( static_cast<std::uint8_t>( pixel >> 8 ) );
    mLine.push_back( static_cast<std::uint8_t>( pixel >> 16 ) );
  }
  if ( mHblankOut && !mOldHblankOut && !mLine.empty() )
  {
    if ( mLineCount == mLines.size() )
      mLines.emplace_back();
    std::swap( mLines[mLineCount], mLine ); // the line kept, a buffer of an earlier frame taken up
    ++mLineCount;
    mLine.clear();
  }
  if ( mVblankOut && !mOldVblankOut )
  {
    if ( mStarted && mLineCount != 0 )
      finishFrame();
    mStarted = true;
    mFieldBlanked = true;
    mLineCount = 0;
    mLine.clear();
  }
  mOldHblankOut = mHblankOut;
  mOldVblankOut = mVblankOut;

  bool const viden = k.viden;
  std::uint32_t const ppn = k.ppn;
  std::uint32_t const hc = mRhc;
  std::uint32_t const vc = mRvc & 0x7ffu;

  // vid.v's display signals
  bool const hdb1eq = k.hdb1 == hc;
  bool const hdb2eq = k.hdb2 == hc;
  bool const hdeeq = k.hde == hc;
  bool const hdb = hdb1eq || hdb2eq;
  bool const startd = hdb && mVvactive;
  bool const dd = mVdactive && hdb;
  bool const longpix = k.longpix;
  bool const vsxp = k.vsxp;
  bool const pp = mPp == 0;
  bool const nextpixd = ( mWord2 && pp ) || ( longpix && pp );
  bool const nextpixa = vsxp ? mNextpixaq : nextpixd;
  bool const bgw = k.bgEnable && nextpixa && vsxp;
  // lbuf.v: the RAM of the buffer shown drives its bus while the video output reads it (aactive,
  // bactive: vactive); else the write path does (wral, wrbl): what the object processor writes
  // (lbwd, lben), or the data bus (dw)
  auto& shown = mLbufa ? mBuffers.a : mBuffers.b;
  std::uint32_t lbrd = shown[mLbra & 0x1ffu];
  if ( bgw && mVactive ) // the pair cleared: its RAM is written, the background drives the bus (bgwa)
    lbrd = static_cast<std::uint32_t>( mBgc ) << 16 | mBgc;
  else if ( !mVactive )
  {
    std::uint32_t wd = 0;
    lbrd = mOp.lineWrite( wd ) ? wd : mDw;
  }

  // pix.v and cryrgb.v: the colour stages are taken only as the pixel's phase latches them (ppd),
  // so they are worked out only then
  bool const psxp = cry16 || rgb16;
  bool const border = psxp ? mBord3 : mBord1;
  bool const s1 = blank || border;
  bool const s0 = blank || ( !border && rgb24 );
  std::uint32_t const pd5 = s1 ? ( s0 ? 0u : mBorder ) : ( s0 ? mPd1 & 0xffffffu : mCryOut );

  mLbView = { mLbufa, mVactive, mLbra, bgw && mVactive };

  // the clock edge
  mPd6 = pd5;
  if ( mPpd )
  {
    std::uint32_t const r3b = mRgbd ? ( mR2 & 0xf8u ) : ( ( mR2 * mI2 ) >> 8 ) & 0xffu;
    std::uint32_t const b3b = mRgbd ? ( ( mR2 & 7u ) << 5 | ( mI2 >> 6 ) << 3 ) : ( ( mB2 * mI2 ) >> 8 ) & 0xffu;
    std::uint32_t const g3b = mRgbd ? ( ( mI2 & 0x3fu ) << 2 ) : ( ( mG2 * mI2 ) >> 8 ) & 0xffu;
    mCryOut = r3b | g3b << 8 | b3b << 16;
    mBord3 = mBord2;
  }
  if ( pp ) // the stage the next cycle's ppd takes
  {
    std::uint32_t const pd2 = mWord2 ? mPd1 >> 16 : mPd1 & 0xffffu;
    bool const incen = k.incen, varmod = k.varmod;
    bool const rgb = rgb16 || ( varmod && ( pd2 & 1u ) );
    std::uint32_t const cry = ( pd2 & 0xfffeu ) | ( ( pd2 & 1u ) && !incen && !varmod ? 1u : 0u );
    std::uint32_t const cc = cry >> 8;
    mR2 = rgb ? cc : CRY_ROM[0][cc];
    mG2 = rgb ? cc : CRY_ROM[1][cc];
    mB2 = rgb ? cc : CRY_ROM[2][cc];
    mI2 = cry & 0xffu;
    mRgbd = rgb;
  }
  mBord2 = mBord1;
  mBord1 = !mVactive;
  mPpd = pp;
  if ( nextpixd )
    mPd1 = lbrd;
  if ( bgw && mVactive ) // the pair shown cleared to the background (bgwa, bgwb)
    shown[mLbra & 0x1ffu] = static_cast<std::uint32_t>( mBgc ) << 16 | mBgc;
  mLbra = dd ? 0u : ( nextpixa ? ( mLbra + 1 ) & 0x1ffu : mLbra );
  mNextpixaq = nextpixd && !startd;
  mWord2 = ( mWord2 != pp ) || startd;
  mPp = startd ? 0u : ( pp ? ppn : mPp - 1 );
  mPixDiv = mPixDiv == 0 ? ( ppn >= 2 ? 1u : ppn & 1u ) : mPixDiv - 1;
  if ( mStart )
    mVcl = vc;
  if ( !viden )
  {
    mHblank = mVblank = false;
    mHblankOut = mVblankOut = false;
    mVvactive = mVdactive = mVactive = false;
    mLbufa = false;
    mStart = false;
    return;
  }
  bool const hbbeq = k.hbb == hc;
  bool const hbeeq = k.hbe == hc;
  bool const vbbeq = k.vbb == vc;
  bool const vbeeq = k.vbe == vc;
  bool const vdbeq = k.vdb == vc;
  bool const vdeeq = k.vde == vc;
  mHblankOut = mHblank;
  if ( !mHblank )
  {
    mVblankOut = mVblankOut1;
    mVblankOut1 = mVblank;
  }
  mHblank = ( hbbeq && !mHblank ) || ( !hbeeq && mHblank );
  mVblank = ( vbbeq && !mVblank ) || ( !vbeeq && mVblank );
  mStart = startd;
  if ( hdb )
    mVdactive = mVvactive;
  mVvactive = ( vdbeq && !mVvactive ) || ( !vdeeq && mVvactive );
  mVactive = ( dd && !mVactive ) || ( !hdeeq && mVactive );
  mLbufa = dd ? !mLbufa : mLbufa;
  // the counters, a cycle
  std::uint32_t const hp = k.hp;
  if ( ( mRhc & 0x3ffu ) == hp )
  {
    mRhc = ( mRhc ^ 0x400u ) & 0x400u;
    std::uint32_t const vp = k.vp;
    if ( ( mRvc & 0x7ffu ) == vp )
    {
      // a field without a vertical blank (VBB past VP: Native) ends its frame as VC wraps, where a
      // television's would end at the vertical sync
      if ( !mFieldBlanked && mStarted && mLineCount != 0 )
      {
        finishFrame();
        mLineCount = 0;
        mLine.clear();
      }
      mFieldBlanked = false;
    }
    mRvc = ( mRvc & 0x7ffu ) == vp ? ( mRvc ^ 0x800u ) & 0x800u : ( mRvc & 0x800u ) | ( ( mRvc + 1 ) & 0x7ffu );
  }
  else
    mRhc = ( mRhc & 0x400u ) | ( ( mRhc + 1 ) & 0x3ffu );
}

// The cycles from this one in which the video output only moves its pixels along: HC at none of
// HDB1, HDB2, HDE, HBB, HBE nor wrapping, VC's comparisons leaving the blanking and the display as
// they are, nothing to collect but pixels. 0 if this cycle is not one of them.
std::uint64_t Tom::steadyRun( VideoConsts const& k ) const
{
  if ( !k.viden || mStart || mHblankOut != mHblank || mOldHblankOut != mHblankOut || mOldVblankOut != mVblankOut )
    return 0;
  if ( !mHblank && ( mVblankOut != mVblankOut1 || mVblankOut1 != mVblank ) )
    return 0;
  std::uint32_t const vc = mRvc & 0x7ffu;
  bool const vbbeq = k.vbb == vc, vbeeq = k.vbe == vc, vdbeq = k.vdb == vc, vdeeq = k.vde == vc;
  if ( ( ( vbbeq && !mVblank ) || ( !vbeeq && mVblank ) ) != mVblank ||
       ( ( vdbeq && !mVvactive ) || ( !vdeeq && mVvactive ) ) != mVvactive )
    return 0;
  std::uint32_t const low = mRhc & 0x3ffu, half = mRhc & 0x400u;
  if ( low >= k.hp )
    return 0;
  std::uint64_t n = k.hp - low; // up to the cycle HC wraps in
  for ( std::uint32_t const e : { k.hdb1, k.hdb2, k.hde, k.hbb, k.hbe } )
    if ( ( e & 0x400u ) == half && ( e & 0x3ffu ) >= low )
      n = std::min<std::uint64_t>( n, ( e & 0x3ffu ) - low );
  return n;
}

// `n` cycles of a steady run (steadyRun): renderCycle's, of them only what changes.
void Tom::renderSteady( VideoConsts const& k, std::uint64_t n )
{
  bool const blank = mHblank || mVblank;
  bool const collect = !mHblankOut && !mVblankOut;
  bool const psxp = k.cry16 || k.rgb16;
  auto& shown = mLbufa ? mBuffers.a : mBuffers.b;
  std::uint32_t const background = static_cast<std::uint32_t>( mBgc ) << 16 | mBgc;
  std::uint32_t idle = mDw; // what the bus carries outside the display
  {
    std::uint32_t wd = 0;
    if ( mOp.lineWrite( wd ) )
      idle = wd;
  }
  bool const vactive = mVactive;
  std::uint32_t viewLbra = mLbra;
  bool viewBgw = false;
  for ( std::uint64_t i = 0; i < n; ++i )
  {
    std::uint32_t pixel = mPd6;
    if ( k.rg16 )
      pixel = mPd1 >> 16 | ( ( vactive ? 2u : 0u ) | ( blank ? 1u : 0u ) ) << 16;
    if ( mPixDiv == 0 && collect && mLine.size() < LINE_BYTES )
    {
      mLine.push_back( static_cast<std::uint8_t>( pixel ) );
      mLine.push_back( static_cast<std::uint8_t>( pixel >> 8 ) );
      mLine.push_back( static_cast<std::uint8_t>( pixel >> 16 ) );
    }
    bool const pp = mPp == 0;
    bool const nextpixd = pp && ( mWord2 || k.longpix );
    bool const nextpixa = k.vsxp ? mNextpixaq : nextpixd;
    bool const bgw = k.bgEnable && nextpixa && k.vsxp;
    bool const border = psxp ? mBord3 : mBord1;
    bool const s1 = blank || border;
    bool const s0 = blank || ( !border && k.rgb24 );
    viewLbra = mLbra;
    viewBgw = bgw && vactive;
    mPd6 = s1 ? ( s0 ? 0u : mBorder ) : ( s0 ? mPd1 & 0xffffffu : mCryOut );
    if ( mPpd )
    {
      std::uint32_t const r3b = mRgbd ? ( mR2 & 0xf8u ) : ( ( mR2 * mI2 ) >> 8 ) & 0xffu;
      std::uint32_t const b3b = mRgbd ? ( ( mR2 & 7u ) << 5 | ( mI2 >> 6 ) << 3 ) : ( ( mB2 * mI2 ) >> 8 ) & 0xffu;
      std::uint32_t const g3b = mRgbd ? ( ( mI2 & 0x3fu ) << 2 ) : ( ( mG2 * mI2 ) >> 8 ) & 0xffu;
      mCryOut = r3b | g3b << 8 | b3b << 16;
      mBord3 = mBord2;
    }
    if ( pp )
    {
      std::uint32_t const pd2 = mWord2 ? mPd1 >> 16 : mPd1 & 0xffffu;
      bool const rgb = k.rgb16 || ( k.varmod && ( pd2 & 1u ) );
      std::uint32_t const cry = ( pd2 & 0xfffeu ) | ( ( pd2 & 1u ) && !k.incen && !k.varmod ? 1u : 0u );
      std::uint32_t const cc = cry >> 8;
      mR2 = rgb ? cc : CRY_ROM[0][cc];
      mG2 = rgb ? cc : CRY_ROM[1][cc];
      mB2 = rgb ? cc : CRY_ROM[2][cc];
      mI2 = cry & 0xffu;
      mRgbd = rgb;
    }
    mBord2 = mBord1;
    mBord1 = !vactive;
    mPpd = pp;
    if ( nextpixd )
      mPd1 = !vactive ? idle : bgw ? background : shown[mLbra & 0x1ffu];
    if ( bgw && vactive )
      shown[mLbra & 0x1ffu] = background;
    if ( nextpixa )
      mLbra = ( mLbra + 1 ) & 0x1ffu;
    mNextpixaq = nextpixd;
    mWord2 = mWord2 != pp;
    mPp = pp ? k.ppn : mPp - 1;
    mPixDiv = mPixDiv == 0 ? ( k.ppn >= 2 ? 1u : k.ppn & 1u ) : mPixDiv - 1;
  }
  if ( n != 0 )
    mLbView = { mLbufa, vactive, viewLbra, viewBgw };
  mRhc += static_cast<std::uint32_t>( n );
}

// `n` cycles of the video output, its registers as they stand.
void Tom::renderCycles( VideoConsts const& k, std::uint64_t n )
{
  while ( n != 0 )
  {
    if ( mRenderAt < mSteadyUntil )
    {
      std::uint64_t const run = std::min( n, mSteadyUntil - mRenderAt );
      renderSteady( k, run );
      n -= run;
      mRenderAt += run;
      continue;
    }
    if ( std::uint64_t const run = steadyRun( k ); run != 0 )
    {
      mSteadyUntil = mRenderAt + run;
      continue;
    }
    renderCycle( k );
    ++mRenderAt;
    --n;
  }
}

std::uint64_t Tom::renderToStart( std::uint64_t from, std::uint64_t to )
{
  renderTo( from );
  VideoConsts const& k = consts();
  for ( std::uint64_t c = from; c < to; ++c )
  {
    if ( mStart )
      return c;
    if ( !k.viden )
      renderTo( c + 1 );
    else if ( mRenderAt <= c )
    {
      // start is set only by a cycle with HC at HDB, which ends a steady run
      if ( mRenderAt >= mSteadyUntil )
        mSteadyUntil = mRenderAt + steadyRun( k );
      if ( mRenderAt < mSteadyUntil )
      {
        std::uint64_t const run = std::min( to - c, mSteadyUntil - mRenderAt );
        renderSteady( k, run );
        mRenderAt += run;
        c += run - 1;
        continue;
      }
      renderCycle( k );
      ++mRenderAt;
    }
  }
  return to;
}

std::uint64_t Tom::nextLineStart( std::uint64_t cycle )
{
  renderTo( cycle );
  if ( mStart || !( mReg[REG_VMODE] & VIDEN ) )
    return cycle;
  // HC counts a half line up to HP and goes on with the other half (bit 10); the cycle rendered
  // with HC at HDB sets start for the next one
  std::uint32_t const hp = mReg[REG_HP] & 0x3ffu;
  if ( ( mRhc & 0x3ffu ) > hp )
    return cycle;
  std::uint64_t const half = hp + 1;
  auto const position = [half]( std::uint32_t hc ) { return ( hc >> 10 & 1u ) * half + ( hc & 0x3ffu ); };
  std::uint64_t const at = position( mRhc );
  std::uint64_t next = UINT64_MAX;
  for ( std::uint32_t const hdb : { mReg[REG_HDB1] & 0x7ffu, mReg[REG_HDB2] & 0x7ffu } )
    if ( ( hdb & 0x3ffu ) <= hp )
      next = std::min( next, ( position( hdb ) + 2 * half - at ) % ( 2 * half ) );
  return next == UINT64_MAX ? UINT64_MAX : cycle + next + 1;
}

ObjectProcessor::Inputs Tom::opInputsWaiting( std::uint64_t cycle )
{
  if ( cycle < mWaitUntil && mObfwCycle != cycle )
  {
    ObjectProcessor::Inputs in;
    in.vc = mVcl;
    in.hcb10 = ( mRhc >> 10 ) & 1u; // as last rendered: read only with a branch object, not waiting
    in.lbufa = mLbufa;
    return in;
  }
  ObjectProcessor::Inputs const in = opInputs( cycle );
  mWaitUntil = in.start ? 0 : nextLineStart( cycle );
  return in;
}

ObjectProcessor::Inputs Tom::opInputs( std::uint64_t cycle )
{
  renderTo( cycle );
  ObjectProcessor::Inputs in;
  in.start = mStart;
  in.vc = mVcl;
  in.hcb10 = ( mRhc >> 10 ) & 1u;
  in.lbufa = mLbufa;
  in.obfw = mObfwCycle == cycle;
  return in;
}

void Tom::noteInterrupt( std::uint64_t from )
{
  bool const line = mPending != 0;
  if ( line == mIntl[( mIntlCount - 1 ) % INTL_KEPT].line )
    return;
  mIntl[mIntlCount % INTL_KEPT] = { from, line };
  ++mIntlCount;
}

bool Tom::interrupt( std::uint64_t cycle )
{
  advance( cycle );
  for ( std::uint32_t k = 0; k < INTL_KEPT && k < mIntlCount; ++k )
  {
    IntlChange const& c = mIntl[( mIntlCount - 1 - k ) % INTL_KEPT];
    if ( c.from <= cycle )
      return c.line;
  }
  return false;
}

void Tom::gpuInterrupt( std::uint64_t cycle )
{
  advance( cycle );
  if ( mIe & 2u )
  {
    mPending |= 2u;
    mIhandlerFrom = std::min( mIhandlerFrom, cycle + 2 );
    noteInterrupt( cycle + 1 );
  }
}

void Tom::objectInterrupt( std::uint64_t cycle )
{
  advance( cycle );
  if ( mIe & 4u )
  {
    mPending |= 4u;
    mIhandlerFrom = std::min( mIhandlerFrom, cycle + 2 );
    noteInterrupt( cycle + 1 );
  }
}

void Tom::renderTo( std::uint64_t cycle )
{
  while ( mRenderAt < cycle )
  {
    // with the time base stopped and nothing more to collect, only the divider moves
    bool const settled = !mHblank && !mVblank && !mHblankOut && !mVblankOut && mPd6 == mBorder && !mVactive &&
                         !mVvactive && !mStart && ( !mStarted || mLine.size() >= LINE_BYTES ) &&
                         mOldHblankOut == mHblankOut && mOldVblankOut == mVblankOut;
    if ( !( mReg[REG_VMODE] & VIDEN ) && settled )
    {
      std::uint64_t const n = cycle - mRenderAt;
      std::uint32_t const ppn = ( mReg[REG_VMODE] >> 9 ) & 7u;
      if ( ppn == 0 )
        mPixDiv = 0;
      else
        mPixDiv ^= static_cast<std::uint32_t>( n & 1 );
      mRenderAt = cycle;
      return;
    }
    VideoConsts const& k = consts();
    if ( !k.viden )
    {
      renderCycle( k );
      ++mRenderAt;
      continue;
    }
    renderCycles( k, cycle - mRenderAt );
  }
}

// The video output clears the pair it reads in this cycle to the background (vid.v bgw, while
// vactive).
bool Tom::backgroundWrite() const
{
  std::uint32_t const vm = mReg[REG_VMODE];
  bool const cry16 = ( vm & 6u ) == 0, rgb16 = ( vm & 6u ) == 6;
  std::uint32_t const ppn = ( vm >> 9 ) & 7u;
  bool const longpix = !( cry16 || rgb16 );
  bool const vsxp = !longpix || ppn != 0;
  bool const pp = mPp == 0;
  bool const nextpixd = ( mWord2 && pp ) || ( longpix && pp );
  bool const nextpixa = vsxp ? mNextpixaq : nextpixd;
  return ( vm & 0x80u ) && nextpixa && vsxp && mVactive;
}

// A RISC's word of the line buffers (abus.v lba, lbb; lbuf.v): A at $F00800, B at $F01000, the one
// not shown at $F01800, a long word of two pixels at each long word address, the word at an
// address two past it the high pixel. The buffer shown is addressed by the video output's read
// pointer while the line is displayed (lbaactive, lbbactive): `shown` tells. As the output `view`
// has it.
std::uint32_t* Tom::lineBufferWord( std::uint32_t address, LineBufferView const& view, bool& shown )
{
  std::uint32_t const offset = address & 0x1fffu;
  bool const a = offset < 0x1000u || ( offset >= 0x1800u && !view.lbufa );
  shown = view.vactive && a == view.lbufa;
  return &( a ? mBuffers.a : mBuffers.b )[shown ? view.lbra & 0x1ffu : ( offset >> 2 ) & 0x1ffu];
}

void Tom::write( std::uint64_t cycle, std::uint32_t address, std::uint16_t value )
{
  std::uint32_t const reg = ( address & 0xffu ) >> 1;
  renderTo( cycle );
  if ( ( address & 0x1fffu ) >= 0x800u )
  {
    // into the buffer shown, at the output's read pointer, the RAM takes what its bus carries: the
    // background colour while the pair is cleared, else nothing (the data not driven there, wral)
    bool shown = false;
    LineBufferView const now{ mLbufa, mVactive, mLbra, backgroundWrite() };
    std::uint32_t* word = lineBufferWord( address, now, shown );
    std::uint32_t const v = shown ? ( now.bgw ? mBgc : 0u ) : value;
    *word = ( address & 2u ) ? ( *word & 0xffffu ) | v << 16 : ( *word & 0xffff0000u ) | v;
    return;
  }
  if ( ( address & 0xc00u ) == 0x400u ) // the CLUT, both copies
  {
    mOp.writeClut( ( address >> 1 ) & 0xffu, value );
    return;
  }
  if ( ( address & 0xf00u ) != 0 )
    return;
  advance( cycle );
  switch ( reg )
  {
  case REG_PIT0:
    mPit.strobe( cycle, 0, value );
    return;
  case REG_PIT1:
    mPit.strobe( cycle, 1, value );
    return;
  case REG_INT1:
    // the requests acknowledged (ack, bits 8 to 12), the vertical one's latch cleared (vclr), with
    // vintbugfix also as the vertical interrupt is enabled; the enables (ie)
    mPending &= ~( ( value >> 8 ) & 31u );
    noteInterrupt( cycle + 1 );
    if ( ( value & 0x100u ) || ( mVintFix && ( value & 1u ) && !( mIe & 1u ) ) )
    {
      mLvint = false;
      mVip = false;
    }
    mIe = value & 31u;
    return;
  case REG_INT2:
    // ihandler cleared from the next cycle, unless a request is pending
    if ( mPending == 0 )
      mIhandlerFrom = UINT64_MAX;
    return;
  case REG_HC: // loaded at the end of the cycle (hcwr), in place of the count
    advance( cycle + 1 );
    renderTo( cycle + 1 );
    mHc = mRhc = value & 0x7ffu;
    break;
  case REG_VC:
    advance( cycle + 1 );
    renderTo( cycle + 1 );
    mVc = mRvc = value & 0xfffu;
    break;
  case REG_OLP1:
  case REG_OLP2:
    mOp.writeOlp( reg == REG_OLP2, value );
    break;
  case REG_OBF:
    mOp.writeObf( value );
    mObfwCycle = cycle;
    break;
  case REG_BG:
    mBgc = value;
    break;
  case REG_BORD1:
    mBorder = ( mBorder & 0xff0000u ) | value;
    break;
  case REG_BORD2:
    mBorder = ( mBorder & 0xffffu ) | ( value & 0xffu ) << 16;
    break;
  default:
    break;
  }
  mReg[reg & 0x7fu] = value;
  mVintKnown = false;
  mConstsValid = false;
  mSteadyUntil = 0;
  mWaitUntil = 0;
}

std::uint16_t Tom::read( std::uint64_t cycle, std::uint32_t address )
{
  std::uint32_t const reg = ( address & 0xffu ) >> 1;
  if ( ( address & 0x1fffu ) >= 0x800u )
  {
    // the read latched a cycle late (lbd): the buffer's bus as the output left it in the cycle before,
    // the background colour on it while that pair was cleared (bgwa, bgwb)
    renderTo( cycle );
    bool shown = false;
    std::uint32_t const* word = lineBufferWord( address, mLbView, shown );
    if ( shown && mLbView.bgw )
      return mBgc;
    return static_cast<std::uint16_t>( ( address & 2u ) ? *word >> 16 : *word );
  }
  if ( ( address & 0xc00u ) == 0x400u )
    return mOp.readClut( ( address >> 1 ) & 0xffu );
  if ( ( address & 0xf00u ) != 0 )
    return 0;
  switch ( reg )
  {
  case REG_HC:
    advance( cycle );
    return static_cast<std::uint16_t>( mHc );
  case REG_VC:
    advance( cycle );
    return static_cast<std::uint16_t>( mVc );
  case REG_PIT0:
    return mPit.read( cycle, 0 );
  case REG_PIT1:
    return mPit.read( cycle, 1 );
  case REG_INT1:
    advance( cycle );
    return static_cast<std::uint16_t>( mPending );
  default:
    return 0;
  }
}

} // namespace jag
