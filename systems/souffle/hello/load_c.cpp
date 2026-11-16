#define SOUFFLE_GENERATOR_VERSION "2.5"
#include "souffle/CompiledSouffle.h"
#include "souffle/SignalHandler.h"
#include "souffle/SouffleInterface.h"
#include "souffle/datastructure/BTree.h"
#include "souffle/io/IOSystem.h"
#include "souffle/utility/MiscUtil.h"
#include <any>
namespace functors {
extern "C" {
}
} //namespace functors
namespace souffle::t_btree_000_uu__0_1__11 {
using namespace souffle;
struct Type {
static constexpr Relation::arity_type Arity = 2;
using t_tuple = Tuple<RamDomain, 2>;
struct t_comparator_0{
 int operator()(const t_tuple& a, const t_tuple& b) const {
  return (ramBitCast<RamUnsigned>(a[0]) < ramBitCast<RamUnsigned>(b[0])) ? -1 : (ramBitCast<RamUnsigned>(a[0]) > ramBitCast<RamUnsigned>(b[0])) ? 1 :((ramBitCast<RamUnsigned>(a[1]) < ramBitCast<RamUnsigned>(b[1])) ? -1 : (ramBitCast<RamUnsigned>(a[1]) > ramBitCast<RamUnsigned>(b[1])) ? 1 :(0));
 }
bool less(const t_tuple& a, const t_tuple& b) const {
  return (ramBitCast<RamUnsigned>(a[0]) < ramBitCast<RamUnsigned>(b[0]))|| ((ramBitCast<RamUnsigned>(a[0]) == ramBitCast<RamUnsigned>(b[0])) && ((ramBitCast<RamUnsigned>(a[1]) < ramBitCast<RamUnsigned>(b[1]))));
 }
bool equal(const t_tuple& a, const t_tuple& b) const {
return (ramBitCast<RamUnsigned>(a[0]) == ramBitCast<RamUnsigned>(b[0]))&&(ramBitCast<RamUnsigned>(a[1]) == ramBitCast<RamUnsigned>(b[1]));
 }
};
using t_ind_0 = btree_set<t_tuple,t_comparator_0>;
t_ind_0 ind_0;
using iterator = t_ind_0::iterator;
struct context {
t_ind_0::operation_hints hints_0_lower;
t_ind_0::operation_hints hints_0_upper;
};
context createContext() { return context(); }
bool insert(const t_tuple& t);
bool insert(const t_tuple& t, context& h);
bool insert(const RamDomain* ramDomain);
bool insert(RamDomain a0,RamDomain a1);
bool contains(const t_tuple& t, context& h) const;
bool contains(const t_tuple& t) const;
std::size_t size() const;
iterator find(const t_tuple& t, context& h) const;
iterator find(const t_tuple& t) const;
range<iterator> lowerUpperRange_00(const t_tuple& /* lower */, const t_tuple& /* upper */, context& /* h */) const;
range<iterator> lowerUpperRange_00(const t_tuple& /* lower */, const t_tuple& /* upper */) const;
range<t_ind_0::iterator> lowerUpperRange_11(const t_tuple& lower, const t_tuple& upper, context& h) const;
range<t_ind_0::iterator> lowerUpperRange_11(const t_tuple& lower, const t_tuple& upper) const;
bool empty() const;
std::vector<range<iterator>> partition() const;
void purge();
iterator begin() const;
iterator end() const;
void printStatistics(std::ostream& o) const;
};
} // namespace souffle::t_btree_000_uu__0_1__11 
namespace souffle::t_btree_000_uu__0_1__11 {
using namespace souffle;
using t_ind_0 = Type::t_ind_0;
using iterator = Type::iterator;
using context = Type::context;
bool Type::insert(const t_tuple& t) {
context h;
return insert(t, h);
}
bool Type::insert(const t_tuple& t, context& h) {
if (ind_0.insert(t, h.hints_0_lower)) {
return true;
} else return false;
}
bool Type::insert(const RamDomain* ramDomain) {
RamDomain data[2];
std::copy(ramDomain, ramDomain + 2, data);
const t_tuple& tuple = reinterpret_cast<const t_tuple&>(data);
context h;
return insert(tuple, h);
}
bool Type::insert(RamDomain a0,RamDomain a1) {
RamDomain data[2] = {a0,a1};
return insert(data);
}
bool Type::contains(const t_tuple& t, context& h) const {
return ind_0.contains(t, h.hints_0_lower);
}
bool Type::contains(const t_tuple& t) const {
context h;
return contains(t, h);
}
std::size_t Type::size() const {
return ind_0.size();
}
iterator Type::find(const t_tuple& t, context& h) const {
return ind_0.find(t, h.hints_0_lower);
}
iterator Type::find(const t_tuple& t) const {
context h;
return find(t, h);
}
range<iterator> Type::lowerUpperRange_00(const t_tuple& /* lower */, const t_tuple& /* upper */, context& /* h */) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<iterator> Type::lowerUpperRange_00(const t_tuple& /* lower */, const t_tuple& /* upper */) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<t_ind_0::iterator> Type::lowerUpperRange_11(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_0 comparator;
int cmp = comparator(lower, upper);
if (cmp == 0) {
    auto pos = ind_0.find(lower, h.hints_0_lower);
    auto fin = ind_0.end();
    if (pos != fin) {fin = pos; ++fin;}
    return make_range(pos, fin);
}
if (cmp > 0) {
    return make_range(ind_0.end(), ind_0.end());
}
return make_range(ind_0.lower_bound(lower, h.hints_0_lower), ind_0.upper_bound(upper, h.hints_0_upper));
}
range<t_ind_0::iterator> Type::lowerUpperRange_11(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_11(lower,upper,h);
}
bool Type::empty() const {
return ind_0.empty();
}
std::vector<range<iterator>> Type::partition() const {
return ind_0.getChunks(400);
}
void Type::purge() {
ind_0.clear();
}
iterator Type::begin() const {
return ind_0.begin();
}
iterator Type::end() const {
return ind_0.end();
}
void Type::printStatistics(std::ostream& o) const {
o << " arity 2 direct b-tree index 0 lex-order [0,1]\n";
ind_0.printStats(o);
}
} // namespace souffle::t_btree_000_uu__0_1__11 
namespace souffle::t_btree_000_i__0__1 {
using namespace souffle;
struct Type {
static constexpr Relation::arity_type Arity = 1;
using t_tuple = Tuple<RamDomain, 1>;
struct t_comparator_0{
 int operator()(const t_tuple& a, const t_tuple& b) const {
  return (ramBitCast<RamSigned>(a[0]) < ramBitCast<RamSigned>(b[0])) ? -1 : (ramBitCast<RamSigned>(a[0]) > ramBitCast<RamSigned>(b[0])) ? 1 :(0);
 }
bool less(const t_tuple& a, const t_tuple& b) const {
  return (ramBitCast<RamSigned>(a[0]) < ramBitCast<RamSigned>(b[0]));
 }
bool equal(const t_tuple& a, const t_tuple& b) const {
return (ramBitCast<RamSigned>(a[0]) == ramBitCast<RamSigned>(b[0]));
 }
};
using t_ind_0 = btree_set<t_tuple,t_comparator_0>;
t_ind_0 ind_0;
using iterator = t_ind_0::iterator;
struct context {
t_ind_0::operation_hints hints_0_lower;
t_ind_0::operation_hints hints_0_upper;
};
context createContext() { return context(); }
bool insert(const t_tuple& t);
bool insert(const t_tuple& t, context& h);
bool insert(const RamDomain* ramDomain);
bool insert(RamDomain a0);
bool contains(const t_tuple& t, context& h) const;
bool contains(const t_tuple& t) const;
std::size_t size() const;
iterator find(const t_tuple& t, context& h) const;
iterator find(const t_tuple& t) const;
range<iterator> lowerUpperRange_0(const t_tuple& /* lower */, const t_tuple& /* upper */, context& /* h */) const;
range<iterator> lowerUpperRange_0(const t_tuple& /* lower */, const t_tuple& /* upper */) const;
range<t_ind_0::iterator> lowerUpperRange_1(const t_tuple& lower, const t_tuple& upper, context& h) const;
range<t_ind_0::iterator> lowerUpperRange_1(const t_tuple& lower, const t_tuple& upper) const;
bool empty() const;
std::vector<range<iterator>> partition() const;
void purge();
iterator begin() const;
iterator end() const;
void printStatistics(std::ostream& o) const;
};
} // namespace souffle::t_btree_000_i__0__1 
namespace souffle::t_btree_000_i__0__1 {
using namespace souffle;
using t_ind_0 = Type::t_ind_0;
using iterator = Type::iterator;
using context = Type::context;
bool Type::insert(const t_tuple& t) {
context h;
return insert(t, h);
}
bool Type::insert(const t_tuple& t, context& h) {
if (ind_0.insert(t, h.hints_0_lower)) {
return true;
} else return false;
}
bool Type::insert(const RamDomain* ramDomain) {
RamDomain data[1];
std::copy(ramDomain, ramDomain + 1, data);
const t_tuple& tuple = reinterpret_cast<const t_tuple&>(data);
context h;
return insert(tuple, h);
}
bool Type::insert(RamDomain a0) {
RamDomain data[1] = {a0};
return insert(data);
}
bool Type::contains(const t_tuple& t, context& h) const {
return ind_0.contains(t, h.hints_0_lower);
}
bool Type::contains(const t_tuple& t) const {
context h;
return contains(t, h);
}
std::size_t Type::size() const {
return ind_0.size();
}
iterator Type::find(const t_tuple& t, context& h) const {
return ind_0.find(t, h.hints_0_lower);
}
iterator Type::find(const t_tuple& t) const {
context h;
return find(t, h);
}
range<iterator> Type::lowerUpperRange_0(const t_tuple& /* lower */, const t_tuple& /* upper */, context& /* h */) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<iterator> Type::lowerUpperRange_0(const t_tuple& /* lower */, const t_tuple& /* upper */) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<t_ind_0::iterator> Type::lowerUpperRange_1(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_0 comparator;
int cmp = comparator(lower, upper);
if (cmp == 0) {
    auto pos = ind_0.find(lower, h.hints_0_lower);
    auto fin = ind_0.end();
    if (pos != fin) {fin = pos; ++fin;}
    return make_range(pos, fin);
}
if (cmp > 0) {
    return make_range(ind_0.end(), ind_0.end());
}
return make_range(ind_0.lower_bound(lower, h.hints_0_lower), ind_0.upper_bound(upper, h.hints_0_upper));
}
range<t_ind_0::iterator> Type::lowerUpperRange_1(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_1(lower,upper,h);
}
bool Type::empty() const {
return ind_0.empty();
}
std::vector<range<iterator>> Type::partition() const {
return ind_0.getChunks(400);
}
void Type::purge() {
ind_0.clear();
}
iterator Type::begin() const {
return ind_0.begin();
}
iterator Type::end() const {
return ind_0.end();
}
void Type::printStatistics(std::ostream& o) const {
o << " arity 1 direct b-tree index 0 lex-order [0]\n";
ind_0.printStats(o);
}
} // namespace souffle::t_btree_000_i__0__1 
namespace  souffle {
using namespace souffle;
class Stratum_edge_9543a759b165a2cb {
public:
 Stratum_edge_9543a759b165a2cb(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_000_uu__0_1__11::Type& rel_edge_04d25f5060e9043b);
void run([[maybe_unused]] const std::vector<RamDomain>& args,[[maybe_unused]] std::vector<RamDomain>& ret);
private:
SymbolTable& symTable;
RecordTable& recordTable;
ConcurrentCache<std::string,std::regex>& regexCache;
bool& pruneImdtRels;
bool& performIO;
SignalHandler*& signalHandler;
std::atomic<std::size_t>& iter;
std::atomic<RamDomain>& ctr;
std::string& inputDirectory;
std::string& outputDirectory;
t_btree_000_uu__0_1__11::Type* rel_edge_04d25f5060e9043b;
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
 Stratum_edge_9543a759b165a2cb::Stratum_edge_9543a759b165a2cb(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_000_uu__0_1__11::Type& rel_edge_04d25f5060e9043b):
symTable(symTable),
recordTable(recordTable),
regexCache(regexCache),
pruneImdtRels(pruneImdtRels),
performIO(performIO),
signalHandler(signalHandler),
iter(iter),
ctr(ctr),
inputDirectory(inputDirectory),
outputDirectory(outputDirectory),
rel_edge_04d25f5060e9043b(&rel_edge_04d25f5060e9043b){
}

void Stratum_edge_9543a759b165a2cb::run([[maybe_unused]] const std::vector<RamDomain>& args,[[maybe_unused]] std::vector<RamDomain>& ret){
if (performIO) {
try {std::map<std::string, std::string> directiveMap({{R"_(IO)_",R"_(file)_"},{R"_(attributeNames)_",R"_(x	y)_"},{R"_(auxArity)_",R"_(0)_"},{R"_(fact-dir)_",R"_(.)_"},{R"_(filename)_",R"_(edge.facts)_"},{R"_(name)_",R"_(edge)_"},{R"_(operation)_",R"_(input)_"},{R"_(params)_",R"_({"records": {}, "relation": {"arity": 2, "params": ["x", "y"]}})_"},{R"_(types)_",R"_({"ADTs": {}, "records": {}, "relation": {"arity": 2, "types": ["u:unsigned", "u:unsigned"]}})_"}});
if (!inputDirectory.empty()) {directiveMap["fact-dir"] = inputDirectory;}
IOSystem::getInstance().getReader(directiveMap, symTable, recordTable)->readAll(*rel_edge_04d25f5060e9043b);
} catch (std::exception& e) {std::cerr << "Error loading edge data: " << e.what() << '\n';
exit(1);
}
}
}

} // namespace  souffle

namespace  souffle {
using namespace souffle;
class Stratum_total_0df831dc4cd8afd3 {
public:
 Stratum_total_0df831dc4cd8afd3(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_000_uu__0_1__11::Type& rel_edge_04d25f5060e9043b,t_btree_000_i__0__1::Type& rel_total_fe4e9c103af60cd7);
void run([[maybe_unused]] const std::vector<RamDomain>& args,[[maybe_unused]] std::vector<RamDomain>& ret);
private:
SymbolTable& symTable;
RecordTable& recordTable;
ConcurrentCache<std::string,std::regex>& regexCache;
bool& pruneImdtRels;
bool& performIO;
SignalHandler*& signalHandler;
std::atomic<std::size_t>& iter;
std::atomic<RamDomain>& ctr;
std::string& inputDirectory;
std::string& outputDirectory;
t_btree_000_uu__0_1__11::Type* rel_edge_04d25f5060e9043b;
t_btree_000_i__0__1::Type* rel_total_fe4e9c103af60cd7;
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
 Stratum_total_0df831dc4cd8afd3::Stratum_total_0df831dc4cd8afd3(SymbolTable& symTable,RecordTable& recordTable,ConcurrentCache<std::string,std::regex>& regexCache,bool& pruneImdtRels,bool& performIO,SignalHandler*& signalHandler,std::atomic<std::size_t>& iter,std::atomic<RamDomain>& ctr,std::string& inputDirectory,std::string& outputDirectory,t_btree_000_uu__0_1__11::Type& rel_edge_04d25f5060e9043b,t_btree_000_i__0__1::Type& rel_total_fe4e9c103af60cd7):
symTable(symTable),
recordTable(recordTable),
regexCache(regexCache),
pruneImdtRels(pruneImdtRels),
performIO(performIO),
signalHandler(signalHandler),
iter(iter),
ctr(ctr),
inputDirectory(inputDirectory),
outputDirectory(outputDirectory),
rel_edge_04d25f5060e9043b(&rel_edge_04d25f5060e9043b),
rel_total_fe4e9c103af60cd7(&rel_total_fe4e9c103af60cd7){
}

void Stratum_total_0df831dc4cd8afd3::run([[maybe_unused]] const std::vector<RamDomain>& args,[[maybe_unused]] std::vector<RamDomain>& ret){
signalHandler->setMsg(R"_(total(@generator_0) :- 
   @generator_0 = count : { edge(+underscore_0,+underscore_1) }.
in file load.dl [6:1-6:35])_");
[&](){
Tuple<RamDomain,1> env0;
env0[0] = rel_edge_04d25f5060e9043b->size();
PARALLEL_START
CREATE_OP_CONTEXT(rel_edge_04d25f5060e9043b_op_ctxt,rel_edge_04d25f5060e9043b->createContext());
CREATE_OP_CONTEXT(rel_total_fe4e9c103af60cd7_op_ctxt,rel_total_fe4e9c103af60cd7->createContext());
if( (ramBitCast<RamDomain>(env0[0]) == ramBitCast<RamDomain>(env0[0]))) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_total_fe4e9c103af60cd7->insert(tuple,READ_OP_CONTEXT(rel_total_fe4e9c103af60cd7_op_ctxt));
}
PARALLEL_END
}
();if (performIO) {
try {std::map<std::string, std::string> directiveMap({{R"_(IO)_",R"_(stdout)_"},{R"_(attributeNames)_",R"_(c)_"},{R"_(auxArity)_",R"_(0)_"},{R"_(name)_",R"_(total)_"},{R"_(operation)_",R"_(output)_"},{R"_(output-dir)_",R"_(.)_"},{R"_(params)_",R"_({"records": {}, "relation": {"arity": 1, "params": ["c"]}})_"},{R"_(types)_",R"_({"ADTs": {}, "records": {}, "relation": {"arity": 1, "types": ["i:number"]}})_"}});
if (outputDirectory == "-"){directiveMap["IO"] = "stdout"; directiveMap["headers"] = "true";}
else if (!outputDirectory.empty()) {directiveMap["output-dir"] = outputDirectory;}
IOSystem::getInstance().getWriter(directiveMap, symTable, recordTable)->writeAll(*rel_total_fe4e9c103af60cd7);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}
if (pruneImdtRels) rel_edge_04d25f5060e9043b->purge();
}

} // namespace  souffle

namespace  souffle {
using namespace souffle;
class Sf_load_c: public SouffleProgram {
public:
 Sf_load_c();
 ~Sf_load_c();
void run();
void runAll(std::string inputDirectoryArg = "",std::string outputDirectoryArg = "",bool performIOArg = true,bool pruneImdtRelsArg = true);
void printAll([[maybe_unused]] std::string outputDirectoryArg = "");
void loadAll([[maybe_unused]] std::string inputDirectoryArg = "");
void dumpInputs();
void dumpOutputs();
SymbolTable& getSymbolTable();
RecordTable& getRecordTable();
void setNumThreads(std::size_t numThreadsValue);
void executeSubroutine(std::string name,const std::vector<RamDomain>& args,std::vector<RamDomain>& ret);
private:
void runFunction(std::string inputDirectoryArg,std::string outputDirectoryArg,bool performIOArg,bool pruneImdtRelsArg);
SymbolTableImpl symTable;
SpecializedRecordTable<0> recordTable;
ConcurrentCache<std::string,std::regex> regexCache;
Own<t_btree_000_uu__0_1__11::Type> rel_edge_04d25f5060e9043b;
souffle::RelationWrapper<t_btree_000_uu__0_1__11::Type> wrapper_rel_edge_04d25f5060e9043b;
Own<t_btree_000_i__0__1::Type> rel_total_fe4e9c103af60cd7;
souffle::RelationWrapper<t_btree_000_i__0__1::Type> wrapper_rel_total_fe4e9c103af60cd7;
Stratum_edge_9543a759b165a2cb stratum_edge_31e40d0d94fe9f7c;
Stratum_total_0df831dc4cd8afd3 stratum_total_b36e362480238d62;
std::string inputDirectory;
std::string outputDirectory;
SignalHandler* signalHandler{SignalHandler::instance()};
std::atomic<RamDomain> ctr{};
std::atomic<std::size_t> iter{};
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
 Sf_load_c::Sf_load_c():
symTable(),
recordTable(),
regexCache(),
rel_edge_04d25f5060e9043b(mk<t_btree_000_uu__0_1__11::Type>()),
wrapper_rel_edge_04d25f5060e9043b(0, *rel_edge_04d25f5060e9043b, *this, "edge", std::array<const char *,2>{{"u:unsigned","u:unsigned"}}, std::array<const char *,2>{{"x","y"}}, 0),
rel_total_fe4e9c103af60cd7(mk<t_btree_000_i__0__1::Type>()),
wrapper_rel_total_fe4e9c103af60cd7(1, *rel_total_fe4e9c103af60cd7, *this, "total", std::array<const char *,1>{{"i:number"}}, std::array<const char *,1>{{"c"}}, 0),
stratum_edge_31e40d0d94fe9f7c(symTable,recordTable,regexCache,pruneImdtRels,performIO,signalHandler,iter,ctr,inputDirectory,outputDirectory,*rel_edge_04d25f5060e9043b),
stratum_total_b36e362480238d62(symTable,recordTable,regexCache,pruneImdtRels,performIO,signalHandler,iter,ctr,inputDirectory,outputDirectory,*rel_edge_04d25f5060e9043b,*rel_total_fe4e9c103af60cd7){
addRelation("edge", wrapper_rel_edge_04d25f5060e9043b, true, false);
addRelation("total", wrapper_rel_total_fe4e9c103af60cd7, false, true);
}

