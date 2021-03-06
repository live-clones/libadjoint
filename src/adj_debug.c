#include "libadjoint/adj_debug.h"

int adj_adjointer_check_consistency(adj_adjointer* adjointer)
{
  int ierr;
  adj_variable_hash* hash_ptr;

  hash_ptr = adjointer->varhash;
  while(hash_ptr != NULL)
  {
    adj_variable var = hash_ptr->variable;
    adj_variable_data* data_ptr = hash_ptr->data;

    if (var.auxiliary == ADJ_FALSE && data_ptr->equation < 0)
    {
      char buf[ADJ_NAME_LEN];
      adj_variable_str(var, buf, ADJ_NAME_LEN);
      ierr = ADJ_ERR_INVALID_INPUTS;
      snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "Variable %s is not auxiliary, but has no equation set for it.", buf);
      return adj_chkierr_auto(ierr);
    }

    hash_ptr = (adj_variable_hash*) hash_ptr->hh.next;
  }

  return ADJ_OK;
}

/* Loops over the checkpoint equations and checks that the required variable are recorded */
int adj_adjointer_check_checkpoints(adj_adjointer* adjointer)
{
  int cp_num, cp_iter;
  int* cp_eqns;
  int eqn_num, i, j, ierr;
  adj_equation eqn, eqn_tmp;

  if (adjointer->options[ADJ_ACTIVITY] == ADJ_ACTIVITY_NOTHING) return ADJ_OK;

  if (adjointer->revolve_data.verbose)
    printf("======= Revolve: starting checkpoint check =======\n");

  /* Query the checkpoint equations */
  cp_num = 0;
  /* First get the number of checkpoints */
  for (eqn_num=0; eqn_num < adjointer->nequations; eqn_num++)
  {
    eqn = adjointer->equations[eqn_num];
    if ((eqn.memory_checkpoint == ADJ_TRUE) ||
        (eqn.disk_checkpoint == ADJ_TRUE))
      cp_num++;
  }
  cp_eqns = (int*) malloc(sizeof(int)*cp_num);
  ADJ_CHKMALLOC(cp_eqns);
  /* Then get the equation number of each checkpoint */
  i=0;
  for (eqn_num=0; eqn_num < adjointer->nequations; eqn_num++)
  {
    eqn = adjointer->equations[eqn_num];
    if ((eqn.memory_checkpoint == ADJ_TRUE) ||
        (eqn.disk_checkpoint == ADJ_TRUE))
    {
      cp_eqns[i]=eqn_num;
      i++;
    }
  }

  /* Now loop over every checkpoint and ensure that all required variables are recorded */
  for (cp_iter=0; cp_iter < cp_num; cp_iter++)
  {
    int cp_eqn_num = cp_eqns[cp_iter];
    int eqn_num = cp_eqn_num;
    adj_variable_data* var_data_ptr;
    eqn = adjointer->equations[eqn_num];

    assert((eqn.memory_checkpoint == ADJ_TRUE) || (eqn.disk_checkpoint == ADJ_TRUE));
    if (adjointer->revolve_data.verbose)
    {
      if (eqn.memory_checkpoint == ADJ_TRUE)
        printf("Revolve: Equation %i is a memory checkpoint.\n", cp_eqns[cp_iter]);
      if (eqn.disk_checkpoint == ADJ_TRUE)
        printf("Revolve: Equation %i is a disk checkpoint.\n", cp_eqns[cp_iter]);
    }

    /* Now check for the checkpoint and all future equations that the variables that are computed before the checkpoint equation are recorded */
    for (; eqn_num < adjointer->nequations; eqn_num++)
    {
        eqn_tmp = adjointer->equations[eqn_num];

        /* Check that we have all the forward values we need to restart the simulation */
        for (i=0; i < eqn_tmp.nblocks; i++)
        {
          adj_variable fwd_var;

          /* Check that we have the nonlinear block dependencies */
          if (eqn_tmp.blocks[i].has_nonlinear_block)
          {
            adj_nonlinear_block nl_block;

            nl_block = eqn_tmp.blocks[i].nonlinear_block;
            for (j=0; j < nl_block.ndepends; j++)
            {
              fwd_var = nl_block.depends[j];
              ierr = adj_find_variable_data(&(adjointer->varhash), &fwd_var, &var_data_ptr);
              if (ierr != ADJ_OK)
              {
                char buf[255];
                adj_variable_str(fwd_var, buf, 255);
                snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "Internal error in adj_adjointer_check_checkpoints: No variable data for %s.", buf);
                return adj_chkierr_auto(ierr);
              }

              /* If the variable is computed after the checkpoint equation, we do not need to have it as a checkpoint variable */
              if (var_data_ptr->equation >= cp_eqn_num)
                  continue;

              if (eqn.memory_checkpoint == ADJ_TRUE)
                if ((adj_has_variable_value_memory(adjointer, fwd_var) != ADJ_OK) || (adj_is_variable_memory_checkpoint(adjointer, fwd_var) != ADJ_TRUE))
                {
                  char buf[255];
                  adj_variable_str(fwd_var, buf, 255);
                  snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "Need a memory checkpoint of variable %s, but don't have one.", buf);
                  return adj_chkierr_auto(ADJ_ERR_NEED_VALUE);
                }
              if (eqn.disk_checkpoint == ADJ_TRUE)
                if ((adj_has_variable_value_disk(adjointer, fwd_var) != ADJ_OK) || (adj_is_variable_disk_checkpoint(adjointer, fwd_var) != ADJ_TRUE))
                {
                  char buf[255];
                  adj_variable_str(fwd_var, buf, 255);
                  snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "Need a disk checkpoint of variable %s, but don't have one.", buf);
                  return adj_chkierr_auto(ADJ_ERR_NEED_VALUE);
                }
            }
          }

          /* Get the forward variable we want this to multiply */
          fwd_var = eqn_tmp.targets[i];
          if (adj_variable_equal(&eqn_tmp.variable, &fwd_var, 1)) continue; /* that term goes in the lhs */
          ierr = adj_find_variable_data(&(adjointer->varhash), &fwd_var, &var_data_ptr);
          if (ierr != ADJ_OK)
          {
            char buf[255];
            adj_variable_str(fwd_var, buf, 255);
            snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "Internal error in adj_adjointer_check_checkpoints: No variable data for %s.", buf);
            return adj_chkierr_auto(ierr);
          }
          /* If the variable is computed before the checkpoint equation, we need to have it as a checkpoint variable */
          if (var_data_ptr->equation < cp_eqn_num)
          {
              /* and now check it has the checkpoint value */
              if (eqn.memory_checkpoint == ADJ_TRUE)
                if ((adj_has_variable_value_memory(adjointer, fwd_var) != ADJ_OK) || (adj_is_variable_memory_checkpoint(adjointer, fwd_var) != ADJ_TRUE))
                {
                  char buf[255], varbuf[255];
                  adj_variable_str(eqn_tmp.rhsdeps[i], buf, 255);
                  adj_variable_str(eqn_tmp.variable, varbuf, 255);
                  snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "Need a memory checkpoint of variable %s for the equation for %s, but don't have one.", buf, varbuf);
                  return adj_chkierr_auto(ADJ_ERR_NEED_VALUE);
                }
              if (eqn.disk_checkpoint == ADJ_TRUE)
                if ((adj_has_variable_value_disk(adjointer, fwd_var) != ADJ_OK) || (adj_is_variable_disk_checkpoint(adjointer, fwd_var) != ADJ_TRUE))
                {
                  char buf[255], varbuf[255];
                  adj_variable_str(eqn_tmp.rhsdeps[i], buf, 255);
                  adj_variable_str(eqn_tmp.variable, varbuf, 255);
                  snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "Need a disk checkpoint of variable %s for the equation for %s, but don't have one.", buf, varbuf);
                  return adj_chkierr_auto(ADJ_ERR_NEED_VALUE);
                }
          }
        }

        for (i = 0; i < eqn_tmp.nrhsdeps; i++)
        {
          /* Ignore self-dependency of nonlinear equations */
          if (adj_variable_equal(&eqn_tmp.rhsdeps[i], &eqn_tmp.variable, 1))
            continue;
          ierr = adj_find_variable_data(&(adjointer->varhash), &eqn_tmp.rhsdeps[i], &var_data_ptr);
          if (ierr != ADJ_OK)
          {
            char buf[255];
            adj_variable_str(eqn_tmp.rhsdeps[i], buf, 255);
            snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "Internal error in adj_adjointer_check_checkpoints: No variable data for %s.", buf);
            return adj_chkierr_auto(ierr);
          }
          /* If the variable is computed after the checkpoint equation, we do not need to have it as a checkpoint variable */
          if (var_data_ptr->equation >= cp_eqn_num)
              continue;

          if (eqn.memory_checkpoint == ADJ_TRUE)
            if ((adj_has_variable_value_memory(adjointer, eqn_tmp.rhsdeps[i]) != ADJ_OK) || (adj_is_variable_memory_checkpoint(adjointer, eqn_tmp.rhsdeps[i]) != ADJ_TRUE))
            {
              char buf[255], varbuf[255];
              adj_variable_str(eqn_tmp.rhsdeps[i], buf, 255);
              adj_variable_str(eqn_tmp.variable, varbuf, 255);
              snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "Need a memory checkpoint of variable %s for the equation for %s, but don't have one.", buf, varbuf);
              return adj_chkierr_auto(ADJ_ERR_NEED_VALUE);
            }
          if (eqn.disk_checkpoint == ADJ_TRUE)
            if ((adj_has_variable_value_disk(adjointer, eqn_tmp.rhsdeps[i]) != ADJ_OK) || (adj_is_variable_disk_checkpoint(adjointer, eqn_tmp.rhsdeps[i]) != ADJ_TRUE))
            {
              char buf[255], varbuf[255];
              adj_variable_str(eqn_tmp.rhsdeps[i], buf, 255);
              adj_variable_str(eqn_tmp.variable, varbuf, 255);
              snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "Need a disk checkpoint of variable %s for the equation for %s, but don't have one.", buf, varbuf);
              return adj_chkierr_auto(ADJ_ERR_NEED_VALUE);
            }
        }
    }
  }

  if (cp_num>0)
  {
    adj_variable_hash* varhash;
    adj_variable_data* data;

    /* The first checkpoint equation must be zero, otherwise */
    /* we couldn't replay the whole forward system */
    if(cp_eqns[0] != 0)
    {
      snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "Internal error: the first equation must be a checkpoint equation but it is not.");
      return adj_chkierr_auto(ADJ_ERR_REVOLVE_ERROR);
    }

    /* Check we have the required variables for the adjoint equations */
    for(varhash = adjointer->varhash; varhash != NULL; varhash = (adj_variable_hash*) varhash->hh.next)
    {
    	data = varhash->data;
      /* We are only interested in forward variables */
      if (data->equation < 0)
        continue;

      /* Find out in which checkpoint slot the variable is computed */
      for (cp_iter=0; cp_iter < cp_num-1; cp_iter++)
      {
        if (data->equation < cp_eqns[cp_iter+1])
          break;
      }

      /* Check for dependencies of the functional right-hand-sides */
      if (data->ndepending_timesteps > 0)
      {
        int start_equation, max_timestep;
        max_timestep = adj_minval(data->depending_timesteps, data->ndepending_timesteps);
        ierr = adj_timestep_start_equation(adjointer, max_timestep, &start_equation);
        assert(ierr == ADJ_OK);

        if (cp_eqns[cp_iter] > start_equation)
        {
          adj_variable var = adjointer->equations[data->equation].variable;

          if (adj_has_variable_value(adjointer, var) != ADJ_OK)
          {
            char buf[255];
            adj_variable_str(var, buf, 255);
            snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "Need a checkpoint of variable %s (functional dependency), but don't have one.", buf);
            return adj_chkierr_auto(ADJ_ERR_NEED_VALUE);
          }
        }
      }

    }
  }

  free(cp_eqns);

  if (adjointer->revolve_data.verbose)
    printf("Revolve: checkpoint check passed.\n");

  return ADJ_OK;
}

