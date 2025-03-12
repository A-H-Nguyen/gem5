#include "cpu/pred/cs395t_bp.hh"
#include "params/CS395TBP.hh"

#include "base/bitfield.hh"
#include "base/intmath.hh"

// TODO FIXME: See 2bit_local.cc for reference of a simple implementation.
// See bi_mode.cc for reference of how to use bp_history.  I imagine the
// bp_history is a pipeline register that is needed down the line
// during the branch predictor update or a pipeline flush.  One example is the
// state of the global history register.  Since this register is updated
// speculatively during the fetch stage, but the register may need to be reset
// on a squash, each branch instruction would need to keep a copy of the
// global history register.  This is how bp_history is used in the bi_mode
// predictor.

namespace gem5
{

namespace branch_prediction
{

// bool CS395TBP::predict(ThreadID tid, Addr pc, bool cond_branch, void* &b) {
//   return false;
// }

// void
// CS395TBP::update(ThreadID tid, Addr pc, bool taken, void * &bp_history, 
//                  bool squashed, const StaticInstPtr & inst, Addr target)
// {
//     // TODO FIXME: Main update function when ground truth for the branch is
//     // computed.
// }

// void
// CS395TBP::updateHistories(ThreadID tid, Addr branch_pc, bool taken, 
//                          TAGEBase::BranchInfo* b, bool speculative, 
//                          const StaticInstPtr &inst, Addr target)
// {
//     // TODO FIXME: Update the branch predictor's internal structures
//     // (e.g. path, global history) after the branch was resolved. It'll get
//     // called automatically by gem5.
//     //
//     // Note that this also gets called for unconditional branches, so you may
//     // choose to filter them out if necessary.
// }

// void
// CS395TBP::squash(ThreadID tid, bool taken, TAGEBase::BranchInfo *bi,
//                  Addr target)
// {
//     // TODO FIXME: Clean up branch state when there is a pipeline squash.
//     // If bp_history is being used, then this is likely where to free the
//     // memory.  Also, update any speculative state.
// }

// bool
// CS395TBP::lookup(ThreadID tid, Addr branch_addr, void * &bp_history)
// {
//     // TODO FIXME: Make the prediction for the branch.
//     return false;
// }


/************************************************************
 * RCR Functionality
 */

 RCR::RCR(int _T, int _W, int _D, int _shift, int _CTWidth)
 : CTWidth(_CTWidth), T(_T), W(_W), D(_D), S(_shift)
{
  bb[0].resize(maxwindow);
  bb[1].resize(maxwindow);
  ctxs = {0, 0};
  // printf("\n\nRCR: context hash config: [T:%i, W:%i, D:%i, S:%i, CTWidth:%i]\n",
  //         T, W, D, S, CTWidth);
}


/*
* Given the {n} number of branches staring from vec[end-start]
* to vec[end-start-n-1] we create the hash function by shifting
* each PC by {shift} number if bits i.e.
*
*   000000000000|  PC  |    :vec[end-start]
* ^ 0000000000|  PC  |00    :vec[end-start-1]
* ^ 00000000|  PC  |0000    :vec[end-start-2]
*           .                     .
*           .                     .
*           .                     .
* ^ |  PC  |000000000000    :vec[end-start-n-1]
* ----------------------
*       final hash value
* */
unsigned long
RCR::calcHash(std::list<unsigned long> &vec, int n, int start, int shift)
{
  unsigned long hash = 0;
  if (vec.size() < (start + n)) {
      return 0;
  }
  unsigned long sh = 0;
  auto it = vec.begin();
  std::advance(it, start);
  for (; (it != vec.end()) && (n > 0); it++, n--) {
    unsigned long val = *it;

    // Shift the value
    hash ^= val << (unsigned long)(sh);

    sh += shift;
    if (sh >= CTWidth) {
      sh -= (unsigned long)(CTWidth);
    }
  }
  return hash & ((1 << CTWidth) - 1);
}

unsigned long RCR::getCCID()
{
  return ctxs.ccid & ((1 << CTWidth) - 1);
}

unsigned long RCR::getPCID() 
{
  return ctxs.pcid & ((1 << CTWidth) - 1);
}


bool RCR::update(unsigned long pc, OpType opType, bool taken) 
{
  branchCount++;
  // Hash of all branches
  auto isCall = (opType == OPTYPE_CALL_DIRECT_UNCOND)
        || (opType == OPTYPE_CALL_INDIRECT_UNCOND)
        || (opType == OPTYPE_CALL_DIRECT_COND);


  switch (T) {
    case 0: // All branches
    bb[0].push_front(pc);
    bb[1].push_front(branchCount);
    break;

    case 1: // Only calls
    if (isCall) {
        bb[0].push_front(pc);
        bb[1].push_front(branchCount);
    }
    break;

    case 2: // Only calls and returns
    if (isCall || (opType == OPTYPE_RET_UNCOND)) {
        bb[0].push_front(pc);
        bb[1].push_front(branchCount);
    }
    break;

    case 3: // Only unconditional branches
    if (opType != OPTYPE_JMP_DIRECT_COND) {
        bb[0].push_front(pc);
        bb[1].push_front(branchCount);
    }
    break;

    case 4: // All taken branches
    if (taken) {
        bb[0].push_front(pc);
        bb[1].push_front(branchCount);
    }
    break;
  }

// PRINTIF(COND,"UH:%llx, %i, %i\n", pc, opType, taken);
  // If the size has changed the hash has changed
  bool changed = false;
  if (bb[0].size() > maxwindow) {
      changed = true;

      // Resize the history
      bb[0].pop_back();
      bb[1].pop_back();

      // The current context.
      ctxs.ccid = calcHash(bb[0], W, D, S);
      // The prefetch context.
      ctxs.pcid = calcHash(bb[0], W, 0, S);
  }
  return changed;
}

} // namespace branch_prediction
} // namespace gem5
