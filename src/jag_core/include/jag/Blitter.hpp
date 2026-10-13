#pragma once

#include <cstdint>

namespace jag
{

// Tom's blitter, stepped by cycle while it works or is written: the outer and inner loops of
// outer.v and inner.v, the memory interface of mcontrol.v, the address path of acontrol.v and
// address.v, the data path of dcontrol.v, comp_ctrl.v and data.v, the collision stop of
// blitstop.v. A master of Tom's bus (req[1], req[6] with BUSHI); its registers are written and read
// on the GPU's local bus (blitgpu.v). Every signal is the netlist's of the same name: a flip-flop is
// clocked at the end of the cycle, a latch (ldp1q) takes its value at tlw, in the middle of it.
//
// Ports Jaguar_MiSTer rtl/Tom/blit.v, blitgpu.v, blitstop.v, state.v, outer.v, inner.v,
// inner_cnt.v, mcontrol.v, acontrol.v, dcontrol.v, comp_ctrl.v, address.v, addamux.v, addradd.v,
// addrgen.v, addrcomp.v, data.v, srcshift.v, daddamux.v, daddbmux.v, addarray.v, add16sat.v,
// zedcomp.v and data_mux.v, read at 43a761d.
class Blitter
{
public:
  // What the bus and the GPU give it in a cycle.
  struct Inputs
  {
    bool back = false;         // blit_back: its grant (ack_[1] or ack_[6])
    bool ack = false;          // the memory controller's acknowledge
    std::uint64_t data = 0;    // Tom's data bus as tlw takes it
    bool memw = false;         // the GPU's write of a register in the cycle (gpu_memw with bliten)
    std::uint32_t address = 0; // gpu_addr
    std::uint32_t din = 0;     // gpu_din

    bool operator==( Inputs const& ) const = default;
  };

  // What it gives the bus in the cycle: from its flip-flops, the width and justify from the
  // registers the GPU writes too, as they stand after tlw.
  struct Outputs
  {
    std::uint32_t breq = 0; // blit_breq[1:0]
    bool mreq = false;
    bool read = false;
    std::uint32_t width = 0; // width_out, in bytes (8 for a phrase)
    std::uint32_t address = 0;
    bool justify = false;
  };

  Outputs outputs() const;

  // The cycle's first half: the latches take their values at tlw.
  void latch( Inputs const& in );

  // The clock edge that ends the cycle, after latch(). Returns the data it drives on the bus in the
  // cycle (wdata, valid while data_ena) as the latches leave it.
  std::uint64_t clock( Inputs const& in );

  // data_ena: it drives the bus with its write data.
  bool dataEna() const
  {
    return mDataEna;
  }

  // A read of its registers on the GPU's local bus (gpu_dout): the status at B_CMD, the pointers.
  // `back`: Tom's grant is latched for it (blit_back), which the status shows.
  std::uint32_t read( std::uint32_t address, bool back ) const;

  bool operator==( Blitter const& ) const = default;

  // blit_int in the cycle just clocked: the blit has ended.
  bool interrupt() const
  {
    return mInterrupt;
  }

  // blit_int in the cycle to be clocked next, from the flip-flops before it (outer.v: the loops
  // idle, no memory cycle active, idle not yet latched)
  bool interruptAhead() const
  {
    return !mIdled && mOidle && !mWactive && !mRactive;
  }

  // The last cycle changed nothing and the next cannot by itself: no bus request, no memory cycle
  // waiting for its end. Until the GPU writes a register it need not be clocked.
  bool settled() const
  {
    return mSettled;
  }

  // A1 or A2 has its base in Tom's or Jerry's space, where the RISCs' local memories are.
  bool basedAtTomOrJerry() const
  {
    return ( ( mA1Base << 3 ) & 0xfe0000u ) == 0xf00000u || ( ( mA2Base << 3 ) & 0xfe0000u ) == 0xf00000u;
  }

private:
  struct Comb;
  Comb evaluate( Inputs const& in ) const;
  bool takeLatches( Inputs const& in ); // whether a latch changed

  // the bus's big pixel mode (G_END bit 1, set by the boot as by the BIOS)
  static constexpr bool BIG_PIX = true;

  // state.v
  std::uint32_t mZmode = 0;
  std::uint32_t mLfuFunc = 0;
  bool mCmpdst = false;
  bool mBcompen = false;
  bool mDcompen = false;
  bool mBkgwren = false;
  bool mSrcshade = false;

  // inner.v: the states (idle to dzwrite), the command's enables, the pipeline
  bool mIdle = true;
  bool mSreadx = false;
  bool mSzreadx = false;
  bool mSread = false;
  bool mSzread = false;
  bool mDread = false;
  bool mDzread = false;
  bool mDwrite = false;
  bool mDzwrite = false;
  bool mSrcen = false;
  bool mSrcenz = false;
  bool mSrcenx = false;
  bool mDsten = false;
  bool mDstenz = false;
  bool mDstwrz = false;
  bool mDisoA1 = false;
  bool mStupl = false;
  bool mDnrt6 = false;
  std::uint32_t mAtick = 0; // atick[1:0]
  bool mDpipe1t1 = false;
  bool mZpipe1t1 = false;
  bool mOldoutsidel = false;
  bool mSrcaAdd = false;
  bool mGena2 = false;
  bool mSreadx1 = false;
  bool mSread1 = false;
  bool mSzread1 = false;
  bool mDread1 = false;
  bool mDzread1 = false;
  bool mDenat2 = false;
  bool mDwrite1 = false;
  bool mDzwrite1 = false;
  bool mSrcdpend = false;
  bool mSrcdxpend = false;
  bool mSrczpend = false;
  bool mSrczxpend = false;
  bool mDstdpend = false;
  bool mDstzpend = false;
  bool mSrcdreadd = false;
  bool mDataEna = false;

