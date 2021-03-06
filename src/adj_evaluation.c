#include "libadjoint/adj_evaluation.h"

int adj_evaluate_block_action(adj_adjointer* adjointer, adj_block block, adj_vector input, adj_vector* output)
{
  int i, ierr;
  void (*block_action_func)(int, adj_variable*, adj_vector*, int, adj_scalar, adj_vector, void*, adj_vector*) = NULL;
  adj_vector* dependencies = NULL;
  int ndepends = 0;
  adj_variable* variables = NULL;

  ierr = adj_find_operator_callback(adjointer, ADJ_BLOCK_ACTION_CB, block.name, (void (**)(void)) &block_action_func);
  if (ierr != ADJ_OK)
    return adj_chkierr_auto(ierr);

  /* We have the right callback, so let's call it already */ 
  if (block.has_nonlinear_block) 
  { 
    /* we need to set up the dependencies */
    ndepends = block.nonlinear_block.ndepends;
    variables = block.nonlinear_block.depends;
    dependencies = (adj_vector*) malloc(ndepends * sizeof(adj_vector));
    ADJ_CHKMALLOC(dependencies);

    for (i = 0; i < ndepends; i++)
    {
      ierr = adj_get_variable_value(adjointer, variables[i], &dependencies[i]);
      if (ierr != ADJ_OK)
        return adj_chkierr_auto(ierr);
    }
  }

  block_action_func(ndepends, variables, dependencies, block.hermitian, block.coefficient, input, block.context, output );

  ierr = ADJ_OK;
  /* Run the hermitian tests if specified */
  if (block.test_hermitian) 
    ierr = adj_test_block_action_transpose(adjointer, block, input, *output, block.number_of_tests, block.tolerance);

  if (block.has_nonlinear_block)
    free(dependencies);

  return adj_chkierr_auto(ierr);
}


int adj_evaluate_block_assembly(adj_adjointer* adjointer, adj_block block, adj_matrix *output, adj_vector* rhs)
{
  int i, ierr;
  void (*block_assembly_func)(int, adj_variable*, adj_vector*, int, adj_scalar, void*, adj_matrix*, adj_vector*) = NULL;
  adj_vector* dependencies = NULL;
  int ndepends = 0;
  adj_variable* variables = NULL;

  ierr = adj_find_operator_callback(adjointer, ADJ_BLOCK_ASSEMBLY_CB, block.name, (void (**)(void)) &block_assembly_func);
  if (ierr != ADJ_OK)
    return adj_chkierr_auto(ierr);

  /* We have the right callback, so let's call it already */ 
  if (block.has_nonlinear_block)
  {
    /* we need to set up the dependencies */
    ndepends = block.nonlinear_block.ndepends;
    variables = block.nonlinear_block.depends;
    dependencies = (adj_vector*) malloc(ndepends * sizeof(adj_vector));
    ADJ_CHKMALLOC(dependencies);

    for (i = 0; i < ndepends; i++)
    {
      ierr = adj_get_variable_value(adjointer, variables[i], &dependencies[i]);
      if (ierr != ADJ_OK)
        return adj_chkierr_auto(ierr);
    }
  }

  block_assembly_func(ndepends, variables, dependencies, block.hermitian, block.coefficient, block.context, output, rhs);

  if (block.has_nonlinear_block)
    free(dependencies);

  return ADJ_OK;
}


