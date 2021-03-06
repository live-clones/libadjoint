#include "libadjoint/adj_adjointer_visualisation.h"

void adj_html_css(FILE* fp)
{
  fprintf(fp, "<style type=\"text/css\">\n"
      "table.equations\n"
      "{ font-family: monospace;\n"
      "  font-weight: normal;\n"
      "  font-size: 11px;\n"
      "  color: #404040;\n"
      "  table-layout:fixed;\n"
      "  background-color: #fafafa;\n"
      "  border: 0px;\n"
      "  empty-cells: hide;\n"
      "  border-spacing: 0px;\n"
      "  margin-top: 0px;}\n"

      "table.equations td\n"
      "{ font-weight: normal;\n"
      "  font-size: 11px;\n"
      "  color: #404040;\n"
      "  background-color: lightgray;\n"
      "  text-align: left;\n"
      "  padding-left: 3px;\n"
      "  border:1px solid black;\n"
      "  height:40px;"
      "}\n"
      "table.equations th\n"
      "{\n"
      "  width:40px;\n"
      "  height:40px;\n"
      "}\n"

      "table.equations td.new_timestep\n"
      "{ border-top: 5px solid black;}\n"

      "table.equations td.new_iteration\n"
      "{ border-top: 2px solid black}\n"

      "table.equations td.diagonal\n"
      "{ background-color: lightgreen;}\n"

      ".box_rotate {\n"
      "         -moz-transform: rotate(310deg);  /* FF3.5+ */\n"
      "           -o-transform: rotate(310deg);  /* Opera 10.5 */\n"
      "      -webkit-transform: rotate(310deg);  /* Saf3.1+, Chrome */\n"
      "                 filter:  progid:DXImageTransform.Microsoft.BasicImage(rotation=-0.0698131701);  /* IE6,IE7 */\n"
      "              -ms-filter: 'progid:DXImageTransform.Microsoft.BasicImage(rotation=-0.0698131701)'; /* IE8 */\n"
      "}\n"

      ".redfont {\n"
      "     color:red;\n"
      "}\n"

      ".greenfont {\n"
      "     color:green;\n"
      "}\n"

      "</style>\n"
      );
}

void adj_html_header(FILE *fp)
{
  fprintf(fp, "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n");
  fprintf(fp, "<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\">\n");
  fprintf(fp, "<head>\n");
  adj_html_css(fp);
  fprintf(fp, "<title>Libadjoint HTML output</title>\n");
  fprintf(fp, "</head>\n");
  fprintf(fp, "<body>\n"
      );
}

void adj_html_footer(FILE *fp)
{
  fprintf(fp, "</body>\n"
        "</html>\n"
      );
}

void adj_html_table_begin(FILE* fp, char* args)
{
  fprintf(fp, "<table width=\"1\" class=\"equations\" %s>\n", args);
}

void adj_html_table_end(FILE* fp)
{
  fprintf(fp, "</table>\n");
}

void adj_html_write_row(FILE* fp, char* strings[], char* desc[], int nb_strings, int diag_index, char* klass)
{
  int i;
  for (i = 0; i < nb_strings; i++)
  {
    if (strlen(desc[i]))
      if (diag_index == i)
        fprintf(fp, "<td class=\"diagonal %s\"><div title=\"%s\">%s</div></td>\n", klass, desc[i], strings[i]);
      else
        fprintf(fp, "<td class=\"%s\"><div title=\"%s\">%s</div></td>\n", klass, desc[i], strings[i]);
    else
      fprintf(fp, "<td class=\"%s\"></td>\n", klass);
  }
}

/* The first columns are allocated by the auxiliary variables (as they might be targets of blocks as well) followed by the other variables */
int adj_html_find_column_index(adj_adjointer* adjointer, adj_variable* variable, int* col)
{
  int i;
  adj_equation* adj_eqn;
  char buf[ADJ_NAME_LEN];
  adj_variable_hash* varhash;

  /* Check for auxiliary variables */
  *col = 0;
  for (varhash = adjointer->varhash; varhash != NULL; varhash = (adj_variable_hash*) varhash->hh.next)
  {
    if (varhash->variable.auxiliary == ADJ_TRUE)
    {
      if (adj_variable_equal(&varhash->variable, variable, 1))
        return ADJ_OK;
      *col = *col+1;
    }
  }

  /* Check for variables that are solved in an equation */
  for (i=0; i < adjointer->nequations; i++)
  {
    adj_eqn = &adjointer->equations[i];
    if (adj_variable_equal(&adj_eqn->variable, variable, 1))
      return ADJ_OK;
    *col = *col+1;
  }

  adj_variable_str(*variable, buf, ADJ_NAME_LEN);
  snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "Could not find the column index for variable %s.", buf);
  return adj_chkierr_auto(ADJ_ERR_INVALID_INPUTS);
}

void adj_html_print_statistics(FILE* fp, adj_adjointer* adjointer)
{
  fprintf(fp, "<h1>General information</h1>");
  fprintf(fp, "<p>Number of timesteps: %i</p>", adjointer->ntimesteps);
  fprintf(fp, "<p>Number of registered equations: %i</p>", adjointer->nequations);
}

int adj_html_count_auxiliary_variables(adj_adjointer* adjointer)
{
  adj_variable_hash* varhash;
  int n=0;

  for (varhash = adjointer->varhash; varhash != NULL; varhash = (adj_variable_hash*) varhash->hh.next)
  {
    if (varhash->variable.auxiliary == ADJ_TRUE)
      n++;
  }
  return n;
}

