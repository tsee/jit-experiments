#include "LoopCtlTracker.h"
#include "ppport.h"
#include "pj_debug.h"

using namespace PerlJIT;
using namespace std;

LoopCtlTracker::LoopCtlTracker()
{
}

void
LoopCtlTracker::add_loop_control_node(AST::LoopControlStatement *ctrl_term)
{
  loop_control_index[ctrl_term->get_label()].push_back(ctrl_term);
}

const LoopCtlIndex &
LoopCtlTracker::get_loop_control_index() const
{ return loop_control_index; }

void
LoopCtlTracker::add_jump_target_to_loop_control_nodes(pTHX_ AST::Statement *stmt)
{
  assert(stmt->get_kids().size() == 1);
  AST::Term *maybe_loop = stmt->get_kids()[0];
  pj_term_type t = maybe_loop->get_type();
  if (   t == pj_ttype_bareblock || t == pj_ttype_while
      || t == pj_ttype_for       || t == pj_ttype_foreach)
  {
    // Houston we have a loop. It's the target for control
    // statements in one of two conditions:
    // a) There's an explicit label on the loop-preceding nextstate
    // b) We have control statements without a label that point here
    OP *nextstate_op = stmt->get_perl_op();

    // Handle b)
    LoopCtlStatementList &unlabeled = loop_control_index[""];
    if (!unlabeled.empty()) {
      LoopCtlStatementList::iterator it;
      for (it = unlabeled.begin(); it != unlabeled.end(); ++it)
        (*it)->set_jump_target(maybe_loop);
    }
    loop_control_index.erase("");

    // Handle a)
    // *sigh* about the strlen dance
    STRLEN cop_label_len;
#ifdef CopLABEL_len
    const char *cop_label = CopLABEL_len((COP *)nextstate_op, &cop_label_len);
#else
    const char *cop_label = CopLABEL((COP *)nextstate_op);
    if (cop_label)
      cop_label_len = strlen(cop_label);
#endif
    if (cop_label) {
      string clstr(cop_label, cop_label_len);
      LoopCtlStatementList &labeled = loop_control_index[clstr];
      LoopCtlStatementList::iterator it;
      for (it = labeled.begin(); it != labeled.end(); ++it)
        (*it)->set_jump_target(maybe_loop);
      loop_control_index.erase(clstr);
    }
  }
}