int adj_evaluate_nonlinear_derivative_action(adj_adjointer* adjointer, int nderivatives, adj_nonlinear_block_derivative* derivatives, adj_vector value, adj_vector* rhs)
{
  int ierr;
  int deriv;
  void (*nonlinear_derivative_action_func)(int ndepends, adj_variable* variables, adj_vector* dependencies, adj_variable derivative, adj_vector contraction, int hermitian, adj_vector input, adj_scalar coefficient, void* context, adj_vector* output);

  /* As usual, check as much as we can at the start */
  strncpy(adj_error_msg, "Need a data callback, but it hasn't been supplied.", ADJ_ERROR_MSG_BUF);
  if (adjointer->callbacks.vec_destroy == NULL)   return adj_chkierr_auto(ADJ_ERR_NEED_CALLBACK);
  if (adjointer->callbacks.vec_axpy == NULL)      return adj_chkierr_auto(ADJ_ERR_NEED_CALLBACK);
  if (adjointer->callbacks.vec_duplicate == NULL) return adj_chkierr_auto(ADJ_ERR_NEED_CALLBACK);
  strncpy(adj_error_msg, "", ADJ_ERROR_MSG_BUF);
  assert(nderivatives > 0);

  /* Let's also check we have all of the variables available */
  for (deriv = 0; deriv < nderivatives; deriv++)
  {
    int i;
    for (i = 0; i < derivatives[deriv].nonlinear_block.ndepends; i++)
    {
      ierr = adj_has_variable_value(adjointer, derivatives[deriv].nonlinear_block.depends[i]);
      if (ierr != ADJ_OK) return adj_chkierr_auto(ierr);
    }
  }

  for (deriv = 0; deriv < nderivatives; deriv++)
  {
    /* First, try to find a routine supplied by the user. */
    /* Outer refers to the index of contraction: if deriv.outer is FALSE, then we want ADJ_NBLOCK_DERIVATIVE_ACTION_CB; if it is TRUE, we want ADJ_NBLOCK_DERIVATIVE_OUTER_ACTION_CB. */

    if (derivatives[deriv].outer == ADJ_FALSE)
    {
      ierr = adj_find_operator_callback(adjointer, ADJ_NBLOCK_DERIVATIVE_ACTION_CB, derivatives[deriv].nonlinear_block.name, (void (**)(void)) &nonlinear_derivative_action_func);
    }
    else
    {
      ierr = adj_find_operator_callback(adjointer, ADJ_NBLOCK_DERIVATIVE_OUTER_ACTION_CB, derivatives[deriv].nonlinear_block.name, (void (**)(void)) &nonlinear_derivative_action_func);
    }

    if (ierr != ADJ_OK) return adj_chkierr_auto(ierr);

    if (derivatives[deriv].nonlinear_block.test_deriv_hermitian)
    {
      ierr = adj_test_nonlinear_derivative_action_transpose(adjointer, derivatives[deriv], value, *rhs, derivatives[deriv].nonlinear_block.number_of_tests, derivatives[deriv].nonlinear_block.tolerance);
      if (ierr != ADJ_OK) return adj_chkierr_auto(ierr);
    }
    if (derivatives[deriv].nonlinear_block.test_derivative)
    {
      ierr = adj_test_nonlinear_derivative_action_consistency(adjointer, derivatives[deriv], derivatives[deriv].variable, derivatives[deriv].nonlinear_block.number_of_rounds);
      if (ierr != ADJ_OK) return adj_chkierr_auto(ierr);
    }
    adj_vector rhs_tmp;
    ierr = adj_evaluate_nonlinear_derivative_action_supplied(adjointer, nonlinear_derivative_action_func, derivatives[deriv], value, &rhs_tmp);
    if (ierr != ADJ_OK) return adj_chkierr_auto(ierr);
    adjointer->callbacks.vec_axpy(rhs, (adj_scalar) -1.0, rhs_tmp);
    adjointer->callbacks.vec_destroy(&rhs_tmp);
  }

  return ADJ_OK;
}