void adj_html_print_auxiliary_variables(FILE* fp, adj_adjointer* adjointer)
{
  adj_variable_hash* varhash;
  char adj_name[ADJ_NAME_LEN];
  int ierr;

  fprintf(fp, "<h1>Auxiliary variables</h1>");

  for (varhash = adjointer->varhash; varhash != NULL; varhash = (adj_variable_hash*) varhash->hh.next)
  {

    if (varhash->variable.auxiliary == ADJ_TRUE)
    {
      ierr = adj_has_variable_value(adjointer, varhash->variable);
      /* Green color -> Variable is recorded, red otherwise */
      if (ierr != ADJ_OK)
        fprintf(fp, "<span class=\"redfont\">");
      else
        fprintf(fp, "<span class=\"greenfont\">");

      adj_variable_str(varhash->variable, adj_name, ADJ_NAME_LEN);
      fprintf(fp, "%s</span> ", encode_html(adj_name));
    }

  }

}

void adj_html_print_callback_information(FILE* fp, adj_adjointer* adjointer)
{
  adj_op_callback* cb_ptr;
  adj_func_callback* func_cb_ptr;
  adj_func_deriv_callback* func_deriv_cb_ptr;
  fprintf(fp, "<h1>Callback information</h1>");

  fprintf(fp, "<h2>Data callbacks</h2>");

  if (adjointer->callbacks.vec_duplicate == NULL)
    fprintf(fp, "<div class=\"redfont\" title='Not registered'>vec_duplicate</div>");
  else
    fprintf(fp, "<div class=\"greenfont\">vec_duplicate</div>");

  if (adjointer->callbacks.vec_axpy == NULL)
    fprintf(fp, "<div class=\"redfont\" title='Not registered'>vec_axpy</div>");
  else
    fprintf(fp, "<div class=\"greenfont\">vec_axpy</div>");

  if (adjointer->callbacks.vec_destroy == NULL)
    fprintf(fp, "<div class=\"redfont\" title='Not registered'>vec_destroy</div>");
  else
    fprintf(fp, "<div class=\"greenfont\">vec_destroy</div>");

  if (adjointer->callbacks.vec_set_values == NULL)
    fprintf(fp, "<div class=\"redfont\" title='Not registered'>vec_set_values</div>");
  else
    fprintf(fp, "<div class=\"greenfont\">vec_set_values</div>");

  if (adjointer->callbacks.vec_get_size == NULL)
    fprintf(fp, "<div class=\"redfont\" title='Not registered'>vec_get_size</div>");
  else
    fprintf(fp, "<div class=\"greenfont\">vec_get_size</div>");

  if (adjointer->callbacks.vec_divide == NULL)
    fprintf(fp, "<div class=\"redfont\"' title='Not registered'>vec_divide</div>");
  else
    fprintf(fp, "<div class=\"greenfont\">vec_divide</div>");

  if (adjointer->callbacks.vec_get_norm == NULL)
    fprintf(fp, "<div class=\"redfont\" title='Not registered'>vec_get_norm</div>");
  else
    fprintf(fp, "<div class=\"greenfont\">vec_get_norm</div>");

  if (adjointer->callbacks.vec_dot_product == NULL)
    fprintf(fp, "<div class=\"redfont\" title='Not registered'>vec_dot_product</div>");
  else
    fprintf(fp, "<div class=\"greenfont\">vec_dot_product</div>");

  if (adjointer->callbacks.vec_set_random == NULL)
    fprintf(fp, "<div class=\"redfont\" title='Not registered'>vec_set_random</div>");
  else
    fprintf(fp, "<div class=\"greenfont\">vec_set_random</div>");

  if (adjointer->callbacks.mat_duplicate == NULL)
    fprintf(fp, "<div class=\"redfont\" title='Not registered'>mat_duplicate</div>");
  else
    fprintf(fp, "<div class=\"greenfont\">mat_duplicate</div>");

  if (adjointer->callbacks.mat_duplicate == NULL)
    fprintf(fp, "<div class=\"redfont\" title='Not registered'>mat_duplicate</div>");
  else
    fprintf(fp, "<div class=\"greenfont\">mat_duplicate</div>");

  if (adjointer->callbacks.mat_axpy == NULL)
    fprintf(fp, "<div class=\"redfont\" title='Not registered'>mat_axpy</div>");
  else
    fprintf(fp, "<div class=\"greenfont\">mat_axpy</div>");

  if (adjointer->callbacks.mat_destroy == NULL)
    fprintf(fp, "<div class=\"redfont\" title='Not registered'>mat_destroy</div>");
  else
    fprintf(fp, "<div class=\"greenfont\">mat_destroy</div>");


  fprintf(fp, "<h2>Block action callbacks</h2>");
  cb_ptr= adjointer->block_action_list.firstnode;
  fprintf(fp, "<div>");
  while (cb_ptr != NULL) {
    fprintf(fp, "%s<br/>\n", encode_html(cb_ptr->name));
    cb_ptr = cb_ptr->next;
  }
  fprintf(fp, "</div>");

  fprintf(fp, "<h2>Block assembly callbacks</h2>");
  cb_ptr= adjointer->block_assembly_list.firstnode;
  fprintf(fp, "<div>");
  while (cb_ptr != NULL) {
    fprintf(fp, "%s<br/>\n", encode_html(cb_ptr->name));
    cb_ptr = cb_ptr->next;
  }
  fprintf(fp, "</div>");

  fprintf(fp, "<h2>Nonlinear block action callbacks</h2>");
  cb_ptr= adjointer->nonlinear_action_list.firstnode;
  fprintf(fp, "<div>");
  while (cb_ptr != NULL) {
    fprintf(fp, "%s<br/>\n", encode_html(cb_ptr->name));
    cb_ptr = cb_ptr->next;
  }
  fprintf(fp, "</div>");

  fprintf(fp, "<h2>Nonlinear block derivative action callbacks</h2>");
  cb_ptr= adjointer->nonlinear_derivative_action_list.firstnode;
  fprintf(fp, "<div>");
  while (cb_ptr != NULL) {
    fprintf(fp, "%s<br/>\n", encode_html(cb_ptr->name));
    cb_ptr = cb_ptr->next;
  }
  fprintf(fp, "</div>");

  fprintf(fp, "<h2>Nonlinear block second derivative action callbacks</h2>");
  cb_ptr= adjointer->nonlinear_second_derivative_action_list.firstnode;
  fprintf(fp, "<div>");
  while (cb_ptr != NULL) {
    fprintf(fp, "%s<br/>\n", encode_html(cb_ptr->name));
    cb_ptr = cb_ptr->next;
  }
  fprintf(fp, "</div>");

  fprintf(fp, "<h2>Nonlinear derivative block assembly callbacks</h2>");
  cb_ptr= adjointer->nonlinear_derivative_assembly_list.firstnode;
  fprintf(fp, "<div>");
  while (cb_ptr != NULL) {
    fprintf(fp, "%s<br/>\n", encode_html(cb_ptr->name));
    cb_ptr = cb_ptr->next;
  }
  fprintf(fp, "</div>");

  fprintf(fp, "<h2>Functional callbacks</h2>");
  func_cb_ptr= adjointer->functional_list.firstnode;
  fprintf(fp, "<div>");
  while (func_cb_ptr != NULL) {
    fprintf(fp, "<div class=\"greenfont\">%s</div><br/>\n", encode_html(func_cb_ptr->name));
    func_cb_ptr = func_cb_ptr->next;
  }
  fprintf(fp, "</div>");

  fprintf(fp, "<h2>Functional derivative callbacks</h2>");
  func_deriv_cb_ptr= adjointer->functional_derivative_list.firstnode;
  fprintf(fp, "<div>");
  while (func_deriv_cb_ptr != NULL) {
    fprintf(fp, "<div class=\"greenfont\">%s</div><br/>\n", encode_html(func_deriv_cb_ptr->name));
    func_deriv_cb_ptr = func_deriv_cb_ptr->next;
  }
  fprintf(fp, "</div>");
}

