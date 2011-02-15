#include "libadjoint/adj_adjointer_routines.h"

int adj_create_adjointer(adj_adjointer* adjointer)
{
  int i;

  adjointer->nequations = 0;
  adjointer->equations_sz = 0;
  adjointer->equations = NULL;

  adjointer->ntimesteps = 0;
  adjointer->timestep_data = NULL;

  adjointer->varhash = NULL;
  adjointer->vardata.firstnode = NULL;
  adjointer->vardata.lastnode = NULL;

  adjointer->callbacks.vec_duplicate = NULL;
  adjointer->callbacks.vec_axpy = NULL;
  adjointer->callbacks.vec_destroy = NULL;
  adjointer->callbacks.vec_setvalues = NULL;
  adjointer->callbacks.vec_getsize = NULL;
  adjointer->callbacks.vec_divide = NULL;

  adjointer->callbacks.mat_duplicate = NULL;
  adjointer->callbacks.mat_axpy = NULL;
  adjointer->callbacks.mat_destroy = NULL;

  adjointer->nonlinear_colouring_list.firstnode = NULL;
  adjointer->nonlinear_colouring_list.lastnode = NULL;
  adjointer->nonlinear_action_list.firstnode = NULL;
  adjointer->nonlinear_action_list.lastnode = NULL;
  adjointer->nonlinear_derivative_action_list.firstnode = NULL;
  adjointer->nonlinear_derivative_action_list.lastnode = NULL;
  adjointer->nonlinear_derivative_assembly_list.firstnode = NULL;
  adjointer->nonlinear_derivative_assembly_list.lastnode = NULL;
  adjointer->block_action_list.firstnode = NULL;
  adjointer->block_action_list.lastnode = NULL;
  adjointer->block_assembly_list.firstnode = NULL;
  adjointer->block_assembly_list.lastnode = NULL;

  for (i = 0; i < ADJ_NO_OPTIONS; i++)
    adjointer->options[i] = 0; /* 0 is the default for all options */

  return ADJ_ERR_OK;
}

int adj_destroy_adjointer(adj_adjointer* adjointer)
{
  int i;
  int j;
  int ierr;
  adj_variable_data* data_ptr;
  adj_variable_data* data_ptr_tmp;
  adj_op_callback* cb_ptr;
  adj_op_callback* cb_ptr_tmp;

  for (i = 0; i < adjointer->nequations; i++)
  {
    ierr = adj_destroy_equation(&(adjointer->equations[i]));
    if (ierr != ADJ_ERR_OK) return ierr;
  }
  if (adjointer->equations != NULL) free(adjointer->equations);

  if (adjointer->timestep_data != NULL)
  {
    for (i = 0; i < adjointer->ntimesteps; i++)
      for (j = 0; j < adjointer->timestep_data[i].nfunctionals; j++)
        if (adjointer->timestep_data[i].functional_data[j].dependencies != NULL) 
          free(adjointer->timestep_data[i].functional_data[j].dependencies);
    free(adjointer->timestep_data);
  }

  data_ptr = adjointer->vardata.firstnode;
  while (data_ptr != NULL)
  {
    ierr = adj_destroy_variable_data(adjointer, data_ptr);
    if (ierr != ADJ_ERR_OK) return ierr;
    data_ptr_tmp = data_ptr;
    data_ptr = data_ptr->next;
    free(data_ptr_tmp);
  }

  cb_ptr = adjointer->nonlinear_colouring_list.firstnode;
  while(cb_ptr != NULL)
  {
    cb_ptr_tmp = cb_ptr;
    cb_ptr = cb_ptr->next;
    free(cb_ptr_tmp);
  }

  cb_ptr = adjointer->nonlinear_action_list.firstnode;
  while(cb_ptr != NULL)
  {
    cb_ptr_tmp = cb_ptr;
    cb_ptr = cb_ptr->next;
    free(cb_ptr_tmp);
  }

  cb_ptr = adjointer->nonlinear_derivative_action_list.firstnode;
  while(cb_ptr != NULL)
  {
    cb_ptr_tmp = cb_ptr;
    cb_ptr = cb_ptr->next;
    free(cb_ptr_tmp);
  }

  cb_ptr = adjointer->nonlinear_derivative_assembly_list.firstnode;
  while(cb_ptr != NULL)
  {
    cb_ptr_tmp = cb_ptr;
    cb_ptr = cb_ptr->next;
    free(cb_ptr_tmp);
  }

  cb_ptr = adjointer->block_action_list.firstnode;
  while(cb_ptr != NULL)
  {
    cb_ptr_tmp = cb_ptr;
    cb_ptr = cb_ptr->next;
    free(cb_ptr_tmp);
  }

  cb_ptr = adjointer->block_assembly_list.firstnode;
  while(cb_ptr != NULL)
  {
    cb_ptr_tmp = cb_ptr;
    cb_ptr = cb_ptr->next;
    free(cb_ptr_tmp);
  }

  adj_create_adjointer(adjointer);
  return ADJ_ERR_OK;
}

