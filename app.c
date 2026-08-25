#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <lmdb.h>
#include "parson.h"

#ifndef DB_PATH
  #define DB_PATH "./data/music"
#endif

// ==========================================
// THUẬT TOÁN MD5 THUẦN C
// ==========================================
typedef struct {
    uint32_t state[4];
    uint32_t count[2];
    unsigned char buffer[64];
} MD5_CTX;

#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

#define FF(a, b, c, d, x, s, ac) { (a) += F((b), (c), (d)) + (x) + (uint32_t)(ac); (a) = ROTATE_LEFT((a), (s)); (a) += (b); }
#define GG(a, b, c, d, x, s, ac) { (a) += G((b), (c), (d)) + (x) + (uint32_t)(ac); (a) = ROTATE_LEFT((a), (s)); (a) += (b); }
#define HH(a, b, c, d, x, s, ac) { (a) += H((b), (c), (d)) + (x) + (uint32_t)(ac); (a) = ROTATE_LEFT((a), (s)); (a) += (b); }
#define II(a, b, c, d, x, s, ac) { (a) += I((b), (c), (d)) + (x) + (uint32_t)(ac); (a) = ROTATE_LEFT((a), (s)); (a) += (b); }

static void md5_transform(uint32_t state[4], const unsigned char block[64]) {
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3], x[16];
    for (int i = 0, j = 0; i < 16; i++, j += 4)
        x[i] = ((uint32_t)block[j]) | (((uint32_t)block[j + 1]) << 8) | (((uint32_t)block[j + 2]) << 16) | (((uint32_t)block[j + 3]) << 24);

    FF(a, b, c, d, x[ 0],  7, 0xd76aa478); FF(d, a, b, c, x[ 1], 12, 0xe8c7b756);
    FF(c, d, a, b, x[ 2], 17, 0x242070db); FF(b, c, d, a, x[ 3], 22, 0xc1bdceee);
    FF(a, b, c, d, x[ 4],  7, 0xf57c0faf); FF(d, a, b, c, x[ 5], 12, 0x4787c62a);
    FF(c, d, a, b, x[ 6], 17, 0xa8304613); FF(b, c, d, a, x[ 7], 22, 0xfd469501);
    FF(a, b, c, d, x[ 8],  7, 0x698098d8); FF(d, a, b, c, x[ 9], 12, 0x8b44f7af);
    FF(c, d, a, b, x[10], 17, 0xffff5bb1); FF(b, c, d, a, x[11], 22, 0x895cd7be);
    FF(a, b, c, d, x[12],  7, 0x6b901122); FF(d, a, b, c, x[13], 12, 0xfd987193);
    FF(c, d, a, b, x[14], 17, 0xa679438e); FF(b, c, d, a, x[15], 22, 0x49b40821);

    GG(a, b, c, d, x[ 1],  5, 0xf61e2562); GG(d, a, b, c, x[ 6],  9, 0xc040b340);
    GG(c, d, a, b, x[11], 14, 0x265e5a51); GG(b, c, d, a, x[ 0], 20, 0xe9b6c7aa);
    GG(a, b, c, d, x[ 5],  5, 0xd62f105d); GG(d, a, b, c, x[10],  9, 0x02441453);
    GG(c, d, a, b, x[15], 14, 0xd8a1e681); GG(b, c, d, a, x[ 4], 20, 0xe7d3fbc8);
    GG(a, b, c, d, x[ 9],  5, 0x21e1cde6); GG(d, a, b, c, x[14],  9, 0xc33707d6);
    GG(c, d, a, b, x[ 3], 14, 0xf4d50d87); GG(b, c, d, a, x[ 8], 20, 0x455a14ed);
    GG(a, b, c, d, x[13],  5, 0xa9e3e905); GG(d, a, b, c, x[ 2],  9, 0xfcefa3f8);
    GG(c, d, a, b, x[ 7], 14, 0x676f02d9); GG(b, c, d, a, x[12], 20, 0x8d2a4c8a);

    HH(a, b, c, d, x[ 5],  4, 0xfffa3942); HH(d, a, b, c, x[ 8], 11, 0x8771f681);
    HH(c, d, a, b, x[11], 16, 0x6d9d6122); HH(b, c, d, a, x[14], 23, 0xfde5380c);
    HH(a, b, c, d, x[ 1],  4, 0xa4beea44); HH(d, a, b, c, x[ 4], 11, 0x4bdecfa9);
    HH(c, d, a, b, x[ 7], 16, 0xf6bb4b60); HH(b, c, d, a, x[10], 23, 0xbebfbc70);
    HH(a, b, c, d, x[13],  4, 0x289b7ec6); HH(d, a, b, c, x[ 0], 11, 0xeaa127fa);
    HH(c, d, a, b, x[ 3], 16, 0xd4ef3085); HH(b, c, d, a, x[ 6], 23, 0x04881d05);
    HH(a, b, c, d, x[ 9],  4, 0xd9d4d039); HH(d, a, b, c, x[12], 11, 0xe6db99e5);
    HH(c, d, a, b, x[15], 16, 0x1fa27cf8); HH(b, c, d, a, x[ 2], 23, 0xc4ac5665);

    II(a, b, c, d, x[ 0],  6, 0xf4292244); II(d, a, b, c, x[ 7], 10, 0x432aff97);
    II(c, d, a, b, x[14], 15, 0xab9423a7); II(b, c, d, a, x[ 5], 21, 0xfc93a039);
    II(a, b, c, d, x[12],  6, 0x655b59c3); II(d, a, b, c, x[ 3], 10, 0x8f0ccc92);
    II(c, d, a, b, x[10], 15, 0xffeff47d); II(b, c, d, a, x[ 1], 21, 0x85845dd1);
    II(a, b, c, d, x[ 8],  6, 0x6fa87e4f); II(d, a, b, c, x[15], 10, 0xfe2ce6e0);
    II(c, d, a, b, x[ 6], 15, 0xa3014314); II(b, c, d, a, x[13], 21, 0x4e0811a1);
    II(a, b, c, d, x[ 4],  6, 0xf7537e82); II(d, a, b, c, x[11], 10, 0xbd3af235);
    II(c, d, a, b, x[ 2], 15, 0x2ad7d2bb); II(b, c, d, a, x[ 9], 21, 0xeb86d391);

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
}

