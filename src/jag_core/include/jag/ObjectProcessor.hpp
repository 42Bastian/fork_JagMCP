#pragma once

#include <array>
#include <cstdint>
#include <tuple>

namespace jag
{

// The line buffers (lbuf.v): two of 512 long words, each two 16 bit pixels, the high half the even
// one. The video output reads one while the object processor writes the other; they change places
// at the start of each displayed line.
struct LineBuffers
{
  std::array<std::uint32_t, 512> a{};
  std::array<std::uint32_t, 512> b{};
};

// Tom's object processor, stepped by cycle while it works: the list walker of ob.v, the header
// write-back of wbk.v and the pixel path of obdata.v into the line buffers, with the CLUT. A master
// of Tom's bus (req[5]); it reads the list and the pixel data a phrase at a time, and writes an
// object's header back. Every signal is the netlist's of the same name.
//
// Ports Jaguar_MiSTer rtl/Tom/ob.v, rtl/Tom/wbk.v, rtl/Tom/obdata.v and the object processor's side
// of rtl/Tom/lbuf.v, read at 43a761d.
class ObjectProcessor
{
public:
  explicit ObjectProcessor( LineBuffers& buffers ) : mBuffers( buffers ) {}

  // What the time base and the bus give it in a cycle.
  struct Inputs
  {
    bool start = false;     // vid.v start: a displayed line begins
    std::uint32_t vc = 0;   // vcl: VC as the line began
    bool hcb10 = false;     // HC's bit 10
    bool mack = false;      // the memory controller's acknowledge (ack)
    bool obback = false;    // its grant (ack_[5])
    bool ewr = false;       // the GPU's write acknowledged (ewr), which takes the data bus
    bool obfw = false;      // OBF written in the cycle
    bool lbufa = false;     // the video output reads line buffer A (it writes B), or B (lbufb)
    bool hilo = true;       // MEMCON2's HILO: the high pixel of a pair first
    std::uint16_t dout = 0; // dbus.v dout's low word: what the CLUT's bus carries while it is not read
  };

  // What it asks of the bus in the cycle (obbreq, mreq, a, rw), and whether one of its latches takes
  // the data bus (a phrase read ends).
  struct Outputs
  {
    bool obbreq = false;
    bool mreq = false;
    std::uint32_t address = 0;
    bool read = true;
    bool latch = false;
    bool startref = false;     // a stop object: the refresh may start (startref)
    bool interrupt = false;    // a stop object with its interrupt flag (obint)
    bool gpuInterrupt = false; // a GPU object (grpintreq)
  };

  Outputs outputs( Inputs const& in ) const;

  // The clock edge, `d` the data bus: the phrase the memory cycle ending in the cycle reads.
  void clock( Inputs const& in, std::uint64_t d );

  // The phrase it drives as it writes a header back (wbken).
  std::uint64_t writeData() const;

  // Nothing is under way, and no line is to start.
  // Waiting for a line, its registers settled (the cycles until a line starts change nothing).
  bool idleSettled() const
  {
    return mIdleSettled;
  }

  // Cycles from this one in which it cannot interrupt the GPU (it does so in q3 with a GPU object,
  // reached from q1 through q2), at the least, while it works.
  std::uint32_t gpuQuietCycles() const;

  bool idle() const
  {
    return mQ == 1u && !mStart1 && !mOverrun && mSmq == 1u && !mFull && mWq == 1u && !mLatchQ;
  }

  // OLP and OBF as written (olp1w, olp2w, obfw's latch), the CLUT (both of its copies).
  void writeOlp( bool high, std::uint16_t value );

  void writeObf( std::uint16_t value )
  {
    mObf = value & 1u;
    mCombValid = false;
    mIdleSettled = false;
  }

  void writeClut( std::uint32_t index, std::uint16_t value )
  {
    mClut[index & 0xffu] = value;
  }

  std::uint16_t readClut( std::uint32_t index ) const
  {
    return mClut[index & 0xffu];
  }