int adj_register_equation(adj_adjointer* adjointer, adj_equation equation)
{
  adj_variable_data* data_ptr;
  int ierr;
  int i;
  int j;

  if (adjointer->options[ADJ_ACTIVITY] == ADJ_ACTIVITY_NOTHING) return ADJ_ERR_OK;

  /* Let's check we haven't solved for this variable before */
  ierr = adj_find_variable_data(&(adjointer->varhash), &(equation.variable), &data_ptr);
  if (ierr != ADJ_ERR_HASH_FAILED)
  {
    char buf[ADJ_NAME_LEN];
    adj_variable_str(equation.variable, buf, ADJ_NAME_LEN);
    snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "We have already registered an equation for variable %s.", buf);
    return ADJ_ERR_INVALID_INPUTS;
  }

  /* Let's check the timesteps match up */
  if (adjointer->nequations == 0) /* we haven't registered any equations yet */
  {
    if (equation.variable.timestep != 0) /* this isn't timestep 0 */
    {
      strncpy(adj_error_msg, "The first equation registered must have timestep 0.", ADJ_ERROR_MSG_BUF);
      return ADJ_ERR_INVALID_INPUTS;
    }
  }
  else /* we have registered an equation before */
  {
    int old_timestep;
    /* if  (not same timestep as before)                     &&  (not the next timestep) */
    old_timestep = adjointer->equations[adjointer->nequations-1].variable.timestep;
    if ((equation.variable.timestep != old_timestep) && (equation.variable.timestep != old_timestep+1))
    {
      snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, \
          "Timestep numbers must either stay the same or increment by one. Valid values are %d or %d, but you have supplied %d.", \
          old_timestep, old_timestep+1, equation.variable.timestep);
      return ADJ_ERR_INVALID_INPUTS;
    }
  }

  /* OK. We're good to go. */

  /* Let's add it to the hash table. */
  ierr = adj_add_new_hash_entry(adjointer, &(equation.variable), &data_ptr);
  if (ierr != ADJ_ERR_OK) return ierr;
  data_ptr->equation = adjointer->nequations + 1;
  /* OK. Next create an entry for the adj_equation in the adjointer. */

  /* Check we have enough room, and if not, make some */
  if (adjointer->nequations == adjointer->equations_sz)
  {
    adjointer->equations = (adj_equation*) realloc(adjointer->equations, (adjointer->equations_sz + ADJ_PREALLOC_SIZE) * sizeof(adj_equation));
    adjointer->equations_sz = adjointer->equations_sz + ADJ_PREALLOC_SIZE;
  }

  adjointer->nequations++;
  adjointer->equations[adjointer->nequations - 1] = equation;

  /* Do any necessary recording of timestep indices */
  if (adjointer->ntimesteps < equation.variable.timestep + 1) /* adjointer->ntimesteps should be at least equation.variable.timestep + 1 */
  {
    adj_extend_timestep_data(adjointer, equation.variable.timestep + 1); /* extend the array as necessary */
    adjointer->timestep_data[equation.variable.timestep].start_equation = adjointer->nequations - 1; /* and fill in the start equation */
  }

  /* now we have copies of the pointer to the arrays of targets, blocks, rhs deps. */
  /* but for consistency, any libadjoint object that the user creates, he must destroy --
     it's simpler that way. */
  /* so we're going to make our own copies, so that the user can destroy his. */
  adjointer->equations[adjointer->nequations - 1].blocks = (adj_block*) malloc(equation.nblocks * sizeof(adj_block));
  memcpy(adjointer->equations[adjointer->nequations - 1].blocks, equation.blocks, equation.nblocks * sizeof(adj_block));
  adjointer->equations[adjointer->nequations - 1].targets = (adj_variable*) malloc(equation.nblocks * sizeof(adj_variable));
  memcpy(adjointer->equations[adjointer->nequations - 1].targets, equation.targets, equation.nblocks * sizeof(adj_variable));
  if (equation.nrhsdeps > 0)
  {
    adjointer->equations[adjointer->nequations - 1].rhsdeps = (adj_variable*) malloc(equation.nrhsdeps * sizeof(adj_variable));
    memcpy(adjointer->equations[adjointer->nequations - 1].rhsdeps, equation.rhsdeps, equation.nrhsdeps * sizeof(adj_variable));
  }

  /* Now find all the entries we need to update in the hash table, and update them */
  /* First: targeting equations */

  for (i = 0; i < equation.nblocks; i++)
  {
    ierr = adj_find_variable_data(&(adjointer->varhash), &(equation.targets[i]), &data_ptr);
    if (ierr != ADJ_ERR_OK)
    {
      char buf[ADJ_NAME_LEN];
      adj_variable_str(equation.targets[i], buf, ADJ_NAME_LEN);
      snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "The equation to be registered has a block is targeting %s, but I do not have an equation for that variable yet.", buf);
      return ierr;
    }

    /* this is already guaranteed to be a unique entry -- we have never seen this equation before.
       so we don't need adj_append_unique */
    data_ptr->ntargeting_equations++;
    data_ptr->targeting_equations = (int*) realloc(data_ptr->targeting_equations, data_ptr->ntargeting_equations * sizeof(int));
    data_ptr->targeting_equations[data_ptr->ntargeting_equations - 1] = adjointer->nequations - 1;
  }

  /* Next: nonlinear dependencies of the right hand side */
  for (i = 0; i < equation.nrhsdeps; i++)
  {
    ierr = adj_find_variable_data(&(adjointer->varhash), &(equation.rhsdeps[i]), &data_ptr);
    if (ierr == ADJ_ERR_HASH_FAILED && equation.rhsdeps[i].auxiliary)
    {
      /* It's ok if it's auxiliary -- it legitimately can be the first time we've seen it */
      adj_variable_data* new_data;
      ierr = adj_add_new_hash_entry(adjointer, &(equation.rhsdeps[i]), &new_data);
      if (ierr != ADJ_ERR_OK) return ierr;
      new_data->equation = -1; /* it doesn't have an equation */
      data_ptr = new_data;
    }
    else
    {
      return ierr;
    }

    /* Now data_ptr points to the data we're storing */
    adj_append_unique(&(data_ptr->rhs_equations), &(data_ptr->nrhs_equations), adjointer->nequations - 1);
  }

  /* Now we need to record what we need for which adjoint equation from the rhs dependencies */
  /* We do this after the previous loop to make sure that all of the dependencies exist in the hash table. */
  for (i = 0; i < equation.nrhsdeps; i++)
  {
    /* We will loop through all other dependencies and register that the adjoint equation
       associated with equation.rhsdeps[i] depends on all other equation.rhsdeps[j] */
    int eqn_no;

    if (equation.rhsdeps[i].auxiliary)
    {
      /* We don't solve any equation for it, and auxiliary variables thus don't have any associated
         adjoint equation -- so we don't need to register any dependencies for it */
      continue;
    }

    ierr = adj_find_variable_data(&(adjointer->varhash), &(equation.rhsdeps[i]), &data_ptr);
    if (ierr != ADJ_ERR_OK) return ierr;

    eqn_no = data_ptr->equation;
    assert(eqn_no >= 0);

    for (j = 0; j < equation.nrhsdeps; j++)
    {
      ierr = adj_find_variable_data(&(adjointer->varhash), &(equation.rhsdeps[j]), &data_ptr);
      if (ierr != ADJ_ERR_OK) return ierr;
      adj_append_unique(&(data_ptr->adjoint_equations), &(data_ptr->nadjoint_equations), eqn_no); /* dependency j is necessary for equation i */
    }
  }

  /* And finally nonlinear dependencies of the left hand side */

  for (i = 0; i< equation.nblocks; i++)
  {
    if (equation.blocks[i].has_nonlinear_block)
    {
      for (j = 0; j < equation.blocks[i].nonlinear_block.ndepends; j++)
      {
        /* Register that this equation depends on this variable */
        ierr = adj_find_variable_data(&(adjointer->varhash), &(equation.blocks[i].nonlinear_block.depends[j]), &data_ptr);
        if (ierr == ADJ_ERR_HASH_FAILED && equation.blocks[i].nonlinear_block.depends[j].auxiliary)
        {
          /* It's ok if it's auxiliary -- it legitimately can be the first time we've seen it */
          adj_variable_data* new_data;
          ierr = adj_add_new_hash_entry(adjointer, &(equation.blocks[i].nonlinear_block.depends[j]), &new_data);
          if (ierr != ADJ_ERR_OK) return ierr;
          new_data->equation = -1; /* it doesn't have an equation */
          data_ptr = new_data;
        }
        else if (ierr == ADJ_ERR_HASH_FAILED)
        {
          return ierr;
        }
        adj_append_unique(&(data_ptr->depending_equations), &(data_ptr->ndepending_equations), adjointer->nequations - 1);
      }
    }
  }

  /* And now perform the updates for .adjoint_equations implied by the existence of these dependencies. 
     This is probably the hardest, most mindbending thing in the whole library -- sorry. There's no way
     to really make this easy; you just have to work through it. */
  for (i = 0; i < equation.nblocks; i++)
  {
    if (equation.blocks[i].has_nonlinear_block)
    {
      adj_variable_data* block_target_data; /* fetch the hash entry associated with the target of this block */
      ierr = adj_find_variable_data(&(adjointer->varhash), &(equation.targets[i]), &block_target_data);
      if (ierr != ADJ_ERR_OK) return ierr;

      for (j = 0; j < equation.blocks[i].nonlinear_block.ndepends; j++)
      {
        int k;
        adj_variable_data* j_data;

        /* j_data ALWAYS refers to the data associated with the j'th dependency, throughout this whole loop */
        ierr = adj_find_variable_data(&(adjointer->varhash), &(equation.blocks[i].nonlinear_block.depends[j]), &j_data);
        if (ierr != ADJ_ERR_OK) return ierr;

        /* One set of dependencies: the (adjoint equation of) (the target of this block) (needs) (this dependency) */
        adj_append_unique(&(j_data->adjoint_equations), &(j_data->nadjoint_equations), block_target_data->equation);

        /* Another set of dependencies: the (adjoint equation of) (the j'th dependency) (needs) (the target of this block) */
        adj_append_unique(&(block_target_data->adjoint_equations), &(block_target_data->nadjoint_equations), j_data->equation);

        /* Now we loop over all the dependencies again and fill in the cross-dependencies */
        for (k = 0; k < equation.blocks[i].nonlinear_block.ndepends; k++)
        {
          adj_variable_data* k_data;

          /* k_data ALWAYS refers to the data associated with the k'th dependency, throughout this whole loop */
          ierr = adj_find_variable_data(&(adjointer->varhash), &(equation.blocks[i].nonlinear_block.depends[k]), &k_data);
          if (ierr != ADJ_ERR_OK) return ierr;

          /* Another set of dependencies: the (adjoint equation of) (the j'th dependency) (needs) (the k'th dependency) */
          adj_append_unique(&(k_data->adjoint_equations), &(k_data->nadjoint_equations), j_data->equation);
        }
      }
    }
  }
  return ADJ_ERR_OK;
}