int adj_test_block_action_transpose(adj_adjointer* adjointer, adj_block block, adj_vector model_input, adj_vector model_output, int N, adj_scalar tol)
{
  int i;
  int ierr;
  void (*block_action_func)(int, adj_variable*, adj_vector*, int, adj_scalar, adj_vector, void*, adj_vector*) = NULL;
  adj_vector x, y, Ax, ATy;
  adj_scalar yAx, ATyx;

  (void) tol;

  if (adjointer->callbacks.vec_set_random == NULL)
  {
    snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "In order to test the transpose of an operator, you need the ADJ_VEC_SET_RANDOM_CB callback.");
    return adj_chkierr_auto(ADJ_ERR_NEED_CALLBACK);
  }
  if (adjointer->callbacks.vec_dot_product == NULL)
  {
    snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "In order to test the transpose of an operator, you need the ADJ_VEC_DOT_PRODUCT_CB callback.");
    return adj_chkierr_auto(ADJ_ERR_NEED_CALLBACK);
  }
  if (adjointer->callbacks.vec_duplicate == NULL)
  {
    snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "In order to test the transpose of an operator, you need the ADJ_VEC_DUPLICATE_CB callback.");
    return adj_chkierr_auto(ADJ_ERR_NEED_CALLBACK);
  }
  if (adjointer->callbacks.vec_destroy == NULL)
  {
    snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "In order to test the transpose of an operator, you need the ADJ_VEC_DESTROY_CB callback.");
    return adj_chkierr_auto(ADJ_ERR_NEED_CALLBACK);
  }

  ierr = adj_find_operator_callback(adjointer, ADJ_BLOCK_ACTION_CB, block.name, (void (**)(void)) &block_action_func);
  if (ierr != ADJ_OK)
    return adj_chkierr_auto(ierr);


  adjointer->callbacks.vec_duplicate(model_input, &x);
  adjointer->callbacks.vec_duplicate(model_output, &y);
  block.test_hermitian = ADJ_FALSE;

  for (i = 0; i < N; i++)
  {
    adjointer->callbacks.vec_set_random(&x);
    adjointer->callbacks.vec_set_random(&y);

    ierr = adj_evaluate_block_action(adjointer, block, x, &Ax);
    if (ierr != ADJ_OK)
      break;

    block.hermitian = !block.hermitian;
    ierr = adj_evaluate_block_action(adjointer, block, y, &ATy);
    if (ierr != ADJ_OK)
      break;
    block.hermitian = !block.hermitian;

    adjointer->callbacks.vec_dot_product(x, ATy, &ATyx);
    adjointer->callbacks.vec_dot_product(y, Ax, &yAx);
    adjointer->callbacks.vec_destroy(&ATy);
    adjointer->callbacks.vec_destroy(&Ax);

    if (COMPLEX_ABS(yAx - ATyx) > tol) 
    {
      snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "Transpose verification of block \"%s\" failed: |<y, Ax> - <A^Ty, x>| == %e (> tolerance of %e, iteration %d).", block.name, COMPLEX_ABS(yAx - ATyx), (double) tol, i+1);
      ierr = ADJ_ERR_TOLERANCE_EXCEEDED;
      break;
    }
  }

  adjointer->callbacks.vec_destroy(&x);
  adjointer->callbacks.vec_destroy(&y);

  return adj_chkierr_auto(ierr);
}

