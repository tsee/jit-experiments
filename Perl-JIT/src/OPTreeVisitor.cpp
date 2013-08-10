#include "OPTreeVisitor.h"
#include "ppport.h"
#include "pj_debug.h"

#include <list>

/* inspired by B.xs */
#define PMOP_pmreplstart(o)	o->op_pmstashstartu.op_pmreplstart
#define PMOP_pmreplroot(o)	o->op_pmreplrootu.op_pmreplroot

using namespace PerlJIT;

void
OPTreeVisitor::visit(pTHX_ OP *o, OP *parentop)
{
  visit_control_t status;
  OP *kid;
  std::list<OP *> backlog;

  backlog.push_back(parentop);
  backlog.push_back(o);

  // Iterative tree traversal using stack
  while ( !backlog.empty() ) {
    o = backlog.back();
    backlog.pop_back();
    parentop = backlog.back();
    backlog.pop_back();

    status = this->visit_op(aTHX_ o, parentop);
    assert(status == VISIT_CONT || status == VISIT_ABORT || status == VISIT_SKIP);

    switch (status) {
    case VISIT_CONT:
      // "Recurse": Put kids on stack
      if (o && (o->op_flags & OPf_KIDS)) {
        for (kid = ((UNOP*)o)->op_first; kid; kid = kid->op_sibling) {
          backlog.push_back(o); // parent for kid
          backlog.push_back(kid);
        }
      }

      if (o && OP_CLASS(o) == OA_PMOP && o->op_type != OP_PUSHRE
            && (kid = PMOP_pmreplroot(cPMOPo)))
      {
        backlog.push_back(o); /* parent for kid */
        backlog.push_back(kid);
      }
      break;
    case VISIT_SKIP:
      // fall-through, do not recurse into this OP's kids
      break;
    case VISIT_ABORT:
      goto done;
    default:
      abort();
    }
  } // end while stuff on todo stack

done:
  return;
} // end 'OPTreeVisitor::visit_op'

