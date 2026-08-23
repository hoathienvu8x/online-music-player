#include "pug.h"

int lte_pug_render(
  const char *input, size_t len,
  struct pug_str_t *ctx, struct pug_str_t *out
) {
  (void)input;
  (void)len;
  (void)ctx;
  (void)out;
  return -1;
}

int lte_pug_file_render(
  const char *fpath, struct pug_str_t *ctx, struct pug_str_t *out
) {
  (void)fpath;
  (void)ctx;
  (void)out;
  return -1;
}

void pug_str_free(struct pug_str_t *s) {
  (void)s;
}
