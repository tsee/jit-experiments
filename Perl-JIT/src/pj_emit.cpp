#include "pj_emit.h"
#include "pj_optree.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/Analysis/Passes.h>
#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Support/TargetSelect.h>

#include <deque>
#include <tr1/unordered_set>

using namespace PerlJIT;
using namespace PerlJIT::AST;
using namespace llvm;
using namespace std;
using namespace std::tr1;

#define JIT_SCALAR_OP OP_STUB
#define JIT_LIST_OP   OP_LIST

#define ITEM_COUNT(A) (sizeof(A) / sizeof((A)[0]))
#define MY_CXT_KEY "Perl::JIT::_guts" XS_VERSION

static Perl_ophook_t previous_free_hook = NULL;

namespace {
  struct PerlMutex {
    PerlMutex() { MUTEX_INIT(&mutex); }
    ~PerlMutex() { MUTEX_DESTROY(&mutex); }

    operator perl_mutex *() { return &mutex; }

  private:
    perl_mutex mutex;
  };

  PerlMutex jit_ops_mutex;
  unordered_set<OP *> jit_ops;
}

static pj_op_type JITTABLE_OPS[] = {
  pj_binop_add
};
static unordered_set<int> Jittable_Ops(
  JITTABLE_OPS,
  JITTABLE_OPS + ITEM_COUNT(JITTABLE_OPS)
);

struct ScalarOP : public OP {
  shared_ptr<ExecutionEngine> execution_engine;
};

struct ListOP : public LISTOP {
  shared_ptr<ExecutionEngine> execution_engine;
};

static
void free_execution_engine(pTHX_ OP *op)
{
  if (op->op_type == JIT_SCALAR_OP || op->op_type == JIT_LIST_OP) {
    MUTEX_LOCK(jit_ops_mutex);
    if (jit_ops.find(op) != jit_ops.end()) {
      if (op->op_type == JIT_SCALAR_OP)
          ((ScalarOP *) op)->~ScalarOP();
      else
          ((ListOP *) op)->~ListOP();
    }
    MUTEX_UNLOCK(jit_ops_mutex);
  }

  if (previous_free_hook)
    previous_free_hook(aTHX_ op);
}

namespace PerlJIT {
  struct Cxt {
    IRBuilder<> builder;
    shared_ptr<ExecutionEngine> engine;
    Module *module;
    FunctionPassManager *fpm;
    PerlAPI *pa;

    void create_module();

    Cxt();
    ~Cxt();
  };
}
typedef Cxt my_cxt_t;
START_MY_CXT;

Cxt::Cxt() :
  builder(getGlobalContext()), module(NULL), fpm(NULL), pa(NULL)
{
}

Cxt::~Cxt()
{
  delete pa;
  delete fpm;
}

void
Cxt::create_module()
{
  if (module)
    return;

  string errstr;

  module = new Module("PerlJIT", getGlobalContext());
  engine = shared_ptr<ExecutionEngine>(EngineBuilder(module).setErrorStr(&errstr).create());

  if (!engine) {
    delete module;
    croak("Could not create ExecutionEngine: %s", errstr.c_str());
  }

  fpm = new FunctionPassManager(module);

  module->setDataLayout(engine->getDataLayout()->getStringRepresentation());
  fpm->add(createBasicAliasAnalysisPass());
  fpm->add(createPromoteMemoryToRegisterPass());
  fpm->add(createInstructionCombiningPass());
  fpm->add(createReassociatePass());
  fpm->add(createGVNPass());
  fpm->add(createCFGSimplificationPass());

  fpm->doInitialization();

  pa = new PerlAPI(module, &builder, engine.get());
}

static void
cleanup_emitter(pTHX_ void *ptr)
{
  dMY_CXT;
  MY_CXT.~Cxt();
}

void
PerlJIT::pj_init_emitter(pTHX)
{
  InitializeNativeTarget();

  previous_free_hook = PL_opfreehook;
  PL_opfreehook = free_execution_engine;

  MY_CXT_INIT;
  new (&MY_CXT) Cxt();

  Perl_call_atexit(aTHX_ cleanup_emitter, NULL);
}

