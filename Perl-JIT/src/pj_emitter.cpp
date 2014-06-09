#include "pj_emitter.h"
#include "pj_emit.h"
#include "pj_optree.h"
#include "pj_codegen.h"

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

// part of B::Replace API
#define KEEP_OPS     1
#define KEEP_TARGETS 2
#define REPLACE_TREE 4
#define KEEP_OP_NEXT 8

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
START_MY_CXT

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


static void
_jit_clear_op_next(OP *op)
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

void
PerlJIT::pj_jit_sub(SV *coderef)
{
  dTHX;
  dMY_CXT;
  SV *error = NULL;

  MY_CXT.create_module();

  {
    std::vector<Term *> asts = pj_find_jit_candidates(aTHX_ coderef);
    Emitter emitter(aTHX_ aMY_CXT_ (CV *)SvRV(coderef));

    if (emitter.process_jit_candidates(asts))
      return;

    std::string error_message = emitter.error();
    error = sv_2mortal(newSVpv(error_message.c_str(), error_message.size()));
  }

  // here we can croak because all C++ objects have been destroyed
  croak_sv(error);
}


bool
Emitter::FunctionState::empty()
{
  return function->empty() || (
    function->size() == 1 && function->front().empty()
  );
}

Emitter::Emitter(pTHX_ pMY_CXT_ CV *_cv) :
  cv(_cv),
  module(MY_CXT.module), fpm(MY_CXT.fpm),
  execution_engine(MY_CXT.engine),
  pa(*MY_CXT.pa)
{
  SET_CXT_MEMBER;
  SET_THX_MEMBER;
}

Emitter::Emitter(pTHX_ pMY_CXT_ const Emitter &other) :
  cv(other.cv),
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

  _push_empty_function();

  error_message.clear();
  while (queue.size()) {
    Term *ast = queue.front();

    queue.pop_front();

    switch (ast->get_type()) {
    case pj_ttype_lexical:
    case pj_ttype_variabledeclaration:
    case pj_ttype_global:
    case pj_ttype_constant:
      continue;
    case pj_ttype_statementsequence: {
      const std::vector<Term *> &kids = ast->get_kids();
      Statement *first = NULL, *last = NULL;
      EmitValue last_value;

      for (size_t i = 0, max = kids.size(); i < max; ++i) {
        Statement *stmt = static_cast<Statement *>(kids[i]);
        EmitValue result = run(PerlJIT::CodegenNode(stmt));

        if (result.is_invalid())
          return false;
        if (result.value == OPTREE && first) {
          _emit_current_function_and_push_empty(first, last, last_value);
          first = last = NULL;
        }
        if (result.value != OPTREE) {
          if (!first)
            first = stmt;
          last = stmt;
          last_value = result;
        }
      }

      if (last)
        _emit_current_function_and_push_empty(first, last, last_value);
    }
      break;
    default: {
      EmitValue result = run(PerlJIT::CodegenNode(ast));

      if (result.value != OPTREE)
        _emit_current_function_and_push_empty(ast, ast, result);
    }
      break;
    }
  }

  _pop_empty_function();

  return true;
}

void
Emitter::_emit_current_function_and_push_empty(Term *first, Term *last, const EmitValue &value)
{
  if (first->get_type() == pj_ttype_statement && last->get_type() == pj_ttype_statement) {
    first = static_cast<Statement *>(first)->kids[0];
    last = static_cast<Statement *>(last)->kids[0];
  }

  OP *op = _replace_with_clean_emitter_state(last, value);
  if (!op)
    return;
  replace_sequence(first->first_op(), last->last_op(),
                   op, KEEP_TARGETS|KEEP_OP_NEXT);
}

OP *
Emitter::_replace_with_clean_emitter_state(Term *root, const EmitValue &value)
{
  if (emitter_states.back().empty())
    return NULL;
  OP *op = _generate_current_function(root, value);
  if (!op)
    return NULL;
  _push_empty_function();
  return op;
}

