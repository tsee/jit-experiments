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
  uv_type = llvm::Type::getIntNTy(module->getContext(), sizeof(UV) * 8);
  nv_type = llvm::Type::getDoubleTy(module->getContext());

  ptr_op_type = module->getTypeByName("struct.op")->getPointerTo();
  interpreter_type = module->getTypeByName("struct.interpreter")->getPointerTo();
  ptr_sv_type = module->getTypeByName("struct.sv")->getPointerTo();

  ptr_type = IntegerType::get(module->getContext(), 8)->getPointerTo();
  ptr_ptr_sv_type = ptr_sv_type->getPointerTo();
  pp_type = function_type(ptr_op_type, jit_tTHX_ NULL);

  // TODO autogenerate
  pa_call_runloop = Function::Create(
      function_type(void_type, jit_tTHX_ ptr_op_type, NULL),
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
  Value *addr = builder->CreatePointerCast(
    builder->CreateConstInBoundsGEP1_32(
      builder->CreatePointerCast(arg_thx, ptr_type),
      offset),
    type->getPointerTo());
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
