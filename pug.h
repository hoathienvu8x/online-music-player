#ifndef _PUG_H
#define _PUG_H

#include <stddef.h>

struct pug_str_t {
  char *buf;
  size_t len, capacity;  
};

int lte_pug_render(
  const char *input, size_t len,
  struct pug_str_t *ctx, struct pug_str_t *out
);
int lte_pug_file_render(
  const char *fpath, struct pug_str_t *ctx, struct pug_str_t *out
);
void pug_str_free(struct pug_str_t *s);

#endif