SV *
PerlJIT::pj_jit_sub(SV *coderef)
{
  dTHX;
  dMY_CXT;
  SV *error = NULL;

  MY_CXT.create_module();

  {
    std::vector<Term *> asts = pj_find_jit_candidates(aTHX_ coderef);
    AV *ops = newAV();
    Emitter emitter(aTHX_ aMY_CXT_ (CV *)SvRV(coderef), ops);

    if (emitter.process_jit_candidates(asts))
      return newRV_noinc((SV *) ops);

    SvREFCNT_dec(ops);
    std::string error_message = emitter.error();
    error = sv_2mortal(newSVpv(error_message.c_str(), error_message.size()));
  }

  // here we can croak because all C++ objects have been destroyed
  croak_sv(error);
  return NULL;
}


Emitter::Emitter(pTHX_ pMY_CXT_ CV *_cv, AV *_ops) :
  cv(_cv), ops(_ops),
  module(MY_CXT.module), fpm(MY_CXT.fpm),
  execution_engine(MY_CXT.engine),
  pa(*MY_CXT.pa)
{
  SET_CXT_MEMBER;
  SET_THX_MEMBER;
}

Emitter::Emitter(pTHX_ pMY_CXT_ const Emitter &other) :
  cv(other.cv), ops(other.ops),
  module(other.module), fpm(other.fpm),
  execution_engine(other.execution_engine),
  pa(other.pa)
{
  SET_CXT_MEMBER;
  SET_THX_MEMBER;
}

Emitter::~Emitter()
{
}

bool
Emitter::process_jit_candidates(const std::vector<Term *> &asts)
{
  std::deque<Term *> queue(asts.begin(), asts.end());

  error_message.clear();
  while (queue.size()) {
    Term *ast = queue.front();

    queue.pop_front();

    switch (ast->type) {
    case pj_ttype_lexical:
    case pj_ttype_variabledeclaration:
    case pj_ttype_global:
    case pj_ttype_constant:
      continue;
    case pj_ttype_statementsequence: {
      const std::vector<Term *> &kids = ast->get_kids();
      std::vector<Term *> seq;

      for (size_t i = 0, max = kids.size(); i < max; ++i) {
        Term *stmt = kids[i];

        if (is_jittable(stmt)) {
          seq.push_back(static_cast<Statement *>(stmt));
        } else {
          if (seq.size()) {
            if (!jit_statement_sequence(seq))
              return false;
            seq.clear();
          }

          if (!process_jit_candidates(stmt->get_kids()))
            return false;
        }
      }

      if (seq.size())
        if (!jit_statement_sequence(seq))
          return false;

      break;
    }
    default:
      if (is_jittable(ast)) {
        jit_tree(ast);
      } else {
        const std::vector<Term *> &kids = ast->get_kids();

        queue.insert(queue.begin(), kids.begin(), kids.end());
      }
      break;
    }
  }

  return true;
}

bool
Emitter::jit_tree(Term *ast)
{
  std::vector<Term *> asts(1, ast);
  OP *op = _jit_trees(asts);
  if (!op)
    return false;

  replace_sequence(ast->first_op(), ast->last_op(), op, false);
  return true;
}


bool
Emitter::jit_statement_sequence(const std::vector<Term *> &asts)
{
  OP *op = _jit_trees(asts);
  if (!op)
    return false;

  // this assumes all the ASTs are statements, and that all nextstate
  // OPs have been detached
  replace_sequence(static_cast<Statement *>(asts.front())->kids[0]->first_op(),
                   static_cast<Statement *>(asts.back())->kids[0]->last_op(),
                   op, false);

  return true;
}

OP *
Emitter::_jit_trees(const std::vector<Term *> &asts)
{
  Function *f = Function::Create(pa.ppaddr_type(), GlobalValue::ExternalLinkage, "", module);
  BasicBlock *bb = BasicBlock::Create(module->getContext(), "entry", f);

  pa.set_current_function(f);
  MY_CXT.builder.SetInsertPoint(bb);

  bool valid = true;
  for (size_t i = 0, max = asts.size(); i < max && valid; ++i)
    valid = valid && _jit_emit_root(asts[i]);

  MY_CXT.builder.CreateRet(pa.emit_OP_op_next());

  if (!valid) {
    subtrees.clear();
    return NULL;
  }

  // f->dump();
  verifyFunction(*f);
  fpm->run(*f);
  // f->dump();

  // here it'd be nice to use custom ops, but they are registered by
  // PP function address; we could use a trampoline address (with
  // just an extra jump, but then we'd need to store the pointer to the
  // JITted function as an extra op member
  OP *op;

  if (subtrees.size()) {
    ListOP *listop;
    NewOp(4242, listop, 1, ListOP);
    new (listop) ListOP();

    listop->op_type = JIT_LIST_OP;
    listop->op_flags = OPf_KIDS;
    listop->op_first = listop->op_last = subtrees[0];
    listop->execution_engine = execution_engine;

    for (size_t i = 1, max = subtrees.size(); i < max; ++i) {
      OP *sibling = subtrees[i];

      listop->op_last->op_sibling = sibling;
      listop->op_last = sibling;
      sibling->op_sibling = NULL;
    }

    op = (OP *) listop;
  } else {
      ScalarOP *scalarop;
      NewOp(4242, scalarop, 1, ScalarOP);
      new (scalarop) ScalarOP();

      scalarop->op_type = JIT_SCALAR_OP;
      scalarop->execution_engine = execution_engine;

      op = (OP *) scalarop;
  }

  MUTEX_LOCK(jit_ops_mutex);
  jit_ops.insert(op);
  MUTEX_UNLOCK(jit_ops_mutex);

  op->op_ppaddr = (OP *(*)(pTHX)) execution_engine->getPointerToFunction(f);
  op->op_targ = asts.back()->perl_op->op_targ;

  subtrees.clear();

  return op;
}