/* Writes a html row containing the variables into fp */
void adj_html_vars(FILE* fp, adj_adjointer* adjointer, int type)
{
  int i, ierr;
  char adj_name[ADJ_NAME_LEN];
  adj_variable adj_var;
  adj_variable_data* data_ptr;
  adj_variable_hash* varhash;

  fprintf(fp, "<div style=\"height:150px\"></div>\n");
  fprintf(fp, "<tr>\n");

  /* First print the auxiliary variables */
  for (varhash = adjointer->varhash; varhash != NULL; varhash = (adj_variable_hash*) varhash->hh.next)
  {
    if (varhash->variable.auxiliary == ADJ_TRUE)
    {
      adj_var = varhash->variable;

      ierr = adj_has_variable_value(adjointer, adj_var);
      /* Green color -> Variable is recorded, red otherwise */
      if (ierr != ADJ_OK)
        fprintf(fp, "<th class=\"box_rotate redfont\">");
      else
        fprintf(fp, "<th class=\"box_rotate greenfont\">");
      /* Print the variables name */
      adj_var.type = type;
      adj_variable_str(adj_var, adj_name, ADJ_NAME_LEN);
      fprintf(fp, "%s", encode_html(adj_name));

      fprintf(fp, "</th>\n");
    }
  }

  /* And then everything else */

  for (i = 0; i < adjointer->nequations; i++)
  {
    if (type!=ADJ_ADJOINT)
    {
      adj_var = adjointer->equations[i].variable;

      ierr = adj_has_variable_value(adjointer, adj_var);
      /* Green color -> Variable is recorded, red otherwise */
      if (ierr != ADJ_OK)
        fprintf(fp, "<th class=\"box_rotate redfont\">");
      else
        fprintf(fp, "<th class=\"box_rotate greenfont\">");

      /* Print the variables name */
      adj_var.type = type;
      adj_variable_str(adj_var, adj_name, ADJ_NAME_LEN);
      fprintf(fp, "%s", encode_html(adj_name));

      if (adjointer->equations[i].memory_checkpoint==ADJ_TRUE)
        fprintf(fp, "(memory_checkpoint_equation)");
      if (adjointer->equations[i].disk_checkpoint==ADJ_TRUE)
        fprintf(fp, "(disk_checkpoint_equation)");


      /* Also print how the value is stored */
      if (ierr==ADJ_OK) /* ierr from adj_has_variable_value is still valid */
      {
        int comma = 0;
        fprintf(fp, " (");
        ierr = adj_find_variable_data(&(adjointer->varhash), &adj_var, &data_ptr);
        if (ierr==ADJ_OK)
        {
          if (data_ptr->storage.storage_memory_has_value)
          {
            if (comma) fprintf(fp, ",");
            else comma = 1;
            fprintf(fp, "mem");
            if (data_ptr->storage.storage_memory_is_checkpoint)
              fprintf(fp, "(checkpoint)");
          }
          if (data_ptr->storage.storage_disk_has_value)
          {
            if (comma) fprintf(fp, ",");
            else comma = 1;
            fprintf(fp, "disk");
            if (data_ptr->storage.storage_disk_is_checkpoint)
              fprintf(fp, "(checkpoint)");
          }
        }
        fprintf(fp, ")");
      }
      fprintf(fp, "</th>\n");
    }
    else if (adjointer->functional_derivative_list.firstnode==NULL)
    {
      /* Adjoint output but no functional specified... */
      fprintf(fp, "<th>\n");
      adj_var = adjointer->equations[i].variable;
      adj_var.type = type;
      adj_variable_str(adj_var, adj_name, ADJ_NAME_LEN);
      ierr = adj_has_variable_value(adjointer, adj_var);
      if (ierr != ADJ_OK)
        fprintf(fp, "<div class=\"box_rotate redfont\">%s</div>\n", encode_html(adj_name));
      else
        fprintf(fp, "<div class=\"box_rotate greenfont\">%s</div>\n", encode_html(adj_name));
      fprintf(fp, "</th>\n");
    }
    else
    {
      /* Loop over the defined functionals */
      adj_func_deriv_callback *func_deriv_cb = adjointer->functional_derivative_list.firstnode;
      fprintf(fp, "<th>\n");
      while (func_deriv_cb != NULL)
      {
        adj_var = adjointer->equations[i].variable;
        adj_var.type = type;
        strncpy(adj_var.functional, func_deriv_cb->name, ADJ_NAME_LEN);
        adj_var.functional[ADJ_NAME_LEN-1] = '\0';
        adj_variable_str(adj_var, adj_name, ADJ_NAME_LEN);
        ierr = adj_has_variable_value(adjointer, adj_var);
        if (ierr != ADJ_OK)
          fprintf(fp, "<div class=\"box_rotate redfont\">%s</div>\n", encode_html(adj_name));
        else
          fprintf(fp, "<div class=\"box_rotate greenfont\">%s</div>\n", encode_html(adj_name));
        func_deriv_cb = func_deriv_cb->next;
      }
      fprintf(fp, "</th>\n");
    }
  }
  fprintf(fp, "</tr>\n");
 }

