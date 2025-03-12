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
    : BPredUnit(params), example_size(params.size)
    // TODO: Add more params here
{
}

bool
CS395TBP::lookup(ThreadID tid, Addr branch_addr, void * &bp_history)
{
    // TODO FIXME: Make the prediction for the branch.
    return false;
}

void
CS395TBP::updateHistories(ThreadID tid, Addr pc, bool uncond,
                          bool taken, Addr target, void * &bp_history)
{
    // TODO FIXME: Update the branch predictor's internal structures
    // (e.g. path, global history) after the branch was resolved. It'll get
    // called automatically by gem5.
    //
    // Note that this also gets called for unconditional branches, so you may
    // choose to filter them out if necessary.
}

void
CS395TBP::squash(ThreadID tid, void * &bp_history)
{
    // TODO FIXME: Clean up branch state when there is a pipeline squash.
    // If bp_history is being used, then this is likely where to free the
    // memory.  Also, update any speculative state.
}

void
CS395TBP::update(ThreadID tid, Addr pc, bool taken, void * &bp_history,
                 bool squashed, const StaticInstPtr &inst, Addr target)
{
    // TODO FIXME: Main update function when ground truth for the branch is
    // computed.
}

} // namespace branch_prediction
} // namespace gem5