bool
Emitter::_jit_emit_root(Term *ast)
{
  EmitValue jv = _jit_emit(ast, ast->perl_op->op_targ ? &ANY_T : &SCALAR_T);

  if (jv.is_invalid())
    return false;
  if (jv.value)
    return _jit_emit_return(ast, ast->context(), jv.value, jv.type);
}

EmitValue
Emitter::_jit_emit(Term *ast, const PerlJIT::AST::Type *type)
{
  switch (ast->type) {
  case pj_ttype_constant:
    return _jit_emit_const(static_cast<PerlJIT::AST::Constant *>(ast), type);
  case pj_ttype_lexical:
    return _jit_get_lexical_sv(static_cast<Lexical *>(ast));
  case pj_ttype_variabledeclaration:
    return _jit_get_lexical_declaration_sv(static_cast<VariableDeclaration *>(ast));
  case pj_ttype_op:
    if (is_jittable(ast))
      return _jit_emit_op(static_cast<Op *>(ast), type);
    else
      return _jit_emit_optree_jit_kids(ast, type);
  default:
    return _jit_emit_optree_jit_kids(ast, type);
  }
}

EmitValue
Emitter::_jit_emit_op(Op *ast, const PerlJIT::AST::Type *type)
{
  switch (ast->op_class()) {
  case pj_opc_binop:
    return _jit_emit_binop(static_cast<Binop *>(ast), type);
  default:
    return _jit_emit_optree_jit_kids(ast, type);
  }
}

EmitValue
Emitter::_jit_emit_optree_jit_kids(Term *ast, const PerlJIT::AST::Type *type)
{
  Emitter emitter(aTHX_ aMY_CXT_ *this);

  if (!emitter.process_jit_candidates(ast->get_kids()))
    return EmitValue::invalid();
  return _jit_emit_optree(ast);
}

bool
Emitter::is_jittable(PerlJIT::AST::Term *ast)
{
  switch (ast->type) {
  case pj_ttype_constant:
  case pj_ttype_lexical:
  case pj_ttype_variabledeclaration:
    return true;
  case pj_ttype_optree:
    return false;
  case pj_ttype_statement:
    return is_jittable(static_cast<PerlJIT::AST::Statement *>(ast)->kids[0]);
  case pj_ttype_statementsequence: {
    std::vector<Term *> all = ast->get_kids();
    unsigned int jittable = 0;

    for (size_t i = 0, max = all.size(); i < max; ++i)
      jittable += is_jittable(all[i]);

    // TODO arbitrary threshold, it's probably better to look for
    //      long stretches of JITtable ops
    return jittable * 2 >= all.size();

  }
  case pj_ttype_op: {
    Op *op = static_cast<Op *>(ast);
    bool known = Jittable_Ops.find(op->optype) != Jittable_Ops.end();

    if (!known)
      return false;
    if (op->may_have_explicit_overload())
      return true;
    if (op->op_class() == pj_opc_binop &&
        static_cast<Binop *>(op)->is_synthesized_assignment())
      return is_jittable(op->kids[1]);
    return !needs_excessive_magic(op);
  }
  default:
    return false;
  }
}