  // OB0 to OB3 as read: the header's fields as they stand.
  std::uint16_t readOb( std::uint32_t n ) const;

  // lbuf.v's wd in the cycle last clocked, while the object processor writes a line buffer (lben):
  // what it writes, which the bus of the buffer shown carries too.
  bool lineWrite( std::uint32_t& value ) const
  {
    value = mLineWd;
    return mLineWrites;
  }

private:
  // Every combinational signal of a cycle, from the state and the inputs.
  struct Comb
  {
    // ob.v
    bool bitob, scaled, grpob, branchob, stopob;
    bool obmack, startover, heightnz, wbkdone, onscreen, offscreen, obdone, obdready;
    bool d[10];
    bool obbreq, obmreq, obldd0, obldd1, obldd2, pc1en, pc2en, dmaen, datasel, pclink, pcinc, pcld;
    bool obrw, wbkstart, wbken2d, wbken0d, xld;
    std::uint32_t oa; // address bits 3 to 23, at bit 0
    bool latch[4];    // obld[0] to obld[2], obdlatch
    // wbk.v
    std::uint32_t wd;
    bool decheight, addnewdata, addrem, decrem;
    // obdata.v
    bool nextphrase, nextx, nextbits, phdone, notremgte2, remgte2, remgte1, phys, left, right, obdOnscreen;
    std::uint32_t smd, pra, prb, d9next, lbwdOut;
    bool lbwea8, lbweb8, lbend, pswapd, lbwrite;
    bool lbwea1, lbweb1, lbwea4, lbweb4;
    std::uint32_t pa;
  };

  void evaluate( Inputs const& in, Comb& c ) const;
  // evaluate()'s logic with some registers fixed by the profile P (ObjectProcessor.cpp)
  template <typename P>
  void evaluateT( Inputs const& in, Comb& c ) const;
  // calls f with the profile the registers are in
  template <typename F>
  void withProfile( F&& f ) const;
  void clockFull( Inputs const& in, std::uint64_t d );
  template <typename P>
  void clockFullT( Inputs const& in, std::uint64_t d );
  // the cycle's signals as outputs() worked them out, for clock(): none of the state they come
  // from changes in between but by a write of OLP or OBF, and of the inputs clock() adds, OBF's
  // write and the GPU's or the blitter's write (obfw, ewr) reach the next state alone
  mutable Comb mComb{};
  mutable bool mCombValid = false;
  mutable Inputs mCombIn{};

  // Waiting for a line, a cycle that left every register as it was leaves them so until a line
  // starts, OLP or OBF is written or the bus is granted it: but for the pixel register following
  // the data bus (d9) and the line buffer word an RMW object would add to (rmwd1), which the
  // cycles then only bring forward, its outputs those of that cycle.
  bool mIdleSettled = false;
  std::uint32_t mIdleD9 = 0; // d9next of a physical depth
  Outputs mIdleOutputs{};

  // the GPU's or the blitter's write (ewr) reaches only wbkeni[0], the write-back's enable of the
  // data bus, which a waiting processor does not drive
  bool idleFast( Inputs const& in ) const
  {
    return mIdleSettled && !in.start && !in.obback && !in.obfw;
  }

  auto idleKey() const
  {
    return std::make_tuple( mBusy1,
                            mD1,
                            mD2,
                            mDa,
                            mData,
                            mDepth,
                            mDmainc,
                            mDwidth,
                            mFull,
                            mHscale,
                            mIndex,
                            mIp,
                            mIwidth,
                            mLatchQ,
                            mLben,
                            mLbw,
                            mLbwa,
                            mLbwad,
                            mLbwe,
                            mLbwea2,
                            mLbweb2,
                            mLbweb51,
                            mLbweb52,
                            mLink,
                            mNewdata,
                            mNewheight,
                            mNewrem,
                            mNextbits1,
                            mNextbits2,
                            mNextx1,
                            mOlp,
                            mOverrun,
                            mPa10,
                            mPaaq,
                            mPabq,
                            mPad,
                            mPswap,
                            mQ,
                            mReflected,
                            mRelease,
                            mRem,
                            mRmw,
                            mRmw1,
                            mScaledi,
                            mSkip,
                            mSmq,
                            mStart1,
                            mStart2,
                            mTransen,
                            mType,
                            mVscale,
                            mWbkeni2,
                            mWq,
                            mXrem,
                            mYpos );
  }

