#include "libadjoint/adj_error_handling.h"

void adj_chkierr_private(int ierr, char* file, int line)
{
  char adj_error_codes[9][ADJ_ERROR_MSG_BUF] = {"ADJ_ERR_OK", "ADJ_ERR_INVALID_INPUTS", "ADJ_ERR_HASH_FAILED",
                                                "ADJ_ERR_NEED_CALLBACK", "ADJ_ERR_NEED_VALUE", "ADJ_ERR_NOT_IMPLEMENTED",
                                                "ADJ_ERR_DICT_FAILED", "ADJ_ERR_TOLERANCE_EXCEEDED", "ADJ_ERR_MALLOC_FAILED"};
  char adj_warn_codes[2][ADJ_ERROR_MSG_BUF] = {"ADJ_WARN_ALREADY_RECORDED", "ADJ_WARN_COMPARISON_FAILED"};

  if (ierr > 0)
  {
    fprintf(stderr, "Error: file %s:%d\n", file, line);
    fprintf(stderr, "Error: got error code %s\n", adj_error_codes[ierr]);
    fprintf(stderr, "Error: %s\n", strlen(adj_error_msg) == 0 ? "(no error message)" : adj_error_msg);
    exit(ierr);
  }
  else if (ierr < 0)
  {
    fprintf(stderr, "Warning: file %s:%d\n", file, line);
    fprintf(stderr, "Warning: got error code %s\n", adj_warn_codes[abs(ierr)-1]);
    fprintf(stderr, "Warning: %s\n", strlen(adj_error_msg) == 0 ? "(no error message)" : adj_error_msg);
  }
}
