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
  typedef std::tr1::unordered_map<std::string, LoopCtlStatementList> LoopCtlIndex;

  class LoopCtlTracker {
  public:
    LoopCtlTracker();

    void add_loop_control_node(AST::LoopControlStatement *ctrl_term);
    void add_jump_target_to_loop_control_nodes(pTHX_ AST::Statement *stmt);
    const LoopCtlIndex &get_loop_control_index() const;

  private:
    // label => control Terms; Keep track of next/redo/last label being "" means "none"
    LoopCtlIndex loop_control_index;
  };
}

#endif