int adj_set_option(adj_adjointer* adjointer, int option, int choice)
{
  if (option < 0 || option >= ADJ_NO_OPTIONS)
  {
    strncpy(adj_error_msg, "Unknown option.", ADJ_ERROR_MSG_BUF);
    return ADJ_ERR_INVALID_INPUTS;
  }

  adjointer->options[option] = choice;
  return ADJ_ERR_OK;
}

int adj_equation_count(adj_adjointer* adjointer, int* count)
{
  *count = adjointer->nequations;
  return ADJ_ERR_OK;
}

int adj_record_variable(adj_adjointer* adjointer, adj_variable var, adj_storage_data storage)
{
  adj_variable_data* data_ptr;
  int ierr;

  if (adjointer->options[ADJ_ACTIVITY] == ADJ_ACTIVITY_NOTHING) return ADJ_ERR_OK;

  ierr = adj_find_variable_data(&(adjointer->varhash), &var, &data_ptr);
  if (ierr != ADJ_ERR_OK && !var.auxiliary)
  {
    char buf[255];
    adj_variable_str(var, buf, 255);
    snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, \
        "Tried to record a value for variable %s, but couldn't look it up in the hash table.", buf);
    return ierr;
  }

  if (ierr != ADJ_ERR_OK && var.auxiliary)
  {
    /* If the variable is auxiliary, it's alright that this is the first time we've ever seen it */
    adj_variable_data* new_data;
    ierr = adj_add_new_hash_entry(adjointer, &var, &new_data);
    if (ierr != ADJ_ERR_OK) return ierr;
    new_data->equation = -1; /* it doesn't have an equation */
    data_ptr = new_data;
  }

  if (data_ptr->storage.has_value)
  {
    char buf[ADJ_NAME_LEN];
    adj_variable_str(var, buf, ADJ_NAME_LEN);
    snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "Variable %s already has a value.", buf);
    return ADJ_ERR_INVALID_INPUTS;
  }

  /* Just in case */
  strncpy(adj_error_msg, "Need data callback.", ADJ_ERROR_MSG_BUF);

  switch (storage.storage_type)
  {
    case ADJ_STORAGE_MEMORY:
      if (adjointer->callbacks.vec_duplicate == NULL) return ADJ_ERR_NEED_CALLBACK;
      if (adjointer->callbacks.vec_axpy == NULL) return ADJ_ERR_NEED_CALLBACK;
      data_ptr->storage.storage_type = ADJ_STORAGE_MEMORY;
      data_ptr->storage.has_value = storage.has_value;
      adjointer->callbacks.vec_duplicate(storage.value, &(data_ptr->storage.value));
      adjointer->callbacks.vec_axpy(&(data_ptr->storage.value), (adj_scalar)1.0, storage.value);
      break;
    default:
      strncpy(adj_error_msg, "Storage types other than ADJ_STORAGE_MEMORY are not implemented yet.", ADJ_ERROR_MSG_BUF);
      return ADJ_ERR_NOT_IMPLEMENTED;
  }

  return ADJ_ERR_OK;
}