/* Writes a html row containing the supplied equation into fp */
int adj_html_eqn(FILE* fp, adj_adjointer* adjointer, adj_equation adj_eqn, int diag_index, char* klass)
{
  int i,k;
  int nb_vars = adjointer->nequations + adj_html_count_auxiliary_variables(adjointer);
  char* row[nb_vars];
  char* desc[nb_vars];
  char buf[ADJ_NAME_LEN];
  char* row_rhs[1];
  char* desc_rhs[1];
  int col, ierr;
  unsigned int max_desc_size = 32*ADJ_NAME_LEN; // The descriptions can become very long
  adj_variable_data* var_data_ptr;
  int eqn_num;

  /* Work out the equation number */
  ierr = adj_find_variable_data(&(adjointer->varhash), &adj_eqn.variable, &var_data_ptr);
  eqn_num = var_data_ptr->equation;

  /* Allocate the strings for this row */
  for (i = 0; i < nb_vars; ++i)
  {
    row[i] = (char*) malloc(ADJ_NAME_LEN*sizeof(char));
    ADJ_CHKMALLOC(row[i]);
    row[i][0]='\0';
    desc[i] = (char*) malloc(max_desc_size*sizeof(char));  
    ADJ_CHKMALLOC(desc[i]);
    desc[i][0]='\0';
  }
  /* Allocate the strings for the rhs */
  row_rhs[0] = (char*) malloc(ADJ_NAME_LEN*sizeof(char));
  desc_rhs[0] = (char*) malloc(max_desc_size*sizeof(char));

  for (i = 0; i < adj_eqn.nblocks; i++)
  {
    /* Add the block data to the table */
    ierr = adj_html_find_column_index(adjointer, &adj_eqn.targets[i], &col);
    if (ierr != ADJ_OK)
      return adj_chkierr_auto(ierr);

    strncpy(row[col], encode_html(adj_eqn.blocks[i].name), 5);
    row[col][5]='\0';

    /* Fill in the description */
    snprintf(desc[col], ADJ_NAME_LEN, "Equation number: %i\n", eqn_num);
    if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], "Targets: ", ADJ_NAME_LEN);
    if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], adj_eqn.targets[i].name, ADJ_NAME_LEN);
    if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], "\nTimestep:", ADJ_NAME_LEN);
    snprintf(buf, ADJ_NAME_LEN, "%d", adj_eqn.targets[i].timestep);
    if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], buf, ADJ_NAME_LEN);
    if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], "\nIteration:", ADJ_NAME_LEN);
    snprintf(buf, ADJ_NAME_LEN, "%d", adj_eqn.targets[i].iteration);
    if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], buf, ADJ_NAME_LEN);

    if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], "\n\n===== Block description =====\n\n", ADJ_NAME_LEN);
    if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], adj_eqn.blocks[i].name, ADJ_NAME_LEN);
    if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], "\n------------------", ADJ_NAME_LEN);

    if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], "\nCoefficient: ", ADJ_NAME_LEN);
    snprintf(buf, ADJ_NAME_LEN, "%f", adj_eqn.blocks[i].coefficient);
    if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], buf, ADJ_NAME_LEN);
    if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], "\nHermitian: ", ADJ_NAME_LEN);
    if (adj_eqn.blocks[i].hermitian==ADJ_TRUE)
      snprintf(buf, ADJ_NAME_LEN, "true");
    else
      snprintf(buf, ADJ_NAME_LEN, "false");
    if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], buf, ADJ_NAME_LEN);

    if (adj_eqn.blocks[i].has_nonlinear_block)
    {
      if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], "\nNonlinear Block: ", ADJ_NAME_LEN);
      if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], adj_eqn.blocks[i].nonlinear_block.name, ADJ_NAME_LEN);
      if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], " Dependencies: ", ADJ_NAME_LEN);
      for (k=0; k<adj_eqn.blocks[i].nonlinear_block.ndepends; k++)
      {
        if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], adj_eqn.blocks[i].nonlinear_block.depends[k].name, ADJ_NAME_LEN);
        if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], ":", ADJ_NAME_LEN);
        snprintf(buf, ADJ_NAME_LEN, "%d", adj_eqn.blocks[i].nonlinear_block.depends[k].timestep);
        if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], buf, ADJ_NAME_LEN);
        if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], ":", ADJ_NAME_LEN);
        snprintf(buf, ADJ_NAME_LEN, "%d", adj_eqn.blocks[i].nonlinear_block.depends[k].iteration);
        if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], buf, ADJ_NAME_LEN);
        if (k!=adj_eqn.blocks[i].nonlinear_block.ndepends-1)
          if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], ", ", ADJ_NAME_LEN);
      }

    }
  }
  /* Write the row */
  adj_html_write_row(fp, row, desc, adjointer->nequations, diag_index, klass);

  if (adj_eqn.nrhsdeps>0)
  {
    /* Write the rhs information to the last column */
    strncpy(row_rhs[0], "Rhs\0", ADJ_NAME_LEN);

    /* Add the dependency information as hover text */
    strncpy(desc_rhs[0], "Dependencies: ", ADJ_NAME_LEN);
    for (i=0; i<adj_eqn.nrhsdeps; i++)
    {
      if (strlen(desc_rhs[0]) < max_desc_size - ADJ_NAME_LEN) strncat(desc_rhs[0], adj_eqn.rhsdeps[i].name, ADJ_NAME_LEN);
      if (strlen(desc_rhs[0]) < max_desc_size - ADJ_NAME_LEN) strncat(desc_rhs[0], ":", ADJ_NAME_LEN);
      snprintf(buf, ADJ_NAME_LEN, "%d", adj_eqn.rhsdeps[i].timestep);
      if (strlen(desc_rhs[0]) < max_desc_size - ADJ_NAME_LEN) strncat(desc_rhs[0], buf, ADJ_NAME_LEN);
      if (strlen(desc_rhs[0]) < max_desc_size - ADJ_NAME_LEN) strncat(desc_rhs[0], ":", ADJ_NAME_LEN);
      snprintf(buf, ADJ_NAME_LEN, "%d", adj_eqn.rhsdeps[i].iteration);
      if (strlen(desc_rhs[0]) < max_desc_size - ADJ_NAME_LEN) strncat(desc_rhs[0], buf, ADJ_NAME_LEN);
      if (i!=adj_eqn.nrhsdeps-1)
       if (strlen(desc_rhs[0]) < max_desc_size - ADJ_NAME_LEN) strncat(desc_rhs[0], ", ", ADJ_NAME_LEN);
    }

    strncpy(buf, "rhs\0", ADJ_NAME_LEN);
    adj_html_write_row(fp, row_rhs, desc_rhs, 1, -1, buf);
  }

  /* Tidy up */
  for (i = 0; i < adjointer->nequations; ++i)
  {
    free(row[i]);
    free(desc[i]);
  }
  free(row_rhs[0]);
  free(desc_rhs[0]);

  return ADJ_OK;
}

