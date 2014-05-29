#include "pj_ast_map.h"
#include "pj_types.h"
#include "pj_codegen.h"

using namespace PerlJIT::AST;
using namespace PerlJIT;

CodegenNode
PerlJIT::map_codegen_arg(CodegenNode node, int functor_id, int index)
{
  switch (functor_id) {
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

    abort();
  } else if (node.value_kind == CodegenNode::TERM) {
    PerlJIT::AST::Term *ast = node.term;

    switch (ast->get_type()) {
    default:
      FUNCTOR_ARITY(Codegen::Optree, 0, ast->get_kids().size());
    }
  }

  abort();
}
