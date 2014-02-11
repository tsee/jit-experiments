#include "pj_perlapi.h"

#include <llvm/IR/Constants.h>

using namespace PerlJIT;
using namespace llvm;

#ifndef USE_ITHREADS
#error "TODO implement me"
#endif

#define OFF(STRUCT, NAME) PTR2IV(&((STRUCT *)0)->NAME)
#define IOFF(NAME) PTR2IV(&((PerlInterpreter *)0)->I##NAME)
#define jit_tTHX  ptr_type
#define jit_tTHX_ ptr_type,
#define jit_aTHX  thx
#define jit_aTHX_ thx,

static void
_pa_sv_setsv_nosteal(pTHX_ SV *dsv, SV *ssv)
{
  SvSetSV_nosteal(dsv, ssv);
}

static void
_pa_push_sv(pTHX_ SV *sv)
{
  dSP;
  XPUSHs(sv);
  PUTBACK;
}

static NV
_pa_sv_nv(pTHX_ SV *sv)
{
  return SvNV(sv);
}

static SV *
_pa_pop_sv(pTHX)
{
  dSP;
  SV *sv = POPs;
  PUTBACK;
  return sv;
}

static void
_pa_call_runloop(pTHX_ OP *op)
{
  OP *oldop = PL_op;
  PL_op = op;
  CALLRUNOPS(aTHX);
  PL_op = oldop;
}

PerlAPI::PerlAPI(Module *_module, IRBuilder<> *_builder, ExecutionEngine *ee) :
  module(_module), builder(_builder)
{
  llvm::Type *void_type = Type::getVoidTy(module->getContext());

  byte_type = IntegerType::get(module->getContext(), 8);
  iv_type = IntegerType::get(module->getContext(), sizeof(IV) * 8);
  uv_type = IntegerType::get(module->getContext(), sizeof(UV) * 8);
  nv_type = Type::getDoubleTy(module->getContext());
  ptr_type = byte_type->getPointerTo();
  ptr_ptr_type = byte_type->getPointerTo()->getPointerTo();
  ptr_iv_type = iv_type->getPointerTo();
  ptr_uv_type = uv_type->getPointerTo();
  ptr_nv_type = nv_type->getPointerTo();
  pp_type = function_type(ptr_type, jit_tTHX, NULL);

  // XXX only once per module
  // TODO autogenerate
  pa_sv_setnv = Function::Create(
      function_type(void_type, jit_tTHX_ ptr_type, nv_type, NULL),
      GlobalValue::ExternalLinkage, "Perl_sv_setnv", module);
  pa_sv_nv = Function::Create(
      function_type(nv_type, jit_tTHX_ ptr_type, NULL),
      GlobalValue::ExternalLinkage, "_pa_sv_nv", module);
  pa_sv_setiv = Function::Create(
      function_type(void_type, jit_tTHX_ ptr_type, iv_type, NULL),
      GlobalValue::ExternalLinkage, "Perl_sv_setiv", module);
  pa_sv_setsv_nosteal = Function::Create(
      function_type(void_type, jit_tTHX_ ptr_type, ptr_type, NULL),
      GlobalValue::ExternalLinkage, "_pa_sv_setsv_nosteal", module);
  pa_sv_newmortal = Function::Create(
      function_type(ptr_type, jit_tTHX_ NULL),
      GlobalValue::ExternalLinkage, "Perl_sv_newmortal", module);
  pa_push_sv = Function::Create(
      function_type(void_type, jit_tTHX_ ptr_type, NULL),
      GlobalValue::ExternalLinkage, "_pa_push_sv", module);
  pa_pop_sv = Function::Create(
      function_type(ptr_type, jit_tTHX_ NULL),
      GlobalValue::ExternalLinkage, "_pa_pop_sv", module);
  pa_call_runloop = Function::Create(
      function_type(void_type, jit_tTHX_ ptr_type, NULL),
      GlobalValue::ExternalLinkage, "_pa_call_runloop", module);
  pa_save_clearsv = Function::Create(
      function_type(ptr_type, jit_tTHX_ ptr_ptr_type, NULL),
      GlobalValue::ExternalLinkage, "Perl_save_clearsv", module);

  // XXX only once
  ee->addGlobalMapping(pa_sv_nv, (void *) _pa_sv_nv);
  ee->addGlobalMapping(pa_sv_setsv_nosteal, (void *) _pa_sv_setsv_nosteal);
  ee->addGlobalMapping(pa_push_sv, (void *) _pa_push_sv);
  ee->addGlobalMapping(pa_pop_sv, (void *) _pa_pop_sv);
  ee->addGlobalMapping(pa_call_runloop, (void *) _pa_call_runloop);
}