  static Outputs outputsOf( Comb const& c, std::uint32_t q, std::uint32_t ypos );

  LineBuffers& mBuffers;
  std::array<std::uint16_t, 256> mClut{};

  // ob.v: the header (type, ypos, link, data, depth, skip, dwidth, index, reflected, rmw, transen,
  // release, hscale, vscale, rem), the list pointer and its register, the states q0 to q9 (one
  // hot, bit n for qn), the line start's edge and overrun, the load latches (ack_pipe), the
  // write-back enables
  std::uint32_t mType = 0;
  std::uint32_t mYpos = 0;
  std::uint32_t mLink = 0;
  std::uint32_t mData = 0;
  std::uint32_t mDepth = 0;
  std::uint32_t mSkip = 0;
  std::uint32_t mDwidth = 0;
  std::uint32_t mIndex = 0; // index[7:1], at bit 1
  bool mReflected = false;
  bool mRmw = false;
  bool mTransen = false;
  bool mRelease = false;
  std::uint32_t mHscale = 0;
  std::uint32_t mVscale = 0;
  std::uint32_t mRem = 0;  // rem[7:5], at bit 0
  std::uint32_t mOlpd = 0; // olpd[23:3], at bit 0
  std::uint32_t mOlp = 0;  // olp[21:3], at bit 0
  bool mDmainc = false;
  bool mObf = false;
  std::uint32_t mIwidth = 0;
  std::uint32_t mQ = 1;
  bool mStart1 = false;
  bool mStart2 = false;
  bool mOverrun = false;
  std::uint32_t mLatchQ = 0; // ack_pipe q of obld[0] (bit 0) to obld[2], obdlatch (bit 3)
  bool mWbkeni0 = true;
  bool mWbkeni2 = false;

  // wbk.v
  std::uint32_t mWq = 1;
  std::uint32_t mNewrem = 0;
  std::uint32_t mNewheight = 0;
  std::uint32_t mNewdata = 0;

  // obdata.v
  std::uint32_t mXrem = 0;
  std::uint64_t mD1 = 0;
  std::uint64_t mD2 = 0;
  bool mScaledi = false;
  bool mRmw1 = false;
  bool mFull = false;
  std::uint32_t mSmq = 1; // smq0 to smq2, one hot
  std::uint32_t mPad = 0;
  std::uint32_t mIp = 0;
  std::uint32_t mPaaq = 0;
  std::uint32_t mPabq = 0;
  bool mBusy1 = false;
  std::uint32_t mD9 = 0;
  bool mNextbits1 = false;
  bool mNextbits2 = false;
  std::uint32_t mDa = 0;
  std::uint32_t mLbwad = 0; // 12 bits
  bool mNextx1 = false;
  std::uint32_t mLbwa = 0; // lbwa_[9:0]
  bool mLbwea2 = false;
  bool mLbweb2 = false;
  bool mLbweb51 = false;
  bool mLbweb52 = false;
  std::uint32_t mLbwe = 0;
  std::uint32_t mLbw = 0; // lbw[3:1], at bit 0
  bool mLben = false;
  bool mPa10 = false;
  bool mPswap = false;
  std::uint32_t mRmwd1 = 0; // lbuf.v: what the buffer written holds at the address, a cycle late
  bool mLineWrites = false; // and its wd in the cycle last clocked
  std::uint32_t mLineWd = 0;
};

} // namespace jag
