#ifndef __CPU_PRED_CS395T_BP_PRED_HH__
#define __CPU_PRED_CS395T_BP_PRED_HH__

#include "base/sat_counter.hh"
#include "cpu/pred/bpred_unit.hh"
#include "cpu/pred/llbp_cache.h"
#include "cpu/pred/llbp_hist_registers.h"
// #include "cpu/pred/tage_sc_l.hh"
#include "cpu/pred/tage_sc_l_64KB.hh"
#include "params/TAGE_SC_L_64KB.hh"
#include "params/TAGE_SC_L_64KB_StatisticalCorrector.hh"
#include "params/TAGE_SC_L_TAGE_64KB.hh"
#include "params/CS395TBP.hh"



#define HASHVALS 3, 8, 8, 2 // LLBP default hash values: [T, W, D, S]
const unsigned MAXNHIST = 40; // Constant limit for the number of tables

inline int center(int8_t ctr) {
  return 2 * ctr + 1;
}

namespace gem5
{

struct CS395TBPParams;

namespace branch_prediction
{

//JD2_17_2016 break down types into COND/UNCOND
typedef enum {
  OPTYPE_OP =2,

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

class CS395TBP : public BPredUnit
// class CS395TBP : public TAGE_SC_L_TAGE
{
  public:
    CS395TBP(const CS395TBPParams &params);

    bool lookup(ThreadID tid, Addr pc, void * &bp_history) override;
    void updateHistories(ThreadID tid, Addr pc, bool uncond,
                         bool taken, Addr target, void * &bp_history) override;
    void squash(ThreadID tid, void * &bp_history) override;
    void update(ThreadID tid, Addr pc, bool taken, void * &bp_history,
                bool squashed, const StaticInstPtr &inst, Addr target) override;

  // Parameters
  protected:
    int numContexts;
    int numPatterns;
    int ctxAssoc;
    int ptrnAssoc;
    int TTWidth;
    int CTWidth;
    int pbSize;
    int pbAssoc;
    int CtrWidth;
    int CtxReplCtrWidth;
    int nHistoryTables;
    int minHist;
    int maxHist;

  private:
    OpType getOpType(const StaticInstPtr & inst);

    inline bool getPrediction(Addr pc);
    void llbpPredict(Addr pc);

    // chooser functions to arbitrate between
    // the baseline TAGE and LLBP
    bool isNotUseful(bool taken);
    bool isUseful(bool taken);
    void updateL2Usefulness(bool taken);

    unsigned chooseProvider();

    inline bool llbpCorrect(bool taken);
    inline bool primCorrect(bool taken);
    inline bool tageCorrect(bool taken);
    inline bool llbpUseful(bool taken);

  private:
    // Prediction Structures

    // The global history register
    HistoryRegisterFast ghr;

    typedef uint64_t Key;
    Key KEY[MAXNHIST];
    
    bool NOSKIP[MAXNHIST];  // to manage the associativity for different
                            // history lengths
    
    // A map to filter the used history lengths.
    std::unordered_map<int,int> fltTables;


    TAGE_SC_L_64KB *tage;

    /********************************************************************
     * LLBP Pattern
     *
     * Consists of the history length field and the tag.
     * In the model we concatenate both to form a key.
     * key = (tag << 10) | length
     * This simplifies model complexity
     ******************************************************************/
    struct Pattern {
      int length;
      uint tag;
      int idx;
      int8_t ctr;
      uint replace;
      bool dir;
      int useful = 0;
      int correct = 0;
      int incorrect = 0;
      Key key = 0;
      int evicted = 0;
      int evicted_ctx = 0;
      uint64_t pc = 0;
    };


    /********************************************************************
     * Pattern Set
     *
     * The pattern sets are implemented as set associative cache. The
     * lower bits of the key - to lookup a pattern in the pattern set
     * - are used for the history length which realizes the four way
     * associativity. In the constructor we assign each history an
     * index
     ******************************************************************/
    struct PatternSet : public BaseCache<uint64_t, Pattern>{
        PatternSet(size_t max_size, size_t assoc) :
            BaseCache<uint64_t, Pattern>(max_size, assoc)
        {}

        Pattern* insert(const uint64_t &key) {
            return BaseCache<uint64_t, Pattern>::insert(key);
        }
    };

    /********************************************************************
     * Program Context
     *
     * A program context contains one pattern set and is indexed by
     * a key formed by hashing W unconditional branches.
     * This struct contains some additional meta data for replacement
     * and statistics.
     ********************************************************************/
    struct Context {
      bool valid;
      uint64_t key;
      uint64_t pc;
      int correct;
      int incorrect;
      int useful;
      int conflict;
      uint replace;
      int ctr;
      int usefulPtrns;

      // The contexts pattern set.
      PatternSet patterns;

      Context(uint64_t k, uint64_t p, int n, int assoc)
        : valid(true), key(k), pc(p),
          correct(0), incorrect(0), useful(0), conflict(0),
          replace(0), ctr(0), usefulPtrns(0),
          patterns(n, assoc)
      {}

      // Before a pattern in the pattern set is replaced, the patterns are
      // sorted from the highest to the lowest confidence. This is done to
      // determine which pattern should be evicted.
      void sortPatters(const uint64_t key) {
          auto& set = patterns.getSet(key);
          set.sort(
              [](const std::pair<uint64_t, Pattern>& a, const std::pair<uint64_t, Pattern>& b)
              {
                  return abs(center(a.second.ctr)) > abs(center(b.second.ctr));
              });
      }
    };


    /********************************************************************
      * LLBP Storage
      *
      * LLBPs high-capacity structure to store all pattern sets.
      * It's implemented as a set associative cache.
      * The Context directory (CD) can be thought of as the tag array while the
      * LLBPStorage is the data array. In this simulation model, both LLBP
      * and CD are represented with a single data structure.
      ********************************************************************/
    class LLBPStorage : public BaseCache<uint64_t, Context>{
        typedef typename std::pair<uint64_t, Context> key_value_pair_t;
      typedef typename std::list<key_value_pair_t>::iterator list_iterator_t;
        const int n_patterns;
        const int _ptrn_assoc;

    public:

        LLBPStorage(int n_ctx, int n_patterns, int ctx_assoc, int ptrn_assoc)
          : BaseCache<uint64_t, Context>(n_ctx, ctx_assoc),
            n_patterns(n_patterns), _ptrn_assoc(ptrn_assoc)
        {
        }

        // This function creates a new context but does not install it.
        Context* createNew(uint64_t key, uint64_t pc) {
            return new Context(key, pc, n_patterns, _ptrn_assoc);
        }

        // This function will allocate a new context for the
        // given key if it does not exist.
        // It Will return the created context.
        // Note that this function will NOT sort the contexts.
        // Therefore, make sure to call the sorting function before
        // this function
        Context* allocate(uint64_t key, uint64_t pc) {

        auto c = this->get(key);
            if (c != nullptr) {
                return c;
            }

            auto& set = this->getResizedSet(key);

            set.push_front(
                key_value_pair_t(key, Context(key, pc, n_patterns, _ptrn_assoc)));
            _index[key] = set.begin();
            return &set.front().second;
        }

        // Sort the contexts in a set based on the replacement counter.
        void sortContexts(uint64_t key) {
            auto& set = this->getSet(key);
            set.sort(
                [](const key_value_pair_t& a, const key_value_pair_t& b)
                {
                    return a.second.replace > b.second.replace;
                });
        }
    } llbpStorage;

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

      uint64_t 
      calcHash(std::list<uint64_t> &vec, int n, int start=0, int shift=0);

      // The context tag width
      const int CTWidth;

      // A list of previouly taken branches
      std::list<uint64_t> bb[10];

      // We compute the context ID and prefetch context ID
      // only when the content of the RCR changes.
      struct {
          uint64_t ccid = 0;
          uint64_t pcid = 0;
      } ctxs;

      int branchCount = 0;

    public:
        // The hash constants
        const int T, W, D, S;

        RCR(int _T, int _W, int _D, int _shift, int _CTWidth);

        // Push a new branch into the RCR.
        bool update(Addr pc, OpType type, bool taken);

        // Get the current context ID
        uint64_t getCCID();

        // Get the prefetch context ID
        uint64_t getPCID();
    } rcr;

    /********************************************************************
     * Pattern Buffer
     *
     * The pattern buffer is a small set associative cache that maintains
     * the most recent executed pattern set. Upcomming contexts
     * are prefetched into the pattern buffer and predictions are made from
     * the pattern buffer.
     *
     * Note that in the model we don't move the patterns into the pattern
     * buffer. Instead we directly modify the patterns in the LLBPStorage.
     * The pattern buffer models the caching behaviour and is only used
     * in the timing model.
     */
    struct PBEntry {
      Key key;
      bool dirty;
      bool newlyAllocated;
      bool used;
      bool useful;
      int origin;
      bool valid;
      int prefetchtime;
      bool locked;
      PBEntry(Key c)
        : key(c), dirty(false),
          newlyAllocated(false),
          used(false), useful(false),
          origin(0),
          valid(false),
          prefetchtime(0),
          locked(false)
      {}
      PBEntry() : PBEntry(0) {}
    };

    class PatternBuffer : public BaseCache<uint64_t, PBEntry> {
      public:
        PatternBuffer(int n, int assoc)
          : BaseCache<uint64_t, PBEntry>(n, assoc)
        {
        }

        PBEntry* insert(PBEntry &entry) {;
            auto v = get(entry.key);
            if (v != nullptr) {
                return v;
            }

            // Get the set with a free item
            auto& set = getResizedSet(entry.key);

            set.push_front(key_value_pair_t(entry.key, entry));
            _index[entry.key] = set.begin();
            return &set.front().second;
        }
    } patternBuffer;

    // Pointers to context, llbp pattern and PB entry in case of a
    // LLBP pattern/context match.
    Context* HitContext;
    Pattern* llbpEntry;
    PBEntry* pbEntry;

    // A struct to maintain the prediction info from LLBP.
    struct LLBPPredInfo {
        bool hit = false;
        int pVal = 0;
        bool pred = false;
        unsigned conf = 0;
        int histLength = 0;
        bool prefetched = false;
        bool isProvider = false;
        bool shorter = false;
    } llbp;

    // Folded history register. Same as in the TAGE predictor.
    FoldedHistoryFast* fghrT1[MAXNHIST];
    FoldedHistoryFast* fghrT2[MAXNHIST];
};

// DECLARE_SIM_OBJECT(CS395TBP);

} // namespace branch_prediction

} // namespace gem5

#endif // __CPU_PRED_CS395T_BP_PRED_HH__