int adj_register_operator_callback(adj_adjointer* adjointer, int type, char* name, void (*fn)(void))
{
  adj_op_callback_list* cb_list_ptr;
  adj_op_callback* cb_ptr;

  if (adjointer->options[ADJ_ACTIVITY] == ADJ_ACTIVITY_NOTHING) return ADJ_ERR_OK;

  switch(type)
  {
    case ADJ_NBLOCK_COLOURING_CB:
      cb_list_ptr = &(adjointer->nonlinear_colouring_list);
      break;
    case ADJ_NBLOCK_ACTION_CB:
      cb_list_ptr = &(adjointer->nonlinear_action_list);
      break;
    case ADJ_NBLOCK_DERIVATIVE_ACTION_CB:
      cb_list_ptr = &(adjointer->nonlinear_derivative_action_list);
      break;
    case ADJ_NBLOCK_DERIVATIVE_ASSEMBLY_CB:
      cb_list_ptr = &(adjointer->nonlinear_derivative_assembly_list);
      break;
    case ADJ_BLOCK_ACTION_CB:
      cb_list_ptr = &(adjointer->block_action_list);
      break;
    case ADJ_BLOCK_ASSEMBLY_CB:
      cb_list_ptr = &(adjointer->block_assembly_list);
      break;
    default:
      strncpy(adj_error_msg, "Unknown callback type.", ADJ_ERROR_MSG_BUF);
      return ADJ_ERR_INVALID_INPUTS;
  }
  /* First, we look for an existing callback data structure that might already exist, to replace the function */
  cb_ptr = cb_list_ptr->firstnode;
  while (cb_ptr != NULL)
  {
    if (strncmp(cb_ptr->name, name, ADJ_NAME_LEN) == 0)
    {
      cb_ptr->callback = fn;
      return ADJ_ERR_OK;
    }
    cb_ptr = cb_ptr->next;
  }

  /* If we got here, that means that we didn't find it. Tack it on to the end of the list. */
  cb_ptr = (adj_op_callback*) malloc(sizeof(adj_op_callback));
  strncpy(cb_ptr->name, name, ADJ_NAME_LEN);
  cb_ptr->callback = fn;

  /* Special case for the first callback */
  if (cb_list_ptr->firstnode == NULL)
  {
    cb_list_ptr->firstnode = cb_ptr;
    cb_list_ptr->lastnode = cb_ptr;
  }
  else
  {
    cb_list_ptr->lastnode->next = cb_ptr;
    cb_list_ptr->lastnode = cb_ptr;
  }

  return ADJ_ERR_OK;
}

