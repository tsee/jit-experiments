#include "pj_ast_map.h"
#include "pj_types.h"
#include "pj_codegen.h"

using namespace PerlJIT::AST;
using namespace PerlJIT;

#define PJ_ABORT(msg) { fprintf(stderr, msg); abort(); }

CodegenNode
PerlJIT::map_codegen_arg(CodegenNode node, int functor_id, int index)
{
  switch (functor_id) {
  case Codegen::AstBinop: {
    PerlJIT::AST::Binop *ast = static_cast<PerlJIT::AST::Binop *>(node.term);

    switch (index) {
    case 0:
      return CodegenNode(ast->get_op_type());
    case 1:
    case 2:
      return CodegenNode(ast->kids[index - 1]);
    }
  }
    break;
  case Codegen::AstUnop: {
    PerlJIT::AST::Unop *ast = static_cast<PerlJIT::AST::Unop *>(node.term);

    switch (index) {
    case 0:
      return CodegenNode(ast->get_op_type());
    case 1:
      return CodegenNode(ast->kids[0]);
    }
  }
    break;
  case Codegen::AstListop: {
    PerlJIT::AST::Listop *ast = static_cast<PerlJIT::AST::Listop *>(node.term);

    switch (index) {
    case 0:
      return CodegenNode(ast->get_op_type());
    default:
      return CodegenNode(ast->kids[index - 1]);
    }
  }
    break;
  case Codegen::AstConst:
  case Codegen::AstLexicalDeclaration:
  case Codegen::AstLexical: {
    if (index == 0)
      return CodegenNode(node.term->get_value_type());
  }
    break;
  case Codegen::AstStatement: {
    PerlJIT::AST::Statement *ast = static_cast<PerlJIT::AST::Statement *>(node.term);

    if (index == 0)
      return CodegenNode(ast->kids[0]);
  }
    break;
  case Codegen::AstFor: {
    PerlJIT::AST::For *ast = static_cast<PerlJIT::AST::For *>(node.term);

    if (index == 0)
      return CodegenNode(ast->init);
    if (index == 1)
      return CodegenNode(ast->condition);
    if (index == 2)
      return CodegenNode(ast->step);
    if (index == 3)
      return CodegenNode(ast->body);
  }
    break;
  case Codegen::Optree: {
    std::vector<Term *> kids = node.term->get_kids();

    return CodegenNode(kids[index]);
  }
  default:
    PJ_ABORT("Missing mapping in map_codegen_arg\n");
  }

  PJ_ABORT("Reached the bottom of map_codegen_arg, this should not happen\n");
}

MappedFunctor
PerlJIT::map_codegen_functor(CodegenNode node)
{
  if (node.value_kind == CodegenNode::VALUE) {
    return MappedFunctor(node.value, 0);
  } else if (node.value_kind == CodegenNode::TYPE) {
    PerlJIT::AST::Type *type = node.type;

    if (type->equals(&DOUBLE_T)) {
      return MappedFunctor(Codegen::TypeDouble, 0);
    } else if (type->equals(&INT_T)) {
      return MappedFunctor(Codegen::TypeInt, 0);
    } else if (type->equals(&UNSIGNED_INT_T)) {
      return MappedFunctor(Codegen::TypeUnsignedInt, 0);
    } else if (type->equals(&UNSPECIFIED_T)) {
      return MappedFunctor(Codegen::TypeAny, 0);
    } else if (type->equals(&OPAQUE_T)) {
      return MappedFunctor(Codegen::TypeOpaque, 0);
    } else if (type->is_composite()) {
      // for now map composite types to unspecified types
      return MappedFunctor(Codegen::TypeAny, 0);
    }

    PJ_ABORT("Missing mapping for type in map_codegen_functor\n");
  } else if (node.value_kind == CodegenNode::TERM) {
    PerlJIT::AST::Term *ast = node.term;

    switch (ast->get_type()) {
    case pj_ttype_constant:
      return MappedFunctor(Codegen::AstConst, 1);
    case pj_ttype_lexical:
      return MappedFunctor(Codegen::AstLexical, 1);
    case pj_ttype_variabledeclaration:
      return MappedFunctor(Codegen::AstLexicalDeclaration, 1);
    case pj_ttype_op: {
      Op * op = static_cast<Op *>(ast);

      switch (op->get_op_type()) {
// includes the switch for all ops mentioned by emitter.txt
#include "pj_ast_map_ops-gen.inc"
      default:
        return MappedFunctor(Codegen::Optree, 0, ast->get_kids().size());
      }
    }
    case pj_ttype_statement:
      return MappedFunctor(Codegen::AstStatement, 1);
    case pj_ttype_for:
      return MappedFunctor(Codegen::AstFor, 4);
    default:
      return MappedFunctor(Codegen::Optree, 0, ast->get_kids().size());
    }
  }

  PJ_ABORT("Reached the bottom of map_codegen_functor, this should not happen\n");
}
