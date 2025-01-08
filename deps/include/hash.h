/* The MIT License

   Copyright (c) 2008, 2009, 2011 by Attractive Chaos <attractor@live.co.uk>

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
   BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
   ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
*/

/*
  An example:

#include "hash.h"
KHASH_MAP_INIT_INT(32, char)
int main() {
  int ret, is_missing;
  ht_iter_t k;
  ht_hash_t(32) *h = ht_init(32);
  k = ht_put(32, h, 5, &ret);
  ht_value(h, k) = 10;
  k = ht_get(32, h, 10);
  is_missing = (k == ht_end(h));
  k = ht_get(32, h, 5);
  ht_del(32, h, k);
  for (k = ht_begin(h); k != ht_end(h); ++k)
    if (ht_exist(h, k)) ht_value(h, k) = 1;
  ht_destroy(32, h);
  return 0;
}
*/


#ifndef __AC_KHASH_H
#define __AC_KHASH_H

/*!
  @header

  Generic hash table library.
 */

#define AC_VERSION_KHASH_H "0.2.8"

#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* compiler specific configuration */

#if UINT_MAX == 0xffffffffu
typedef unsigned int ht_int32_t;
#elif ULONG_MAX == 0xffffffffu
typedef unsigned long ht_int32_t;
#endif

#if ULONG_MAX == ULLONG_MAX
typedef unsigned long khint64_t;
#else
typedef unsigned long long khint64_t;
#endif

#ifndef ht_inline
#ifdef _MSC_VER
#define ht_inline __inline
#else
#define ht_inline inline
#endif
#endif /* ht_inline */

#ifndef klib_unused
#if (defined __clang__ && __clang_major__ >= 3) || (defined __GNUC__ && __GNUC__ >= 3)
#define klib_unused __attribute__ ((__unused__))
#else
#define klib_unused
#endif
#endif /* klib_unused */

typedef ht_int32_t ht_int_t;
typedef ht_int_t ht_iter_t;

#define __ac_isempty(flag, i) ((flag[i >> 4] >> ((i & 0xfU) << 1)) & 2)
#define __ac_isdel(flag, i) ((flag[i >> 4] >> ((i & 0xfU) << 1)) & 1)
#define __ac_iseither(flag, i) ((flag[i >> 4] >> ((i & 0xfU) << 1)) & 3)
#define __ac_set_isdel_false(flag, i) (flag[i >> 4] &= ~(1ul << ((i & 0xfU) << 1)))
#define __ac_set_isempty_false(flag, i) (flag[i >> 4] &= ~(2ul << ((i & 0xfU) << 1)))
#define __ac_set_isboth_false(flag, i) (flag[i >> 4] &= ~(3ul << ((i & 0xfU) << 1)))
#define __ac_set_isdel_true(flag, i) (flag[i >> 4] |= 1ul << ((i & 0xfU) << 1))

#define __ac_fsize(m) ((m) < 16? 1 : (m) >> 4)

#ifndef kroundup32
#define kroundup32(x) (--(x), (x)|=(x)>>1, (x)|=(x)>>2, (x)|=(x)>>4, (x)|=(x)>>8, (x)|=(x)>>16, ++(x))
#endif

#ifndef kcalloc
#define kcalloc(N,Z) calloc(N,Z)
#endif
#ifndef kmalloc
#define kmalloc(Z) malloc(Z)
#endif
#ifndef krealloc
#define krealloc(P,Z) realloc(P,Z)
#endif
#ifndef kfree
#define kfree(P) free(P)
#endif

#define ht(__name, ht_key_t, ht_val_t)                                                                                                \
  struct ht_##__name##_t {                                                                                                           \
    ht_int_t n_buckets, size, n_occupied, upper_bound, last_inserted;                                                                \
    ht_int32_t *flags;                                                                                                               \
    ht_key_t *keys;                                                                                                                  \
    ht_val_t *vals;                                                                                                                  \
  }