void
PerlAPI::set_thx(Value *_thx)
{
  thx = builder->CreatePointerCast(_thx, ptr_type);
}

void
PerlAPI::push_sv(Value *value)
{
  builder->CreateCall2(pa_push_sv, jit_aTHX_ value);
}

Value *
PerlAPI::pop_sv()
{
  return builder->CreateCall(pa_pop_sv, jit_aTHX);
}

void
PerlAPI::call_runloop(OP *op)
{
  builder->CreateCall2(pa_call_runloop, jit_aTHX_ UV_constant(PTR2UV(op)));
}

void
PerlAPI::save_clearsv(Value *svp)
{
  builder->CreateCall2(pa_save_clearsv, jit_aTHX_ svp);
}

void
PerlAPI::sv_set_nv(Value *sv, Value *value)
{
  builder->CreateCall3(pa_sv_setnv, jit_aTHX_ sv, value);
}

Value*
PerlAPI::sv_nv(Value *sv)
{
  return builder->CreateCall2(pa_sv_nv, jit_aTHX_ sv);
}

void
PerlAPI::sv_set_iv(Value *sv, Value *value)
{
  builder->CreateCall3(pa_sv_setiv, jit_aTHX_ sv, value);
}

void
PerlAPI::sv_set_sv_nosteal(Value *sv, Value *value)
{
  builder->CreateCall3(pa_sv_setsv_nosteal, jit_aTHX_ sv, value);
}

Value *
PerlAPI::get_targ()
{
  Value *pl_op = interp_value(IOFF(op), ptr_type, "PL_op");
  Value *targ_ptr = builder->CreatePointerCast(builder->CreateConstInBoundsGEP1_32(pl_op, OFF(OP, op_targ)), ptr_iv_type);
  Value *targ = builder->CreateLoad(targ_ptr);
  Value *pl_curpad = interp_value(IOFF(curpad), ptr_ptr_type, "PL_curpad");
  std::vector<Value *> idx(1, targ);

  return builder->CreateLoad(builder->CreateInBoundsGEP(pl_curpad, idx), "targ");
}

Value *
PerlAPI::get_op_next()
{
  Value *pl_op = interp_value(IOFF(op), ptr_type, "PL_op");
  Value *next_ptr = builder->CreatePointerCast(builder->CreateConstInBoundsGEP1_32(pl_op, OFF(OP, op_next)), ptr_ptr_type);

  return builder->CreateLoad(next_ptr, "op_next");
}

Value *
PerlAPI::new_mortal_sv()
{
  return builder->CreateCall(pa_sv_newmortal, jit_aTHX);
}

Value *
PerlAPI::get_pad_sv(UV padix)
{
  Value *pl_curpad = interp_value(IOFF(curpad), ptr_ptr_type, "PL_curpad");
  std::vector<Value *> idx(1, UV_constant(padix));

  return builder->CreateLoad(builder->CreateInBoundsGEP(pl_curpad, idx), "padsv");
}

Value *
PerlAPI::get_pad_sv_address(UV padix)
{
  Value *pl_curpad = interp_value(IOFF(curpad), ptr_ptr_type, "PL_curpad");

  return builder->CreateConstInBoundsGEP1_32(pl_curpad, padix);
}

Constant *
PerlAPI::IV_constant(IV value)
{
  return ConstantInt::get(module->getContext(), APInt(sizeof(IV) * 8, value, true));
}

Constant *
PerlAPI::UV_constant(UV value)
{
  return ConstantInt::get(module->getContext(), APInt(sizeof(UV) * 8, value, false));
}

Constant *
PerlAPI::NV_constant(NV value)
{
  return ConstantFP::get(module->getContext(), APFloat(value));
}

Value *
PerlAPI::interp_value(unsigned int offset, llvm::Type *type, const llvm::Twine& name)
{
  Value *addr = builder->CreatePointerCast(builder->CreateConstInBoundsGEP1_32(thx, offset), type->getPointerTo());
  return builder->CreateLoad(addr, name);
}

llvm::FunctionType *
PerlAPI::function_type(llvm::Type *ret, ...)
{
  va_list args;
  va_start(args, ret);
  std::vector<llvm::Type *> types;

  for (llvm::Type *t; t = va_arg(args, llvm::Type*); )
    types.push_back(t);

  va_end(args);

  return FunctionType::get(ret, types, false);
}