int adj_test_nonlinear_derivative_action_transpose(adj_adjointer* adjointer, adj_nonlinear_block_derivative nonlinear_block_derivative, adj_vector model_input, adj_vector model_output, int N, adj_scalar tol)
{
  int i;
  int ierr;
  void (*nonlinear_derivative_action_func)(int ndepends, adj_variable* variables, adj_vector* dependencies, adj_variable derivative, adj_vector contraction, int hermitian, adj_vector input, adj_scalar coefficient, void* context, adj_vector* output) = NULL;
  adj_vector x, y, Gx, GTy;
  adj_scalar yGx, GTyx;

  if (adjointer->callbacks.vec_set_random == NULL)
  {
    snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "In order to test the transpose of an operator, you need the ADJ_VEC_SET_RANDOM_CB callback.");
    return adj_chkierr_auto(ADJ_ERR_NEED_CALLBACK);
  }
  if (adjointer->callbacks.vec_dot_product == NULL)
  {
    snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "In order to test the transpose of an operator, you need the ADJ_VEC_DOT_PRODUCT_CB callback.");
    return adj_chkierr_auto(ADJ_ERR_NEED_CALLBACK);
  }
  if (adjointer->callbacks.vec_duplicate == NULL)
  {
    snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "In order to test the transpose of an operator, you need the ADJ_VEC_DUPLICATE_CB callback.");
    return adj_chkierr_auto(ADJ_ERR_NEED_CALLBACK);
  }
  if (adjointer->callbacks.vec_destroy == NULL)
  {
    snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "In order to test the transpose of an operator, you need the ADJ_VEC_DESTROY_CB callback.");
    return adj_chkierr_auto(ADJ_ERR_NEED_CALLBACK);
  }

  ierr = adj_find_operator_callback(adjointer, ADJ_NBLOCK_DERIVATIVE_ACTION_CB, nonlinear_block_derivative.nonlinear_block.name, (void (**)(void)) &nonlinear_derivative_action_func);
  if (ierr != ADJ_OK)
    return adj_chkierr_auto(ierr);


  adjointer->callbacks.vec_duplicate(model_input, &x);
  adjointer->callbacks.vec_duplicate(model_output, &y);
  nonlinear_block_derivative.nonlinear_block.test_deriv_hermitian = ADJ_FALSE;
  nonlinear_block_derivative.nonlinear_block.test_derivative = ADJ_FALSE;

  for (i = 0; i < N; i++)
  {
    adjointer->callbacks.vec_set_random(&x);
    adjointer->callbacks.vec_set_random(&y);
    adjointer->callbacks.vec_duplicate(model_output, &Gx);
    adjointer->callbacks.vec_duplicate(model_input, &GTy);

    ierr = adj_evaluate_nonlinear_derivative_action(adjointer, 1, &nonlinear_block_derivative, x, &Gx);
    if (ierr != ADJ_OK)
      break;

    nonlinear_block_derivative.hermitian = !nonlinear_block_derivative.hermitian;
    ierr = adj_evaluate_nonlinear_derivative_action(adjointer, 1, &nonlinear_block_derivative, y, &GTy);
    if (ierr != ADJ_OK)
      break;
    nonlinear_block_derivative.hermitian = !nonlinear_block_derivative.hermitian;

    adjointer->callbacks.vec_dot_product(x, GTy, &GTyx);
    adjointer->callbacks.vec_dot_product(y, Gx, &yGx);
    adjointer->callbacks.vec_destroy(&GTy);
    adjointer->callbacks.vec_destroy(&Gx);

    if (COMPLEX_ABS(yGx - GTyx) > tol) 
    {
      snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "Transpose verification of the derivative of nonlinear block \"%s\" failed: |<y, Gx> - <G^Ty, x>| == %e (> tolerance of %e, iteration %d).", nonlinear_block_derivative.nonlinear_block.name, COMPLEX_ABS(yGx - GTyx), (double) tol, i+1);
      ierr = ADJ_ERR_TOLERANCE_EXCEEDED;
      break;
    }
  }

  adjointer->callbacks.vec_destroy(&x);
  adjointer->callbacks.vec_destroy(&y);

  return adj_chkierr_auto(ierr);
}