/* Writes a html row containing the supplied adjoint equation into fp */
int adj_html_adjoint_eqn(FILE* fp, adj_adjointer* adjointer, adj_equation fwd_eqn, int diag_index, char* klass)
{
  int i, j, k, l;
  int nb_vars = adjointer->nequations + adj_html_count_auxiliary_variables(adjointer);
  char* row[nb_vars];
  char* desc[nb_vars];
  char buf[ADJ_NAME_LEN];
  int col, ierr;
  adj_variable fwd_var;
  adj_variable_data *fwd_data;
  unsigned int max_desc_size = 32*ADJ_NAME_LEN; // The descriptions can become very long

  /* Allocate the strings for this row */
  for (i = 0; i < nb_vars; ++i)
  {
    row[i] = (char*) malloc(ADJ_NAME_LEN*sizeof(char));
    ADJ_CHKMALLOC(row[i]);
    row[i][0]='\0';
    desc[i] = (char*) malloc(max_desc_size*sizeof(char)); // The description can become very long
    ADJ_CHKMALLOC(desc[i]);
    desc[i][0]='\0';
  }

  fwd_var = fwd_eqn.variable;
  ierr = adj_find_variable_data(&(adjointer->varhash), &fwd_var, &fwd_data);
  assert(ierr == ADJ_OK);

  /* --------------------------------------------------------------------------
   * Visualisation of A* terms                                                  |
   * --------------------------------------------------------------------------
   */
  {
    /* Each targeting equation corresponds to one column in the considered row */
    for (i = 0; i < fwd_data->ntargeting_equations; i++)
    {
      adj_equation other_fwd_eqn;
      adj_variable other_adj_var;

      other_fwd_eqn = adjointer->equations[fwd_data->targeting_equations[i]];

      /* Find the index in other_fwd_eqn of the block fwd_var is multiplied with */
      for (j=0; j<other_fwd_eqn.nblocks; j++)
      {
        if (adj_variable_equal(&other_fwd_eqn.targets[j], &fwd_var, 1))
          break;
      }
      assert(j!=other_fwd_eqn.nblocks);

      /* find the column in which this blocks belongs */
      ierr = adj_html_find_column_index(adjointer, &other_fwd_eqn.variable, &col);
      if (ierr != ADJ_OK)
        return adj_chkierr_auto(ierr);

      other_adj_var = other_fwd_eqn.targets[j];
      other_adj_var.type = ADJ_ADJOINT;

      /* Add the block data to the table */
      strncpy(row[col], encode_html(other_fwd_eqn.blocks[j].name), 5);
      row[col][5]='\0';

      /* Write the information that is displayed when hovering over the block */
      if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncpy(desc[col], "Targets: ", ADJ_NAME_LEN);
      adj_variable_str(other_adj_var, buf, ADJ_NAME_LEN);
      if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], buf, ADJ_NAME_LEN);
      if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], "\nTimestep:", ADJ_NAME_LEN);
      snprintf(buf, ADJ_NAME_LEN, "%d", other_adj_var.timestep);
      if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], buf, ADJ_NAME_LEN);
      if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], "\nIteration:", ADJ_NAME_LEN);
      snprintf(buf, ADJ_NAME_LEN, "%d", other_adj_var.iteration);
      if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], buf, ADJ_NAME_LEN);
      if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], "\n\n===== Block description =====\n\n", ADJ_NAME_LEN);

      if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], other_fwd_eqn.blocks[j].name, ADJ_NAME_LEN);
      if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], "\n------------------", ADJ_NAME_LEN);
      if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], "\nCoefficient: ", ADJ_NAME_LEN);
      snprintf(buf, ADJ_NAME_LEN, "%f", other_fwd_eqn.blocks[j].coefficient);
      if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], buf, ADJ_NAME_LEN);
      if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], "\nHermitian: ", ADJ_NAME_LEN);

      /* We are printing the adjoint equation, therefore hermitian has to be NOT'ed */
      if (other_fwd_eqn.blocks[j].hermitian==ADJ_TRUE)
        snprintf(buf, ADJ_NAME_LEN, "false");
      else
        snprintf(buf, ADJ_NAME_LEN, "true");
      if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], buf, ADJ_NAME_LEN);

      if (other_fwd_eqn.blocks[j].has_nonlinear_block)
      {
        if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], "\nNonlinear Block: ", ADJ_NAME_LEN);
        if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], other_fwd_eqn.blocks[j].nonlinear_block.name, ADJ_NAME_LEN);
        if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], " Dependencies: ", ADJ_NAME_LEN);
        for (k=0; k<other_fwd_eqn.blocks[j].nonlinear_block.ndepends; k++)
        {
          if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], other_fwd_eqn.blocks[j].nonlinear_block.depends[k].name, ADJ_NAME_LEN);
          if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], ":", ADJ_NAME_LEN);
          snprintf(buf, ADJ_NAME_LEN, "%d", other_fwd_eqn.blocks[j].nonlinear_block.depends[k].timestep);
          if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], buf, ADJ_NAME_LEN);
          if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], ":", ADJ_NAME_LEN);
          snprintf(buf, ADJ_NAME_LEN, "%d", other_fwd_eqn.blocks[j].nonlinear_block.depends[k].iteration);
          if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], buf, ADJ_NAME_LEN);
          if (k!=other_fwd_eqn.blocks[j].nonlinear_block.ndepends-1)
            if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], ", ", ADJ_NAME_LEN);
        }
      }
    }

    /* --------------------------------------------------------------------------
     * Visualisation of G* terms                                                  |
     * --------------------------------------------------------------------------
     */
    {
     /* We need to loop through the equations that depend on fwd_var; each one of those will produce
      * a term in this row of G*.
      */
      for (i = 0; i < fwd_data->ndepending_equations; i++)
      {
        /* We are looking for the block of G*. */
        int ndepending_eqn;
        adj_equation depending_eqn;
        int k;

        ndepending_eqn = fwd_data->depending_equations[i];
        depending_eqn = adjointer->equations[ndepending_eqn];

        for (j = 0; j < depending_eqn.nblocks; j++)
        {
          /* We do not have a G* contribution of linear blocks */
          if (!depending_eqn.blocks[j].has_nonlinear_block)
            continue;

          for (k = 0; k < depending_eqn.blocks[j].nonlinear_block.ndepends; k++)
          {
            /* Does this G* contribute to the adjoint equation? */
            if (adj_variable_equal(&fwd_var, &depending_eqn.blocks[j].nonlinear_block.depends[k], 1))
            {
              ierr = adj_html_find_column_index(adjointer, &depending_eqn.variable, &col);
              if (ierr != ADJ_OK)
                return adj_chkierr_auto(ierr);

              /* Add the block data to the table */
              if (strlen(row[col])==0)
              {
                /* If no block name was provided to the html table yet, we conclude that this block
                 * does not have a A* contribution. In order to make the "hover over" functionality
                 * work we need to provide a dummy name...
                 */
                strncpy(row[col], "_____", 5);
                row[col][5]='\0';
              }
              else
              {
                /* ... otherwise this block contains of a sum, so lets add a plus sign
                 * to the description text
                 */
                if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], "\n\n+\n\n", ADJ_NAME_LEN);
              }

              /* Write the information that is displayed when hovering over the block */
              if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], "Derivative of ", ADJ_NAME_LEN);
              if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], depending_eqn.blocks[j].nonlinear_block.name, ADJ_NAME_LEN);
              if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], "\nwith respect to ", ADJ_NAME_LEN);
              adj_variable_str(fwd_var, buf, ADJ_NAME_LEN);
              if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], buf, ADJ_NAME_LEN);
              if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], "\ncontracted with ", ADJ_NAME_LEN);
              adj_variable_str(depending_eqn.targets[j], buf, ADJ_NAME_LEN);
              if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], buf, ADJ_NAME_LEN);
              if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], "\n------------------", ADJ_NAME_LEN);
              if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], "\nCoefficient: ", ADJ_NAME_LEN);
              snprintf(buf, ADJ_NAME_LEN, "%f", depending_eqn.blocks[j].nonlinear_block.coefficient);
              if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], buf, ADJ_NAME_LEN);
              if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], "\nHermitian: true", ADJ_NAME_LEN);
              if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], "\nDependencies: ", ADJ_NAME_LEN);
              for (l=0; l<depending_eqn.blocks[j].nonlinear_block.ndepends; l++)
              {
                if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], depending_eqn.blocks[j].nonlinear_block.depends[l].name, ADJ_NAME_LEN);
                if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], ":", ADJ_NAME_LEN);
                snprintf(buf, ADJ_NAME_LEN, "%d", depending_eqn.blocks[j].nonlinear_block.depends[l].timestep);
                if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], buf, ADJ_NAME_LEN);
                if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], ":", ADJ_NAME_LEN);
                snprintf(buf, ADJ_NAME_LEN, "%d", depending_eqn.blocks[j].nonlinear_block.depends[l].iteration);
                if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], buf, ADJ_NAME_LEN);
                if (l!=depending_eqn.blocks[j].nonlinear_block.ndepends-1)
                  if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], ", ", ADJ_NAME_LEN);
              }
            }
          }
        }
      }
    }

    /* --------------------------------------------------------------------------
     * Visualisation of R* terms                                                  |
     * --------------------------------------------------------------------------
     */
    {
      for (i = 0; i < fwd_data->nrhs_equations; i++)
      {
        int rhs_equation = fwd_data->rhs_equations[i];

        /* Every rhs which depends on fwd_var contributes to the adjoint matrix. */
        ierr = adj_html_find_column_index(adjointer, &(adjointer->equations[rhs_equation].variable), &col);
        if (ierr != ADJ_OK)
          return adj_chkierr_auto(ierr);

        /* Add the rhs data to the table */
        if (strlen(row[col])==0)
        {
          /* If no block name was provided to the html table yet, we conclude that this block
           * does not have a A* contribution. In order to make the "hover over" functionality
           * work we need to provide a dummy name...
           */
          strncpy(row[col], "_____", 5);
          row[col][5]='\0';
        }
        else
        {
          /* ... otherwise this block contains of a sum, so lets add a plus sign
           * to the description text
           */
          if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], "\n\n+\n\n", ADJ_NAME_LEN);
        }

        /* Write the information that is displayed when hovering over the block */
        if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], "Derivative of rhs of equation targeting variable ", ADJ_NAME_LEN);
        adj_variable_str(adjointer->equations[rhs_equation].variable, buf, ADJ_NAME_LEN);
        if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], buf, ADJ_NAME_LEN);
        if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], "\nwith respect to ", ADJ_NAME_LEN);
        adj_variable_str(fwd_var, buf, ADJ_NAME_LEN);
        if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], buf, ADJ_NAME_LEN);
        if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], "\n------------------", ADJ_NAME_LEN);
        if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], "\nCoefficient: ", ADJ_NAME_LEN);
        snprintf(buf, ADJ_NAME_LEN, "%f", -1.0); /* The coefficient is always 1.0 */
        if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], buf, ADJ_NAME_LEN);
        if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], "\nHermitian: true", ADJ_NAME_LEN); /* The rhs contribution is always hermitian */
        if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], "\nDependencies: ", ADJ_NAME_LEN);
        for (l=0; l<adjointer->equations[rhs_equation].nrhsdeps; l++)
        {
          if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], adjointer->equations[rhs_equation].rhsdeps[l].name, ADJ_NAME_LEN);
          if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], ":", ADJ_NAME_LEN);
          snprintf(buf, ADJ_NAME_LEN, "%d", adjointer->equations[rhs_equation].rhsdeps[l].timestep);
          if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], buf, ADJ_NAME_LEN);
          if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], ":", ADJ_NAME_LEN);
          snprintf(buf, ADJ_NAME_LEN, "%d", adjointer->equations[rhs_equation].rhsdeps[l].iteration);
          if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], buf, ADJ_NAME_LEN);
          if (l!=adjointer->equations[rhs_equation].nrhsdeps-1)
            if (strlen(desc[col]) < max_desc_size - ADJ_NAME_LEN) strncat(desc[col], ", ", ADJ_NAME_LEN);
        }
      }
    }

  }

  /* Write it to file */
  adj_html_write_row(fp, row, desc, adjointer->nequations, diag_index, klass);

  /* Tidy up */
  for (i = 0; i < adjointer->nequations; ++i)
  {
    free(row[i]);
    free(desc[i]);
  }
  return ADJ_OK;
}

