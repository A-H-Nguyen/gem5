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

CS395TBP::CS395TBP(const CS395TBPParams &params) 
  : BPredUnit(params), 
    ghr(8192),
    tage(params.tage),
    llbpStorage(params.numContexts, params.numPatterns, 
                params.ctxAssoc, params.ptrnAssoc),
    rcr(HASHVALS, params.CTWidth),
    patternBuffer(params.pbSize, params.pbAssoc) {

  llbpStorage.allocate(0,0);
  llbpStorage.erase(0);

  int m[MAXNHIST];
  int mllbp[MAXNHIST];
  m[1] = params.minHist;
  m[params.nHistoryTables / 2] = params.maxHist;
  for (int i = 2; i <= params.nHistoryTables / 2; i++) {
    m[i] = (int)(((double)params.minHist *
                  pow((double)(params.maxHist) / (double)params.minHist,
                  (double)(i - 1) / (double)(((params.nHistoryTables / 2) - 1)))) +
                  0.5);
  }

  // for (int i = 1; i <= params.nHistoryTables; i++) {
  //   NOSKIP[i] = ((i - 1) & 1) || ((i >= assoc_start) & (i < assoc_end));
  // }

  // if (params.nHistoryTables > 30) {
  //     NOSKIP[4] = 0;
  //     NOSKIP[params.nHistoryTables - 2] = 0;
  //     NOSKIP[8] = 0;
  //     NOSKIP[params.nHistoryTables - 6] = 0;
  //     // just eliminate some extra tables (very very marginal)
  // }

  for (int i = params.nHistoryTables; i > 1; i--) {
    m[i] = m[(i + 1) / 2];
  }

  for (int i = 1; i <= params.nHistoryTables; i++) {
    mllbp[i] = (i%2) ? m[i] : m[i]+2;

    fghrT1[i] = new FoldedHistoryFast(ghr, mllbp[i], TTWidth);
    fghrT2[i] = new FoldedHistoryFast(ghr, mllbp[i], TTWidth - 1);
  }
}

bool CS395TBP::lookup(ThreadID tid, Addr branch_addr, void * &bp_history)
{
  bool tagePred = tage->lookup(tid, branch_addr, bp_history);
  return tagePred;
}

void CS395TBP::updateHistories(ThreadID tid, Addr pc, bool uncond,
                               bool taken, Addr target, void * &bp_history)
{
  tage->updateHistories(tid, pc, uncond, taken, target, bp_history);
}

void CS395TBP::squash(ThreadID tid, void * &bp_history)
{
  tage->squash(tid, bp_history);
  
}

void CS395TBP::update(ThreadID tid, Addr pc, bool taken,
                    void * &bp_history, bool squashed,
                    const StaticInstPtr & inst, Addr target) {
  tage->update(tid, pc, taken, bp_history, squashed, inst, target);

  rcr.update(pc, getOpType(inst), taken);

  // ctrupdate(llbpEntry->ctr, resolveDir, CtrWidth);

  // // This function updates the context replacement counter
  // // - If a pattern becomes confident (correct prediction)
  // //   the replacement counter is increased
  // // - If a pattern becomes low confident (incorrect prediction)
  // //   the replacement counter is decreased
  // if (llbpEntry->ctr == (resolveDir ? 1 : -2)) {
  //     // entry became medium confident
  //     ctrupdate(HitContext->replace, true, CtxReplCtrWidth);
  // }
  // else if (llbpEntry->ctr == (resolveDir ? -1 : 0)) {
  //     // entry became low confident
  //     ctrupdate(HitContext->replace, false, CtxReplCtrWidth);
  // }

  // // If the prediction wrong update also the BIM
  // if (!llbpCorrect(resolveDir) && (llbp.conf == LowConf)) {
  //     updateBim = true;
  // }
}


OpType CS395TBP::getOpType(const StaticInstPtr & inst) {
  if (!inst) {
    return OPTYPE_ERROR;  // Return error if instruction is null
  }

  // Unconditional branches
  if (inst->isUncondCtrl()) {
    if (inst->isReturn()) return OPTYPE_RET_UNCOND;
    if (inst->isCall()) {
        return inst->isDirectCtrl() ? OPTYPE_CALL_DIRECT_UNCOND : 
                                      OPTYPE_CALL_INDIRECT_UNCOND;
    }
    return inst->isDirectCtrl() ? OPTYPE_JMP_DIRECT_UNCOND : 
                                  OPTYPE_JMP_INDIRECT_UNCOND;
  }

  // Conditional branches
  if (inst->isCondCtrl()) {
    if (inst->isReturn()) return OPTYPE_RET_COND;
    if (inst->isCall()) {
        return inst->isDirectCtrl() ? OPTYPE_CALL_DIRECT_COND : 
                                      OPTYPE_CALL_INDIRECT_COND;
    }
    return inst->isDirectCtrl() ? OPTYPE_JMP_DIRECT_COND : 
                                  OPTYPE_JMP_INDIRECT_COND;
  }

  // Default: Regular operation (not a branch)
  return OPTYPE_OP;
}

// bool CS395TBP::GetPrediction(Addr pc) {


// }

// void CS395TBP::llbpPredict(Addr pc) {
//   // Get the current context (CCID)
//   auto ctx_key = rcr.getCCID();
//   HitContext = llbpStorage.get(ctx_key);


//   if (HitContext) {
//       for (int i = nHistoryTables; i > 0; i--) {
//           if (NOSKIP[i]) {
//               llbpEntry = HitContext->patterns.get(KEY[i]);

//               if (llbpEntry) {
//                   llbp.hit = i;
//                   llbp.pVal = llbpEntry->ctr;
//                   llbp.pred = llbp.pVal >= 0;
//                   llbp.conf = compConf(llbp.pVal, CtrWidth);
//                   llbp.histLength = i;
//                   break;
//               }
//           }
//       }
//   }  
// }

/************************************************************
 * RCR Functionality
 */

CS395TBP::RCR::RCR(int _T, int _W, int _D, int _shift, int _CTWidth)
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
uint64_t
CS395TBP::RCR::calcHash(std::list<uint64_t> &vec, int n, int start, int shift)
{
  uint64_t hash = 0;
  if (vec.size() < (start + n)) {
      return 0;
  }
  uint64_t sh = 0;
  auto it = vec.begin();
  std::advance(it, start);
  for (; (it != vec.end()) && (n > 0); it++, n--) {
    uint64_t val = *it;

    // Shift the value
    hash ^= val << (uint64_t)(sh);

    sh += shift;
    if (sh >= CTWidth) {
      sh -= (uint64_t)(CTWidth);
    }
  }
  return hash & ((1 << CTWidth) - 1);
}

uint64_t CS395TBP::RCR::getCCID()
{
  return ctxs.ccid & ((1 << CTWidth) - 1);
}

uint64_t CS395TBP::RCR::getPCID() 
{
  return ctxs.pcid & ((1 << CTWidth) - 1);
}


bool CS395TBP::RCR::update(Addr pc, OpType opType, bool taken) 
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
 
// DEFINE_SIM_OBJECT(gem5::branch_prediction::CS395TBP);
