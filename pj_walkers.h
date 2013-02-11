#ifndef PJ_WALKERS_H_
#define PJ_WALKERS_H_

#include <pj_terms.h>

void pj_tree_extract_vars(pj_term_t *term, pj_variable_t * **vars, unsigned int *nvars);

pj_basic_type pj_tree_determine_funtype(pj_term_t *term);

#endif