int adj_html_adjoint_system(adj_adjointer* adjointer, char* filename)
{
  FILE *fp;
  int i, ierr, timestep, iteration;
  adj_equation adj_eqn;

  fp=fopen(filename, "w+");
  if (fp==NULL)
  {
    snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "File %s could not be opened.", filename);
    return adj_chkierr_auto(ADJ_ERR_INVALID_INPUTS);
  }

  adj_html_header(fp);

  adj_html_print_statistics(fp, adjointer);

  /* Print the annontate matrix system in a html table */
  fprintf(fp, "<h1>Adjoint system</h1>\n");
  if (adjointer->nequations==0)
    return ADJ_OK;
  timestep = adjointer->equations[0].variable.timestep-1;
  iteration = adjointer->equations[0].variable.iteration - 1 ;

  adj_html_table_begin(fp, "");
  adj_html_vars(fp, adjointer, ADJ_ADJOINT);

  for (i=0; i<adjointer->nequations; i++)
  {
    fprintf(fp, "<tr>\n");
    adj_eqn = adjointer->equations[i];
    if (timestep != adj_eqn.variable.timestep) {
      ierr = adj_html_adjoint_eqn(fp, adjointer, adj_eqn, i, "new_timestep");
    }
    else {
      if (iteration != adj_eqn.variable.iteration)
        ierr = adj_html_adjoint_eqn(fp, adjointer, adj_eqn, i, "");
      else
        ierr = adj_html_adjoint_eqn(fp, adjointer, adj_eqn, i, "");
    }

    if (ierr != ADJ_OK)
    {
      fclose(fp);
      return adj_chkierr_auto(ierr);
    }
    fprintf(fp, "</tr>\n");
    timestep = adj_eqn.variable.timestep;
    iteration = adj_eqn.variable.iteration;
  }
  adj_html_table_end(fp);

  adj_html_print_auxiliary_variables(fp, adjointer);
  adj_html_print_callback_information(fp, adjointer);

  adj_html_footer(fp);
  fclose(fp);
  return ADJ_OK;
}