  // inner_cnt.v
  std::uint32_t mCntvall = 0;
  std::uint32_t mIcount = 0;
  bool mCntlden = false;
  bool mUnderflow = false;

  // outer.v
  bool mOidle = true;
  bool mInner = false;
  bool mOa1fupdate = false;
  bool mOa1update = false;
  bool mOa2update = false;
  bool mUpda1f = false;
  bool mUpda1 = false;
  bool mUpda2 = false;
  bool mBushi = false;
  bool mGo = false;
  bool mSshftld = false;
  std::uint32_t mOcount = 0;
  bool mIdled = true;
  bool mInterrupt = false;

  // mcontrol.v
  std::uint32_t mBlita = 0;
  bool mRactive = false;
  bool mWactive = false;
  bool mWaitack = false;

  // acontrol.v
  bool mDsta2 = false;
  bool mAa1update = false;
  bool mAa1fupdate = false;
  bool mAa2update = false;
  bool mA1Add = false;
  bool mA2Add = false;
  std::uint32_t mAddasel = 0;
  bool mAddareg = false;
  std::uint32_t mAddbsel = 0;
  std::uint32_t mDstartbl = 0;
  std::uint32_t mDstartp = 0;
  std::uint32_t mDstart = 0;
  std::uint32_t mDendbl = 0;
  std::uint32_t mDendp = 0;
  std::uint32_t mDend = 0;
  std::uint32_t mSrcshift = 0;

  // dcontrol.v
  bool mGourd = false;
  bool mGourz = false;
  bool mTopben = false;
  bool mTopnen = false;
  bool mPatdsel = false;
  bool mAdddsel = false;
  bool mDzwrite1d = false;

  // comp_ctrl.v
  std::uint32_t mBcompsel = 0;
  bool mBcompbitp = false;

  // blitstop.v
  bool mStopen = false;
  bool mCollideb = false;
  bool mStopped = false;
  bool mDrvReset = false;

  // address.v: the registers the GPU writes (latches), the pointers (flip-flops)
  std::uint32_t mA1Base = 0;  // a1_base[20:0]: address bits 3 to 23
  std::uint32_t mA1Flags = 0; // a1_flags[20:0]
  std::uint32_t mA1WinX = 0;
  std::uint32_t mA1WinY = 0;
  std::uint32_t mA1StepX = 0, mA1StepY = 0;
  std::uint32_t mA1StepfX = 0, mA1StepfY = 0;
  std::uint32_t mA1IncX = 0, mA1IncY = 0;
  std::uint32_t mA1IncfX = 0, mA1IncfY = 0;
  std::uint32_t mA2Base = 0;
  std::uint32_t mA2Flags = 0;
  std::uint32_t mA2MaskX = 0, mA2MaskY = 0;
  std::uint32_t mA2StepX = 0, mA2StepY = 0;
  std::uint32_t mA1Xt = 0, mA1Y = 0;
  std::uint32_t mA1FracX = 0, mA1FracY = 0;
  std::uint32_t mA2Xr = 0, mA2Yr = 0;
  bool mCxt1 = false; // addradd.v: the fraction's carries
  bool mCyt1 = false;
  std::uint32_t mAddressi = 0; // addrgen.v

  // data.v: the phrases (sd1, sz1, sz2 a half word a lane; the rest as low and high long words)
  std::uint32_t mSd1[4] = {};
  std::uint32_t mSz1[4] = {};
  std::uint32_t mSz2[4] = {};
  std::uint64_t mPdu = 0;
  std::uint32_t mPatdo[2] = {};
  std::uint32_t mSrczpt[2] = {};
  std::uint32_t mSrczo[2] = {};
  std::uint32_t mCarryq = 0; // addarray.v
  std::uint32_t mSrcd2[2] = {};
  std::uint32_t mDstd[2] = {};
  std::uint32_t mDstz[2] = {};
  std::uint32_t mPatd[2] = {};
  std::uint32_t mZinc = 0;
  std::uint32_t mIinc = 0;

  bool mLatched = false; // the latches changed in the cycle
  bool mSettled = true;

  // The last cycle clocked changed nothing, its inputs `in`: the next with the same inputs changes
  // nothing either, its latches included, and drives the same data and outputs, and is not worked
  // out. Not part of the state compared.
  struct Repeat
  {
    Inputs in{};
    std::uint64_t wdata = 0;
    Outputs out{};
    bool valid = false;

    bool operator==( Repeat const& ) const
    {
      return true;
    }
  };

  Repeat mRepeat;
};

} // namespace jag