int adj_register_data_callback(adj_adjointer* adjointer, int type, void (*fn)(void))
{
  if (adjointer->options[ADJ_ACTIVITY] == ADJ_ACTIVITY_NOTHING) return ADJ_ERR_OK;

  switch (type)
  {
    case ADJ_VEC_DUPLICATE_CB:
      adjointer->callbacks.vec_duplicate = (void(*)(adj_vector x, adj_vector *newx)) fn;
      break;
    case ADJ_VEC_AXPY_CB:
      adjointer->callbacks.vec_axpy = (void(*)(adj_vector *y, adj_scalar alpha, adj_vector x)) fn;
      break;
    case ADJ_VEC_DESTROY_CB:
      adjointer->callbacks.vec_destroy = (void(*)(adj_vector*)) fn;
      break;
    case ADJ_VEC_DIVIDE_CB:
      adjointer->callbacks.vec_divide = (void(*)(adj_vector *numerator, adj_vector denominator)) fn;
      break;
    case ADJ_VEC_SETVALUES_CB:
      adjointer->callbacks.vec_setvalues = (void(*)(adj_vector *vec, adj_scalar scalars[])) fn;
      break;
    case ADJ_VEC_GETSIZE_CB:
      adjointer->callbacks.vec_getsize = (void(*)(adj_vector vec, int *sz)) fn;
      break;

    case ADJ_MAT_DUPLICATE_CB:
      adjointer->callbacks.mat_duplicate = (void(*)(adj_matrix matin, adj_matrix *matout)) fn;
      break;
    case ADJ_MAT_AXPY_CB:
      adjointer->callbacks.mat_axpy = (void(*)(adj_matrix *Y, adj_scalar alpha, adj_matrix X)) fn;
      break;
    case ADJ_MAT_DESTROY_CB:
      adjointer->callbacks.mat_destroy = (void(*)(adj_matrix *mat)) fn;
      break;

   default:
      snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "Unknown data callback type %d.", type);
      return ADJ_ERR_INVALID_INPUTS;
  }

  return ADJ_ERR_OK;
}

