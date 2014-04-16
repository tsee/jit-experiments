#ifndef PJ_LOOPCTLTRACKER_H_
#define PJ_LOOPCTLTRACKER_H_

#include "EXTERN.h"
#include "perl.h"

#include "pj_ast_terms.h"

#include <list>
#include <string>
#include <tr1/unordered_map>

namespace PerlJIT {

  typedef std::list<AST::LoopControlStatement *> LoopCtlStatementList;
  typedef std::list<LoopCtlStatementList> LoopCtlScopeStack;
  typedef std::tr1::unordered_map<std::string, LoopCtlScopeStack> LoopCtlIndex;

  class LoopCtlTracker {
  public:
    LoopCtlTracker();

    static std::string get_label_from_nextstate(pTHX_ OP *nextstate_op);

    void push_loop_scope(const std::string &label);
    void add_loop_control_node(pTHX_ AST::LoopControlStatement *ctrl_term);
    void pop_loop_scope(pTHX_ const std::string &label, AST::Term *loop);

    const LoopCtlIndex &get_loop_control_index() const;

    void dump() const;

  private:
    LoopCtlIndex loop_control_index;
  };
}

#endif