int adj_evaluate_nonlinear_second_derivative_action(adj_adjointer* adjointer, int nderivatives, adj_nonlinear_block_second_derivative* derivatives, adj_vector* rhs)
{
  int ierr;
  int deriv;
  void (*nonlinear_second_derivative_action_func)(int ndepends, adj_variable* variables, adj_vector* dependencies, adj_variable inner_derivative, adj_vector inner_contraction, adj_variable outer_derivative, adj_vector outer_contraction, int hermitian, adj_vector input, adj_scalar coefficient, void* context, adj_vector* output);
  adj_vector* dependencies = NULL;

  /* As usual, check as much as we can at the start */
  strncpy(adj_error_msg, "Need a data callback, but it hasn't been supplied.", ADJ_ERROR_MSG_BUF);
  if (adjointer->callbacks.vec_destroy == NULL)   return adj_chkierr_auto(ADJ_ERR_NEED_CALLBACK);
  if (adjointer->callbacks.vec_axpy == NULL)      return adj_chkierr_auto(ADJ_ERR_NEED_CALLBACK);
  if (adjointer->callbacks.vec_duplicate == NULL) return adj_chkierr_auto(ADJ_ERR_NEED_CALLBACK);
  strncpy(adj_error_msg, "", ADJ_ERROR_MSG_BUF);
  assert(nderivatives > 0);

  /* Let's also check we have all of the variables available */
  for (deriv = 0; deriv < nderivatives; deriv++)
  {
    int i;
    for (i = 0; i < derivatives[deriv].nonlinear_block.ndepends; i++)
    {
      ierr = adj_has_variable_value(adjointer, derivatives[deriv].nonlinear_block.depends[i]);
      if (ierr != ADJ_OK) return adj_chkierr_auto(ierr);
    }
  }

  for (deriv = 0; deriv < nderivatives; deriv++)
  {
    ierr = adj_find_operator_callback(adjointer, ADJ_NBLOCK_SECOND_DERIVATIVE_ACTION_CB, derivatives[deriv].nonlinear_block.name, (void (**)(void)) &nonlinear_second_derivative_action_func);
    if (ierr == ADJ_OK)
    {
      int i;
      adj_vector rhs_tmp;

      dependencies = (adj_vector*) malloc(derivatives[deriv].nonlinear_block.ndepends * sizeof(adj_vector));
      ADJ_CHKMALLOC(dependencies);
      for (i = 0; i < derivatives[deriv].nonlinear_block.ndepends; i++)
      {
        ierr = adj_get_variable_value(adjointer, derivatives[deriv].nonlinear_block.depends[i], &(dependencies[i]));
        assert(ierr == ADJ_OK); /* We checked for them earlier */
      }
      nonlinear_second_derivative_action_func(derivatives[deriv].nonlinear_block.ndepends, derivatives[deriv].nonlinear_block.depends, dependencies,
                                              derivatives[deriv].inner_variable, derivatives[deriv].inner_contraction,
                                              derivatives[deriv].outer_variable, derivatives[deriv].outer_contraction,
                                              derivatives[deriv].hermitian, derivatives[deriv].block_action, derivatives[deriv].nonlinear_block.coefficient,
                                              derivatives[deriv].nonlinear_block.context, &rhs_tmp);
      free(dependencies);
      adjointer->callbacks.vec_axpy(rhs, (adj_scalar) -1.0, rhs_tmp);
      adjointer->callbacks.vec_destroy(&rhs_tmp);
    }
    else
    {
      return adj_chkierr_auto(ierr);
    }
  }

  return ADJ_OK;
}

int adj_evaluate_nonlinear_derivative_action_supplied(adj_adjointer* adjointer, void (*nonlinear_derivative_action_func)(int ndepends, adj_variable* variables, 
     adj_vector* dependencies, adj_variable derivative, adj_vector contraction, int hermitian, adj_vector input, adj_scalar coefficient, void* context, adj_vector* output),
     adj_nonlinear_block_derivative derivative, adj_vector value, adj_vector* rhs)
{
  adj_vector* dependencies = NULL;
  int i;
  int ierr;

  dependencies = (adj_vector*) malloc(derivative.nonlinear_block.ndepends * sizeof(adj_vector));
  ADJ_CHKMALLOC(dependencies);
  for (i = 0; i < derivative.nonlinear_block.ndepends; i++)
  {
    ierr = adj_get_variable_value(adjointer, derivative.nonlinear_block.depends[i], &(dependencies[i]));
    assert(ierr == ADJ_OK); /* We checked for them earlier */
  }

  nonlinear_derivative_action_func(derivative.nonlinear_block.ndepends, derivative.nonlinear_block.depends, dependencies, derivative.variable,
                                   derivative.contraction, derivative.hermitian, value, derivative.nonlinear_block.coefficient, derivative.nonlinear_block.context, rhs);

  free(dependencies);
  return ADJ_OK;
}