int adj_html_forward_system(adj_adjointer* adjointer, char* filename)
{
  FILE *fp;
  int i, ierr, timestep, iteration;
  adj_equation adj_eqn;

  fp=fopen(filename, "w+");
  if (fp==NULL)
  {
    snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "File %s could not be opened.", filename);
    return adj_chkierr_auto(ADJ_ERR_INVALID_INPUTS);
  }

  adj_html_header(fp);

  adj_html_print_statistics(fp, adjointer);

  /* Print the annontate matrix system in a html table */
  fprintf(fp, "<h1>Forward system</h1>\n");
  if (adjointer->nequations==0)
    return ADJ_OK;
  timestep = adjointer->equations[0].variable.timestep - 1;
  iteration = adjointer->equations[0].variable.iteration - 1 ;

  adj_html_table_begin(fp, "");
  adj_html_vars(fp, adjointer, ADJ_FORWARD);

  for (i=0; i<adjointer->nequations; i++)
  {
    fprintf(fp, "<tr>\n");
    adj_eqn = adjointer->equations[i];
    if (timestep != adj_eqn.variable.timestep) {
      ierr = adj_html_eqn(fp, adjointer, adj_eqn, i, "new_timestep");
    }
    else {
      if (iteration != adj_eqn.variable.iteration)
        ierr = adj_html_eqn(fp, adjointer, adj_eqn, i, "");
      else
        ierr = adj_html_eqn(fp, adjointer, adj_eqn, i, "");
    }


    if (ierr != ADJ_OK)
    {
      fclose(fp);
      return adj_chkierr_auto(ierr);
    }
    fprintf(fp, "</tr>\n");
    timestep = adj_eqn.variable.timestep;
    iteration = adj_eqn.variable.iteration;
  }
  adj_html_table_end(fp);

  adj_html_print_auxiliary_variables(fp, adjointer);
  adj_html_print_callback_information(fp, adjointer);

  adj_html_footer(fp);
  fclose(fp);
  return ADJ_OK;
}

int adj_adjointer_to_html(adj_adjointer* adjointer, char* filename, int type)
{
  if (type == ADJ_FORWARD)
    return adj_html_forward_system(adjointer, filename);
  else if(type == ADJ_ADJOINT)
    return adj_html_adjoint_system(adjointer, filename);
  else
  {
    snprintf(adj_error_msg, ADJ_ERROR_MSG_BUF, "The type parameter must either be ADJ_FORWARD or ADJ_ADJOINT.");
    return adj_chkierr_auto(ADJ_ERR_INVALID_INPUTS);
  }
}