 Sf_load_c::~Sf_load_c(){
}

void Sf_load_c::runFunction(std::string inputDirectoryArg,std::string outputDirectoryArg,bool performIOArg,bool pruneImdtRelsArg){

    this->inputDirectory  = std::move(inputDirectoryArg);
    this->outputDirectory = std::move(outputDirectoryArg);
    this->performIO       = performIOArg;
    this->pruneImdtRels   = pruneImdtRelsArg;

    // set default threads (in embedded mode)
    // if this is not set, and omp is used, the default omp setting of number of cores is used.
#if defined(_OPENMP)
    if (0 < getNumThreads()) { omp_set_num_threads(static_cast<int>(getNumThreads())); }
#endif

    signalHandler->set();
// -- query evaluation --
{
 std::vector<RamDomain> args, ret;
stratum_edge_31e40d0d94fe9f7c.run(args, ret);
}
{
 std::vector<RamDomain> args, ret;
stratum_total_b36e362480238d62.run(args, ret);
}

// -- relation hint statistics --
signalHandler->reset();
}

void Sf_load_c::run(){
runFunction("", "", false, false);
}

void Sf_load_c::runAll(std::string inputDirectoryArg,std::string outputDirectoryArg,bool performIOArg,bool pruneImdtRelsArg){
runFunction(inputDirectoryArg, outputDirectoryArg, performIOArg, pruneImdtRelsArg);
}

void Sf_load_c::printAll([[maybe_unused]] std::string outputDirectoryArg){
try {std::map<std::string, std::string> directiveMap({{R"_(IO)_",R"_(stdout)_"},{R"_(attributeNames)_",R"_(c)_"},{R"_(auxArity)_",R"_(0)_"},{R"_(name)_",R"_(total)_"},{R"_(operation)_",R"_(output)_"},{R"_(output-dir)_",R"_(.)_"},{R"_(params)_",R"_({"records": {}, "relation": {"arity": 1, "params": ["c"]}})_"},{R"_(types)_",R"_({"ADTs": {}, "records": {}, "relation": {"arity": 1, "types": ["i:number"]}})_"}});
if (!outputDirectoryArg.empty()) {directiveMap["output-dir"] = outputDirectoryArg;}
IOSystem::getInstance().getWriter(directiveMap, symTable, recordTable)->writeAll(*rel_total_fe4e9c103af60cd7);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}

void Sf_load_c::loadAll([[maybe_unused]] std::string inputDirectoryArg){
try {std::map<std::string, std::string> directiveMap({{R"_(IO)_",R"_(file)_"},{R"_(attributeNames)_",R"_(x	y)_"},{R"_(auxArity)_",R"_(0)_"},{R"_(fact-dir)_",R"_(.)_"},{R"_(filename)_",R"_(edge.facts)_"},{R"_(name)_",R"_(edge)_"},{R"_(operation)_",R"_(input)_"},{R"_(params)_",R"_({"records": {}, "relation": {"arity": 2, "params": ["x", "y"]}})_"},{R"_(types)_",R"_({"ADTs": {}, "records": {}, "relation": {"arity": 2, "types": ["u:unsigned", "u:unsigned"]}})_"}});
if (!inputDirectoryArg.empty()) {directiveMap["fact-dir"] = inputDirectoryArg;}
IOSystem::getInstance().getReader(directiveMap, symTable, recordTable)->readAll(*rel_edge_04d25f5060e9043b);
} catch (std::exception& e) {std::cerr << "Error loading edge data: " << e.what() << '\n';
exit(1);
}
}

void Sf_load_c::dumpInputs(){
try {std::map<std::string, std::string> rwOperation;
rwOperation["IO"] = "stdout";
rwOperation["name"] = "edge";
rwOperation["types"] = R"_({"relation": {"arity": 2, "auxArity": 0, "types": ["u:unsigned", "u:unsigned"]}})_";
IOSystem::getInstance().getWriter(rwOperation, symTable, recordTable)->writeAll(*rel_edge_04d25f5060e9043b);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}

void Sf_load_c::dumpOutputs(){
try {std::map<std::string, std::string> rwOperation;
rwOperation["IO"] = "stdout";
rwOperation["name"] = "total";
rwOperation["types"] = R"_({"relation": {"arity": 1, "auxArity": 0, "types": ["i:number"]}})_";
IOSystem::getInstance().getWriter(rwOperation, symTable, recordTable)->writeAll(*rel_total_fe4e9c103af60cd7);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}

SymbolTable& Sf_load_c::getSymbolTable(){
return symTable;
}

RecordTable& Sf_load_c::getRecordTable(){
return recordTable;
}

void Sf_load_c::setNumThreads(std::size_t numThreadsValue){
SouffleProgram::setNumThreads(numThreadsValue);
symTable.setNumLanes(getNumThreads());
recordTable.setNumLanes(getNumThreads());
regexCache.setNumLanes(getNumThreads());
}

void Sf_load_c::executeSubroutine(std::string name,const std::vector<RamDomain>& args,std::vector<RamDomain>& ret){
if (name == "edge") {
stratum_edge_31e40d0d94fe9f7c.run(args, ret);
return;}
if (name == "total") {
stratum_total_b36e362480238d62.run(args, ret);
return;}
fatal(("unknown subroutine " + name).c_str());
}

} // namespace  souffle
namespace souffle {
SouffleProgram *newInstance_load_c(){return new  souffle::Sf_load_c;}
SymbolTable *getST_load_c(SouffleProgram *p){return &reinterpret_cast<souffle::Sf_load_c*>(p)->getSymbolTable();}
} // namespace souffle

#ifndef __EMBEDDED_SOUFFLE__
#include "souffle/CompiledOptions.h"
int main(int argc, char** argv)
{
try{
souffle::CmdOptions opt(R"_(load.dl)_",
R"_()_",
R"_()_",
false,
R"_()_",
8);
if (!opt.parse(argc,argv)) return 1;
souffle::Sf_load_c obj;
#if defined(_OPENMP) 
obj.setNumThreads(opt.getNumJobs());

#endif
obj.runAll(opt.getInputFileDir(), opt.getOutputFileDir());
return 0;
} catch(std::exception &e) { souffle::SignalHandler::instance()->error(e.what());}
}
#endif

namespace  souffle {
using namespace souffle;
class factory_Sf_load_c: souffle::ProgramFactory {
public:
souffle::SouffleProgram* newInstance();
 factory_Sf_load_c();
private:
};
} // namespace  souffle
namespace  souffle {
using namespace souffle;
souffle::SouffleProgram* factory_Sf_load_c::newInstance(){
return new  souffle::Sf_load_c();
}

 factory_Sf_load_c::factory_Sf_load_c():
souffle::ProgramFactory("load_c"){
}

} // namespace  souffle
namespace souffle {

#ifdef __EMBEDDED_SOUFFLE__
extern "C" {
souffle::factory_Sf_load_c __factory_Sf_load_c_instance;
}
#endif
} // namespace souffle