int adj_forget_adjoint_equation(adj_adjointer* adjointer, int equation)
{
  adj_variable_data* data;
  int should_we_delete;
  int i;
  int ierr;

  if (adjointer->options[ADJ_ACTIVITY] == ADJ_ACTIVITY_NOTHING) return ADJ_ERR_OK;

  data = adjointer->vardata.firstnode;
  while (data != NULL)
  {
    if (data->storage.has_value && data->nadjoint_equations > 0)
    {
      should_we_delete = 1;
      for (i = 0; i < data->nadjoint_equations; i++)
      {
        if (equation > data->adjoint_equations[i])
        {
          should_we_delete = 0;
          break;
        }
      }

      if (should_we_delete)
      {
        ierr = adj_forget_variable_value(adjointer, data);
        if (ierr != ADJ_ERR_OK) return ierr;
      }
    }

    data = data->next;
  }

  return ADJ_ERR_OK;
}

int adj_find_operator_callback(adj_adjointer* adjointer, int type, char* name, void (**fn)(void))
{
  adj_op_callback_list* cb_list_ptr;
  adj_op_callback* cb_ptr;

  switch(type)
  {
    case ADJ_NBLOCK_COLOURING_CB:
      cb_list_ptr = &(adjointer->nonlinear_colouring_list);
      break;
    case ADJ_NBLOCK_ACTION_CB:
      cb_list_ptr = &(adjointer->nonlinear_action_list);
      break;
    case ADJ_NBLOCK_DERIVATIVE_ACTION_CB:
      cb_list_ptr = &(adjointer->nonlinear_derivative_action_list);
      break;
    case ADJ_NBLOCK_DERIVATIVE_ASSEMBLY_CB:
      cb_list_ptr = &(adjointer->nonlinear_derivative_assembly_list);
      break;
    case ADJ_BLOCK_ACTION_CB:
      cb_list_ptr = &(adjointer->block_action_list);
      break;
    case ADJ_BLOCK_ASSEMBLY_CB:
      cb_list_ptr = &(adjointer->block_assembly_list);
      break;
    default:
      strncpy(adj_error_msg, "Unknown callback type.", ADJ_ERROR_MSG_BUF);
      return ADJ_ERR_INVALID_INPUTS;
  }

  cb_ptr = cb_list_ptr->firstnode;
  while (cb_ptr != NULL)
  {
    if (strncmp(cb_ptr->name, name, ADJ_NAME_LEN) == 0)
    {
      *fn = cb_ptr->callback;
      return ADJ_ERR_OK;
    }
    cb_ptr = cb_ptr->next;
  }

  snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "Could not find callback type %d for operator %s.", type, name);
  return ADJ_ERR_NEED_CALLBACK;
}

