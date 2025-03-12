#ifndef __CPU_PRED_CS395T_BP_PRED_HH__
#define __CPU_PRED_CS395T_BP_PRED_HH__

#include "base/sat_counter.hh"
// #include "cpu/pred/bpred_unit.hh"
#include "cpu/pred/tage_sc_l.hh"
#include "cpu/pred/tage_sc_l_64KB.hh"
#include "cpu/pred/statistical_corrector.hh"
#include "params/TAGE_SC_L.hh"
#include "params/TAGE_SC_L_LoopPredictor.hh"
#include "params/TAGE_SC_L_TAGE.hh"
#include "params/CS395TBP.hh"

namespace gem5
{

struct CS395TBPParams;

namespace branch_prediction
{

//JD2_17_2016 break down types into COND/UNCOND
typedef enum {
  OPTYPE_OP               =2,

  OPTYPE_RET_UNCOND,
  OPTYPE_JMP_DIRECT_UNCOND,
  OPTYPE_JMP_INDIRECT_UNCOND,
  OPTYPE_CALL_DIRECT_UNCOND,
  OPTYPE_CALL_INDIRECT_UNCOND,

  OPTYPE_RET_COND,
  OPTYPE_JMP_DIRECT_COND,
  OPTYPE_JMP_INDIRECT_COND,
  OPTYPE_CALL_DIRECT_COND,
  OPTYPE_CALL_INDIRECT_COND,

  OPTYPE_ERROR,

  OPTYPE_MAX
} OpType;

typedef enum {
    NoBranch,
    Return,
    CallDirect,
    CallIndirect,
    DirectCond,
    DirectUncond,
    IndirectCond,
    IndirectUncond,
    MAX
} BrType;

/********************************************************************
 * Rolling Context Register RCR
 *
 * The RCR maintains the previous executed branches to compute
 * a context ID.
 *
 * The hash function is defined by 4 paramenters
 *
 * T: Type of history (T). Which branches should be hased
 *     0: All branches, 1: Only calls, 2: Calls and returns
 *     3: All unconditional branches, 4: All taken branches
 *
 * W: Number of branches that should be hashed (W in the paper).
 * D: Number of most recent branches skipped for CCID. Adds delay which
 *    is used to prefetch. (D in the paper.)
 * S: Number of bits to shift the PC's. Is useful to avoid ping-pong context
 *    due to the XOR function in case a loop is executed
 *
 * ********************************************************************* *
 * EXAMPLE                                       
 *                       pb-index (2.)  (3.)                             *
 *                      v             v     v                            *
 * history buffer : |l|k|j|i|h|g|f|e|d|c|b|a|                            *
 *                            ^prefetch (2.)^                            *
 * a is the newest branch PC added to the buffer, l the oldest.          *
 * (2.) = W = 7; (3.) = D = 3                                            *
 * branches used to obtain PB index hash: j to d                         *
 * branches used to obtain hash to prefetch into PB: g to a  
 * *********************************************************************
 */
class RCR {
  const int maxwindow = 120;

  unsigned long 
  calcHash(std::list<unsigned long> &vec, int n, int start=0, int shift=0);

  // The context tag width
  const int CTWidth;

  // A list of previouly taken branches
  std::list<unsigned long> bb[10];

  // We compute the context ID and prefetch context ID
  // only when the content of the RCR changes.
  struct {
      unsigned long ccid = 0;
      unsigned long pcid = 0;
  } ctxs;

  int branchCount = 0;

public:
    // The hash constants
    const int T, W, D, S;

    RCR(int _T, int _W, int _D, int _shift, int _CTWidth);

    // Push a new branch into the RCR.
    bool update(unsigned long pc, OpType type, bool taken);

    // Get the current context ID
    unsigned long getCCID();

    // Get the prefetch context ID
    unsigned long getPCID();
};

// class CS395TBP : public BPredUnit
class CS395TBP : public TAGE_SC_L_TAGE
{
  protected:
    const unsigned example_size; // Example parameter.
    // TODO: Add more params here

  public:
    CS395TBP(const CS395TBPParams &params) : TAGE_SC_L_TAGE(p)
    {}

    // bool predict(ThreadID tid, Addr pc, bool cond_branch, void* &b) override;
    // void update(ThreadID tid, Addr pc, bool taken,
    //             void * &bp_history, bool squashed,
    //             const StaticInstPtr & inst, Addr target) override;
    // void updateHistories(
    //     ThreadID tid, Addr branch_pc, bool taken, TAGEBase::BranchInfo* b,
    //     bool speculative, const StaticInstPtr &inst,
    //     Addr target) override;
    // void squash(ThreadID tid, bool taken, TAGEBase::BranchInfo *bi,
    //     Addr target) override;

    // bool lookup(ThreadID tid, Addr pc, void * &bp_history);
};

} // namespace branch_prediction
} // namespace gem5

#endif // __CPU_PRED_CS395T_BP_PRED_HH__
