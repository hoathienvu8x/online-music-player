#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "pug.h"

#define PUG_MAX_STACK_DEPTH 100

enum pug_node_t {
  pug_node_root,
  pug_node_doctype,
  pug_node_tag,
  pug_node_text,
  pug_node_code_block,
  pug_node_if,
  pug_node_else,
  pug_node_each
};

struct pug_ast_t {
  enum pug_node_t type;
  struct pug_str_t tag, selector, attrs, text;
  int indent;
  struct pug_ast_t *parent, *head, *next;
};

static struct pug_ast_t *pug_ast_create(enum pug_node_t type, int indent) {
  struct pug_ast_t *ast = (struct pug_ast_t *)calloc(
    1, sizeof(struct pug_ast_t)
  );
  if (ast) {
    ast->type = type;
    ast->tag = pug_str_n(NULL, 0);
    ast->selector = pug_str_n(NULL, 0);
    ast->attrs = pug_str_n(NULL, 0);
    ast->text = pug_str_n(NULL, 0);
    ast->indent = indent;
    ast->parent = ast->head = ast->next = NULL;
  }
  return ast;
}

static void pug_ast_free(struct pug_ast_t *node) {
  if (node) {
    pug_ast_free(node->head);
    pug_ast_free(node->next);
    free(node);
  }
}

static int pug_ast_append(struct pug_ast_t *parent, struct pug_ast_t *node) {
  if (!parent || !node) return -1;
  if (parent->head == NULL) {
    parent->head = node;
  } else {
    struct pug_ast_t *p = parent->head;
    while (p->next != NULL) p = p->next;
    p->next = node;
  }
  node->parent = parent;
  return 0;
}

static const char* pug_str_memrchr(const char *s, int c, size_t n) {
  if (n == 0) return NULL;
  const char *ptr = s + n - 1;
  while (ptr >= s) {
    if (*ptr == (char)c) {
      return ptr;
    }
    ptr--;
  }
  return NULL;
}

static struct pug_str_t pug_str_trim(struct pug_str_t s) {
  while (s.len > 0 && isspace((unsigned char)*s.buf)) {
    s.buf++, s.len--;
  }
  while (s.len > 0 && isspace((unsigned char)s.buf[s.len - 1])) {
    s.len--;
  }
  return s;
}

static int pug_str_starts_with(struct pug_str_t s, const char *prefix) {
  size_t p_len = strlen(prefix);
  if (s.len < p_len) return 0;
  return memcmp(s.buf, prefix, p_len) == 0;
}

static int pug_str_equals(struct pug_str_t a, struct pug_str_t b) {
  if (a.len != b.len) return 0;
  if (a.buf == NULL || b.buf == NULL) return a.buf == b.buf;
  return memcmp(a.buf, b.buf, a.len) == 0;
}

struct pug_ast_t* pug_last_text_node(struct pug_ast_t *parent) {
  if (!parent) return NULL;
  if (parent->type == pug_node_text && parent->head == NULL) {
    return parent;
  }
  struct pug_ast_t *curr = parent->head;
  if (!curr) return NULL;
  
  while (curr->next) {
    curr = curr->next;
  }
  
  if (curr->type == pug_node_text) {
    struct pug_ast_t *deep = pug_last_text_node(curr);
    return deep ? deep : curr;
  }
  return NULL;
}