bool
Emitter::needs_excessive_magic(PerlJIT::AST::Op *ast)
{
  std::deque<Term *> nodes(1, ast);

  while (nodes.size()) {
    Term *node = nodes.front();
    nodes.pop_front();

    if ((node->type == pj_ttype_lexical || node->type == pj_ttype_variabledeclaration) &&
            node->get_value_type()->is_opaque())
      return true;

    if (node->type != pj_ttype_op)
      continue;
    Op *op = static_cast<Op *>(node);

    bool known = Jittable_Ops.find(op->optype) != Jittable_Ops.end();

    if (!known || !op->may_have_explicit_overload())
      continue;

    std::vector<Term *> kids = node->get_kids();
    nodes.insert(nodes.end(), kids.begin(), kids.end());
  }

  return false;
}

void
Emitter::_jit_clear_op_next(OP *op)
{
  // in most cases, the exit point for an optree is the op_next
  // pointer of the root op, but conditional operators have interesting
  // control flows
  switch (op->op_type) {
  case OP_COND_EXPR: {
    LOGOP *logop = cLOGOPx(op);

    _jit_clear_op_next(logop->op_first->op_sibling);
    _jit_clear_op_next(logop->op_first->op_sibling->op_sibling);
  }
    break;
  case OP_AND:
  case OP_ANDASSIGN:
  case OP_OR:
  case OP_ORASSIGN:
  case OP_DOR:
  case OP_DORASSIGN: {
    LOGOP *logop = cLOGOPx(op);

    _jit_clear_op_next(logop->op_first->op_sibling);
    op->op_next = NULL;
  }
    break;
  default:
    op->op_next = NULL;
    break;
  }
}

EmitValue
Emitter::_jit_emit_optree(Term *ast)
{
  // unfortunately there is (currently) no way to clone an optree,
  // so just detach the ops from the root tree
  detach_tree(ast->perl_op, true);
  _jit_clear_op_next(ast->perl_op);
  subtrees.push_back(ast->perl_op);

  pa.emit_call_runloop(ast->start_op());

  if (ast->context() == pj_context_caller) {
    set_error("Caller-determined context not implemented");
    return EmitValue::invalid();
  }
  if (ast->context() != pj_context_scalar)
    return EmitValue(NULL, NULL);

  pa.alloc_sp();
  llvm::Value *res = pa.emit_POPs();
  pa.emit_PUTBACK();

  return EmitValue(res, &SCALAR_T);
}

bool
Emitter::_jit_emit_return(Term *ast, pj_op_context context, Value *value, const PerlJIT::AST::Type *type)
{
  // TODO OPs with OPpTARGET_MY flag are in scalar context even when
  // they should be in void context, so we're emitting an useless push
  // below

  if (context == pj_context_caller) {
    set_error("Caller-determined context not implemented");
    return false;
  }

  if (context != pj_context_scalar)
    return true;

  Op *op = dynamic_cast<Op *>(ast);
  Value *res;

  switch (op->op_class()) {
  case pj_opc_binop: {
    // the assumption here is that the OPf_STACKED assignment
    // has been handled by _jit_emit below, and here we only need
    // to handle cases like '$x = $y += 7'
    if (op->optype == pj_binop_sassign) {
      if (type->equals(&SCALAR_T)) {
        res = value;
      } else {
        // TODO suboptimal, but correct, need to ask for SCALAR
        // in the call to _jit_emit() above
        res = pa.emit_sv_newmortal();
      }
    } else {
      if (!op->perl_op->op_targ && type->equals(&SCALAR_T)) {
        res = value;
      } else {
        if (!op->perl_op->op_targ) {
          set_error("Binary OP without target");
          return false;
        }
        res = pa.emit_OP_targ();
      }
    }
  }
  case pj_opc_unop:
    if (!op->perl_op->op_targ) {
      set_error("Unary OP without target");
      return false;
    }
    res = pa.emit_OP_targ();
  default:
    res = pa.emit_sv_newmortal();
    break;
  }

  if (res != value)
    if (!_jit_assign_sv(res, value, type))
      return false;

  pa.alloc_sp();
  pa.emit_SPAGAIN();
  pa.emit_XPUSHs(res);
  pa.emit_PUTBACK();

  return true;
}