int adj_evaluate_nonlinear_action(adj_adjointer* adjointer, void (*nonlinear_action_func)(int ndepends, adj_variable* variables, adj_vector* dependencies,
                                  adj_vector input, void* context, adj_vector* output), adj_nonlinear_block nonlinear_block, adj_vector input,
                                  adj_variable* perturbed_var, adj_vector* perturbation, adj_vector* output)
{
  int ierr;
  int i;
  int perturbed_idx;
  adj_vector* dependencies;
  adj_vector perturbed_dependency;

  strncpy(adj_error_msg, "Need a data callback, but it hasn't been supplied.", ADJ_ERROR_MSG_BUF);
  if (adjointer->callbacks.vec_destroy == NULL)   return adj_chkierr_auto(ADJ_ERR_NEED_CALLBACK);
  if (adjointer->callbacks.vec_axpy == NULL)      return adj_chkierr_auto(ADJ_ERR_NEED_CALLBACK);
  if (adjointer->callbacks.vec_duplicate == NULL) return adj_chkierr_auto(ADJ_ERR_NEED_CALLBACK);
  if (adjointer->callbacks.vec_set_values == NULL) return adj_chkierr_auto(ADJ_ERR_NEED_CALLBACK);
  strncpy(adj_error_msg, "", ADJ_ERROR_MSG_BUF);

  /* If you want to compute at a perturbed state, you need to give me both perturbed_var
     and perturbation */
  if (perturbed_var != NULL || perturbation != NULL)
  {
    assert(perturbed_var != NULL && perturbation != NULL);
    perturbed_idx = -1;
  }

  dependencies = (adj_vector*) malloc(nonlinear_block.ndepends * sizeof(adj_vector));
  ADJ_CHKMALLOC(dependencies);
  for (i = 0; i < nonlinear_block.ndepends; i++)
  {
    ierr = adj_get_variable_value(adjointer, nonlinear_block.depends[i], &(dependencies[i]));
    assert(ierr == ADJ_OK); /* We checked for them earlier */

    /* Check if this is the variable we want to perturb */
    if (perturbed_var != NULL)
    {
      if (adj_variable_equal(perturbed_var, &(nonlinear_block.depends[i]), 1))
      {
        /* Check we don't perturb twice */
        if (perturbed_idx != -1)
        {
          snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "Dependencies %d and %d of nonlinear block %s are equal, which is invalid.", perturbed_idx, i, nonlinear_block.name);
          free(dependencies);
          return adj_chkierr_auto(ADJ_ERR_INVALID_INPUTS);
        }

        adjointer->callbacks.vec_duplicate(dependencies[i], &perturbed_dependency);
        adjointer->callbacks.vec_axpy(&perturbed_dependency, (adj_scalar) 1.0, dependencies[i]);
        adjointer->callbacks.vec_axpy(&perturbed_dependency, (adj_scalar) 1.0, *perturbation);
        dependencies[i] = perturbed_dependency;
        perturbed_idx = i;
      }
    }
  }

  /* Check we perturbed something */
  if (perturbed_var != NULL)
    assert(perturbed_idx != -1);

  /* Now evaluate the function */
  nonlinear_action_func(nonlinear_block.ndepends, nonlinear_block.depends, dependencies, input, nonlinear_block.context, output);

  /* If we perturbed something, we allocated it, so we have to destroy it */
  if (perturbed_var != NULL)
    adjointer->callbacks.vec_destroy(&perturbed_dependency);

  free(dependencies);
  return ADJ_OK;
}

int adj_evaluate_functional(adj_adjointer* adjointer, int timestep, char* functional, adj_scalar* output)
{
  int ierr;
  void (*functional_func)(adj_adjointer* adjointer, int timestep, int ndepends, adj_variable* variables, adj_vector* dependencies, char* name, adj_scalar* output) = NULL;
  adj_vector* dependencies = NULL;
  int ndepends = 0;
  adj_variable* variables = NULL;
  adj_functional_data* functional_data_ptr = NULL;

  ierr = adj_find_functional_callback(adjointer, functional, &functional_func);
  if (ierr != ADJ_OK)
    return adj_chkierr_auto(ierr);

  if (adjointer->ntimesteps <= timestep) 
  {
    snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "No data is associated with this timestep %d.", timestep);
    return adj_chkierr_auto(ADJ_ERR_INVALID_INPUTS);
  }
  
  functional_data_ptr = adjointer->timestep_data[timestep].functional_data_start;
  while (functional_data_ptr != NULL)
  {
    if (strncmp(functional_data_ptr->name, functional, ADJ_NAME_LEN) == 0)
    {
      int k;
      ndepends = functional_data_ptr->ndepends;
      dependencies = (adj_vector*) malloc( ndepends * sizeof(adj_vector));
      ADJ_CHKMALLOC(dependencies);
      variables = functional_data_ptr->dependencies;

      for (k = 0; k < ndepends; k++)
      {
        ierr = adj_get_variable_value(adjointer, variables[k], &(dependencies[k]));
        if (ierr != ADJ_OK) return adj_chkierr_auto(ierr);
      }
    }
    functional_data_ptr = functional_data_ptr->next;
  }
  if (dependencies == NULL) 
  {
    snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "Warning: Evaluating functional %s at timestep %d without having any dependencies registered for it.\n", functional, timestep);
    return adj_chkierr_auto(ADJ_WARN_UNINITIALISED_VALUE);
  }
  
  /* We have the right callback, so let's call it already */ 
  functional_func(adjointer, timestep, ndepends, variables, dependencies, functional, output);

  free(dependencies);
  return ADJ_OK;
}