#define __KHASH_PROTOTYPES(__name, ht_key_t, ht_val_t)                                                                                \
  extern void __name##_destroy(struct ht_##__name##_t *h);                                                                                 \
  extern void __name##_clear(struct ht_##__name##_t *h);                                                                                   \
  extern ht_int_t __name##_get(const struct ht_##__name##_t *h, ht_key_t key);                                                               \
  extern int __name##_resize(struct ht_##__name##_t *h, ht_int_t new_n_buckets);                                                            \
  extern ht_int_t __name##_put(struct ht_##__name##_t *h, ht_key_t key, int *ret);                                                           \
  extern void __name##_del(struct ht_##__name##_t *h, ht_int_t x);

#define __KHASH_IMPL(__name, SCOPE, ht_key_t, ht_val_t, __hash_func, __hash_equal)                                                    \
  SCOPE void __name##_destroy(struct ht_##__name##_t *h)                                                                                   \
  {                                                                                                                                 \
    if (h) {                                                                                                                        \
      kfree((void *)h->keys); kfree(h->flags);                                                                                      \
      kfree((void *)h->vals);                                                                                                       \
    }                                                                                                                               \
  }                                                                                                                                 \
  SCOPE void __name##_clear(struct ht_##__name##_t *h)                                                                                     \
  {                                                                                                                                 \
    if (h && h->flags) {                                                                                                            \
      memset(h->flags, 0xaa, __ac_fsize(h->n_buckets) * sizeof(ht_int32_t));                                                         \
      h->size = h->n_occupied = 0;                                                                                                  \
    }                                                                                                                               \
  }                                                                                                                                 \
  SCOPE ht_val_t* __name##_get(const struct ht_##__name##_t *h, ht_key_t key)                                                                 \
  {                                                                                                                                 \
    if (h->n_buckets) {                                                                                                             \
      ht_int_t k, i, last, mask, step = 0;                                                                                           \
      mask = h->n_buckets - 1;                                                                                                      \
      k = __hash_func(key); i = k & mask;                                                                                           \
      last = i;                                                                                                                     \
      while (!__ac_isempty(h->flags, i) && (__ac_isdel(h->flags, i) || !__hash_equal(h->keys[i], key))) {                           \
        i = (i + (++step)) & mask;                                                                                                  \
        if (i == last) return NULL;                                                                                         \
      }                                                                                                                             \
      return __ac_iseither(h->flags, i) ? NULL : h->vals + i;                                                                          \
    } else return 0;                                                                                                                \
  }                                                                                                                                 \
  SCOPE int __name##_resize(struct ht_##__name##_t *h, ht_int_t new_n_buckets)                                                               \
  { /* This function uses 0.25*n_buckets bytes of working space instead of [sizeof(key_t+val_t)+.25]*n_buckets. */                  \
    ht_int32_t *new_flags = 0;                                                                                                       \
    ht_int_t j = 1;                                                                                                                  \
    {                                                                                                                               \
      kroundup32(new_n_buckets);                                                                                                    \
      if (new_n_buckets < 4) new_n_buckets = 4;                                                                                     \
      if (h->size >= (new_n_buckets>>1) + (new_n_buckets>>2)) j = 0;  /* requested size is too small */                             \
      else { /* hash table size to be changed (shrink or expand); rehash */                                                         \
        new_flags = (ht_int32_t*)kmalloc(__ac_fsize(new_n_buckets) * sizeof(ht_int32_t));                                             \
        if (!new_flags) return -1;                                                                                                  \
        memset(new_flags, 0xaa, __ac_fsize(new_n_buckets) * sizeof(ht_int32_t));                                                     \
        if (h->n_buckets < new_n_buckets) {  /* expand */                                                                           \
          ht_key_t *new_keys = (ht_key_t*)krealloc((void *)h->keys, new_n_buckets * sizeof(ht_key_t));                                 \
          if (!new_keys) { kfree(new_flags); return -1; }                                                                           \
          h->keys = new_keys;                                                                                                       \
                                                                                                                                    \
          ht_val_t *new_vals = (ht_val_t*)krealloc((void *)h->vals, new_n_buckets * sizeof(ht_val_t));                                 \
          if (!new_vals) { kfree(new_flags); return -1; }                                                                           \
          h->vals = new_vals;                                                                                                       \
        } /* otherwise shrink */                                                                                                    \
      }                                                                                                                             \
    }                                                                                                                               \
    if (j) { /* rehashing is needed */                                                                                              \
      for (j = 0; j != h->n_buckets; ++j) {                                                                                         \
        if (__ac_iseither(h->flags, j) == 0) {                                                                                      \
          ht_key_t key = h->keys[j];                                                                                                 \
          ht_val_t val;                                                                                                              \
          ht_int_t new_mask;                                                                                                         \
          new_mask = new_n_buckets - 1;                                                                                             \
          val = h->vals[j];                                                                                                         \
          __ac_set_isdel_true(h->flags, j);                                                                                         \
          while (1) { /* kick-out process; sort of like in Cuckoo hashing */                                                        \
            ht_int_t k, i, step = 0;                                                                                                 \
            k = __hash_func(key);                                                                                                   \
            i = k & new_mask;                                                                                                       \
            while (!__ac_isempty(new_flags, i)) i = (i + (++step)) & new_mask;                                                      \
            __ac_set_isempty_false(new_flags, i);                                                                                   \
            if (i < h->n_buckets && __ac_iseither(h->flags, i) == 0) { /* kick out the existing element */                          \
              { ht_key_t tmp = h->keys[i]; h->keys[i] = key; key = tmp; }                                                            \
              ht_val_t tmp = h->vals[i];                                                                                             \
              h->vals[i] = val;                                                                                                     \
              val = tmp;                                                                                                            \
              __ac_set_isdel_true(h->flags, i); /* mark it as deleted in the old hash table */                                      \
            } else { /* write the element and jump out of the loop */                                                               \
              h->keys[i] = key;                                                                                                     \
              h->vals[i] = val;                                                                                                     \
              break;                                                                                                                \
            }                                                                                                                       \
          }                                                                                                                         \
        }                                                                                                                           \
      }                                                                                                                             \
      if (h->n_buckets > new_n_buckets) { /* shrink the hash table */                                                               \
        h->keys = (ht_key_t*)krealloc((void *)h->keys, new_n_buckets * sizeof(ht_key_t));                                             \
        h->vals = (ht_val_t*)krealloc((void *)h->vals, new_n_buckets * sizeof(ht_val_t));                                             \
      }                                                                                                                             \
      kfree(h->flags); /* free the working space */                                                                                 \
      h->flags = new_flags;                                                                                                         \
      h->n_buckets = new_n_buckets;                                                                                                 \
      h->n_occupied = h->size;                                                                                                      \
      h->upper_bound = (new_n_buckets>>1) + (new_n_buckets>>2);                                                                     \
    }                                                                                                                               \
    return 0;                                                                                                                       \
  }                                                                                                                                 \
  SCOPE ht_val_t* __name##_put(struct ht_##__name##_t *h, ht_key_t key)                                                              \
  {                                                                                                                                 \
    ht_int_t x;                                                                                                                      \
    if (h->n_occupied >= h->upper_bound) /* update the hash table */                                                                \
      __name##_resize(h, h->n_buckets + (h->n_buckets > (h->size << 1) ? -1 : 1));                                                 \
                                                                                                                                    \
    ht_int_t k, i, site, last, mask = h->n_buckets - 1, step = 0;                                                                    \
    x = site = h->n_buckets; k = __hash_func(key); i = k & mask;                                                                    \
    if (__ac_isempty(h->flags, i)) x = i; /* for speed up */                                                                        \
    else {                                                                                                                          \
      last = i;                                                                                                                     \
      while (!__ac_isempty(h->flags, i) && (__ac_isdel(h->flags, i) || !__hash_equal(h->keys[i], key))) {                           \
        if (__ac_isdel(h->flags, i)) site = i;                                                                                      \
        i = (i + (++step)) & mask;                                                                                                  \
        if (i == last) { x = site; break; }                                                                                         \
      }                                                                                                                             \
      if (x == h->n_buckets) {                                                                                                      \
        if (__ac_isempty(h->flags, i) && site != h->n_buckets) x = site;                                                            \
        else x = i;                                                                                                                 \
      }                                                                                                                             \
    }                                                                                                                               \
    if (__ac_isempty(h->flags, x)) { /* not present at all */                                                                       \
      h->keys[x] = key;                                                                                                             \
      __ac_set_isboth_false(h->flags, x);                                                                                           \
      ++h->size; ++h->n_occupied;                                                                                                   \
    } else if (__ac_isdel(h->flags, x)) { /* deleted */                                                                             \
      h->keys[x] = key;                                                                                                             \
      __ac_set_isboth_false(h->flags, x);                                                                                           \
      ++h->size;                                                                                                                    \
    }                                                                                                                               \
    return h->vals + x;                                                                                                                       \
  }                                                                                                                                 \
  SCOPE void __name##_del(struct ht_##__name##_t *h, ht_int_t x)                                                                             \
  {                                                                                                                                 \
    if (x != h->n_buckets && !__ac_iseither(h->flags, x)) {                                                                         \
      __ac_set_isdel_true(h->flags, x);                                                                                             \
      --h->size;                                                                                                                    \
    }                                                                                                                               \
  }

