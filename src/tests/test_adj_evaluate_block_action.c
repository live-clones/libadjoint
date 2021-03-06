#include "libadjoint/adj_adjointer_routines.h"
#include "libadjoint/adj_test_tools.h"
#include "libadjoint/adj_evaluation.h"
#include "libadjoint/adj_test_main.h"
#include <string.h>

#ifndef HAVE_PETSC
void test_adj_evaluate_block_action(void)
{
  adj_test_assert(1 == 1, "Don't have PETSc so can't run this test.");
}
#else
#include "libadjoint/adj_petsc_data_structures.h"
#include "libadjoint/adj_petsc.h"

void identity_action_callback(int ndepends, adj_variable* variables, adj_vector* dependencies, int hermitian, adj_scalar coefficient, adj_vector input, void* context, adj_vector* output);

void test_adj_evaluate_block_action(void)
{
  adj_adjointer adjointer;
  adj_block I;
  int ierr, dim=10;
  PetscReal norm;
  Vec input, output, difference;
  adj_vector adj_output;

  adj_create_adjointer(&adjointer);
  adj_set_petsc_data_callbacks(&adjointer);
  adj_register_operator_callback(&adjointer, ADJ_BLOCK_ACTION_CB, "IdentityOperator", (void (*)(void)) identity_action_callback);

  adj_create_block("IdentityOperator", NULL, NULL, 1.0, &I);
  VecCreateSeq(PETSC_COMM_SELF, dim, &input);
  VecSet(input, 1.0);

  strncpy(I.name, "NotTheIdentityOperator", 22);
  I.name[22]='\0';


  ierr = adj_evaluate_block_action(&adjointer, I, petsc_vec_to_adj_vector(&input), &adj_output);
  adj_test_assert(ierr!=ADJ_OK, "Should have not worked");

  strncpy(I.name, "IdentityOperator", 19);
  I.name[19]='\0';
  ierr = adj_block_set_test_hermitian(&I, ADJ_TRUE, 10, -10.0);
  adj_test_assert(ierr != ADJ_OK, "Should not have worked");
  ierr = adj_block_set_test_hermitian(&I, ADJ_TRUE, 10, 0.0);
  adj_test_assert(ierr == ADJ_OK, "Should have worked");

  ierr = adj_evaluate_block_action(&adjointer, I, petsc_vec_to_adj_vector(&input), &adj_output);
  adj_test_assert(ierr==ADJ_OK, "Should have worked");
 
  output = petsc_vec_from_adj_vector(adj_output);
  VecDuplicate(input, &difference);
  VecCopy(input, difference);
  VecAXPY(difference, -1.0, output);
  VecNorm(difference, NORM_2, &norm);
  adj_test_assert(norm == 0.0, "Norm should be zero");
}

void identity_action_callback(int ndepends, adj_variable* variables, adj_vector* dependencies, int hermitian, adj_scalar coefficient, adj_vector input, void* context, adj_vector* output)
{
  (void) hermitian;
  (void) context;
  (void) ndepends;
  (void) variables;
  (void) dependencies;
  Vec* xvec;
  xvec = (Vec*) malloc(sizeof(Vec));

  VecDuplicate(petsc_vec_from_adj_vector(input), xvec);
  VecCopy(petsc_vec_from_adj_vector(input), *xvec);
  VecScale(*xvec, (PetscScalar) coefficient);
  *output = petsc_vec_to_adj_vector(xvec);
}
#endif