int adj_evaluate_functional_derivative(adj_adjointer* adjointer, adj_variable variable, char* functional, adj_vector* output, int* has_output)
{
  int i, ierr;
  void (*functional_derivative_func)(adj_adjointer* adjointer, adj_variable variable, int ndepends, adj_variable* variables, adj_vector* dependencies, char* name, adj_vector* output) = NULL;
  adj_vector* dependencies = NULL;
  int ndepends = 0;
  adj_variable* variables = NULL;
  adj_functional_data* functional_data_ptr = NULL;
  adj_variable_data* data_ptr = NULL;
  adj_variable_hash* hash = NULL;
  int ntimesteps;
  int ntimesteps_to_consider;
  int* timesteps_to_consider;

  ierr = adj_variable_get_ndepending_timesteps(adjointer, variable, functional, &ntimesteps);
  if (ierr != ADJ_OK)
    return adj_chkierr_auto(ierr);
  if (ntimesteps == 0)
  {
    *has_output = ADJ_FALSE;
    return ADJ_OK;
  }
  else
  {
    *has_output = ADJ_TRUE;
  }

  ierr = adj_find_functional_derivative_callback(adjointer, functional, &functional_derivative_func);
  if (ierr != ADJ_OK)
    return adj_chkierr_auto(ierr);

  if (adjointer->ntimesteps <= variable.timestep) 
  {
    snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "No data is associated with this timestep %d.", variable.timestep);
    return adj_chkierr_auto(ADJ_ERR_INVALID_INPUTS);
  }

  ierr = adj_find_variable_data((&adjointer->varhash), &variable, &data_ptr);
  if (ierr != ADJ_OK)
    return adj_chkierr_auto(ierr);

  /* Create the dependency list for the functional derivative evaluation. */
  /* For that we loop over all timesteps that need 'variable' for the functional evaluation */
  /* and add its dependency to the dependency list */ 
  ntimesteps_to_consider = data_ptr->ndepending_timesteps + 1;
  timesteps_to_consider = (int*) malloc(ntimesteps_to_consider * sizeof(int));
  ADJ_CHKMALLOC(timesteps_to_consider);
  for (i = 0; i < data_ptr->ndepending_timesteps; i++)
    timesteps_to_consider[i] = data_ptr->depending_timesteps[i];
  timesteps_to_consider[i] = variable.timestep;

  for (i = 0; i < ntimesteps_to_consider; i++)
  {
    int timestep = timesteps_to_consider[i];
    functional_data_ptr = adjointer->timestep_data[timestep].functional_data_start;
    while (functional_data_ptr != NULL)
    {
      if (strncmp(functional_data_ptr->name, functional, ADJ_NAME_LEN) == 0)
      {
        int k;
        for (k = 0; k < functional_data_ptr->ndepends; k++)
        {
          adj_variable_data tmp_data;
          /* We're going to use this hash as a set, to see if we've seen this variable before */
          /* as we want to not pass any duplicates to the user code */
          ierr = adj_add_variable_data(&hash, &(functional_data_ptr->dependencies[k]), &tmp_data);
          if (ierr == ADJ_OK)
          {
            /* that means the addition went fine, i.e. we haven't seen it before, so we increment ndepends */
            ndepends++;
          }
        }
        break;
      }
      functional_data_ptr = functional_data_ptr->next;
    }
  }

  variables = (adj_variable*) malloc(ndepends * sizeof(adj_variable));
  ADJ_CHKMALLOC(variables);
  dependencies = (adj_vector*) malloc(ndepends * sizeof(adj_vector));
  ADJ_CHKMALLOC(dependencies);
  ndepends = 0;
  ierr = adj_destroy_hash(&hash);
  if (ierr != ADJ_OK) return adj_chkierr_auto(ierr);

  for (i = 0; i < ntimesteps_to_consider; i++)
  {
    int timestep = timesteps_to_consider[i];
    functional_data_ptr = adjointer->timestep_data[timestep].functional_data_start;
    while (functional_data_ptr != NULL)
    {
      if (strncmp(functional_data_ptr->name, functional, ADJ_NAME_LEN) == 0)
      {
        int k;
        for (k = 0; k < functional_data_ptr->ndepends; k++)
        {
          adj_variable_data tmp_data;
          ierr = adj_add_variable_data(&hash, &(functional_data_ptr->dependencies[k]), &tmp_data);
          if (ierr == ADJ_OK)
          {
            /* this variable is one we should add */
            memcpy(&(variables[ndepends]), &(functional_data_ptr->dependencies[k]), sizeof(adj_variable));
            ierr = adj_get_variable_value(adjointer, variables[ndepends], &(dependencies[ndepends]));
            if (ierr != ADJ_OK) return adj_chkierr_auto(ierr);
            ndepends++;
          }
        }
        break;
      }
      functional_data_ptr = functional_data_ptr->next;
    }
  }

  ierr = adj_destroy_hash(&hash);
  free(timesteps_to_consider);
  if (ierr != ADJ_OK) return adj_chkierr_auto(ierr);

  /* We have the right callback, so let's call it already */ 
  functional_derivative_func(adjointer, variable, ndepends, variables, dependencies, functional, output);

  free(dependencies);
  free(variables);

  return ADJ_OK;
}

