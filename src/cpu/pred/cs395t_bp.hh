#ifndef __CPU_PRED_CS395T_BP_PRED_HH__
#define __CPU_PRED_CS395T_BP_PRED_HH__

#include "base/sat_counter.hh"
#include "cpu/pred/bpred_unit.hh"
#include "params/CS395TBP.hh"

namespace gem5
{

struct CS395TBPParams;

namespace branch_prediction
{

class CS395TBP : public BPredUnit
{
  protected:
    const unsigned example_size; // Example parameter.
    // TODO: Add more params here

  public:
    CS395TBP(const CS395TBPParams &params);
    bool lookup(ThreadID tid, Addr pc, void * &bp_history) override;
    void updateHistories(ThreadID tid, Addr pc, bool uncond,
                         bool taken, Addr target, void * &bp_history) override;
    void squash(ThreadID tid, void * &bp_history) override;
    void update(ThreadID tid, Addr pc, bool taken, void * &bp_history,
                bool squashed, const StaticInstPtr &inst, Addr target) override;
};

} // namespace branch_prediction
} // namespace gem5

#endif // __CPU_PRED_CS395T_BP_PRED_HH__