int adj_get_variable_value(adj_adjointer* adjointer, adj_variable var, adj_vector* value)
{
  int ierr;
  adj_variable_data* data_ptr;

  ierr = adj_find_variable_data(&(adjointer->varhash), &var, &data_ptr);
  if (ierr != ADJ_ERR_OK) return ierr;

  if (data_ptr->storage.storage_type != ADJ_STORAGE_MEMORY)
  {
    ierr = ADJ_ERR_NOT_IMPLEMENTED;
    snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "Sorry, storage strategies other than ADJ_STORAGE_MEMORY are not implemented yet.");
    return ierr;
  }

  if (!data_ptr->storage.has_value)
  {
    char buf[ADJ_NAME_LEN];
    adj_variable_str(var, buf, ADJ_NAME_LEN);

    ierr = ADJ_ERR_NEED_VALUE;
    snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "Need a value for %s, but don't have one recorded.", buf);
    return ierr;
  }

  *value = data_ptr->storage.value;
  return ADJ_ERR_OK;
}

int adj_forget_variable_value(adj_adjointer* adjointer, adj_variable_data* data)
{
  if (adjointer->callbacks.vec_destroy == NULL)
  {
    strncpy(adj_error_msg, "Need vec_destroy data callback.", ADJ_ERROR_MSG_BUF);
    return ADJ_ERR_NEED_CALLBACK;
  }

  assert(data->storage.has_value);

  data->storage.has_value = 0;
  adjointer->callbacks.vec_destroy(&(data->storage.value));
  return ADJ_ERR_OK;
}

int adj_destroy_variable_data(adj_adjointer* adjointer, adj_variable_data* data)
{

  if (data->ntargeting_equations > 0)
  {
    free(data->targeting_equations);
    data->ntargeting_equations = 0;
  }

  if (data->ndepending_equations > 0)
  {
    free(data->depending_equations);
    data->ndepending_equations = 0;
  }

  if (data->nrhs_equations > 0)
  {
    free(data->rhs_equations);
    data->nrhs_equations = 0;
  }

  if (data->nadjoint_equations > 0)
  {
    free(data->adjoint_equations);
    data->nadjoint_equations = 0;
  }

  if (data->storage.has_value)
    return adj_forget_variable_value(adjointer, data);

  return ADJ_ERR_OK;
}