int adj_evaluate_functional_second_derivative(adj_adjointer* adjointer, adj_variable variable, char* functional, adj_vector contraction, adj_vector* output, int* has_output)
{
  int i, ierr;
  void (*functional_second_derivative_func)(adj_adjointer* adjointer, adj_variable variable, int ndepends, adj_variable* variables, adj_vector* dependencies, adj_vector contraction, char* name, adj_vector* output) = NULL;
  adj_vector* dependencies = NULL;
  int ndepends = 0;
  adj_variable* variables = NULL;
  adj_functional_data* functional_data_ptr = NULL;
  adj_variable_data* data_ptr = NULL;
  adj_variable_hash* hash = NULL;
  int ntimesteps;
  int ntimesteps_to_consider;
  int* timesteps_to_consider;

  ierr = adj_variable_get_ndepending_timesteps(adjointer, variable, functional, &ntimesteps);
  if (ierr != ADJ_OK)
    return adj_chkierr_auto(ierr);
  if (ntimesteps == 0)
  {
    *has_output = ADJ_FALSE;
    return ADJ_OK;
  }
  else
  {
    *has_output = ADJ_TRUE;
  }

  ierr = adj_find_functional_second_derivative_callback(adjointer, functional, &functional_second_derivative_func);
  if (ierr != ADJ_OK)
    return adj_chkierr_auto(ierr);

  if (adjointer->ntimesteps <= variable.timestep) 
  {
    snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "No data is associated with this timestep %d.", variable.timestep);
    return adj_chkierr_auto(ADJ_ERR_INVALID_INPUTS);
  }

  ierr = adj_find_variable_data((&adjointer->varhash), &variable, &data_ptr);
  if (ierr != ADJ_OK)
    return adj_chkierr_auto(ierr);

  /* Create the dependency list for the functional derivative evaluation. */
  /* For that we loop over all timesteps that need 'variable' for the functional evaluation */
  /* and add its dependency to the dependency list */ 
  ntimesteps_to_consider = data_ptr->ndepending_timesteps + 1;
  timesteps_to_consider = (int*) malloc(ntimesteps_to_consider * sizeof(int));
  ADJ_CHKMALLOC(timesteps_to_consider);
  for (i = 0; i < data_ptr->ndepending_timesteps; i++)
    timesteps_to_consider[i] = data_ptr->depending_timesteps[i];
  timesteps_to_consider[i] = variable.timestep;

  for (i = 0; i < ntimesteps_to_consider; i++)
  {
    int timestep = timesteps_to_consider[i];
    functional_data_ptr = adjointer->timestep_data[timestep].functional_data_start;
    while (functional_data_ptr != NULL)
    {
      if (strncmp(functional_data_ptr->name, functional, ADJ_NAME_LEN) == 0)
      {
        int k;
        for (k = 0; k < functional_data_ptr->ndepends; k++)
        {
          adj_variable_data tmp_data;
          /* We're going to use this hash as a set, to see if we've seen this variable before */
          /* as we want to not pass any duplicates to the user code */
          ierr = adj_add_variable_data(&hash, &(functional_data_ptr->dependencies[k]), &tmp_data);
          if (ierr == ADJ_OK)
          {
            /* that means the addition went fine, i.e. we haven't seen it before, so we increment ndepends */
            ndepends++;
          }
        }
        break;
      }
      functional_data_ptr = functional_data_ptr->next;
    }
  }

  variables = (adj_variable*) malloc(ndepends * sizeof(adj_variable));
  ADJ_CHKMALLOC(variables);
  dependencies = (adj_vector*) malloc(ndepends * sizeof(adj_vector));
  ADJ_CHKMALLOC(dependencies);
  ndepends = 0;
  ierr = adj_destroy_hash(&hash);
  if (ierr != ADJ_OK) return adj_chkierr_auto(ierr);

  for (i = 0; i < ntimesteps_to_consider; i++)
  {
    int timestep = timesteps_to_consider[i];
    functional_data_ptr = adjointer->timestep_data[timestep].functional_data_start;
    while (functional_data_ptr != NULL)
    {
      if (strncmp(functional_data_ptr->name, functional, ADJ_NAME_LEN) == 0)
      {
        int k;
        for (k = 0; k < functional_data_ptr->ndepends; k++)
        {
          adj_variable_data tmp_data;
          ierr = adj_add_variable_data(&hash, &(functional_data_ptr->dependencies[k]), &tmp_data);
          if (ierr == ADJ_OK)
          {
            /* this variable is one we should add */
            memcpy(&(variables[ndepends]), &(functional_data_ptr->dependencies[k]), sizeof(adj_variable));
            ierr = adj_get_variable_value(adjointer, variables[ndepends], &(dependencies[ndepends]));
            if (ierr != ADJ_OK) return adj_chkierr_auto(ierr);
            ndepends++;
          }
        }
        break;
      }
      functional_data_ptr = functional_data_ptr->next;
    }
  }

  ierr = adj_destroy_hash(&hash);
  free(timesteps_to_consider);
  if (ierr != ADJ_OK) return adj_chkierr_auto(ierr);

  /* We have the right callback, so let's call it already */ 
  functional_second_derivative_func(adjointer, variable, ndepends, variables, dependencies, contraction, functional, output);

  free(dependencies);
  free(variables);

  return ADJ_OK;
}


