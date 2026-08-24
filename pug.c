#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "pug.h"
#ifdef USE_PARSON
  #include "parson.h"
#endif

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

static const char* self_closing_tags[] = {
  "area", "br", "col", "hr", "img",
  "input", "link", "meta", "doctype", NULL
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
          new_node->tag = pug_str_trim(trimmed);
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
              if (new_node) new_node->tag = raw_line;
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
            new_node->tag = pug_str_trim(trimmed);
          }
        }
      } 
      else if (pug_str_starts_with(trimmed, "- each")) {
        new_node = pug_ast_create(pug_node_each, indent);
        if (new_node) {
          trimmed = pug_str_n(trimmed.buf + 2, trimmed.len - 2);
          new_node->tag = pug_str_trim(trimmed);
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
          if (trimmed.buf[i] == ' ') break;
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

        while (name_len < trimmed.len) {
          if (
            trimmed.buf[name_len] == ' ' ||
            trimmed.buf[name_len] == '(' || 
            trimmed.buf[name_len] == '#' || 
            trimmed.buf[name_len] == '.'
          ) {
            break;
          }
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
#ifdef TEST_PUG
static int pug_str_print(struct pug_str_t s) {
  size_t written = fwrite(s.buf, 1, s.len, stdout);
  if (written != s.len) {
    return -1;
  }
  return 0;
}

static void pug_ast_print(struct pug_ast_t *node, int depth) {
  if (!node) return;
  if (node->type == pug_node_root) {
    pug_ast_print(node->head, depth);
    return;
  }
  for (int i = 0; i < depth; i++) printf("  ");

  const char *type_str = "";
  switch(node->type) {
    case pug_node_doctype: type_str = "DOCTYPE"; break;
    case pug_node_tag: type_str = "TAG"; break;
    case pug_node_text: type_str = "TEXT"; break;
    case pug_node_code_block: type_str = "CODE_BLOCK"; break;
    case pug_node_if: type_str = "IF"; break;
    case pug_node_else: type_str = "ELSE"; break;
    case pug_node_each: type_str = "EACH"; break;
    case pug_node_root: default: break;
  }

  printf("[%s] ", type_str);
  
  if (node->tag.len > 0) {
    printf("Name: \x1b[32m");
    pug_str_print(node->tag);
    printf("\x1b[0m ");
  }

  if (node->selector.len > 0) {
    printf("Selector (");
    const char *ptr = node->selector.buf;
    const char *end = node->selector.buf + node->selector.len;
    
    int first = 1;
    while (ptr < end) {
      if (*ptr == '#') {
        ptr++;
        const char *id_start = ptr;
        while (ptr < end && *ptr != '#' && *ptr != '.') ptr++;
        if (!first) printf(", ");
        printf("id: ");
        fwrite(id_start, 1, ptr - id_start, stdout);
        first = 0;
      } else if (*ptr == '.') {
        ptr++;
        const char *class_start = ptr;
        while (ptr < end && *ptr != '#' && *ptr != '.') ptr++;
        if (!first) printf(", ");
        printf("class: ");
        fwrite(class_start, 1, ptr - class_start, stdout);
        first = 0;
      } else {
        ptr++;
      }
    }
    printf(") ");
  }

  if (node->attrs.len > 0) {
    printf("Attrs: \x1b[34m");
    pug_str_print(node->attrs);
    printf("\x1b[0m ");
  }

  if (node->text.len > 0) {
    printf("Content:\n\x1b[33m");
    pug_str_print(node->text);
  }
  
  printf("\x1b[0m\n");

  pug_ast_print(node->head, depth + 1);
  pug_ast_print(node->next, depth);
}
#endif
static const char *pug_skip_whitespace(const char *p, const char *end) {
  while (p < end && isspace((unsigned char)*p)) p++;
  return p;
}

static const char *pug_find_json_value_end(const char *p, const char *end) {
  p = pug_skip_whitespace(p, end);
  if (p >= end) return end;

  if (*p == '{' || *p == '[') {
    char open = *p;
    char close = (open == '{') ? '}' : ']';
    int depth = 0, in_string = 0;
    while (p < end) {
      if (in_string) {
        if (*p == '"' && *(p - 1) != '\\') in_string = 0;
      } else {
        if (*p == '"') in_string = 1;
        else if (*p == open) depth++;
        else if (*p == close) {
          depth--;
          if (depth == 0) { p++; break; }
        }
      }
      p++;
    }
    return p;
  } else {
    int in_string = (*p == '"');
    if (in_string) p++;
    while (p < end) {
      if (in_string) {
        if (*p == '"' && *(p - 1) != '\\') { p++; break; }
      } else {
        if (
          *p == ',' || *p == '}' || *p == ']' ||
          *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'
        ) {
          break;
        }
      }
      p++;
    }
    return p;
  }
}

static const char *pug_json_find_in_object(
  const char *p, const char *end, const char *key_buf, size_t key_len
) {
  p = pug_skip_whitespace(p, end);
  if (p >= end || *p != '{') return NULL;
  p++;

  while (p < end) {
    p = pug_skip_whitespace(p, end);
    if (p >= end || *p == '}') return NULL;
    if (*p != '"') return NULL;
    p++;

    const char *k_start = p;
    while (p < end && *p != '"') p++;
    if (p >= end) return NULL;
    size_t cur_len = (size_t)(p - k_start);
    p++;

    p = pug_skip_whitespace(p, end);
    if (p >= end || *p != ':') return NULL;
    p++;

    p = pug_skip_whitespace(p, end);
    const char *v_start = p;
    const char *v_end = pug_find_json_value_end(v_start, end);

    if (cur_len == key_len && memcmp(k_start, key_buf, key_len) == 0) {
      return v_start;
    }

    p = pug_skip_whitespace(v_end, end);
    if (p < end && *p == ',') p++;
  }
  return NULL;
}

static const char *pug_json_find_in_array(
  const char *p, const char *end, int target_index
) {
  p = pug_skip_whitespace(p, end);
  if (p >= end || *p != '[') return NULL;
  p++;

  int current_index = 0;
  while (p < end) {
    p = pug_skip_whitespace(p, end);
    if (p >= end || *p == ']') return NULL;

    const char *v_start = p;
    const char *v_end = pug_find_json_value_end(v_start, end);

    if (current_index == target_index) return v_start;

    current_index++;
    p = pug_skip_whitespace(v_end, end);
    if (p < end && *p == ',') p++;
  }
  return NULL;
}

static struct pug_str_t pug_json_get_tok(struct pug_str_t json, struct pug_str_t path) {
  struct pug_str_t result = pug_str_n(NULL, 0);
  if (!json.buf || json.len == 0 || !path.buf || path.len == 0) return result;

  const char *p = json.buf;
  const char *end = json.buf + json.len;
  
  const char *path_p = path.buf;
  const char *path_end = path.buf + path.len;

  while (path_p < path_end) {
    path_p = pug_skip_whitespace(path_p, path_end);
    if (path_p >= path_end) break;

    if (*path_p == '[') {
      path_p++;
      int index = 0;
      while (path_p < path_end && *path_p >= '0' && *path_p <= '9') {
        index = index * 10 + (*path_p - '0');
        path_p++;
      }
      if (path_p < path_end && *path_p == ']') path_p++;
      p = pug_json_find_in_array(p, end, index);
      if (!p) return result;
    } else {
      const char *key_start = path_p;
      while (path_p < path_end && *path_p != '.' && *path_p != '[') {
        path_p++;
      }
      size_t key_len = (size_t)(path_p - key_start);
      if (key_len == 0) return result;

      p = pug_json_find_in_object(p, end, key_start, key_len);
      if (!p) return result;
    }

    if (path_p < path_end && *path_p == '.') {
      path_p++;
    }
  }

  const char *tok_start = pug_skip_whitespace(p, end);
  const char *tok_end = pug_find_json_value_end(tok_start, end);

  while (tok_end > tok_start && isspace((unsigned char)*(tok_end - 1))) {
    tok_end--;
  }

  if (tok_end - tok_start >= 2 && *tok_start == '"' && *(tok_end - 1) == '"') {
    tok_start++;
    tok_end--;
  }

  result = pug_str_n(tok_start, (size_t)(tok_end - tok_start));
  return result;
}

static int is_self_closing(const struct pug_str_t tag) {
  int i;
  if (tag.buf == NULL || tag.len == 0) return 0;
  for (i = 0; self_closing_tags[i] != NULL; i++) {
    if (pug_str_equals(tag, pug_str_s(self_closing_tags[i]))) {
      return 1;
    }
  }
  return 0;
}

static int pug_str_resize(struct pug_str_t *s, size_t len) {
  if (s->len + len >= s->capacity) {
    size_t capacity = s->capacity + len + 1;
    char *buf = (char *)calloc(1, capacity * sizeof(char));
    if (!buf) return -1;
    if (s->buf) {
      memcpy(buf, s->buf, s->len);
      free(s->buf);
    }
    buf[s->len] = '\0';
    s->buf = buf, s->capacity = capacity;
  }
  return 0;
}

static int pug_str_push(struct pug_str_t *s, const char *p, size_t len) {
  if (len > 0) {
    if (!p) return -1;
    if (pug_str_resize(s, len) == -1) return -1;
    memcpy(s->buf + s->len, p, len);
    s->len += len, s->buf[s->len] = '\0';
  }
  return 0;
}

static int pug_str_put(struct pug_str_t *s, char c) {
  if (pug_str_resize(s, 1) == -1) return -1;
  s->buf[s->len++] = c;
  return 0;
}

static int pug_str_append(struct pug_str_t *s, const char *p) {
  return pug_str_push(s, p, p ? strlen(p) : 0);
}

static int is_inside_pre(struct pug_ast_t *node) {
  struct pug_ast_t *curr = node ? node->parent : NULL;
  while (curr != NULL) {
    if (curr->type == pug_node_code_block && pug_str_equals(curr->tag, pug_str_s("pre"))) {
      return 1;
    }
    if (curr->type == pug_node_root) {
      break;
    }
    curr = curr->parent;
  }
  return 0;
}

static int pug_interpolate_variables(
  struct pug_str_t s, struct pug_str_t *ctx_json, struct pug_str_t item,
  struct pug_str_t loop_var, struct pug_str_t *out
) {
  const char *cursor = s.buf;
  const char *end = s.buf + s.len;
  
  while (cursor < end) {
    if (cursor + 1 < end && *cursor == '#' && *(cursor + 1) == '{') {
      const char *start = cursor + 2;
      const char *brace = start;
      
      while (brace < end && *brace != '}') {
        brace++;
      }
      
      if (brace < end) {
        struct pug_str_t var_name = pug_str_n(start, (size_t)(brace - start));
        struct pug_str_t val = pug_str_n(NULL, 0);
        
        if (item.buf && item.len > 0) {
          if (loop_var.len > 0 && pug_str_equals(var_name, loop_var)) {
            val = item;
          } else {
            if (loop_var.len > 0 && var_name.len > loop_var.len &&
                memcmp(var_name.buf, loop_var.buf, loop_var.len) == 0 &&
                var_name.buf[loop_var.len] == '.') {
              struct pug_str_t sub_path = pug_str_n(var_name.buf + loop_var.len + 1, var_name.len - loop_var.len - 1);
              val = pug_json_get_tok(item, sub_path);
            } else {
              val = pug_json_get_tok(item, var_name);
            }
          }
        }
        
        if (!val.buf || val.len == 0) {
          val = pug_json_get_tok(*ctx_json, var_name);
        }

        if (val.buf && val.len > 0) {
          const char *v_ptr = val.buf;
          const char *v_end = val.buf + val.len;

          if (val.len >= 2 && *v_ptr == '"' && *(v_end - 1) == '"') {
            v_ptr++;
            v_end--;
          }

          while (v_ptr < v_end) {
            if (*v_ptr == '\\' && v_ptr + 1 < v_end) {
              v_ptr++;
              char esc = *v_ptr;
              if (esc == 'n') {
                if (pug_str_put(out, '\n') == -1) return -1;
              } else if (esc == 't') {
                if (pug_str_put(out, '\t') == -1) return -1;
              } else if (esc == 'r') {
                if (pug_str_put(out, '\r') == -1) return -1;
              } else if (esc == '"' || esc == '\\' || esc == '/') {
                if (pug_str_put(out, esc) == -1) return -1;
              } else if (esc == 'u' && v_ptr + 4 < v_end) {
                char hex[5] = {v_ptr[1], v_ptr[2], v_ptr[3], v_ptr[4], '\0'};
                unsigned int codepoint = (unsigned int)strtoul(hex, NULL, 16);
                v_ptr += 4;

                if (codepoint < 0x80) {
                  if (pug_str_put(out, (char)codepoint) == -1) return -1;
                } else if (codepoint < 0x800) {
                  if (pug_str_put(out, (char)(0xC0 | (codepoint >> 6))) == -1) return -1;
                  if (pug_str_put(out, (char)(0x80 | (codepoint & 0x3F))) == -1) return -1;
                } else {
                  if (pug_str_put(out, (char)(0xE0 | (codepoint >> 12))) == -1) return -1;
                  if (pug_str_put(out, (char)(0x80 | ((codepoint >> 6) & 0x3F))) == -1) return -1;
                  if (pug_str_put(out, (char)(0x80 | (codepoint & 0x3F))) == -1) return -1;
                }
              } else {
                if (pug_str_put(out, '\\') == -1) return -1;
                if (pug_str_put(out, esc) == -1) return -1;
              }
            } else {
              if (pug_str_put(out, *v_ptr) == -1) {
                return -1;
              }
            }
            v_ptr++;
          }
        }
        
        cursor = brace + 1;
        continue;
      }
    }
    
    if (pug_str_put(out, *cursor) == -1) {
      return -1;
    }
    cursor++;
  }
  return 0;
}

static int pug_ast_node_render(
  struct pug_ast_t *node, struct pug_str_t *ctx_json,
  struct pug_str_t item, struct pug_str_t loop_var,
  int indent_level, struct pug_str_t *indent_str,
  struct pug_str_t *out
) {
  if (!node) return -1;
  
  if (node->type == pug_node_root) {
    struct pug_ast_t *child = node->head;
    while (child != NULL) {
      if (pug_ast_node_render(
        child, ctx_json, item, loop_var, indent_level, indent_str, out
      ) == -1) {
        goto fail;
      }
      child = child->next;
    }
    return 0;
  }
  int target_len = indent_level * 2;
  if (target_len < 0) target_len = 0;

  if ((size_t)target_len > indent_str->len) {
    int diff = target_len - (int)indent_str->len;
    for (int i = 0; i < diff; i += 2) {
      if (pug_str_append(indent_str, "  ") == -1) {
        goto fail;
      }
    }
  } else {
    indent_str->len = (size_t)target_len;
    if (indent_str->buf && indent_str->capacity > indent_str->len) {
      indent_str->buf[indent_str->len] = '\0';
    }
  }

  if (node->type == pug_node_doctype) {
    if (pug_str_append(out, "<!DOCTYPE ") == -1) goto fail;
    if (node->attrs.len > 0) {
      if (pug_interpolate_variables(
        node->attrs, ctx_json, item, loop_var, out) == -1
      ) {
        goto fail;
      }
    } else {
      if (pug_str_append(out, "html") == -1) {
        goto fail;
      }
    }
    if (pug_str_append(out, ">\n") == -1) {
      goto fail;
    }
  }  
  else if (node->type == pug_node_tag) {
    int is_id_shortcut = (node->selector.len > 0 && node->selector.buf[0] == '#');
    
    if (pug_str_push(out, indent_str->buf, indent_str->len) == -1) {
      goto fail;
    }
    if (pug_str_put(out, '<') == -1) {
      goto fail;
    }

    if (is_id_shortcut) {
      struct pug_str_t tag_name = node->tag.len > 0 ? node->tag : pug_str_n("div", 3);
      if (pug_str_push(out, tag_name.buf, tag_name.len) == -1) {
        goto fail;
      }
      if (pug_str_append(out, " id=\"") == -1) {
        goto fail;
      }

      const char *id_start = node->selector.buf + 1;
      size_t id_len = 0;
      while (id_len < node->selector.len - 1 && id_start[id_len] != '.') id_len++;
      if (pug_str_push(out, id_start, id_len) == -1) {
        goto fail;
      }
      if (pug_str_put(out, '"') == -1) {
        goto fail;
      }
      
      if (id_len < node->selector.len - 1) {
        if (pug_str_append(out, " class=\"") == -1) {
          goto fail;
        }
        const char *class_start = id_start + id_len + 1;
        size_t class_len = (node->selector.buf + node->selector.len) - class_start;
        for (size_t k = 0; k < class_len; k++) {
          char c = (class_start[k] == '.') ? ' ' : class_start[k];
          if (pug_str_put(out, c) == -1) {
            goto fail;
          }
        }
        if (pug_str_put(out, '"') == -1) {
          goto fail;
        }
      }
    } else {
      struct pug_str_t tag_name = node->tag.len > 0 ? node->tag : pug_str_n("div", 3);
      if (pug_str_push(out, tag_name.buf, tag_name.len) == -1) {
        goto fail;
      }
      
      if (node->selector.len > 0) {
        const char *ptr = node->selector.buf;
        const char *end = node->selector.buf + node->selector.len;
        const char *dot = memchr(ptr, '.', node->selector.len);
        if (dot) {
          if (pug_str_append(out, " class=\"") == -1) {
            goto fail;
          }
          for (const char *p = dot + 1; p < end; p++) {
            char c = (*p == '.') ? ' ' : *p;
            if (pug_str_put(out, c) == -1) {
              goto fail;
            }
          }
          if (pug_str_put(out, '"') == -1) {
            goto fail;
          }
        }
      }
    }

    if (node->attrs.len > 0) {
      if (pug_str_put(out, ' ') == -1) {
        goto fail;
      }
      if (pug_interpolate_variables(node->attrs, ctx_json, item, loop_var, out) == -1) {
        goto fail;
      }
    }

    if (is_self_closing(node->tag)) {
      if (pug_str_append(out, " />\n") == -1) goto fail;
    } else {
      if (node->text.len > 0) {
        if (pug_str_put(out, '>') == -1) goto fail;
        if (pug_interpolate_variables(node->text, ctx_json, item, loop_var, out) == -1) goto fail;
        if (pug_str_append(out, "</") == -1) goto fail;
        struct pug_str_t tag_name = node->tag.len > 0 ? node->tag : pug_str_n("div", 3);
        if (pug_str_push(out, tag_name.buf, tag_name.len) == -1) goto fail;
        if (pug_str_append(out, ">\n") == -1) goto fail;
      } else if (node->head == NULL) {
        if (pug_str_append(out, "></") == -1) goto fail;
        struct pug_str_t tag_name = node->tag.len > 0 ? node->tag : pug_str_n("div", 3);
        if (pug_str_push(out, tag_name.buf, tag_name.len) == -1) goto fail;
        if (pug_str_append(out, ">\n") == -1) goto fail;
      } else {
        if (pug_str_append(out, ">\n") == -1) goto fail;
        struct pug_ast_t *child = node->head;
        while (child != NULL) {
          if (pug_ast_node_render(child, ctx_json, item, loop_var, indent_level + 1, indent_str, out) == -1) {
            goto fail;
          }
          child = child->next;
        }
        int curr_target_len = indent_level * 2;
        indent_str->len = (size_t)curr_target_len;
        if (indent_str->buf && indent_str->capacity > indent_str->len) {
          indent_str->buf[indent_str->len] = '\0';
        }

        if (pug_str_push(out, indent_str->buf, indent_str->len) == -1) goto fail;
        if (pug_str_append(out, "</") == -1) goto fail;
        struct pug_str_t tag_name = node->tag.len > 0 ? node->tag : pug_str_n("div", 3);
        if (pug_str_push(out, tag_name.buf, tag_name.len) == -1) goto fail;
        if (pug_str_append(out, ">\n") == -1) goto fail;
      }
    }
  }  
  else if (node->type == pug_node_code_block) {
    if (pug_str_push(out, indent_str->buf, indent_str->len) == -1) goto fail;
    if (pug_str_put(out, '<') == -1) goto fail;
    if (pug_str_push(out, node->tag.buf, node->tag.len) == -1) goto fail;
    if (pug_str_append(out, ">\n") == -1) goto fail;

    struct pug_ast_t *child = node->head;
    while (child != NULL) {
      if (pug_ast_node_render(child, ctx_json, item, loop_var, indent_level + 1, indent_str, out) == -1) {
        goto fail;
      }
      child = child->next;
    }
    
    int curr_target_len = indent_level * 2;
    indent_str->len = (size_t)curr_target_len;
    if (indent_str->buf && indent_str->capacity > indent_str->len) {
      indent_str->buf[indent_str->len] = '\0';
    }

    if (pug_str_push(out, indent_str->buf, indent_str->len) == -1) goto fail;
    if (pug_str_append(out, "</") == -1) goto fail;
    if (pug_str_push(out, node->tag.buf, node->tag.len) == -1) goto fail;
    if (pug_str_append(out, ">\n") == -1) goto fail;
  }  
  else if (node->type == pug_node_text) {
    if (node->text.len > 0) {
      int inside_pre = is_inside_pre(node);
      if (!inside_pre) {
        if (pug_str_push(out, indent_str->buf, indent_str->len) == -1) goto fail;
      }
      if (pug_interpolate_variables(node->text, ctx_json, item, loop_var, out) == -1) goto fail;
      if (pug_str_put(out, '\n') == -1) goto fail;
    }
    
    struct pug_ast_t *child = node->head;
    while (child != NULL) {
      if (pug_ast_node_render(child, ctx_json, item, loop_var, indent_level, indent_str, out) == -1) {
        goto fail;
      }
      child = child->next;
    }
  }  
  else if (node->type == pug_node_if) {
    const char *p = node->tag.buf + 2;
    const char *end = node->tag.buf + node->tag.len;
    while (p < end && (*p == ' ' || *p == '\t')) p++;
    
    int condition_met = 0;
    if (p < end) {
      size_t k_len = 0;
      while (p + k_len < end && p[k_len] != ' ' && p[k_len] != '\t') k_len++;
      struct pug_str_t local_key = pug_str_n(p, k_len);

      struct pug_str_t val = pug_json_get_tok(*ctx_json, local_key);
      if (
        val.len > 0 && !pug_str_equals(val, pug_str_s("false")) &&
        !pug_str_equals(val, pug_str_s("null")) && !pug_str_equals(val, pug_str_s("\"\""))
      ) {
        condition_met = 1;
      }
    }

    if (condition_met) {
      struct pug_ast_t *child = node->head;
      while (child != NULL) {
        if (pug_ast_node_render(child, ctx_json, item, loop_var, indent_level, indent_str, out) == -1) {
          goto fail;
        }
        child = child->next;
      }
    } 
    else {
      struct pug_ast_t *next_node = node->next;
      if (next_node != NULL && next_node->type == pug_node_else) {
        struct pug_ast_t *child = next_node->head;
        while (child != NULL) {
          if (pug_ast_node_render(child, ctx_json, item, loop_var, indent_level, indent_str, out) == -1) {
            goto fail;
          }
          child = child->next;
        }
      }
    }
  }  
  else if (node->type == pug_node_else) {
    return 0;
  }
  else if (node->type == pug_node_each) {
    const char *p = node->tag.buf;
    const char *end = node->tag.buf + node->tag.len;
    if (node->tag.len > 5 && memcmp(p, "each ", 5) == 0) {
        p += 5;
    }
    p = pug_skip_whitespace(p, end);

    const char *var_name_start = p;
    while (p < end && *p != ' ' && *p != '\t') p++;
    struct pug_str_t loop_var_local = pug_str_n(var_name_start, (size_t)(p - var_name_start)); 
    p = pug_skip_whitespace(p, end);
    if (p + 3 < end && memcmp(p, "in ", 3) == 0) {
        p += 3;
        p = pug_skip_whitespace(p, end);

        struct pug_str_t array_key = pug_str_n(p, (size_t)(end - p));
        struct pug_str_t arr_val = pug_str_n(NULL, 0);
        if (array_key.len > 0 && array_key.buf[0] == '[') {
          arr_val = array_key;
        } else {
          arr_val = pug_json_get_tok(*ctx_json, array_key);
        }
        if (arr_val.buf && arr_val.len > 0) {
            int i = 0;
            while (1) {
                const char *elem_start = pug_json_find_in_array(arr_val.buf, arr_val.buf + arr_val.len, i);
                if (!elem_start) break;

                const char *elem_end = pug_find_json_value_end(elem_start, arr_val.buf + arr_val.len);
                struct pug_str_t item_val = pug_str_n(elem_start, (size_t)(elem_end - elem_start));

                if (item_val.len >= 2 && item_val.buf[0] == '"' && item_val.buf[item_val.len - 1] == '"') {
                    item_val.buf++;
                    item_val.len -= 2;
                }

                struct pug_ast_t *child = node->head;
                while (child != NULL) {
                    if (pug_ast_node_render(child, ctx_json, item_val, loop_var_local, indent_level, indent_str, out) == -1) {
                        goto fail;
                    }
                    child = child->next;
                }
                i++;
            }
        }
    }
  }
  return 0;

fail:
  pug_str_free(out);
  return -1;
}

int lte_pug_render(
  const char *input, size_t len,
  struct pug_str_t *ctx, struct pug_str_t *out
) {
  int result = -1;
  struct pug_ast_t *root = NULL;
  struct pug_str_t indent_str = pug_str_n(NULL, 0);
  if (!input || len == 0 || !out) return -1;
  if (pug_parse_ast(input, len, &root)) return -1;
  result = pug_ast_node_render(root, ctx, pug_str_n(NULL, 0), pug_str_n(NULL, 0), 0, &indent_str, out);
  if (indent_str.buf) free(indent_str.buf);
  #ifdef TEST_PUG
  pug_ast_print(root, 0);
  #endif
  pug_ast_free(root);
  return result;
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
#ifdef TEST_PUG
int main(int argc, char **argv) {
  if(argc > 1) {
    struct pug_str_t ctx = pug_str_n(NULL, 0);
    struct pug_str_t out = pug_str_n(NULL, 0);
    if (argc > 2) {
      char *js = NULL;
      #ifndef USE_PARSON
      size_t len = 0;
      if (read_file(argv[2], &js, &len) == 0) {
        ctx = pug_str_n(js, len);
      }
      #else
      JSON_Value *val = json_parse_file(argv[2]);
      if (val) {
        js = json_serialize_to_string(val);
        if (js) {
          ctx = pug_str_n(js, strlen(js));
          pug_str_print(ctx);
        }
        json_value_free(val);
      }
      #endif
    }
    if (lte_pug_file_render(argv[1], &ctx, &out)) {
      return -1;
    }
    pug_str_free(&ctx);
    printf("%.*s\n", (int)out.len, out.buf);
    pug_str_free(&out);
    return 0;
  }
  return -1;
}
#endif