void md5_init(MD5_CTX *context) {
    context->count[0] = context->count[1] = 0;
    context->state[0] = 0x67452301; context->state[1] = 0xefcdab89;
    context->state[2] = 0x98badcfe; context->state[3] = 0x10325476;
}

void md5_update(MD5_CTX *context, const unsigned char *input, size_t inputLen) {
    size_t i, index, partLen;
    index = (size_t)((context->count[0] >> 3) & 0x3F);
    if ((context->count[0] += ((uint32_t)inputLen << 3)) < ((uint32_t)inputLen << 3)) context->count[1]++;
    context->count[1] += ((uint32_t)inputLen >> 29);
    partLen = 64 - index;

    if (inputLen >= partLen) {
        memcpy(&context->buffer[index], input, partLen);
        md5_transform(context->state, context->buffer);
        for (i = partLen; i + 63 < inputLen; i += 64) md5_transform(context->state, &input[i]);
        index = 0;
    } else { i = 0; }
    memcpy(&context->buffer[index], &input[i], inputLen - i);
}

void md5_final(unsigned char digest[16], MD5_CTX *context) {
    unsigned char bits[8], PADDING[64] = { 0x80 };
    size_t index, padLen;
    for (int i = 0; i < 8; i++) bits[i] = (unsigned char)((context->count[(i >= 4 ? 0 : 1)] >> ((i & 3) * 8)) & 0xFF);
    index = (size_t)((context->count[0] >> 3) & 0x3F);
    padLen = (index < 56) ? (56 - index) : (120 - index);
    md5_update(context, PADDING, padLen);
    md5_update(context, bits, 8);
    for (int i = 0; i < 4; i++) {
        digest[i*4] = (unsigned char)(context->state[i] & 0xFF);
        digest[i*4+1] = (unsigned char)((context->state[i] >> 8) & 0xFF);
        digest[i*4+2] = (unsigned char)((context->state[i] >> 16) & 0xFF);
        digest[i*4+3] = (unsigned char)((context->state[i] >> 24) & 0xFF);
    }
}

void calculate_md5(const char *input, char *output_hex) {
    MD5_CTX ctx;
    unsigned char digest[16];
    md5_init(&ctx);
    md5_update(&ctx, (unsigned char*)input, strlen(input));
    md5_final(digest, &ctx);
    for (int i = 0; i < 16; i++) sprintf(&output_hex[i * 2], "%02x", digest[i]);
    output_hex[32] = '\0';
}

typedef struct {
    char *buf;
    size_t len;
} RawData;

// ==========================================
// HÀM HỖ TRỢ VARINT CHO PROTOBUF
// ==========================================
static int write_varint(uint64_t value, unsigned char *buf) {
    int i = 0;
    while (value >= 0x80) {
        buf[i++] = (unsigned char)((value & 0x7F) | 0x80);
        value >>= 7;
    }
    buf[i++] = (unsigned char)(value & 0x7F);
    return i;
}

static int read_varint(const unsigned char *buf, int max_len, uint64_t *value) {
    int i = 0;
    *value = 0;
    int shift = 0;
    while (i < max_len) {
        uint64_t b = buf[i];
        *value |= (b & 0x7F) << shift;
        i++;
        if (!(b & 0x80)) return i;
        shift += 7;
    }
    return 0;
}

// ==========================================
// 1. CÁC HÀM XỬ LÝ TRACK
// ==========================================

// Chuyển JSON_Value (Track) -> Protobuf Binary Buffer
int track_to_protobuf(JSON_Value *val, RawData *raw) {
    if (!val || json_type(val) != JSONObject || !raw) return -1;
    JSON_Object *obj = json_value_get_object(val);

    size_t capacity = 256;
    size_t offset = 0;
    unsigned char *buf = malloc(capacity);
    if (!buf) return -1;

    #define ENSURE_CAPACITY(needed) \
        if (offset + (needed) > capacity) { \
            capacity = capacity * 2 + (needed); \
            unsigned char *new_buf = realloc(buf, capacity); \
            if (!new_buf) { free(buf); return -1; } \
            buf = new_buf; \
        }

    // Field 1: title (string) - Tag = (1 << 3) | 2 = 10
    const char *title = json_object_get_string(obj, "title");
    if (title) {
        int len = strlen(title);
        ENSURE_CAPACITY(1 + 10 + len);
        buf[offset++] = 10;
        offset += write_varint(len, &buf[offset]);
        memcpy(&buf[offset], title, len);
        offset += len;
    }

    // Field 2: artist (string) - Tag = (2 << 3) | 2 = 18
    const char *artist = json_object_get_string(obj, "artist");
    if (artist) {
        int len = strlen(artist);
        ENSURE_CAPACITY(1 + 10 + len);
        buf[offset++] = 18;
        offset += write_varint(len, &buf[offset]);
        memcpy(&buf[offset], artist, len);
        offset += len;
    }

    // Field 3: duration (int) - Tag = (3 << 3) | 0 = 24
    double duration = json_object_get_number(obj, "duration");
    if (duration > 0) {
        ENSURE_CAPACITY(1 + 10);
        buf[offset++] = 24;
        offset += write_varint((uint64_t)duration, &buf[offset]);
    }

    // Field 4: art (string) - Tag = (4 << 3) | 2 = 34
    const char *art = json_object_get_string(obj, "art");
    if (art) {
        int len = strlen(art);
        ENSURE_CAPACITY(1 + 10 + len);
        buf[offset++] = 34;
        offset += write_varint(len, &buf[offset]);
        memcpy(&buf[offset], art, len);
        offset += len;
    }

    // Field 5: url (string) - Tag = (5 << 3) | 2 = 42
    const char *url = json_object_get_string(obj, "url");
    if (url) {
        int len = strlen(url);
        ENSURE_CAPACITY(1 + 10 + len);
        buf[offset++] = 42;
        offset += write_varint(len, &buf[offset]);
        memcpy(&buf[offset], url, len);
        offset += len;
    }

    #undef ENSURE_CAPACITY

    raw->len = offset;
    raw->buf = (char *)buf;
    return 0;
}

