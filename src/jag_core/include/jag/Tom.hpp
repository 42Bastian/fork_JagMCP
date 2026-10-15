#pragma once

#include "jag/JerryInterrupts.hpp"
#include "jag/ObjectProcessor.hpp"
#include "jag/Timers.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace jag
{

// Tom's registers that are neither the bus's nor the GPU's: the video time base (vid.v), the PIT
// and the interrupt control of the 68000 (misc.v), written and read through Tom's internal I/O
// cycles a word at a time.
//
// The time base counts HC every cycle while VMODE's VIDEN is set: HC's low ten bits up to HP, then
// back to 0 with bit 10 toggled (a half line); VC's low eleven bits once a half line up to VP, then
// back to 0 with bit 11 toggled (a field). The counters are kept at the start of a cycle and brought
// forward by arithmetic. The vertical interrupt is made where VC equals VI and HC equals HDE; it
// reaches INT1 through misc.v's latch and edge detector. The PIT is Jerry's timer circuit, its
// carry the GPU's interrupt 2 (gpu_irq[2], tint).
//
// The video output (renderCycle) reads the line buffers the object processor (ObjectProcessor)
// writes; both are stepped with the bus while the time base runs.
//
// Ports Jaguar_MiSTer rtl/Tom/vid.v, pix.v, cryrgb.v, misc.v and the video output's side of
// rtl/Tom/lbuf.v, read at 43a761d.
class Tom
{
public:
  Tom();
  ~Tom();
  Tom( Tom const& ) = delete;
  Tom& operator=( Tom const& ) = delete;

  // A word written in `cycle` (the internal I/O cycle's write strobe) at `address`.
  void write( std::uint64_t cycle, std::uint32_t address, std::uint16_t value );

  // The word a read latches in `cycle` at `address`.
  std::uint16_t read( std::uint64_t cycle, std::uint32_t address );

  // Whether `address` is one of the registers this models, the CLUT ($F00400 to $F007FF) or the
  // line buffers ($F00800 to $F01FFF).
  static bool decodes( std::uint32_t address )
  {
    return ( address & 0xffffffu ) - 0xf00000u < 0x2000u;
  }

  // Whether a read of `address`, one of decodes', is one a register answers (iodec.v: memc1r to
  // lpvr, ob0r to ob3r, test2r, test3r, pit0r, pit1r, intr, lbrar, the CLUT's and the line buffers').
  // No other drives Tom's read bus, which then holds MEMCON1 (hardware/tom.md).
  static bool readDecodes( std::uint32_t address );

  // The name the Software Reference gives the register at `address`, or null.
  static char const* registerName( std::uint32_t address );

  // The object processor, and what the time base gives it in `cycle` (the output brought there).
  ObjectProcessor& op()
  {
    return mOp;
  }

  ObjectProcessor::Inputs opInputs( std::uint64_t cycle );

  // The same for an object processor waiting for a line: before the next line may start, what it
  // reads of the video output holds, which is then left behind and brought forward in runs.
  ObjectProcessor::Inputs opInputsWaiting( std::uint64_t cycle );

  // OBF was written in `cycle` (obfw).
  bool obfWritten( std::uint64_t cycle ) const
  {
    return mObfwCycle == cycle;
  }

  // The object processor's interrupt to the 68000 (obint) in `cycle`: INT1's bit 2.
  void objectInterrupt( std::uint64_t cycle );

  // The GPU's interrupt to the 68000 (gpuint, G_CTRL's CPUINT written) in `cycle`: INT1's bit 1.
  void gpuInterrupt( std::uint64_t cycle );

  // Jerry's interrupt line (dint), whose rise INT1's bit 4 takes.
  void setJerry( JerryInterrupts* jerry )
  {
    mJerry = jerry;
  }

  // The MiSTer core's vintbugfix (decision 0022): a write of INT1 enabling the vertical interrupt
  // where it was disabled clears its latch, as an acknowledge does.
  void setVintFix( bool on )
  {
    mVintFix = on;
  }

  // misc.v's intl in `cycle`, active: a request is pending in INT1, the 68000's interrupt (level 2).
  // A cycle up to a few changes of it back may be asked for after a later one.
  bool interrupt( std::uint64_t cycle );

  // VIDEN is set: the object processor may start a line.
  bool videoOn() const
  {
    return mReg[0x28 / 2] & 1u;
  }

  // ihandler in `cycle` (below), and the first cycle from it in which it may be otherwise, as far as
  // the interrupts' sources tell: the vertical interrupt, Jerry's line, the PIT. A write of INT1 or
  // INT2, the GPU's or the object processor's interrupt may bring it nearer.
  std::uint64_t ihandlerSteady( std::uint64_t cycle, bool& value );

  // The object processor's inputs while it waits for a line are those of the cycle it last took
  // them in, before this cycle (opInputsWaiting); OBF written in this cycle.
  std::uint64_t opWaitUntil() const
  {
    return mWaitUntil;
  }

  std::uint64_t obfwCycle() const
  {
    return mObfwCycle;
  }

  // misc.v's ihandler in `cycle`: a request pending in INT1 gives the 68000's interrupt handler the
  // bus before Tom's own masters (the arbiter's req[3]), until INT2 is written with none pending.
  bool ihandler( std::uint64_t cycle )
  {
    if ( mIe == 0 && mIhandlerFrom == UINT64_MAX )
      return false;
    advance( cycle );
    return cycle >= mIhandlerFrom;
  }

  // The video output (pix.v) up to the start of `cycle`: the pixels at the MiSTer core's pixel
  // enable (vid_ce), outside its blanking, a line ending as hblank rises and a frame as vblank
  // rises, as the FPGA simulation collects them (sim/sim_main.cpp).
  void renderTo( std::uint64_t cycle );

  // The video output brought forward from `from` towards `to`, stopping at the first cycle a line
  // starts in for the object processor (vid.v start); that cycle, or `to`.
  std::uint64_t renderToStart( std::uint64_t from, std::uint64_t to );

  // The video output brought forward to `cycle`: the first cycle from it a line may start in for
  // the object processor (HC at HDB1 or HDB2, the cycle after), counted by arithmetic.
  std::uint64_t nextLineStart( std::uint64_t cycle );

  // dbus.v's dout and lbuf.v's dw in `cycle`, stepped: the data bus's low long word as the line
  // buffers' write path takes it, which the bus of the buffer shown carries while the video output
  // does not read it (and the object processor does not write). The cycles before it, left behind,
  // are brought forward with the value they had.
  void setDataBus( std::uint64_t cycle, std::uint32_t dw )
  {
    if ( dw != mDw )
    {
      renderTo( cycle );
      mDw = dw;
    }
  }

  // The frames completed: "<FNV-1a of its lines> <width>x<height>".
  std::vector<std::string> const& frames() const;

  // A frame completed, as lines of RGB bytes; null to keep none.
  using FrameSink = void ( * )( void* context, std::vector<std::vector<std::uint8_t>> const& lines );

  void setFrameSink( FrameSink sink, void* context )
  {
    mFrameSink = sink;
    mFrameContext = context;
  }

  // The PIT, as Jerry's timers (timer 0 only): its requests are the GPU's interrupt 2.
  Timers& pit()
  {
    return mPit;
  }

private:
  void advance( std::uint64_t cycle );
  void stepCounters( std::uint64_t n );
  std::uint64_t nextVint();

  Timers mPit{ true };

  // vid.v's registers, by word offset from $F00000
  std::uint16_t mReg[0x80]{};

  // the counters at the start of cycle mAt
  std::uint64_t mAt = 0;
  std::uint32_t mHc = 0; // 11 bits
  std::uint32_t mVc = 0; // 12 bits

  // the vertical interrupt's next cycle (vintd), kept until a write
  std::uint64_t mVintAt = UINT64_MAX;
  bool mVintKnown = false;

  // misc.v at the start of cycle mAt: INT1's enables (ie) and requests (i), the latched vertical
  // interrupt (lvint), the cycle it rose in and whether its edge (vip) is still to be taken
  std::uint32_t mIe = 0;
  std::uint32_t mPending = 0;
  bool mVintFix = false;
  bool mLvint = false;
  std::uint64_t mRise = 0;
  bool mVip = false;
  std::uint64_t mIhandlerFrom = UINT64_MAX; // ihandler is set from this cycle on
  JerryInterrupts* mJerry = nullptr;

  // intl's last changes, each with the cycle it holds from
  struct IntlChange
  {
    std::uint64_t from = 0;
    bool line = false;
  };

  static constexpr std::uint32_t INTL_KEPT = 4;
  IntlChange mIntl[INTL_KEPT]{};
  std::uint32_t mIntlCount = 1;
  void noteInterrupt( std::uint64_t from );

  // the video output at the start of cycle mRenderAt: the counters as the output sees them, vid.v's
  // blanking flip-flops and the MiSTer core's alignment of them, pix.v's border colour and last
  // stage, the pixel enable's divider (jaguar.v pix_ce_div_reg)
  // the registers' values a run of cycles of the video output reads, none of them written in it
  struct VideoConsts
  {
    std::uint32_t vm, ppn, hp, vp;
    bool cry16, rgb24, rg16, rgb16, viden, longpix, vsxp, incen, varmod, bgEnable;
    std::uint32_t hdb1, hdb2, hde, hbb, hbe, vbb, vbe, vdb, vde;
  };

  VideoConsts videoConsts() const;

  VideoConsts const& consts()
  {
    if ( !mConstsValid )
    {
      mConsts = videoConsts();
      mConstsValid = true;
    }
    return mConsts;
  }

  VideoConsts mConsts{};
  bool mConstsValid = false;
  std::uint64_t mSteadyUntil = 0; // the video output's cycles before this one are a steady run
  std::uint64_t mWaitUntil = 0;   // no line starts for the object processor before this cycle
  void renderCycle( VideoConsts const& k );
  std::uint64_t steadyRun( VideoConsts const& k ) const;
  void renderSteady( VideoConsts const& k, std::uint64_t n );
  void renderCycles( VideoConsts const& k, std::uint64_t n );
  struct LineBufferView;
  std::uint32_t* lineBufferWord( std::uint32_t address, LineBufferView const& view, bool& shown );
  bool backgroundWrite() const;
  void finishFrame();
  std::uint64_t mRenderAt = 0;
  std::uint32_t mDw = 0;
  std::uint32_t mRhc = 0;
  std::uint32_t mRvc = 0;
  bool mHblank = false;
  bool mVblank = false;
  bool mHblankOut = false;
  bool mVblankOut1 = false;
  bool mVblankOut = false;
  std::uint32_t mBorder = 0; // bc: BORD1 red and green, BORD2 blue
  std::uint32_t mPd6 = 0;
  std::uint32_t mPixDiv = 0;
  // vid.v's display: the lines displayed (vvactive), the object processor's start, VC as it started
  // (vcl), a displayed line's start (vdactive, dd) and span (vactive), the line buffer read (lbufa),
  // the pixel width's counter (pp), the word of a pair (word2), the read pointer's step (nextpixaq)
  bool mVvactive = false;
  bool mStart = false;
  std::uint32_t mVcl = 0;
  bool mVdactive = false;
  bool mVactive = false;
  bool mLbufa = false;
  std::uint32_t mPp = 0;
  bool mWord2 = false;
  bool mNextpixaq = false;
  // pix.v and cryrgb.v: the read address, the pair read, the pipeline of the colour conversion, the
  // border's delays, the background colour
  std::uint32_t mLbra = 0;
  std::uint32_t mPd1 = 0;
  bool mPpd = false;
  bool mBord1 = true;
  bool mBord2 = true;
  bool mBord3 = true;
  std::uint32_t mR2 = 0, mG2 = 0, mB2 = 0, mI2 = 0;
  bool mRgbd = false;
  std::uint32_t mCryOut = 0; // r_, g_, b_
  std::uint16_t mBgc = 0;
  std::uint64_t mObfwCycle = UINT64_MAX; // OBF written in this cycle

  // the line buffers as the last cycle output left them: what a RISC's read (lbd, a cycle late) sees
  struct LineBufferView
  {
    bool lbufa = false;
    bool vactive = false;
    std::uint32_t lbra = 0;
    bool bgw = false;
  } mLbView;

  LineBuffers mBuffers;
  ObjectProcessor mOp{ mBuffers };

  // the frame being collected
  std::vector<std::uint8_t> mLine;
  std::vector<std::vector<std::uint8_t>> mLines; // the frame's first mLineCount, the rest kept for their room
  std::size_t mLineCount = 0;
  bool mOldHblankOut = false;
  bool mOldVblankOut = false;
  bool mStarted = false;
  bool mFieldBlanked = false; // vblank has risen since VC last wrapped
  // the frames' hashes, worked out on a thread of their own in the order the frames end
  class FrameHasher;
  std::unique_ptr<FrameHasher> mHasher;
  FrameSink mFrameSink = nullptr;
  void* mFrameContext = nullptr;
};

} // namespace jag