#define KHASH_DECLARE(__name, ht_key_t, ht_val_t)                                                                                       \
  __KHASH_PROTOTYPES(__name, ht_key_t, ht_val_t)

#define KHASH_INIT2(__name, SCOPE, ht_key_t, ht_val_t, __hash_func, __hash_equal)                                            \
  __KHASH_IMPL(__name, SCOPE, ht_key_t, ht_val_t, __hash_func, __hash_equal)

#define ht_impl(__name, ht_key_t, ht_val_t, __hash_func, __hash_equal)                                                    \
  KHASH_INIT2(__name, static ht_inline klib_unused, ht_key_t, ht_val_t, __hash_func, __hash_equal)

/* --- BEGIN OF HASH FUNCTIONS --- */

#define ht_int_hash_func(key) (ht_int32_t)(key)
#define ht_int_hash_equal(a, b) ((a) == (b))
#define ht_int64_hash_func(key) (ht_int32_t)((key)>>33^(key)^(key)<<11)
#define ht_int64_hash_equal(a, b) ((a) == (b))
static ht_inline ht_int_t __ac_X31_hash_string(const char *s)
{
  ht_int_t h = (ht_int_t)*s;
  if (h) for (++s ; *s; ++s) h = (h << 5) - h + (ht_int_t)*s;
  return h;
}
#define ht_str_hash_func(key) __ac_X31_hash_string(key)
/*! @function
  @abstract     Const char* comparison function
 */