// Chuyển Protobuf Binary Buffer -> JSON_Value (Track)
JSON_Value* protobuf_to_track(const unsigned char *input, int input_len) {
    JSON_Value *root_val = json_value_init_object();
    JSON_Object *obj = json_value_get_object(root_val);
    int offset = 0;

    while (offset < input_len) {
        uint64_t tag = 0;
        int n = read_varint(&input[offset], input_len - offset, &tag);
        if (n <= 0) break;
        offset += n;

        int field_number = tag >> 3;
        int wire_type = tag & 0x07;

        if (wire_type == 2) { // String / Bytes
            uint64_t str_len = 0;
            n = read_varint(&input[offset], input_len - offset, &str_len);
            if (n <= 0) break;
            offset += n;

            if (offset + str_len > (uint64_t)input_len) break;

            char *str_buf = malloc(str_len + 1);
            memcpy(str_buf, &input[offset], str_len);
            str_buf[str_len] = '\0';

            if (field_number == 1) {
                json_object_set_string(obj, "title", str_buf);
            } else if (field_number == 2) {
                json_object_set_string(obj, "artist", str_buf);
            } else if (field_number == 4) {
                json_object_set_string(obj, "art", str_buf);
            } else if (field_number == 5) {
                json_object_set_string(obj, "url", str_buf);
            }
            
            free(str_buf);
            offset += str_len;

        } else if (wire_type == 0) { // Varint (Integer)
            uint64_t val = 0;
            n = read_varint(&input[offset], input_len - offset, &val);
            if (n <= 0) break;
            offset += n;

            if (field_number == 3) {
                json_object_set_number(obj, "duration", (double)val);
            }
        } else {
            break;
        }
    }
    return root_val;
}

// ==========================================
// 2. CÁC HÀM XỬ LÝ PLAYLIST
// ==========================================

// Chuyển JSON_Value (Playlist) -> Protobuf Binary Buffer
int playlist_to_protobuf(JSON_Value *val, RawData *raw) {
    if (!val || json_type(val) != JSONObject || !raw) return -1;
    JSON_Object *obj = json_value_get_object(val);

    size_t capacity = 256;
    size_t offset = 0;
    unsigned char *buf = malloc(capacity);
    if (!buf) return -1;

    #define ENSURE_CAPACITY(needed) \
        if (offset + (needed) > capacity) { \
            capacity = capacity * 2 + (needed); \
            unsigned char *new_buf = realloc(buf, capacity); \
            if (!new_buf) { free(buf); return -1; } \
            buf = new_buf; \
        }

    // Field 1: name (string) - Tag = (1 << 3) | 2 = 10
    const char *name = json_object_get_string(obj, "name");
    if (name) {
        int len = strlen(name);
        ENSURE_CAPACITY(1 + 10 + len);
        buf[offset++] = 10;
        offset += write_varint(len, &buf[offset]);
        memcpy(&buf[offset], name, len);
        offset += len;
    }

    // Field 2: tracks (Array of strings) - Repeated Field, Tag = (2 << 3) | 2 = 18
    JSON_Array *tracks_arr = json_object_get_array(obj, "tracks");
    if (tracks_arr) {
        size_t count = json_array_get_count(tracks_arr);
        for (size_t i = 0; i < count; i++) {
            const char *t_id = json_array_get_string(tracks_arr, i);
            if (t_id) {
                int len = strlen(t_id);
                ENSURE_CAPACITY(1 + 10 + len);
                buf[offset++] = 18;
                offset += write_varint(len, &buf[offset]);
                memcpy(&buf[offset], t_id, len);
                offset += len;
            }
        }
    }

    #undef ENSURE_CAPACITY

    raw->len = offset;
    raw->buf = (char *)buf;
    return 0;
}

// Chuyển Protobuf Binary Buffer -> JSON_Value (Playlist)
JSON_Value* protobuf_to_playlist(const unsigned char *input, int input_len) {
    JSON_Value *root_val = json_value_init_object();
    JSON_Object *obj = json_value_get_object(root_val);
    JSON_Array *tracks_arr = NULL;
    int offset = 0;

    while (offset < input_len) {
        uint64_t tag = 0;
        int n = read_varint(&input[offset], input_len - offset, &tag);
        if (n <= 0) break;
        offset += n;

        int field_number = tag >> 3;
        int wire_type = tag & 0x07;

        if (wire_type == 2) { // String
            uint64_t str_len = 0;
            n = read_varint(&input[offset], input_len - offset, &str_len);
            if (n <= 0) break;
            offset += n;

            if (offset + str_len > (uint64_t)input_len) break;

            char *str_buf = malloc(str_len + 1);
            memcpy(str_buf, &input[offset], str_len);
            str_buf[str_len] = '\0';

            if (field_number == 1) {
                json_object_set_string(obj, "name", str_buf);
            } else if (field_number == 2) {
                if (!tracks_arr) {
                    json_object_set_value(obj, "tracks", json_value_init_array());
                    tracks_arr = json_object_get_array(obj, "tracks");
                }
                json_array_append_string(tracks_arr, str_buf);
            }
            free(str_buf);
            offset += str_len;
        } else {
            break;
        }
    }
    return root_val;
}