adj_storage_data adj_storage_memory(adj_vector value)
{
  adj_storage_data data;

  data.has_value = 1;
  data.storage_type = ADJ_STORAGE_MEMORY;
  data.value = value;
  return data;
}

int adj_add_new_hash_entry(adj_adjointer* adjointer, adj_variable* var, adj_variable_data** data)
{
  int ierr;

  *data = (adj_variable_data*) malloc(sizeof(adj_variable_data));
  (*data)->next = NULL;
  (*data)->storage.has_value = 0;
  (*data)->ntargeting_equations = 0;
  (*data)->targeting_equations = NULL;
  (*data)->ndepending_equations = 0;
  (*data)->depending_equations = NULL;
  (*data)->nrhs_equations = 0;
  (*data)->rhs_equations = NULL;
  (*data)->nadjoint_equations = 0;
  (*data)->adjoint_equations = NULL;

  /* add to the hash table */
  ierr = adj_add_variable_data(&(adjointer->varhash), var, *data);
  if (ierr != ADJ_ERR_OK) return ierr;

  /* and add to the data list */
  if (adjointer->vardata.firstnode == NULL)
  {
    adjointer->vardata.firstnode = *data;
    adjointer->vardata.lastnode = *data;
  }
  else
  {
    adjointer->vardata.lastnode->next = *data;
    adjointer->vardata.lastnode = *data;
  }

  return ADJ_ERR_OK;
}

int adj_timestep_count(adj_adjointer* adjointer, int* count)
{
  *count = adjointer->ntimesteps;
  return ADJ_ERR_OK;
}

int adj_timestep_start(adj_adjointer* adjointer, int timestep, int* start)
{
  if (timestep < 0 || timestep >= adjointer->ntimesteps)
  {
    strncpy(adj_error_msg, "Invalid timestep supplied to adj_timestep_start.", ADJ_ERROR_MSG_BUF);
    return ADJ_ERR_INVALID_INPUTS;
  }

  *start = adjointer->timestep_data[timestep].start_equation;
  return ADJ_ERR_OK;
}

int adj_timestep_end(adj_adjointer* adjointer, int timestep, int* end)
{
  if (timestep < 0 || timestep >= adjointer->ntimesteps)
  {
    strncpy(adj_error_msg, "Invalid timestep supplied to adj_timestep_end.", ADJ_ERROR_MSG_BUF);
    return ADJ_ERR_INVALID_INPUTS;
  }

  if (timestep < adjointer->ntimesteps-1)
  {
    *end = adjointer->timestep_data[timestep+1].start_equation - 1;
  }
  else
  {
    *end = adjointer->nequations - 1;
  }
  return ADJ_ERR_OK;
}

void adj_append_unique(int** array, int* array_sz, int value)
{
  int i;

  for (i = 0; i < *array_sz; i++)
    if ((*array)[i] == value)
      return;

  /* So if we got here, we really do need to append it */
  *array_sz = *array_sz + 1;
  *array = (int*) realloc(*array, *array_sz * sizeof(int));
  (*array)[*array_sz - 1] = value;
  return;
}

void adj_extend_timestep_data(adj_adjointer* adjointer, int extent)
{
  /* We have an array adjointer->timestep_data, of size adjointer->ntimesteps.
     We want to realloc that to have size extent. We'll also need to zero/initialise
     all the timestep_data's we've just allocated. */
  int i;

  assert(extent > adjointer->ntimesteps);
  adjointer->timestep_data = realloc(adjointer->timestep_data, extent * sizeof(adj_timestep_data));
  for (i = adjointer->ntimesteps; i < extent; i++)
  {
    adjointer->timestep_data[i].start_equation = -1;
    adjointer->timestep_data[i].start_time = -666; /* I wish there was an easy way to get a NaN here */
    adjointer->timestep_data[i].end_time = -666;
    adjointer->timestep_data[i].nfunctionals = 0;
    adjointer->timestep_data[i].functional_data = NULL;
  }
  adjointer->ntimesteps = extent;
}