int adj_evaluate_forward_source(adj_adjointer* adjointer, int equation, adj_vector* output, int* has_output)
{
  assert(adjointer->equations[equation].rhs_callback != NULL);
  adj_variable* variables;
  adj_vector* dependencies;
  int nrhsdeps;
  int j, k;
  int ierr;
  int nonlinear_idx;

  nonlinear_idx = adj_equation_rhs_nonlinear_index(adjointer->equations[equation]);
  if (nonlinear_idx >= 0)
  {
    nrhsdeps = adjointer->equations[equation].nrhsdeps - 1; /* we don't claim to supply a value for the variable we're solving the equation for ... */
  }
  else
  {
    nrhsdeps = adjointer->equations[equation].nrhsdeps;
  }

  variables = (adj_variable*) malloc(nrhsdeps * sizeof(adj_variable));
  ADJ_CHKMALLOC(variables);
  dependencies = (adj_vector*) malloc(nrhsdeps * sizeof(adj_vector));
  ADJ_CHKMALLOC(dependencies);

  for (j=0, k=0; j < adjointer->equations[equation].nrhsdeps; j++)
  {
    if (j == nonlinear_idx) continue;

    memcpy(&variables[k], &adjointer->equations[equation].rhsdeps[j], sizeof(adj_variable));
    ierr = adj_get_variable_value(adjointer, variables[k], &(dependencies[k]));
    if (ierr != ADJ_OK) return adj_chkierr_auto(ierr);

    k++;
  }

  adjointer->equations[equation].rhs_callback((void*) adjointer, adjointer->equations[equation].variable, nrhsdeps, variables, dependencies, adjointer->equations[equation].rhs_context, output, has_output);

  free(variables);
  free(dependencies);
  return ADJ_OK;
}

int adj_evaluate_rhs_derivative_action(adj_adjointer* adjointer, adj_equation source_eqn, adj_variable diff_var, adj_vector contraction, int hermitian, adj_vector* output, int* has_output)
{
  int nrhsdeps;
  int j;
  int ierr;
  adj_variable* variables;
  adj_vector* dependencies;

  if (source_eqn.rhs_deriv_action_callback == NULL)
  {
    char buf[255];
    adj_variable_str(source_eqn.variable, buf, 255);
    snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "Need the derivative action callback for the source term associated with the forward equation for %s.", buf);
    return adj_chkierr_auto(ADJ_ERR_NEED_CALLBACK);
  }

  nrhsdeps = source_eqn.nrhsdeps;

  variables = (adj_variable*) malloc(nrhsdeps * sizeof(adj_variable));
  ADJ_CHKMALLOC(variables);
  dependencies = (adj_vector*) malloc(nrhsdeps * sizeof(adj_vector));
  ADJ_CHKMALLOC(dependencies);

  for (j=0; j < nrhsdeps; j++)
  {
    memcpy(&variables[j], &source_eqn.rhsdeps[j], sizeof(adj_variable));
    ierr = adj_get_variable_value(adjointer, variables[j], &(dependencies[j]));
    if (ierr != ADJ_OK) return adj_chkierr_auto(ierr);
  }

  source_eqn.rhs_deriv_action_callback((void*) adjointer, source_eqn.variable, nrhsdeps, variables, dependencies, diff_var, contraction, hermitian, source_eqn.rhs_context, output, has_output);

  free(variables);
  free(dependencies);
  return ADJ_OK;

}