int db_put(MDB_env *env, MDB_dbi dbi, const char *key_str, RawData *raw) {
    if (!env || !key_str || !raw || !raw->buf) return -1;

    MDB_txn *txn;
    if (mdb_txn_begin(env, NULL, 0, &txn) != 0) return -1;

    MDB_val key = { .mv_size = strlen(key_str) + 1, .mv_data = (void *)key_str };
    MDB_val val = { .mv_size = raw->len, .mv_data = raw->buf };

    int rc = mdb_put(txn, dbi, &key, &val, 0);
    if (rc == 0) {
        rc = mdb_txn_commit(txn);
    } else {
        mdb_txn_abort(txn);
    }
    return rc;
}

int db_delete(MDB_env *env, MDB_dbi dbi, const char *key_str) {
    MDB_txn *txn;
    int rc = mdb_txn_begin(env, NULL, 0, &txn);
    if (rc != 0) return rc;

    MDB_val key = { .mv_size = strlen(key_str) + 1, .mv_data = (void *)key_str };
    rc = mdb_del(txn, dbi, &key, NULL);
    if (rc == 0) rc = mdb_txn_commit(txn);
    else mdb_txn_abort(txn);
    return rc;
}

RawData db_get_raw(MDB_env *env, MDB_dbi dbi, const char *key_str) {
    MDB_txn *txn;
    RawData result = { .buf = NULL, .len = 0 };
    int rc = mdb_txn_begin(env, NULL, MDB_RDONLY, &txn);
    if (rc != 0) return result;

    MDB_val key = { .mv_size = strlen(key_str) + 1, .mv_data = (void *)key_str };
    MDB_val val;
    rc = mdb_get(txn, dbi, &key, &val);
    if (rc == 0) {
      result.buf = (char *)val.mv_data;
      result.len = val.mv_size;
    }
    mdb_txn_abort(txn);
    return result;
}

int format_seconds_to_hhmmss(long total_seconds, char *out_buf, size_t buf_size) {
    long hours, minutes, seconds;
    int written;

    if (!out_buf || buf_size < 9) {
        return -1; // Bộ đệm không hợp lệ hoặc không đủ dung lượng
    }

    if (total_seconds < 0) {
        total_seconds = 0; // Xử lý trường hợp số giây âm
    }

    hours = total_seconds / 3600;
    minutes = (total_seconds % 3600) / 60;
    seconds = total_seconds % 60;

    if (hours == 0) {
        // Nếu số giờ bằng 0, chỉ hiển thị mm:ss
        written = snprintf(out_buf, buf_size, "%02ld:%02ld", minutes, seconds);
    } else {
        // Nếu có giờ, hiển thị đầy đủ hh:mm:ss
        written = snprintf(out_buf, buf_size, "%02ld:%02ld:%02ld", hours, minutes, seconds);
    }

    // Kiểm tra xem snprintf có bị lỗi hoặc vượt quá kích thước bộ đệm không
    if (written < 0 || (size_t)written >= buf_size) {
        return -1;
    }

    return 0;
}

void handle_track_duration(JSON_Object *obj) {
  if (!obj) return;
  if (json_object_has_value(obj, "duration")) {
    char buf[20] = {0};
    long total_seconds = (long)json_object_get_number(obj, "duration");
    if (total_seconds > 0 && format_seconds_to_hhmmss(total_seconds, buf, sizeof(buf)) == 0) {
      json_object_remove(obj, "duration");
      json_object_set_string(obj, "duration", buf);
    }
  }
}

JSON_Value* get_track(MDB_env *env, MDB_dbi track_dbi, const char *track_id) {
    RawData raw = db_get_raw(env, track_dbi, track_id);
    if (!raw.buf) return NULL;
    JSON_Value *val= protobuf_to_track((unsigned char *)raw.buf, raw.len);
    if (json_type(val) == JSONObject) {
      json_object_set_string(json_object(val), "id", track_id);
      handle_track_duration(json_object(val));
    }
    return val;
}

JSON_Value* get_playlist(MDB_env *env, MDB_dbi playlist_dbi, const char *playlist_id) {
    RawData raw = db_get_raw(env, playlist_dbi, playlist_id);
    if (!raw.buf) return NULL;
    JSON_Value *val = protobuf_to_playlist((unsigned char *)raw.buf, raw.len);
    // free(raw_json);
    if (json_type(val) == JSONObject) {
      json_object_set_string(json_object(val), "id", playlist_id);
    }
    return val;
}