#define ht_str_hash_equal(a, b) (strcmp(a, b) == 0)

static ht_inline ht_int_t __ac_Wang_hash(ht_int_t key)
{
    key += ~(key << 15);
    key ^=  (key >> 10);
    key +=  (key << 3);
    key ^=  (key >> 6);
    key += ~(key << 11);
    key ^=  (key >> 16);
    return key;
}
#define ht_int_hash_func2(key) __ac_Wang_hash((ht_int_t)key)

/* --- END OF HASH FUNCTIONS --- */

/* Other convenient macros... */
#define ht_t(__name) struct ht_##__name##_t
#define hdestroy(__name, h) __name##_destroy(&h)
#define hclear(__name, h) __name##_clear(&h)
#define hresize(__name, h, s) __name##_resize(&h, s)
#define hput(__name, h, ...) __name##_put(&h, (typeof((h).keys[0])) __VA_ARGS__)
#define hget(__name, h, ...) __name##_get(&h, (typeof((h).keys[0])) __VA_ARGS__)
#define hdel(__name, h, ...) __name##_del(&h, (typeof((h).keys[0])) __VA_ARGS__)
#define hexist(h, x) (!__ac_iseither((h).flags, (x)))
#define hkey(h, x) ((h).keys[x])
#define hval(h, x) ((h).vals[x])

/* More convenient interfaces */

#define ht_impl_int(__name, ht_val_t)                                                                                           \
  ht_impl(__name, ht_int32_t, ht_val_t, ht_int_hash_func, ht_int_hash_equal)
#define ht_impl_int64(__name, ht_val_t)                                                                                         \
  ht_impl(__name, ht_int64_t, ht_val_t, ht_int64_hash_func, ht_int64_hash_equal)

#define ht_impl_str(__name, ht_val_t)                                                                                           \
  ht_impl(__name, char*, ht_val_t, ht_str_hash_func, ht_str_hash_equal)


#endif /* __AC_KHASH_H */