int adj_evaluate_rhs_second_derivative_action(adj_adjointer* adjointer, adj_equation source_eqn, adj_variable inner_var, adj_vector inner_contraction, adj_variable outer_var, int hermitian, adj_vector action, adj_vector* output, int* has_output)
{
  int nrhsdeps;
  int j;
  int ierr;
  adj_variable* variables;
  adj_vector* dependencies;

  if (source_eqn.rhs_second_deriv_action_callback == NULL)
  {
    char buf[255];
    adj_variable_str(source_eqn.variable, buf, 255);
    snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "Need the second derivative action callback for the source term associated with the forward equation for %s.", buf);
    return adj_chkierr_auto(ADJ_ERR_NEED_CALLBACK);
  }

  nrhsdeps = source_eqn.nrhsdeps;

  variables = (adj_variable*) malloc(nrhsdeps * sizeof(adj_variable));
  ADJ_CHKMALLOC(variables);
  dependencies = (adj_vector*) malloc(nrhsdeps * sizeof(adj_vector));
  ADJ_CHKMALLOC(dependencies);

  for (j=0; j < nrhsdeps; j++)
  {
    memcpy(&variables[j], &source_eqn.rhsdeps[j], sizeof(adj_variable));
    ierr = adj_get_variable_value(adjointer, variables[j], &(dependencies[j]));
    if (ierr != ADJ_OK) return adj_chkierr_auto(ierr);
  }

  source_eqn.rhs_second_deriv_action_callback((void*) adjointer, source_eqn.variable, nrhsdeps, variables, dependencies, inner_var, inner_contraction, outer_var, hermitian, action, source_eqn.rhs_context, output, has_output);

  free(variables);
  free(dependencies);
  return ADJ_OK;

}

int adj_evaluate_rhs_derivative_assembly(adj_adjointer* adjointer, adj_equation source_eqn, int hermitian, adj_matrix* output)
{
  int nrhsdeps;
  int j;
  int ierr;
  adj_variable* variables;
  adj_vector* dependencies;

  if (source_eqn.rhs_deriv_assembly_callback == NULL)
  {
    char buf[255];
    adj_variable_str(source_eqn.variable, buf, 255);
    snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "Need the derivative assembly callback for the source term associated with the forward equation for %s.", buf);
    return adj_chkierr_auto(ADJ_ERR_NEED_CALLBACK);
  }

  nrhsdeps = source_eqn.nrhsdeps;

  variables = (adj_variable*) malloc(nrhsdeps * sizeof(adj_variable));
  ADJ_CHKMALLOC(variables);
  dependencies = (adj_vector*) malloc(nrhsdeps * sizeof(adj_vector));
  ADJ_CHKMALLOC(dependencies);

  for (j=0; j < nrhsdeps; j++)
  {
    memcpy(&variables[j], &source_eqn.rhsdeps[j], sizeof(adj_variable));
    ierr = adj_get_variable_value(adjointer, variables[j], &(dependencies[j]));
    if (ierr != ADJ_OK) return adj_chkierr_auto(ierr);
  }

  source_eqn.rhs_deriv_assembly_callback((void*) adjointer, source_eqn.variable, nrhsdeps, variables, dependencies, hermitian, source_eqn.rhs_context, output);

  free(variables);
  free(dependencies);
  return ADJ_OK;
}

int adj_evaluate_parameter_source(adj_adjointer* adjointer, int equation, adj_variable variable, char* parameter, adj_vector* output, int* has_output)
{
  int ierr;
  int ndepends = 0;
  void (*parameter_source_func)(adj_adjointer* adjointer, int equation, adj_variable variable, int ndepends, adj_variable* variables, adj_vector* dependencies, char* parameter, adj_vector* output, int* has_output) = NULL;
  adj_vector* dependencies = NULL;
  adj_variable* variables = NULL;

  ierr = adj_find_parameter_source_callback(adjointer, parameter, &parameter_source_func);
  if (ierr != ADJ_OK)
    return adj_chkierr_auto(ierr);

  /* at the moment, we assume that the parameter source has no dependencies */

  /* We have the right callback, so let's call it already */ 
  parameter_source_func(adjointer, equation, variable, ndepends, variables, dependencies, parameter, output, has_output);

  return ADJ_OK;
}