// Thêm tham số is_desc: 0 = ASC, 1 = DESC
JSON_Value* get_tracks(MDB_env *env, MDB_dbi track_dbi, int offset, int limit, int is_desc) {
    MDB_txn *txn;
    MDB_cursor *cursor;
    int rc = mdb_txn_begin(env, NULL, MDB_RDONLY, &txn);
    if (rc != 0) return NULL;

    rc = mdb_cursor_open(txn, track_dbi, &cursor);
    if (rc != 0) {
        mdb_txn_abort(txn);
        return NULL;
    }

    JSON_Value *root_val = json_value_init_array();
    JSON_Array *array = json_value_get_array(root_val);

    MDB_val key, val;
    int current_index = 0;
    int fetched_count = 0;

    MDB_cursor_op first_op = is_desc ? MDB_LAST : MDB_FIRST;
    MDB_cursor_op next_op = is_desc ? MDB_PREV : MDB_NEXT;

    rc = mdb_cursor_get(cursor, &key, &val, first_op);
    while (rc == 0) {
        if (current_index >= offset && fetched_count < limit) {
            JSON_Value *item_val = protobuf_to_track((unsigned char *)val.mv_data, val.mv_size);
            if (item_val) {
                JSON_Object *obj = json_value_get_object(item_val);
                //json_object_set_string(obj, "_key", (char *)key.mv_data);
                json_object_set_string(obj, "id", (char *)key.mv_data);
                handle_track_duration(obj);
                json_array_append_value(array, item_val);
                fetched_count++;
            }
        }
        current_index++;
        if (fetched_count >= limit) break;
        rc = mdb_cursor_get(cursor, &key, &val, next_op);
    }

    mdb_cursor_close(cursor);
    mdb_txn_abort(txn);

    if (fetched_count == 0) {
        json_value_free(root_val);
        return NULL;
    }
    return root_val;
}

// Thêm tham số is_desc: 0 = ASC, 1 = DESC
JSON_Value* get_playlists(MDB_env *env, MDB_dbi playlist_dbi, int offset, int limit, int is_desc) {
    MDB_txn *txn;
    MDB_cursor *cursor;
    int rc = mdb_txn_begin(env, NULL, MDB_RDONLY, &txn);
    if (rc != 0) return NULL;

    rc = mdb_cursor_open(txn, playlist_dbi, &cursor);
    if (rc != 0) {
        mdb_txn_abort(txn);
        return NULL;
    }

    JSON_Value *root_val = json_value_init_array();
    JSON_Array *array = json_value_get_array(root_val);

    MDB_val key, val;
    int current_index = 0;
    int fetched_count = 0;

    MDB_cursor_op first_op = is_desc ? MDB_LAST : MDB_FIRST;
    MDB_cursor_op next_op = is_desc ? MDB_PREV : MDB_NEXT;

    rc = mdb_cursor_get(cursor, &key, &val, first_op);
    while (rc == 0) {
        if (current_index >= offset && fetched_count < limit) {
            JSON_Value *item_val = protobuf_to_playlist((unsigned char *)val.mv_data, val.mv_size);
            if (item_val) {
                JSON_Object *obj = json_value_get_object(item_val);
                //json_object_set_string(obj, "_key", (char *)key.mv_data);
                json_object_set_string(obj, "id", (char *)key.mv_data);
                json_array_append_value(array, item_val);
                fetched_count++;
            }
        }
        current_index++;
        if (fetched_count >= limit) break;
        rc = mdb_cursor_get(cursor, &key, &val, next_op);
    }

    mdb_cursor_close(cursor);
    mdb_txn_abort(txn);

    if (fetched_count == 0) {
        json_value_free(root_val);
        return NULL;
    }
    return root_val;
}

int update_track(MDB_env *env, MDB_dbi track_dbi, const char *old_key, JSON_Value *new_track_val) {
    JSON_Object *obj = json_value_get_object(new_track_val);
    const char *new_title = json_object_get_string(obj, "title");
    if (!new_title) return -1;

    if (json_object_has_value(obj, "id")) json_object_remove(obj, "id");

    char new_key[33];
    calculate_md5(new_title, new_key);

    RawData raw = { .buf = NULL, .len = 0 };
    if (track_to_protobuf(new_track_val, &raw) != 0) return -1;

    if (strcmp(old_key, new_key) != 0) {
        db_delete(env, track_dbi, old_key);
    }

    int rc = db_put(env, track_dbi, new_key, &raw);
    free(raw.buf);
    return rc;
}

int update_playlist(MDB_env *env, MDB_dbi playlist_dbi, const char *old_key, JSON_Value *new_playlist_val) {
    JSON_Object *obj = json_value_get_object(new_playlist_val);
    const char *new_name = json_object_get_string(obj, "name");
    if (!new_name) return -1;

    if (json_object_has_value(obj, "id")) json_object_remove(obj, "id");

    char new_key[33];
    calculate_md5(new_name, new_key);

    RawData raw = { .buf = NULL, .len = 0 };
    if (playlist_to_protobuf(new_playlist_val, &raw) != 0) return -1;

    if (strcmp(old_key, new_key) != 0) {
        db_delete(env, playlist_dbi, old_key);
    }

    int rc = db_put(env, playlist_dbi, new_key, &raw);
    free(raw.buf);
    return rc;
}

int add_to_playlist(MDB_env *env, MDB_dbi playlist_dbi, const char *playlist_key, const char *track_key) {
    JSON_Value *pl_val = get_playlist(env, playlist_dbi, playlist_key);
    if (!pl_val) return -1;

    JSON_Object *obj = json_value_get_object(pl_val);
    JSON_Array *tracks_arr = json_object_get_array(obj, "tracks");
    if (!tracks_arr) {
        json_object_set_value(obj, "tracks", json_value_init_array());
        tracks_arr = json_object_get_array(obj, "tracks");
    }

    size_t count = json_array_get_count(tracks_arr);
    for (size_t i = 0; i < count; i++) {
        const char *t_id = json_array_get_string(tracks_arr, i);
        if (t_id && strcmp(t_id, track_key) == 0) {
            json_value_free(pl_val);
            return 0; // Đã tồn tại sẵn
        }
    }

    json_array_append_string(tracks_arr, track_key);
    RawData raw = { .buf = NULL, .len = 0 };
    if (playlist_to_protobuf(pl_val, &raw) != 0) {
        json_value_free(pl_val);
        return -1;
    }
    
    json_value_free(pl_val);
    int rc = db_put(env, playlist_dbi, playlist_key, &raw);

    free(raw.buf);
    return rc;
}

