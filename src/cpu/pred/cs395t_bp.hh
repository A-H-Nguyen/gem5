#ifndef __CPU_PRED_CS395T_BP_PRED_HH__
#define __CPU_PRED_CS395T_BP_PRED_HH__

#include "base/sat_counter.hh"
#include "cpu/pred/bpred_unit.hh"
// #include "cpu/pred/tage_sc_l.hh"
#include "cpu/pred/tage_sc_l_64KB.hh"
#include "params/TAGE_SC_L_64KB.hh"
#include "params/TAGE_SC_L_64KB_StatisticalCorrector.hh"
#include "params/TAGE_SC_L_TAGE_64KB.hh"
#include "params/CS395TBP.hh"

namespace gem5
{
// LLBP's cache implementation. It's too complex to implement gem5's caches
// into a branch predictor, and Edinburgh already did the actual work
template <typename key_t, typename value_t>
class BaseCache {
   protected:
    typedef typename std::pair<key_t, value_t> key_value_pair_t;
    typedef typename std::list<key_value_pair_t>::iterator list_iterator_t;
    typedef typename std::list<key_value_pair_t> set_t;

    std::unordered_map<uint64_t, list_iterator_t> _index;
    std::vector<std::list<key_value_pair_t>> _cache;
    const size_t _max_size;
    const size_t _assoc;
    const uint64_t _sets;
    const uint64_t _set_mask;

   public:
    BaseCache(size_t max_size, size_t assoc)
        : _max_size(max_size),
          _assoc(assoc),
          _sets(max_size / assoc),
          _set_mask(_sets - 1) {
        // Check if number of sets is a power of 2
        assert((_sets & (_sets - 1)) == 0);
        assert(_assoc * _sets == _max_size);
        _cache.resize(_sets);
        // for (auto& set : _cache) {
        //     set.resize(assoc);
        // }
    }

    void printCfg() {
        printf("Max size: %lu, Assoc: %lu, Sets: %lu\n", _max_size, _assoc,
               _sets);
    }

    size_t size() const { return _index.size(); }

    key_t index(const key_t& key) { return key & _set_mask; }

    set_t& getSet(const key_t& key) {
        return _cache[index(key)];
    }

    const std::unordered_map<key_t, list_iterator_t> getMap() { return _index; }

    value_t* get(const key_t& key) {
        auto it = _index.find(key);
        if (it == _index.end()) {
            return nullptr;
        }
        return &it->second->second;
    }

    void erase(const key_t& key) {
        auto it = _index.find(key);
        if (it == _index.end()) {
            return;
        }
        auto& set = getSet(key);
        set.erase(it->second);
        _index.erase(key);
    }

    value_t* getVictim(const key_t& key) {
        auto& set = getSet(key);
        if (set.size() < _assoc) {
            return nullptr;
        }
        return &set.back().second;
    }

    void touch(const key_t& key) {
        auto it = _index.find(key);
        if (it == _index.end()) {
            return;
        }
        auto& set = getSet(key);
        set.splice(set.begin(), set, it->second);
    }

    bool exists(const key_t& key) const {
        return _index.find(key) != _index.end();
    }

    int distance(const key_t& key) {
        auto it = _index.find(key);
        if (it == _index.end()) {
            return -1;
        }
        auto& set = getSet(key);
        return std::distance(set.begin(), it->second);
    }

    set_t& getResizedSet(const key_t& key) {
        auto& set = getSet(key);

        // If this element will exceed the max size, remove the last element
        if (set.size() >= _assoc) {
            auto last = set.end();
            last--;
            _index.erase(last->first);
            set.pop_back();
        }
        return set;
    }

    value_t* insertAt(const key_t& key, int at = 0) {
        auto v = get(key);
        if (v != nullptr) {
            return v;
        }

        // Get the set with a free item
        auto& set = getResizedSet(key);

        // Move to the insert position
        auto it2 = set.begin();
        at = std::min(at, (int)set.size());
        std::advance(it2, at);

        it2 = set.emplace(it2, key_value_pair_t(key, value_t()));
        _index[key] = it2;
        return &(it2->second);
    }

    value_t* insert(const key_t& key) {
        auto v = get(key);
        if (v != nullptr) {
            return v;
        }

        // Get the set with a free item
        auto& set = getResizedSet(key);

        // Move to the insert position
        auto it = set.begin();

        it = set.emplace(it, key_value_pair_t(key, value_t()));
        _index[key] = it;
        return &(it->second);
    }
};

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

// typedef uint64_t Key;
// Key KEY[MAXNHIST];  //

// class CS395TBP : public BPredUnit
class CS395TBP : public TAGE_SC_L_TAGE
{
  public:
    CS395TBP(const CS395TBPParams &params);

  protected:
    const int numContexts = 1000000;
    const int numPatterns = 1000000;
    const int ctxAssoc = numContexts;
    const int ptrnAssoc = numPatterns;
    const int TTWidth = 20;
    const int CTWidth = 31;
    const int pbSize = 1;
    const int pbAssoc = pbSize;
    const int CtrWidth = 3;
    const int CtxReplCtrWidth = 2;

  public:
    bool lookup(ThreadID tid, Addr pc, void * &bp_history) override;
    void updateHistories(ThreadID tid, Addr pc, bool uncond,
                         bool taken, Addr target, void * &bp_history) override;
    void squash(ThreadID tid, void * &bp_history) override;
    void update(ThreadID tid, Addr pc, bool taken, void * &bp_history,
                bool squashed, const StaticInstPtr &inst, Addr target) override;

  private:
    // Prediction Structures

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
};

} // namespace branch_prediction

} // namespace gem5

#endif // __CPU_PRED_CS395T_BP_PRED_HH__
