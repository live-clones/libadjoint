#include "libadjoint/adj_adjointer_routines.h"

/* int adj_create_adjointer(adj_adjointer* adjointer);
int adj_destroy_adjointer(adj_adjointer* adjointer);
int adj_set_option(adj_adjointer* adjointer, int option, int choice);
int adj_equation_count(adj_adjointer* adjointer, int* count);
int adj_register_equation(adj_adjointer* adjointer, adj_variable var, int nblocks, adj_block* blocks, adj_variable* targets, int nrhsdeps, adj_variable* rhsdeps);
int adj_record_variable(adj_adjointer* adjointer, adj_variable var, adj_vector value);
int adj_record_auxiliary(adj_adjointer* adjointer, adj_variable var, adj_vector value);
int adj_register_operator_callback(adj_adjointer* adjointer, int type, char* name, void (*fn)(void));
int adj_register_data_callback(adj_adjointer* adjointer, int type, void (*fn)(void));
int adj_forget_adjoint_equation(adj_adjointer* adjointer, int equation);
int adj_find_operator_callback(adj_adjointer* adjointer, int type, char* name, void (**fn)(void)); */

int adj_get_variable_value(adj_adjointer* adjointer, adj_variable var, adj_vector* value)
{
  int ierr;
  adj_variable_data data;

  ierr = adj_find_variable_data(adjointer->varhash, &var, &data);
  if (ierr != ADJ_ERR_OK)
  {
    char buf[255];
    adj_variable_str(var, buf, 255);

    ierr = ADJ_ERR_NEED_VALUE;
    snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "Need a value for %s, but don't have one recorded.", buf);
    return ierr;
  }

  *value = data.value;
  return ADJ_ERR_OK;
}

int adj_forget_variable_value(adj_adjointer* adjointer, adj_variable_data* data)
{
  if (adjointer->callbacks.vec_destroy == NULL)
  {
    strncpy(adj_error_msg, "Need vec_destroy data callback.", ADJ_ERROR_MSG_BUF);
    return ADJ_ERR_NEED_CALLBACK;
  }
  data->has_value = 0;
  adjointer->callbacks.vec_destroy(data->value);
  return ADJ_ERR_OK;
}

int adj_destroy_variable_data(adj_adjointer* adjointer, adj_variable_data* data)
{

  assert(data->nadjoint_equations > 0);

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

  if (data->has_value)
    return adj_forget_variable_value(adjointer, data);

  return ADJ_ERR_OK;
}