// ==========================================
// CÁC HÀM THÊM TRACK VÀ PLAYLIST CHO MAIN
// ==========================================

int add_track(MDB_env *env, MDB_dbi track_dbi, JSON_Value *val) {
    if (!val || json_type(val) != JSONObject) return -1;
    JSON_Object *obj = json_value_get_object(val);
    
    const char *title = json_object_get_string(obj, "title");
    if (!title) return -1;

    char key_str[33];
    calculate_md5(title, key_str);

    RawData raw = { .buf = NULL, .len = 0 };
    if (track_to_protobuf(val, &raw) != 0) return -1;

    int rc = db_put(env, track_dbi, key_str, &raw);
    free(raw.buf);
    
    // Nếu bạn muốn trả về key hoặc in ra, có thể xử lý ở đây hoặc trong main
    return rc;
}

int add_playlist(MDB_env *env, MDB_dbi playlist_dbi, JSON_Value *val) {
    if (!val || json_type(val) != JSONObject) return -1;
    JSON_Object *obj = json_value_get_object(val);
    
    const char *name = json_object_get_string(obj, "name");
    if (!name) return -1;
    JSON_Value *tracks = json_object_get_value(obj, "tracks");
    if (json_type(tracks) != JSONArray) {
      if (tracks) json_object_remove(obj, "tracks");
      json_object_set_value(obj, "tracks", json_value_init_array());
    }

    char key_str[33];
    calculate_md5(name, key_str);

    RawData raw = { .buf = NULL, .len = 0 };
    if (playlist_to_protobuf(val, &raw) != 0) return -1;

    int rc = db_put(env, playlist_dbi, key_str, &raw);
    free(raw.buf);
    
    return rc;
}

// ==========================================
// HÀM XÓA TRACK VÀ TỰ ĐỘNG CẬP NHẬT PLAYLIST
// ==========================================

