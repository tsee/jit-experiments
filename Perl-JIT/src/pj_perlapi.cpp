#include "pj_perlapi.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/ValueSymbolTable.h>

using namespace PerlJIT;
using namespace llvm;

#define OFF(STRUCT, NAME) PTR2IV(&((STRUCT *)0)->NAME)
#define IOFF(NAME) PTR2IV(&((PerlInterpreter *)0)->I##NAME)
#define jit_tTHX  interpreter_type
#define jit_tTHX_ interpreter_type,
#define jit_aTHX  arg_thx
#define jit_aTHX_ arg_thx,

static void
_pa_call_runloop(pTHX_ OP *op)
{
  OP *oldop = PL_op;
  PL_op = op;
  CALLRUNOPS(aTHX);
  PL_op = oldop;
}

PerlAPI::PerlAPI(Module *_module, IRBuilder<> *_builder, ExecutionEngine *ee) :
  PerlAPIBase(_module, _builder)
{
  llvm::Type *void_type = Type::getVoidTy(module->getContext());

  iv_type = llvm::Type::getIntNTy(module->getContext(), sizeof(IV) * 8);
  i32_type = llvm::Type::getIntNTy(module->getContext(), sizeof(I32) * 8);
  uv_type = llvm::Type::getIntNTy(module->getContext(), sizeof(UV) * 8);
  nv_type = llvm::Type::getDoubleTy(module->getContext());
  bool_type = llvm::Type::getIntNTy(module->getContext(), 1);

  ptr_op_type = module->getTypeByName("struct.op")->getPointerTo();
  interpreter_type = module->getTypeByName("struct.interpreter")->getPointerTo();
  ptr_sv_type = module->getTypeByName("struct.sv")->getPointerTo();
  ptr_av_type = module->getTypeByName("struct.av")->getPointerTo();
  ptr_hv_type = module->getTypeByName("struct.hv")->getPointerTo();

  ptr_type = IntegerType::get(module->getContext(), 8)->getPointerTo();
  ptr_ptr_sv_type = ptr_sv_type->getPointerTo();
  pp_type = function_type(false, ptr_op_type, jit_tTHX_ NULL);

  // TODO autogenerate
  pa_croak = Function::Create(
      function_type(true, void_type, jit_tTHX_ ptr_type, NULL),
      GlobalValue::ExternalLinkage, "Perl_croak", module);
  pa_call_runloop = Function::Create(
      function_type(false, void_type, jit_tTHX_ ptr_op_type, NULL),
      GlobalValue::ExternalLinkage, "_pa_call_runloop", module);
  ee->addGlobalMapping(pa_call_runloop, (void *) _pa_call_runloop);
}

void
PerlAPI::alloc_sp()
{
  if (!arg_sp)
    arg_sp = function->getValueSymbolTable().lookup("sp");
  if (!arg_sp)
    arg_sp = alloc_variable(ptr_ptr_sv_type, "sp");
}

void
PerlAPI::emit_call_runloop(OP *op)
{
  builder->CreateCall2(
    pa_call_runloop, jit_aTHX_
    builder->CreateIntToPtr(UV_constant(PTR2IV(op)), ptr_op_type)
  );
}

void
PerlAPI::emit_nextstate(OP *op)
{
  return emit_pp_nextstate(
    builder->CreateIntToPtr(UV_constant(PTR2IV(op)), ptr_op_type)
  );
}

void
PerlAPI::emit_croak(Value *format, ...)
{
  va_list args;
  va_start(args, format);
  std::vector<Value *> params;

  params.push_back(jit_aTHX);
  params.push_back(format);

  for (llvm::Value *v; (v = va_arg(args, llvm::Value*)); )
    params.push_back(v);

  va_end(args);

  builder->CreateCall(pa_croak, params);
}

Value *
PerlAPI::emit_pad_sv(UV padix)
{
  return emit_PAD_SV(UV_constant(padix));
}

Value *
PerlAPI::emit_pad_sv_address(UV padix)
{
  return emit_PAD_SV_address(UV_constant(padix));
}

Constant *
PerlAPI::IV_constant(IV value)
{
  return ConstantInt::get(module->getContext(), APInt(sizeof(IV) * 8, value, true));
}

Constant *
PerlAPI::I32_constant(I32 value)
{
  return ConstantInt::get(module->getContext(), APInt(sizeof(I32) * 8, value, true));
}

Constant *
PerlAPI::BOOL_constant(bool value)
{
  return ConstantInt::get(module->getContext(), APInt(sizeof(bool) * 8, value, false));
}

Constant *
PerlAPI::UV_constant(UV value)
{
  return ConstantInt::get(module->getContext(), APInt(sizeof(UV) * 8, value, false));
}

Constant *
PerlAPI::U32_constant(UV value)
{
  return ConstantInt::get(module->getContext(), APInt(sizeof(U32) * 8, value, false));
}

Constant *
PerlAPI::NV_constant(NV value)
{
  return ConstantFP::get(module->getContext(), APFloat(value));
}

Value *
PerlAPI::interp_value(unsigned int offset, llvm::Type *type, const llvm::Twine& name)
{
  Value *addr = builder->CreatePointerCast(
    builder->CreateConstInBoundsGEP1_32(
      builder->CreatePointerCast(arg_thx, ptr_type),
      offset),
    type->getPointerTo());
  return builder->CreateLoad(addr, name);
}

llvm::FunctionType *
PerlAPI::function_type(bool is_vararg, llvm::Type *ret, ...)
{
  va_list args;
  va_start(args, ret);
  std::vector<llvm::Type *> types;

  for (llvm::Type *t; (t = va_arg(args, llvm::Type*)); )
    types.push_back(t);

  va_end(args);

  return FunctionType::get(ret, types, is_vararg);
}