int adj_test_nonlinear_derivative_action_consistency(adj_adjointer* adjointer, adj_nonlinear_block_derivative nonlinear_block_derivative, adj_variable deriv_var, int N)
{
  /* Implement the derivative test. If you have a function J(u), then by Taylor's theorem
     ||J(u + delta_u) - J(u)|| should be first-order in ||delta_u||, and
     ||J(u + delta_u) - J(u) - grad(J) . delta_u || should be second-order in ||delta_u||.
     This routine checks the order of convergence of the latter, to make sure that the gradient supplied by the user
     is indeed the gradient of the nonlinear action function. */

  int i, j;
  int ierr;
  void (*nonlinear_derivative_action_func)(int ndepends, adj_variable* variables, adj_vector* dependencies, adj_variable derivative, adj_vector contraction, int hermitian, adj_vector input, adj_scalar coefficient, void* context, adj_vector* output) = NULL;
  void (*nonlinear_action_func)(int ndepends, adj_variable* variables, adj_vector* dependencies, adj_vector input, void* context, adj_vector* output) = NULL;
  adj_scalar perturbation; /* the magnitude of the perturbation */
  adj_scalar* unscaled_perturbation; /* the direction of the perturbation */
  adj_scalar* perturbations; /* an array, with perturbation in each entry */
  adj_scalar* fd_errors; /* the ||J(u + delta_u) - J(u)|| for various ||delta_u||'s */
  adj_scalar* grad_errors; /* the ||J(u + delta_u) - J(u) - grad(J) . delta_u|| for various ||delta_u||'s */
  adj_vector original_dependency; /* u */
  adj_vector dependency_perturbation; /* delta_u */
  adj_vector original_output; /* J(u) */
  adj_vector perturbed_output; /* J(u + delta_u) */
  adj_vector gradient; /* grad(J) . delta_u */
  adj_vector contraction;
  adj_vector contraction_bak;
  adj_scalar contraction_norm;
  adj_scalar* fd_conv; /* the orders of convergence for fd_errors */
  adj_scalar* grad_conv; /* the orders of convergence for grad_errors */
  int return_ierr;
  int sz;

  nonlinear_block_derivative.nonlinear_block.test_derivative = ADJ_FALSE;
  nonlinear_block_derivative.nonlinear_block.test_deriv_hermitian = ADJ_FALSE;
  nonlinear_block_derivative.nonlinear_block.coefficient = (adj_scalar) 1.0;
  nonlinear_block_derivative.hermitian = ADJ_FALSE;

  if (adjointer->callbacks.vec_duplicate == NULL)
  {
    snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "In order to test the nonlinear derivative action consistency of an operator, you need the ADJ_VEC_DUPLICATE_CB callback.");
    return adj_chkierr_auto(ADJ_ERR_NEED_CALLBACK);
  }
  if (adjointer->callbacks.vec_destroy == NULL)
  {
    snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "In order to test the nonlinear derivative action consistency of an operator, you need the ADJ_VEC_DESTROY_CB callback.");
    return adj_chkierr_auto(ADJ_ERR_NEED_CALLBACK);
  }
  if (adjointer->callbacks.vec_axpy == NULL)
  {
    snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "In order to test the nonlinear derivative action consistency of an operator, you need the ADJ_VEC_AXPY_CB callback.");
    return adj_chkierr_auto(ADJ_ERR_NEED_CALLBACK);
  }
  if (adjointer->callbacks.vec_set_values == NULL)
  {
    snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "In order to test the nonlinear derivative action consistency of an operator, you need the ADJ_VEC_SET_VALUES_CB callback.");
    return adj_chkierr_auto(ADJ_ERR_NEED_CALLBACK);
  }
  if (adjointer->callbacks.vec_get_norm == NULL)
  {
    snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "In order to test the nonlinear derivative action consistency of an operator, you need the ADJ_VEC_GET_NORM_CB callback.");
    return adj_chkierr_auto(ADJ_ERR_NEED_CALLBACK);
  }
  if (adjointer->callbacks.vec_get_size == NULL)
  {
    snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "In order to test the nonlinear derivative action consistency of an operator, you need the ADJ_VEC_GET_SIZE_CB callback.");
    return adj_chkierr_auto(ADJ_ERR_NEED_CALLBACK);
  }

  ierr = adj_find_operator_callback(adjointer, ADJ_NBLOCK_ACTION_CB, nonlinear_block_derivative.nonlinear_block.name, (void (**)(void)) &nonlinear_action_func);
  if (ierr != ADJ_OK)
    return adj_chkierr_auto(ierr);

  ierr = adj_find_operator_callback(adjointer, ADJ_NBLOCK_DERIVATIVE_ACTION_CB, nonlinear_block_derivative.nonlinear_block.name, (void (**)(void)) &nonlinear_derivative_action_func);
  if (ierr != ADJ_OK)
    return adj_chkierr_auto(ierr);

  if (N < 2) 
  {
    snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "You need to compute at least two rounds to perform the derivative test.");
    return adj_chkierr_auto(ADJ_ERR_INVALID_INPUTS);
  }

  ierr = adj_get_variable_value(adjointer, deriv_var, &original_dependency);
  if (ierr != ADJ_OK) return adj_chkierr_auto(ierr);

  /* First, let's check if contraction vector is suitable (nonzero) */
  adjointer->callbacks.vec_duplicate(nonlinear_block_derivative.contraction, &contraction_bak);
  adjointer->callbacks.vec_axpy(&contraction_bak, (adj_scalar) 1.0, nonlinear_block_derivative.contraction);

  adjointer->callbacks.vec_get_norm(nonlinear_block_derivative.contraction, &contraction_norm);
  if (contraction_norm > 0)
  {
    adjointer->callbacks.vec_duplicate(nonlinear_block_derivative.contraction, &contraction);
    adjointer->callbacks.vec_axpy(&contraction, (adj_scalar) 1.0, nonlinear_block_derivative.contraction);
  }
  else
  {
    int contraction_sz;
    adj_scalar* contraction_vec; /* the direction of the perturbation */
    adjointer->callbacks.vec_duplicate(nonlinear_block_derivative.contraction, &contraction);
    adjointer->callbacks.vec_get_size(nonlinear_block_derivative.contraction, &contraction_sz);

    contraction_vec = (adj_scalar*) malloc(contraction_sz * sizeof(adj_scalar));
    ADJ_CHKMALLOC(contraction_vec);
    for (j = 0; j < contraction_sz; j++)
    {
      contraction_vec[j] = random() / ((adj_scalar) RAND_MAX) ;
    }
    adjointer->callbacks.vec_set_values(&contraction, contraction_vec);
    free(contraction_vec);
  }
  adjointer->callbacks.vec_axpy(&nonlinear_block_derivative.contraction, (adj_scalar)-1.0, contraction_bak);
  adjointer->callbacks.vec_axpy(&nonlinear_block_derivative.contraction, (adj_scalar) 1.0, contraction);

  /* Compute the unperturbed quantity we'll need through the loop */
  ierr = adj_evaluate_nonlinear_action(adjointer, nonlinear_action_func, nonlinear_block_derivative.nonlinear_block, contraction, NULL, NULL, &original_output);
  if (ierr != ADJ_OK) return adj_chkierr_auto(ierr);

  adjointer->callbacks.vec_duplicate(original_dependency, &dependency_perturbation);
  fd_errors = (adj_scalar*) malloc(N * sizeof(adj_scalar));
  ADJ_CHKMALLOC(fd_errors);
  grad_errors = (adj_scalar*) malloc(N * sizeof(adj_scalar));
  ADJ_CHKMALLOC(grad_errors);

  adjointer->callbacks.vec_get_size(original_dependency, &sz);
  perturbations = (adj_scalar*) malloc(sz * sizeof(adj_scalar));
  ADJ_CHKMALLOC(perturbations);
  unscaled_perturbation = (adj_scalar*) malloc(sz * sizeof(adj_scalar));
  ADJ_CHKMALLOC(unscaled_perturbation);

  /* initialise the perturbation direction */
  srandom((unsigned int) time(NULL));
  for (j = 0; j < sz; j++)
  {
    unscaled_perturbation[j] = random() / ((adj_scalar) RAND_MAX) ;
  }

  perturbation = (adj_scalar) 2.0e-4;
  for (i = 0; i < N; i++)
  {
    perturbation = perturbation / 2.0;
    /* Set dependency_perturbation to have the value perturbation in every entry */
    for (j = 0; j < sz; j++)
    {
      perturbations[j] = perturbation * unscaled_perturbation[j];
    }
    adjointer->callbacks.vec_set_values(&dependency_perturbation, perturbations);

    ierr = adj_evaluate_nonlinear_action(adjointer, nonlinear_action_func, nonlinear_block_derivative.nonlinear_block, contraction, &deriv_var, &dependency_perturbation, &perturbed_output);
    if (ierr != ADJ_OK) return adj_chkierr_auto(ierr);

    adjointer->callbacks.vec_axpy(&perturbed_output, (adj_scalar) -1.0, original_output);
    adjointer->callbacks.vec_get_norm(perturbed_output, &fd_errors[i]);
    if (isnan(fd_errors[i]))
    {
      snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "A norm during the derivative test returned NaN.");
      return adj_chkierr_auto(ADJ_ERR_INVALID_INPUTS);
    }

    adjointer->callbacks.vec_duplicate(original_output, &gradient);
    ierr = adj_evaluate_nonlinear_derivative_action(adjointer, 1, &nonlinear_block_derivative, dependency_perturbation, &gradient);
    adjointer->callbacks.vec_axpy(&perturbed_output, (adj_scalar) 1.0, gradient);
    adjointer->callbacks.vec_destroy(&gradient);

    adjointer->callbacks.vec_get_norm(perturbed_output, &grad_errors[i]);
    if (isnan(grad_errors[i]))
    {
      snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "A norm during the derivative test returned NaN.");
      return adj_chkierr_auto(ADJ_ERR_INVALID_INPUTS);
    }
    adjointer->callbacks.vec_destroy(&perturbed_output);
  }

  free(perturbations);
  free(unscaled_perturbation);
  adjointer->callbacks.vec_destroy(&dependency_perturbation);
  adjointer->callbacks.vec_destroy(&original_output);

  adjointer->callbacks.vec_axpy(&nonlinear_block_derivative.contraction, (adj_scalar)-1.0, contraction);
  adjointer->callbacks.vec_axpy(&nonlinear_block_derivative.contraction, (adj_scalar) 1.0, contraction_bak);

  adjointer->callbacks.vec_destroy(&contraction);

  /* Now we analyse the fd_errors and grad_errors to investigate the order of convergence.
     fd_errors should converge at first order, and grad_errors should converge at second order. */
  fd_conv = (adj_scalar*) malloc((N-1) * sizeof(adj_scalar));
  ADJ_CHKMALLOC(fd_conv);
  grad_conv = (adj_scalar*) malloc((N-1) * sizeof(adj_scalar));
  ADJ_CHKMALLOC(grad_conv);

  return_ierr = ADJ_OK;
  for (i = 0; i < N - 1; i++)
  {
    if (fd_errors[i+1] == (adj_scalar) 0.0 || fd_errors[i] == (adj_scalar) 0.0)
    {
      snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "Expected the nonlinear action %s to give different outputs for different inputs, but got no difference.", nonlinear_block_derivative.nonlinear_block.name);
      return_ierr = ADJ_WARN_COMPARISON_FAILED;
      break;
    }

    fd_conv[i] = log2( (double) (fd_errors[i] / fd_errors[i+1]) );

    /* fd_conv should be in [0.9, 1.1] */
    if (fabs((double) (fd_conv[i] - (adj_scalar) 1.0)) > 0.1)
    {
      snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "Expected the finite differences of operator %s to converge at first order, but got %f.", nonlinear_block_derivative.nonlinear_block.name, (double) fd_conv[i]);
      return_ierr = ADJ_WARN_COMPARISON_FAILED;
      break;
    }

    if (grad_errors[i] == (adj_scalar) 0.0 || grad_errors[i+1] == (adj_scalar) 0.0) /* might happen if the function is linear in its dependency */
    {
      grad_conv[i] = (adj_scalar) 2.0; /* the order we expect */
    }
    else if (grad_errors[i] < ADJ_SCALAR_EPS && grad_errors[i+1] < ADJ_SCALAR_EPS) /* or you might have a perfect gradient, to machine precision */
    {
      grad_conv[i] = (adj_scalar) 2.0; /* the order we expect */
    }
    else
    {
      grad_conv[i] = log2( (double) (grad_errors[i] / grad_errors[i+1]) );
    }

    /* fd_conv should be in [1.9, 2.1] */
    if (fabs((double) (grad_conv[i] - (adj_scalar) 2.0)) > 0.1)
    {
      char var_name[ADJ_NAME_LEN+1];
      adj_variable_str(deriv_var, var_name, ADJ_NAME_LEN);
      snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "Expected the Taylor series remainder of operator %s with respect to %s to converge at second order, but got %f (the error values are %e and %e).", nonlinear_block_derivative.nonlinear_block.name, var_name, (double) grad_conv[i], (double) (grad_errors[i]), (double) (grad_errors[i+1]));
      return_ierr = ADJ_WARN_COMPARISON_FAILED;
      break;
    }
  }

  free(fd_errors);
  free(grad_errors);
  free(fd_conv);
  free(grad_conv);

  return return_ierr;
}