int delete_track_and_clean_playlists(MDB_env *env, MDB_dbi track_dbi, MDB_dbi playlist_dbi, const char *track_key) {
    MDB_txn *txn;
    int rc;

    // 1. Xóa track khỏi database track_dbi trước
    if (mdb_txn_begin(env, NULL, 0, &txn) != 0) return -1;

    MDB_val t_key = { .mv_size = strlen(track_key) + 1, .mv_data = (void *)track_key };
    rc = mdb_del(txn, track_dbi, &t_key, NULL);
    if (rc != 0) {
        mdb_txn_abort(txn);
        return rc; // Không tìm thấy track hoặc lỗi xóa
    }
    
    rc = mdb_txn_commit(txn);
    if (rc != 0) return rc;

    // 2. Mở transaction đọc/ghi trên playlist_dbi để quét và cập nhật các playlist liên quan
    if (mdb_txn_begin(env, NULL, 0, &txn) != 0) return -1;

    MDB_cursor *cursor;
    rc = mdb_cursor_open(txn, playlist_dbi, &cursor);
    if (rc != 0) {
        mdb_txn_abort(txn);
        return -1;
    }

    MDB_val p_key, p_val;
    rc = mdb_cursor_get(cursor, &p_key, &p_val, MDB_FIRST);

    while (rc == 0) {
        // Chuyển đổi dữ liệu nhị phân của playlist hiện tại sang JSON
        JSON_Value *pl_val = protobuf_to_playlist((unsigned char *)p_val.mv_data, p_val.mv_size);
        if (pl_val) {
            JSON_Object *obj = json_value_get_object(pl_val);
            JSON_Array *tracks_arr = json_object_get_array(obj, "tracks");
            
            if (tracks_arr) {
                size_t count = json_array_get_count(tracks_arr);
                int found_index = -1;

                // Tìm xem track_key có nằm trong mảng tracks của playlist này không
                for (size_t i = 0; i < count; i++) {
                    const char *t_id = json_array_get_string(tracks_arr, i);
                    if (t_id && strcmp(t_id, track_key) == 0) {
                        found_index = (int)i;
                        break;
                    }
                }

                // Nếu tìm thấy, tiến hành xóa khỏi mảng và cập nhật lại DB
                if (found_index != -1) {
                    json_array_remove(tracks_arr, found_index);

                    // Đóng gói lại thành Protobuf buffer mới
                    RawData raw = { .buf = NULL, .len = 0 };
                    if (playlist_to_protobuf(pl_val, &raw) == 0) {
                        MDB_val updated_val = { .mv_size = raw.len, .mv_data = raw.buf };
                        
                        // Ghi đè lại playlist đã cập nhật vào LMDB
                        mdb_put(txn, playlist_dbi, &p_key, &updated_val, 0);
                        free(raw.buf);
                    }
                }
            }
            json_value_free(pl_val);
        }
        
        // Chuyển sang playlist tiếp theo
        rc = mdb_cursor_get(cursor, &p_key, &p_val, MDB_NEXT);
    }

    mdb_cursor_close(cursor);
    rc = mdb_txn_commit(txn);
    return rc;
}
#ifdef TEST_APP
void print_usage(const char *prog_name) {
    printf("Cách sử dụng:\n");
    printf("  Thêm track:        %s -it <file.json>\n", prog_name);
    printf("  Thêm playlist:     %s -ip <file.json>\n", prog_name);
    printf("  Truy vấn track:    %s -qt <key>\n", prog_name);
    printf("  Truy vấn pl:       %s -qp <key>\n", prog_name);
    printf("  Phân trang tracks: %s -lt <offset> <limit> [asc|desc]\n", prog_name);
    printf("  Phân trang pls:    %s -lp <offset> <limit> [asc|desc]\n", prog_name);
    printf("  Xóa track:         %s -dt <key>\n", prog_name);
    printf("  Xóa playlist:      %s -dp <key>\n", prog_name);
    printf("  Cập nhật track:    %s -ut <file.json> [key]\n", prog_name);
    printf("  Cập nhật pl:       %s -up <file.json> [key]\n", prog_name);
    printf("  Thêm vào pl:       %s -ap <track_id> <playlist_id>\n", prog_name);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *action = argv[1];

    MDB_env *env;
    MDB_dbi track_dbi, playlist_dbi;
    int rc = mdb_env_create(&env);
    if (rc != 0) return 1;
    mdb_env_set_maxdbs(env, 2);
    rc = mdb_env_open(env, DB_PATH, MDB_NOSUBDIR | MDB_MAPASYNC, 0664);
    if (rc != 0) {
        fprintf(stderr, "Lỗi mở cơ sở dữ liệu.\n");
        return 1;
    }

    MDB_txn *txn;
    mdb_txn_begin(env, NULL, 0, &txn);
    mdb_dbi_open(txn, "tracks", MDB_CREATE, &track_dbi);
    mdb_dbi_open(txn, "playlists", MDB_CREATE, &playlist_dbi);
    mdb_txn_commit(txn);

    // --- 1. THÊM TRACK (-it) ---
    if (strcmp(action, "-it") == 0) {
        if (argc < 3) { print_usage(argv[0]); goto cleanup; }
        JSON_Value *val = json_parse_file(argv[2]);
        if (!val) { fprintf(stderr, "File JSON không hợp lệ!\n"); goto cleanup; }
        if (add_track(env, track_dbi, val) == 0) {
            printf("Thêm track thành công!\n");
        } else {
            printf("Lỗi khi thêm track (Thiếu title hoặc lỗi DB).\n");
        }
        json_value_free(val);

    // --- 2. THÊM PLAYLIST (-ip) ---
    } else if (strcmp(action, "-ip") == 0) {
        if (argc < 3) { print_usage(argv[0]); goto cleanup; }
        JSON_Value *val = json_parse_file(argv[2]);
        if (!val) { fprintf(stderr, "File JSON không hợp lệ!\n"); goto cleanup; }
        if (add_playlist(env, playlist_dbi, val) == 0) {
            printf("Thêm playlist thành công!\n");
        } else {
            printf("Lỗi khi thêm playlist (Thiếu name hoặc lỗi DB).\n");
        }
        json_value_free(val);

    // --- 3. TRUY VẤN CHI TIẾT TRACK (-qt) ---
    } else if (strcmp(action, "-qt") == 0) {
        if (argc < 3) { print_usage(argv[0]); goto cleanup; }
        const char *key = argv[2];
        JSON_Value *t_val = get_track(env, track_dbi, key);
        if (t_val) {
            char *s = json_serialize_to_string_pretty(t_val);
            printf("%s\n", s);
            json_free_serialized_string(s);
            json_value_free(t_val);
        } else printf("Không tìm thấy Track.\n");

    // --- 4. TRUY VẤN CHI TIẾT PLAYLIST KÈM THÔNG TIN TRACKS (-qp) ---
    } else if (strcmp(action, "-qp") == 0) {
        if (argc < 3) { print_usage(argv[0]); goto cleanup; }
        const char *key = argv[2];
        JSON_Value *p_val = get_playlist(env, playlist_dbi, key);
        if (p_val) {
            JSON_Object *p_obj = json_value_get_object(p_val);
            JSON_Array *tracks_arr = json_object_get_array(p_obj, "tracks");
            
            if (tracks_arr) {
                size_t count = json_array_get_count(tracks_arr);
                //JSON_Value *detailed_tracks_val = json_value_init_array();
                //JSON_Array *detailed_tracks_arr = json_value_get_array(detailed_tracks_val);

                for (size_t i = 0; i < count; i++) {
                    const char *track_key = json_array_get_string(tracks_arr, i);
                    if (track_key) {
                        JSON_Value *track_val = get_track(env, track_dbi, track_key);
                        if (track_val) {
                            JSON_Object *track_obj = json_value_get_object(track_val);
                            //json_object_set_string(track_obj, "_key", track_key);
                            json_object_set_string(track_obj, "id", track_key);
                            handle_track_duration(track_obj);
                            
                            json_array_replace_value(tracks_arr, i, track_val);
                            //json_value_free(track_val);
                        } else {
                            //json_array_append_string(detailed_tracks_arr, track_key);
                        }
                    }
                }
                //json_object_set_value(p_obj, "tracks", detailed_tracks_val);
            }

            char *s = json_serialize_to_string_pretty(p_val);
            printf("%s\n", s);
            json_free_serialized_string(s);
            json_value_free(p_val);
        } else printf("Không tìm thấy Playlist.\n");

    // --- 5. PHÂN TRANG TRACKS (-lt) ---
    } else if (strcmp(action, "-lt") == 0) {
        //if (argc < 4) { print_usage(argv[0]); goto cleanup; }
        int offset = argc > 2 ? atoi(argv[2]) : 0;
        int limit = argc > 3 ? atoi(argv[3]) : 5;
        int is_desc = 0;
        if (argc >= 5 && (strcmp(argv[4], "desc") == 0 || strcmp(argv[4], "DESC") == 0)) {
            is_desc = 1;
        }

        JSON_Value *res = get_tracks(env, track_dbi, offset, limit, is_desc);
        if (res) {
            char *s = json_serialize_to_string_pretty(res);
            printf("%s\n", s);
            json_free_serialized_string(s);
            json_value_free(res);
        } else printf("Không có dữ liệu.\n");

    // --- 6. PHÂN TRANG PLAYLISTS (-lp) ---
    } else if (strcmp(action, "-lp") == 0) {
        //if (argc < 4) { print_usage(argv[0]); goto cleanup; }
        int offset = argc > 2 ? atoi(argv[2]) : 0;
        int limit = argc > 3 ? atoi(argv[3]) : 5;
        int is_desc = 0;
        if (argc >= 5 && (strcmp(argv[4], "desc") == 0 || strcmp(argv[4], "DESC") == 0)) {
            is_desc = 1;
        }

        JSON_Value *res = get_playlists(env, playlist_dbi, offset, limit, is_desc);
        if (res) {
            char *s = json_serialize_to_string_pretty(res);
            printf("%s\n", s);
            json_free_serialized_string(s);
            json_value_free(res);
        } else printf("Không có dữ liệu.\n");

    // --- 7. XÓA TRACK (-dt) ---
    } else if (strcmp(action, "-dt") == 0) {
        if (argc < 3) { print_usage(argv[0]); goto cleanup; }
        int rc_del = delete_track_and_clean_playlists(env, track_dbi, playlist_dbi, argv[2]);
        if (rc_del == 0) printf("Đã xóa track và tự động gỡ bỏ khỏi các playlist liên quan thành công!\n");
        else printf("Không tìm thấy Track hoặc lỗi khi xóa.\n");

    // --- 8. XÓA PLAYLIST (-dp) ---
    } else if (strcmp(action, "-dp") == 0) {
        if (argc < 3) { print_usage(argv[0]); goto cleanup; }
        int rc_del = db_delete(env, playlist_dbi, argv[2]);
        if (rc_del == 0) printf("Xóa Playlist thành công!\n");
        else printf("Không tìm thấy Playlist hoặc lỗi khi xóa.\n");

    // --- 9. CẬP NHẬT TRACK (-ut) ---
    } else if (strcmp(action, "-ut") == 0) {
        if (argc < 3) { print_usage(argv[0]); goto cleanup; }
        
        JSON_Value *val = json_parse_file(argv[2]);
        if (!val) { fprintf(stderr, "File JSON không hợp lệ!\n"); goto cleanup; }
        
        JSON_Object *obj = json_value_get_object(val);
        const char *old_key = NULL;

        // Nếu argv[3] được truyền vào, ưu tiên dùng argv[3]
        if (argc >= 4) {
            old_key = argv[3];
        } else {
            // Nếu không có argv[3], kiểm tra xem trong JSON có tồn tại trường "id" không
            if (json_object_has_value(obj, "id")) {
                old_key = json_object_get_string(obj, "id");
            }
        }

        if (!old_key || strlen(old_key) == 0) {
            fprintf(stderr, "Lỗi: Không tìm thấy ID/Key để cập nhật (thiếu tham số thứ 3 hoặc trường 'id' trong JSON)!\n");
            json_value_free(val);
            goto cleanup;
        }

        int rc_up = update_track(env, track_dbi, old_key, val);
        if (rc_up == 0) printf("Cập nhật Track thành công!\n");
        else printf("Lỗi cập nhật Track.\n");
        
        json_value_free(val);

    // --- 10. CẬP NHẬT PLAYLIST (-up) ---
    } else if (strcmp(action, "-up") == 0) {
        if (argc < 3) { print_usage(argv[0]); goto cleanup; }
        
        JSON_Value *val = json_parse_file(argv[2]);
        if (!val) { fprintf(stderr, "File JSON không hợp lệ!\n"); goto cleanup; }
        
        JSON_Object *obj = json_value_get_object(val);
        const char *old_key = NULL;

        // Nếu argv[3] được truyền vào, ưu tiên dùng argv[3]
        if (argc >= 4) {
            old_key = argv[3];
        } else {
            // Nếu không có argv[3], kiểm tra xem trong JSON có tồn tại trường "id" không
            if (json_object_has_value(obj, "id")) {
                old_key = json_object_get_string(obj, "id");
            }
        }

        if (!old_key || strlen(old_key) == 0) {
            fprintf(stderr, "Lỗi: Không tìm thấy ID/Key để cập nhật (thiếu tham số thứ 3 hoặc trường 'id' trong JSON)!\n");
            json_value_free(val);
            goto cleanup;
        }

        int rc_up = update_playlist(env, playlist_dbi, old_key, val);
        if (rc_up == 0) printf("Cập nhật Playlist thành công!\n");
        else printf("Lỗi cập nhật Playlist.\n");
        
        json_value_free(val);

    // --- 11. THÊM TRACK VÀO PLAYLIST (-ap) ---
    } else if (strcmp(action, "-ap") == 0) {
        if (argc < 4) { print_usage(argv[0]); goto cleanup; }
        const char *track_key = argv[2];
        const char *playlist_key = argv[3];
        int rc_add = add_to_playlist(env, playlist_dbi, playlist_key, track_key);
        if (rc_add == 0) printf("Đã thêm Track vào Playlist thành công!\n");
        else printf("Lỗi: Không tìm thấy Playlist hoặc lỗi ghi dữ liệu.\n");

    } else {
        print_usage(argv[0]);
    }

cleanup:
    mdb_dbi_close(env, track_dbi);
    mdb_dbi_close(env, playlist_dbi);
    mdb_env_close(env);
    return 0;
}
#endif