static int pug_parse_ast(const char *source, size_t len, struct pug_ast_t **ast) {
  struct pug_ast_t *root;
  struct pug_ast_t *stack[PUG_MAX_STACK_DEPTH];
  int stack_top;
  const char *cursor;
  const char *end;

  if (!ast || !source) return -1;
  *ast = NULL;

  root = pug_ast_create(pug_node_root, -1);
  if (!root) return -1;
  root->tag = pug_str_n("ROOT", 4);

  stack_top = 0;
  stack[stack_top] = root;

  cursor = source;
  end = source + len;

  while (cursor < end) {
    const char *line_start = cursor;
    size_t line_len;
    struct pug_str_t raw_line;
    int indent;
    struct pug_str_t trimmed;
    struct pug_ast_t *parent;
    struct pug_ast_t *new_node;

    while (cursor < end && *cursor != '\n') {
      cursor++;
    }
    line_len = (size_t)(cursor - line_start);

    if (line_len > 0 && line_start[line_len - 1] == '\r') {
      line_len--;
    }

    if (cursor < end && *cursor == '\n') {
      cursor++;
    }

    raw_line = pug_str_n(line_start, line_len);

    indent = 0;
    while ((size_t)indent < line_len && (line_start[indent] == ' ' || line_start[indent] == '\t')) {
      indent++;
    }

    trimmed = pug_str_trim(raw_line);
    if (trimmed.len == 0) continue;

    while (stack_top > 0 && stack[stack_top]->indent >= indent) {
      stack_top--;
    }
    parent = stack[stack_top];
    new_node = NULL;

    if (pug_str_starts_with(trimmed, "doctype")) {
      new_node = pug_ast_create(pug_node_doctype, indent);
      if (new_node) {
        struct pug_str_t attrs = pug_str_n(trimmed.buf + 7, trimmed.len - 7);
        new_node->tag = pug_str_n("doctype", 7);
        new_node->attrs = pug_str_trim(attrs);
      }
    }
    else if (trimmed.buf[0] == '|') {
      new_node = pug_ast_create(pug_node_text, indent);
      if (new_node) {
        struct pug_str_t text_content = pug_str_n(trimmed.buf + 1, trimmed.len - 1);
        new_node->text = pug_str_trim(text_content);
      }
    }
    else if (trimmed.buf[0] == '-') {
      if (pug_str_starts_with(trimmed, "- if ")) {
        new_node = pug_ast_create(pug_node_if, indent);
        if (new_node) {
          trimmed = pug_str_n(trimmed.buf + 2, trimmed.len - 2);
          new_node->text = pug_str_trim(trimmed);
        }
      } 
      else if (pug_str_starts_with(trimmed, "- else")) {
        if (trimmed.len > 6 && trimmed.buf[6] != ' ' && trimmed.buf[6] != '\t') {
          int inside_code_block = 0;
          struct pug_ast_t *curr = parent;
          while (curr != root && curr != NULL) {
            if (curr->type == pug_node_code_block) {
              inside_code_block = 1;
              break;
            }
            curr = curr->parent;
          }
          if (inside_code_block) {
            struct pug_ast_t *last_text = pug_last_text_node(parent);
            if (last_text != NULL) {
              size_t total_len = (size_t)((raw_line.buf + raw_line.len) - last_text->text.buf);
              last_text->text.len = total_len;
              continue;
            } else {
              new_node = pug_ast_create(pug_node_text, indent);
              if (new_node) new_node->text = raw_line;
            }
          } else {
            new_node = pug_ast_create(pug_node_tag, indent);
            if (new_node) new_node->tag = trimmed;
          }
        } else {
          struct pug_ast_t *last_sibling = parent->head;
          while (last_sibling != NULL && last_sibling->next != NULL) {
            last_sibling = last_sibling->next;
          }
          
          if (last_sibling == NULL || last_sibling->type != pug_node_if) {
            pug_ast_free(root);
            return -1;
          }
          new_node = pug_ast_create(pug_node_else, indent);
          if (new_node) {
            trimmed = pug_str_n(trimmed.buf + 2, trimmed.len - 2);
            new_node->text = pug_str_trim(trimmed);
          }
        }
      } 
      else if (pug_str_starts_with(trimmed, "- each")) {
        new_node = pug_ast_create(pug_node_each, indent);
        if (new_node) {
          trimmed = pug_str_n(trimmed.buf + 2, trimmed.len - 2);
          new_node->text = pug_str_trim(trimmed);
        }
      } 
      else {
        int inside_code_block = 0;
        struct pug_ast_t *curr = parent;
        while (curr != root && curr != NULL) {
          if (curr->type == pug_node_code_block) {
            inside_code_block = 1;
            break;
          }
          curr = curr->parent;
        }

        if (inside_code_block) {
          struct pug_ast_t *last_text = pug_last_text_node(parent);
          if (last_text != NULL) {
            size_t total_len = (size_t)((raw_line.buf + raw_line.len) - last_text->text.buf);
            last_text->text.len = total_len;
            continue;
          } else {
            new_node = pug_ast_create(pug_node_text, indent);
            if (new_node) new_node->text = raw_line;
          }
        } else {
          new_node = pug_ast_create(pug_node_tag, indent);
          if (new_node) new_node->tag = trimmed;
        }
      }
    } 
    else {
      int inside_code_block = 0;
      int is_code_block_decl = 0;
      size_t i;
      struct pug_ast_t *curr = parent;

      while (curr != root && curr != NULL) {
        if (curr->type == pug_node_code_block) {
          inside_code_block = 1;
          break;
        }
        curr = curr->parent;
      }

      if (pug_str_equals(trimmed, pug_str_s("pre")) || pug_str_starts_with(trimmed, "pre ")) {
        is_code_block_decl = 1;
      } else {
        for (i = 0; i < trimmed.len; i++) {
          if (trimmed.buf[i] == '.') {
            if (i + 1 == trimmed.len) is_code_block_decl = 1;
            break;
          }
        }
      }

      if (is_code_block_decl) {
        size_t name_len = 0;
        if (pug_str_equals(trimmed, pug_str_s("pre")) || pug_str_starts_with(trimmed, "pre ")) {
          while (name_len < trimmed.len && trimmed.buf[name_len] != ' ') name_len++;
        } else {
          while (name_len < trimmed.len && trimmed.buf[name_len] != '.') name_len++;
        }
        new_node = pug_ast_create(pug_node_code_block, indent);
        if (new_node) {
          new_node->tag = pug_str_n(trimmed.buf, name_len);
        }
      } 
      else if (inside_code_block) {
        struct pug_ast_t *last_text = pug_last_text_node(parent);
        if (last_text != NULL) {
          size_t total_len = (size_t)((raw_line.buf + raw_line.len) - last_text->text.buf);
          last_text->text.len = total_len;
          continue;
        } else {
          new_node = pug_ast_create(pug_node_text, indent);
          if (new_node) new_node->text = raw_line;
        }
      } 
      else {
        struct pug_str_t name = pug_str_n(NULL, 0);
        struct pug_str_t selector = pug_str_n(NULL, 0);
        struct pug_str_t attrs = pug_str_n(NULL, 0);
        struct pug_str_t content = pug_str_n(NULL, 0);
        size_t name_len = 0;
        size_t rem_offset;

        while (name_len < trimmed.len && 
               trimmed.buf[name_len] != ' ' && 
               trimmed.buf[name_len] != '(' && 
               trimmed.buf[name_len] != '#' && 
               trimmed.buf[name_len] != '.') {
          name_len++;
        }

        if (name_len == 0 && (trimmed.buf[0] == '#' || trimmed.buf[0] == '.')) {
          name = pug_str_n("div", 3);
        } else if (name_len > 0) {
          name = pug_str_n(trimmed.buf, name_len);
        } else {
          name = trimmed;
        }

        rem_offset = (name_len == 0 && (trimmed.buf[0] == '#' || trimmed.buf[0] == '.')) ? 0 : name_len;
        if (rem_offset < trimmed.len) {
          struct pug_str_t remainder = pug_str_n(trimmed.buf + rem_offset, trimmed.len - rem_offset);
          
          if (remainder.buf[0] == '(') {
            const char *paren_open = remainder.buf + 1;
            const char *paren_close = pug_str_memrchr(paren_open, ')', remainder.len);
            if (paren_close) {
              size_t attr_len = (size_t)(paren_close - paren_open);
              attrs = pug_str_n(paren_open, attr_len);
              
              if ((size_t)(paren_close + 1 - trimmed.buf) < trimmed.len) {
                content = pug_str_trim(pug_str_n(paren_close + 1, trimmed.len - (size_t)(paren_close + 1 - trimmed.buf)));
              }
            }
          } else {
            const char *paren_open = memchr(remainder.buf, '(', remainder.len);
            if (paren_open) {
              size_t sel_len = (size_t)(paren_open - remainder.buf);
              selector = pug_str_trim(pug_str_n(remainder.buf, sel_len));
              
              {
                const char *paren_close = pug_str_memrchr(paren_open, ')', remainder.len - sel_len);
                if (paren_close) {
                  attrs = pug_str_n(paren_open, (size_t)(paren_close - paren_open + 1));
                  if ((size_t)(paren_close + 1 - trimmed.buf) < trimmed.len) {
                    content = pug_str_trim(pug_str_n(paren_close + 1, trimmed.len - (size_t)(paren_close + 1 - trimmed.buf)));
                  }
                }
              }
            } else {
              const char *space = memchr(remainder.buf, ' ', remainder.len);
              if (space) {
                size_t sel_len = (size_t)(space - remainder.buf);
                selector = pug_str_trim(pug_str_n(remainder.buf, sel_len));
                content = pug_str_trim(pug_str_n(space, remainder.len - sel_len));
              } else {
                selector = pug_str_trim(remainder);
              }
            }
          }
        }

        new_node = pug_ast_create(pug_node_tag, indent);
        if (new_node) {
          new_node->tag = name;
          new_node->selector = selector;
          new_node->attrs = attrs;
          new_node->text = content;
        }
      }
    }

    if (pug_ast_append(parent, new_node)) {
      pug_ast_free(root);
      return -1;
    }

    if (stack_top < PUG_MAX_STACK_DEPTH - 1) {
      stack_top++;
      stack[stack_top] = new_node;
    }
  }
  
  *ast = root;
  return 0;
}

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
  struct pug_ast_t *root = NULL;
  if (!input || len == 0 || !out) return -1;
  if (pug_parse_ast(input, len, &root)) return -1;
  (void)ctx;
  pug_ast_free(root);
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
