#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pug.h"

static int read_file(const char *fpath, char **content, size_t *len) {
  FILE *fp = NULL;
  char *buffer = NULL;
  long size = 0;
  size_t read_bytes = 0;
  if (!fpath || *fpath == '\0' || !content) return -1;
  fp = fopen(fpath, "rb");
  if (!fp) return -1;
  if (fseek(fp, 0, SEEK_END) != 0) {
    fclose(fp);
    return -1;
  }
  size = ftell(fp);
  if (size < 0) {
    fclose(fp);
    return -1;
  }
  rewind(fp);
  buffer = (char *)malloc(size + 1);
  if (!buffer) {
    fclose(fp);
    return -1;
  }
  read_bytes = fread(buffer, 1, size, fp);
  fclose(fp);
  if (read_bytes != (size_t)size) {
    free(buffer);
    return -1;
  }
  buffer[size] = '\0';
  *content = buffer;
  if (len) *len = (size_t)size;
  return 0;
}

int lte_pug_render(
  const char *input, size_t len,
  struct pug_str_t *ctx, struct pug_str_t *out
) {
  if (!input || len == 0 || !out) return -1;
  (void)ctx;
  return -1;
}

int lte_pug_file_render(
  const char *fpath, struct pug_str_t *ctx, struct pug_str_t *out
) {
  int rc = -1;
  char *content = NULL;
  size_t len = 0;
  if (read_file(fpath, &content, &len) != 0) {
    return -1;
  }
  rc = lte_pug_render(content, len, ctx, out);
  free(content);
  return rc;
}

struct pug_str_t pug_str_n(const char *buf, size_t len) {
  struct pug_str_t s;
  s.buf = (char *)buf, s.len = len, s.capacity = len;
  return s;
}
struct pug_str_t pug_str_s(const char *buf) {
  return pug_str_n(buf, buf ? strlen(buf) : 0);
}

void pug_str_free(struct pug_str_t *s) {
  if (!s) return;
  if (s->buf) free(s->buf);
  s->buf = NULL, s->len = s->capacity = 0;
}
