#include "pj_ast_map.h"
#include "pj_types.h"
#include "pj_codegen.h"

using namespace PerlJIT::AST;
using namespace PerlJIT;

CodegenNode
PerlJIT::map_codegen_arg(CodegenNode node, int functor_id, int index)
{
  switch (functor_id) {
  case Codegen::AstConst:
  case Codegen::AstLexicalDeclaration:
  case Codegen::AstLexical: {
    if (index == 0)
      return CodegenNode(node.term->get_value_type());
  }
    break;
  case Codegen::Optree: {
    std::vector<Term *> kids = node.term->get_kids();

    return CodegenNode(kids[index]);
  }
  default:
    abort();
  }
}

#define FUNCTOR(a, b) \
  *functor_id = (a); \
  *arity = (b); \
  *extra_arity = 0; \
  return

#define FUNCTOR_ARITY(a, b, c)   \
  *functor_id = (a); \
  *arity = (b); \
  *extra_arity = (c); \
  return

void
PerlJIT::map_codegen_functor(CodegenNode node, int *functor_id, int *arity, int *extra_arity)
{
  if (node.value_kind == CodegenNode::VALUE) {
    FUNCTOR(node.value, 0);
  } else if (node.value_kind == CodegenNode::TYPE) {
    PerlJIT::AST::Type *type = node.type;

    if (type->equals(&DOUBLE_T)) {
      FUNCTOR(Codegen::TypeDouble, 0);
    } else if (type->equals(&INT_T)) {
      FUNCTOR(Codegen::TypeInt, 0);
    } else if (type->equals(&UNSIGNED_INT_T)) {
      FUNCTOR(Codegen::TypeUnsignedInt, 0);
    } else if (type->equals(&UNSPECIFIED_T)) {
      FUNCTOR(Codegen::TypeAny, 0);
    } else if (type->equals(&OPAQUE_T)) {
      FUNCTOR(Codegen::TypeOpaque, 0);
    }

    abort();
  } else if (node.value_kind == CodegenNode::TERM) {
    PerlJIT::AST::Term *ast = node.term;

    switch (ast->get_type()) {
    case pj_ttype_constant:
      FUNCTOR(Codegen::AstConst, 1);
    case pj_ttype_lexical:
      FUNCTOR(Codegen::AstLexical, 1);
    case pj_ttype_variabledeclaration:
      FUNCTOR(Codegen::AstLexicalDeclaration, 1);
    default:
      FUNCTOR_ARITY(Codegen::Optree, 0, ast->get_kids().size());
    }
  }

  abort();
}