OP *
Emitter::_generate_current_function(Term *root, const EmitValue &value)
{
  if (!_jit_emit_return(root, root->context(), value.value, value.type))
    return NULL;

  FunctionState &last = emitter_states.back();
  llvm::Function *f = last.function;

  MY_CXT.builder.CreateRet(pa.emit_OP_op_next());

  // f->dump();
  verifyFunction(*f);
  fpm->run(*f);
  // f->dump();

  // here it'd be nice to use custom ops, but they are registered by
  // PP function address; we could use a trampoline address (with
  // just an extra jump, but then we'd need to store the pointer to the
  // JITted function as an extra op member
  OP *op;

  if (last.subtrees.size()) {
    ListOP *listop;
    NewOp(4242, listop, 1, ListOP);
    new (listop) ListOP();

    detach_tree(last.subtrees[0], KEEP_OPS);
    _jit_clear_op_next(last.subtrees[0]);

    listop->op_type = JIT_LIST_OP;
    listop->op_flags = OPf_KIDS;
    listop->op_first = listop->op_last = last.subtrees[0];
    listop->op_first->op_sibling = NULL;
    listop->execution_engine = execution_engine;

    for (size_t i = 1, max = last.subtrees.size(); i < max; ++i) {
      OP *sibling = last.subtrees[i];

      detach_tree(sibling, KEEP_OPS);
      _jit_clear_op_next(sibling);

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
  op->op_targ = root->get_perl_op()->op_targ;
  op->op_next = _jit_find_op_next(root->get_perl_op());

  emitter_states.pop_back();
  if (emitter_states.size()) {
    pa.set_current_function(emitter_states.back().function);
    MY_CXT.builder.SetInsertPoint(emitter_states.back().insertion_point);
  }

  return op;
}

void
Emitter::_push_empty_function()
{
  if (emitter_states.size()) {
    emitter_states.back().insertion_point = MY_CXT.builder.GetInsertBlock();
  }
  Function *f = Function::Create(pa.ppaddr_type(), GlobalValue::ExternalLinkage, "", module);
  BasicBlock *bb = BasicBlock::Create(module->getContext(), "entry", f);

  pa.set_current_function(f);
  MY_CXT.builder.SetInsertPoint(bb);

  emitter_states.push_back(FunctionState());
  emitter_states.back().function = f;
  emitter_states.back().insertion_point = bb;
}

void
Emitter::_pop_empty_function()
{
  emitter_states.back().function->eraseFromParent();
  emitter_states.pop_back();
  if (emitter_states.size()) {
    pa.set_current_function(emitter_states.back().function);
    MY_CXT.builder.SetInsertPoint(emitter_states.back().insertion_point);
  }
}

BasicBlock *
Emitter::_create_basic_block()
{
  return BasicBlock::Create(module->getContext(), "", emitter_states.back().function);
}

bool
Emitter::_jit_emit_optree_args(State *state)
{
  _push_empty_function();

  for (unsigned i = 0; i < state->args.size(); ++i) {
    State *arg = state->args[i];
    reduce(arg);
    if (arg->result.is_invalid())
      return false;
    if (arg->result.value != OPTREE) {
      _emit_current_function_and_push_empty(arg->node.term, arg->node.term, arg->result);
    }
  }

  _pop_empty_function();

  return true;
}

EmitValue
Emitter::_jit_emit_root_optree(State *state)
{
  if (!_jit_emit_optree_args(state))
    return EmitValue::invalid();

  return EmitValue(OPTREE, &SCALAR_T);
}

EmitValue
Emitter::_jit_emit_optree(State *state)
{
  if (!_jit_emit_optree_args(state))
    return EmitValue::invalid();

  Term *ast = state->node.term;
  // unfortunately there is (currently) no way to clone an optree,
  // so just detach the ops from the root tree
  emitter_states.back().subtrees.push_back(ast->get_perl_op());

  pa.emit_call_runloop(ast->start_op());

  if (ast->context() == pj_context_caller) {
    set_error("Caller-determined context not implemented");
    return EmitValue::invalid();
  }
  if (ast->context() != pj_context_scalar)
    return EmitValue(NULL, NULL);

  pa.alloc_sp();
  pa.emit_SPAGAIN();
  llvm::Value *res = pa.emit_POPs();
  pa.emit_PUTBACK();

  return EmitValue(res, &SCALAR_T);
}

OP *
Emitter::_jit_find_op_next(OP *op)
{
  // in most cases, the exit point for an optree is the op_next
  // pointer of the root op, but conditional operators have interesting
  // control flows
  switch (op->op_type) {
  case OP_COND_EXPR: {
    LOGOP *logop = cLOGOPx(op);

    return _jit_find_op_next(logop->op_first->op_sibling);
  }
  default:
    return op->op_next;
  }
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
    if (op->get_op_type() == pj_binop_sassign) {
      if (type->equals(&SCALAR_T)) {
        res = value;
      } else {
        // TODO suboptimal, but correct, need to ask for SCALAR
        // in the call to _jit_emit() above
        res = pa.emit_sv_newmortal();
      }
    } else {
      if (!op->get_perl_op()->op_targ && type->equals(&SCALAR_T)) {
        res = value;
      } else {
        if (!op->get_perl_op()->op_targ) {
          set_error("Binary OP without target");
          return false;
        }
        res = pa.emit_OP_targ();
      }
    }
  }
    break;
  case pj_opc_unop:
    if (!op->get_perl_op()->op_targ) {
      set_error("Unary OP without target");
      return false;
    }
    res = pa.emit_OP_targ();
    break;
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

llvm::Value *
Emitter::_jit_emit_perl_int(llvm::Value *v)
{
  return MY_CXT.builder.CreateFPToSI(v, pa.IV_type());
}

EmitValue
Emitter::_jit_emit_unop(Unop *ast, const EmitValue &v, const PerlJIT::AST::Type *type)
{
  if (v.is_invalid())
    return EmitValue::invalid();

  Value *vv = _to_nv_value(v.value, v.type);

  if (!vv)
    return EmitValue::invalid();

  Value *res = NULL;

  switch (ast->get_op_type()) {
  case pj_unop_perl_int:
    return EmitValue(_jit_emit_perl_int(vv), &INT_T);
  default:
    return EmitValue::invalid();
  }
}

EmitValue
Emitter::_jit_emit_binop(Binop *ast, const EmitValue &lv, const EmitValue &rv, const PerlJIT::AST::Type *type)
{
  if (lv.is_invalid() || rv.is_invalid())
    return EmitValue::invalid();

  Value *lvv = _to_nv_value(lv.value, lv.type),
        *rvv = _to_nv_value(rv.value, rv.type);

  if (!lvv || !rvv)
    return EmitValue::invalid();

  Value *res = NULL;

  switch (ast->get_op_type()) {
  case pj_binop_add:
    res = MY_CXT.builder.CreateFAdd(lvv, rvv);
    break;
  case pj_binop_multiply:
    res = MY_CXT.builder.CreateFMul(lvv, rvv);
    break;
  default:
    return EmitValue::invalid();
  }

  if (ast->is_assignment_form()) {
    // TODO proper LVALUE treatment
    if (!lv.type->equals(&SCALAR_T)) {
      set_error("Can only assign to perl scalars, got a " + lv.type->to_string());
      return EmitValue::invalid();
    }
    if (!_jit_assign_sv(lv.value, res, &DOUBLE_T))
      return EmitValue::invalid();
    res = lv.value;
  }

  return EmitValue(res, &DOUBLE_T);
}

EmitValue
Emitter::_jit_emit_logop(PerlJIT::AST::Binop *ast, State *l, State *r, const PerlJIT::AST::Type *type)
{
  Value *res = pa.alloc_variable(_map_to_llvm_type(type), "logop_res");
  BasicBlock *rhs = _create_basic_block(), *end = _create_basic_block();

  // evaluate LHS
  reduce(l);
  MY_CXT.builder.CreateStore(_to_type_value(l->result.value, l->result.type, type), res);
  Value *cond = _to_bool_value(l->result.value, l->result.type);

  if (ast->get_op_type() == pj_binop_bool_and)
    // for && jump to end if false, evaluate RHS if true
    MY_CXT.builder.CreateCondBr(cond, rhs, end);
  else
    // for || jump to end if true, evaluate RHS if false
    MY_CXT.builder.CreateCondBr(cond, end, rhs);

  // evaluate RHS and jump to end
  MY_CXT.builder.SetInsertPoint(rhs);
  reduce(r);
  MY_CXT.builder.CreateStore(_to_type_value(r->result.value, r->result.type, type), res);
  MY_CXT.builder.CreateBr(end);

  MY_CXT.builder.SetInsertPoint(end);

  return EmitValue(MY_CXT.builder.CreateLoad(res), type);
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
  if (type->is_integer()) {
    if (type->is_unsigned())
      return MY_CXT.builder.CreateUIToFP(value, pa.NV_type());
    else
      return MY_CXT.builder.CreateSIToFP(value, pa.NV_type());
  }
  if (type->equals(&SCALAR_T) || type->equals(&UNSPECIFIED_T))
    return pa.emit_SvNV(value);

  set_error("Handle more NV coercion cases");
  return NULL;
}

Value *
Emitter::_to_bool_value(Value *value, const PerlJIT::AST::Type *type)
{
  if (type->equals(&DOUBLE_T))
    return MY_CXT.builder.CreateFCmpOEQ(value, pa.NV_constant(0.0));
  if (type->is_integer())
    return MY_CXT.builder.CreateICmpEQ(value, pa.IV_constant(0));
  if (type->is_xv())
    return pa.emit_SvTRUE(value);

  set_error("Handle more bool coercion cases");
  return NULL;
}

Value *
Emitter::_to_type_value(Value *value, const PerlJIT::AST::Type *type, const PerlJIT::AST::Type *target)
{
  if (type->equals(target))
    return value;
  if (target->equals(&DOUBLE_T))
    return _to_nv_value(value, type);

  set_error("Handle more coercion cases");
  return NULL;
}

llvm::Type *
Emitter::_map_to_llvm_type(const PerlJIT::AST::Type *type)
{
  if (type->equals(&DOUBLE_T))
    return pa.NV_type();
  if (type->equals(&INT_T))
    return pa.IV_type();
  if (type->equals(&UNSIGNED_INT_T))
    return pa.UV_type();

  return pa.SV_ptr_type();
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
Emitter::replace_sequence(OP *first, OP *last, OP *op, int flags)
{
  dSP;

  PUSHMARK(SP);
  EXTEND(SP, 5);
  PUSHs(newRV_inc((SV *) cv));
  PUSHs(op_2sv(aTHX_ first));
  PUSHs(op_2sv(aTHX_ last));
  PUSHs(op_2sv(aTHX_ op));
  PUSHs(newSViv(flags));
  PUTBACK;

  call_pv("B::Replace::replace_sequence", G_VOID|G_DISCARD);
}

void
Emitter::detach_tree(OP *op, int flags)
{
  dSP;

  PUSHMARK(SP);
  EXTEND(SP, 3);
  PUSHs(newRV_inc((SV *) cv));
  PUSHs(op_2sv(aTHX_ op));
  PUSHs(newSViv(flags));
  PUTBACK;

  call_pv("B::Replace::detach_tree", G_VOID|G_DISCARD);
}