EmitValue
Emitter::_jit_emit_binop(Binop *ast, const PerlJIT::AST::Type *type)
{
  EmitValue lv = _jit_emit(ast->kids[0], &DOUBLE_T);
  EmitValue rv = _jit_emit(ast->kids[1], &DOUBLE_T);
  Value *lvv = _to_nv_value(lv.value, lv.type),
        *rvv = _to_nv_value(rv.value, rv.type);

  if (!lvv || !rvv)
    return EmitValue::invalid();

  Value *res = MY_CXT.builder.CreateFAdd(lvv, rvv);

  if (ast->is_assignment_form()) {
    // TODO proper LVALUE treatment
    if (!lv.type->equals(&SCALAR_T)) {
      set_error("Can only assign to perl scalars, got a " + lv.type->to_string());
      return EmitValue::invalid();
    }
    if (!_jit_assign_sv(lv.value, res, &DOUBLE_T))
      return EmitValue::invalid();
  }

  return EmitValue(res, &DOUBLE_T);
}

EmitValue
Emitter::_jit_get_lexical_declaration_sv(PerlJIT::AST::VariableDeclaration *ast)
{
  Value *svp = pa.emit_pad_sv_address(ast->get_pad_index());

  pa.emit_save_clearsv(svp);

  // TODO this value can be cached
  // FIXME is SCALAR correct in the face of other Perl types? (@,%,etc.)? Or is this a ref to @,%,etc?
  return EmitValue(MY_CXT.builder.CreateLoad(svp), &SCALAR_T);
}

EmitValue
Emitter::_jit_get_lexical_sv(Lexical *ast)
{
  // TODO this value can be cached
  return EmitValue(pa.emit_pad_sv(ast->get_pad_index()), &SCALAR_T);
}

bool
Emitter::_jit_assign_sv(Value *sv, Value *value, const PerlJIT::AST::Type *type)
{
  if (type->equals(&DOUBLE_T))
    pa.emit_sv_setnv(sv, value);
  else if (type->equals(&INT_T))
    pa.emit_sv_setiv(sv, value);
  else if (type->equals(&SCALAR_T))
    pa.emit_SvSetSV_nosteal(sv, value);
  else if (type->equals(&UNSPECIFIED_T))
    pa.emit_SvSetSV_nosteal(sv, value);
  else {
    set_error("Unable to assign " + type->to_string() + " to an SV");
    return false;
  }

  return true;
}

EmitValue
Emitter::_jit_emit_const(PerlJIT::AST::Constant *ast, const PerlJIT::AST::Type *type)
{
  switch (ast->get_value_type()->tag()) {
  case pj_double_type:
    return EmitValue(
      pa.NV_constant(static_cast<NumericConstant *>(ast)->dbl_value),
      &DOUBLE_T);
  case pj_int_type:
    return EmitValue(
      pa.IV_constant(static_cast<NumericConstant *>(ast)->int_value),
      &INT_T);
  case pj_uint_type:
    return EmitValue(
      pa.UV_constant(static_cast<NumericConstant *>(ast)->uint_value),
      &UNSIGNED_INT_T);
  default:
    set_error("Unable to emit this type of constant");
    return EmitValue::invalid();
  }
}

Value *
Emitter::_to_nv_value(Value *value, const PerlJIT::AST::Type *type)
{
  if (type->equals(&DOUBLE_T))
    return value;
  if (type->is_integer())
    // TODO cache type
    return MY_CXT.builder.CreateFPCast(value, llvm::Type::getDoubleTy(module->getContext()));
  if (type->equals(&SCALAR_T) || type->equals(&UNSPECIFIED_T))
    return pa.emit_SvNV(value);

  set_error("Handle more NV coercion cases");
  return NULL;
}

static SV *
op_2sv(pTHX_ OP *op)
{
  return sv_setref_iv(newSV(0), "B::OP", PTR2IV(op));
}

void
Emitter::set_error(const std::string &error)
{
  error_message = error;
}

std::string
Emitter::error() const
{
  return error_message;
}

void
Emitter::replace_sequence(OP *first, OP *last, OP *op, bool keep)
{
  AV *item = newAV();

  av_push(item, newSVpvs("replace"));
  av_push(item, newRV_inc((SV *) cv));
  av_push(item, op_2sv(aTHX_ first));
  av_push(item, op_2sv(aTHX_ last));
  av_push(item, op_2sv(aTHX_ op));
  av_push(item, keep ? &PL_sv_yes : &PL_sv_no);

  av_push(ops, newRV_noinc((SV *) item));
}

void
Emitter::detach_tree(OP *op, bool keep)
{
  AV *item = newAV();

  av_push(item, newSVpvs("detach"));
  av_push(item, newRV_inc((SV *) cv));
  av_push(item, op_2sv(aTHX_ op));
  av_push(item, keep ? &PL_sv_yes : &PL_sv_no);

  av_push(ops, newRV_noinc((SV *) item));
}
