#pragma once

#include <assert.h>
#include <stdckdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define VECTOR_DEFINE(T, FREE_FN)                                              \
  typedef struct {                                                             \
    T *data;                                                                   \
    size_t size;                                                               \
    size_t capacity; /* allocated slots, ALWAYS >= size + 1 */                 \
  } Vector_##T;                                                                \
                                                                               \
  [[nodiscard, gnu::nonnull(1)]]                                               \
  static inline bool Vector_##T##_init(Vector_##T *restrict v) {               \
    v->data = (T *)calloc(1, sizeof(T));                                       \
    if (!v->data) {                                                            \
      v->size = 0;                                                             \
      v->capacity = 0;                                                         \
      return false;                                                            \
    }                                                                          \
    v->size = 0;                                                               \
    v->capacity = 1;                                                           \
    return true;                                                               \
  }                                                                            \
                                                                               \
  [[gnu::nonnull(1)]]                                                          \
  static inline void Vector_##T##_free(Vector_##T *restrict v) {               \
    void (*free_fn)(typeof_unqual(T)) = (void (*)(typeof_unqual(T)))(FREE_FN); \
    if (free_fn) {                                                             \
      for (size_t i = 0; i < v->size; i++)                                     \
        free_fn(v->data[i]);                                                   \
    }                                                                          \
    free(v->data);                                                             \
    v->data = nullptr;                                                         \
    v->size = 0;                                                               \
    v->capacity = 0;                                                           \
  }                                                                            \
                                                                               \
  [[nodiscard]] static inline Vector_##T *Vector_##T##_new(void) {             \
    Vector_##T *v = (Vector_##T *)malloc(sizeof(Vector_##T));                  \
    if (!v)                                                                    \
      return nullptr;                                                          \
    if (!Vector_##T##_init(v)) {                                               \
      free(v);                                                                 \
      return nullptr;                                                          \
    }                                                                          \
    return v;                                                                  \
  }                                                                            \
                                                                               \
  static inline void Vector_##T##_delete(Vector_##T *restrict v) {             \
    if (!v)                                                                    \
      return;                                                                  \
    Vector_##T##_free(v);                                                      \
    free(v);                                                                   \
  }                                                                            \
                                                                               \
  [[nodiscard, gnu::nonnull(1)]]                                               \
  static inline int Vector_##T##_reserve(Vector_##T *restrict v,               \
                                         size_t min_capacity) {                \
    if (min_capacity <= v->capacity)                                           \
      return 1;                                                                \
    size_t bytes;                                                              \
    if (ckd_mul(&bytes, min_capacity, sizeof(T))) /* would overflow size_t */  \
      return 0;                                                                \
    T *new_data = (T *)realloc(v->data, bytes);                                \
    if (!new_data)                                                             \
      return 0;                                                                \
    v->data = new_data;                                                        \
    v->capacity = min_capacity;                                                \
    return 1;                                                                  \
  }                                                                            \
                                                                               \
  [[nodiscard, gnu::nonnull(1)]]                                               \
  static inline int Vector_##T##_push_back(Vector_##T *restrict v, T value) {  \
    if (v->size + 1 >=                                                         \
        v->capacity) { /* no spare slot beyond the terminator */               \
      size_t new_cap = v->capacity < 4 ? 4 : v->capacity;                      \
      if (v->capacity >= 4 && ckd_add(&new_cap, new_cap, new_cap))             \
        return 0;                                                              \
      while (new_cap < v->size + 2)                                            \
        if (ckd_add(&new_cap, new_cap, new_cap)) /* safety for edge cases */   \
          return 0;                                                            \
      if (!Vector_##T##_reserve(v, new_cap))                                   \
        return 0;                                                              \
    }                                                                          \
    v->data[v->size] = value;                                                  \
    v->size++;                                                                 \
    memset(&v->data[v->size], 0, sizeof(T));                                   \
    return 1;                                                                  \
  }                                                                            \
                                                                               \
  [[gnu::nonnull(1)]]                                                          \
  static inline void Vector_##T##_pop_back(Vector_##T *restrict v) {           \
    if (v->size > 0) {                                                         \
      v->size--;                                                               \
      memset(&v->data[v->size], 0, sizeof(T));                                 \
    }                                                                          \
  }                                                                            \
                                                                               \
  [[gnu::nonnull(1)]]                                                          \
  static inline T *Vector_##T##_get(const Vector_##T *restrict v,              \
                                    size_t index) {                            \
    assert(index < v->size);                                                   \
    return (T *)&v->data[index];                                               \
  }                                                                            \
                                                                               \
  [[gnu::nonnull(1)]]                                                          \
  static inline void Vector_##T##_set(const Vector_##T *restrict v,            \
                                      size_t index, T value) {                 \
    assert(index < v->size);                                                   \
    v->data[index] = value;                                                    \
  }                                                                            \
                                                                               \
  [[gnu::nonnull(1)]]                                                          \
  static inline int Vector_##T##_is_empty(const Vector_##T *restrict v) {      \
    return v->size == 0;                                                       \
  }                                                                            \
                                                                               \
  [[gnu::nonnull(1)]]                                                          \
  static inline void Vector_##T##_remove_at(Vector_##T *restrict v,            \
                                            size_t index) {                    \
    assert(index < v->size);                                                   \
    void (*free_fn)(typeof_unqual(T)) = (void (*)(typeof_unqual(T)))(FREE_FN); \
    if (free_fn)                                                               \
      free_fn(v->data[index]);                                                 \
    if (index < v->size - 1) {                                                 \
      memmove(&v->data[index], &v->data[index + 1],                            \
              (v->size - index - 1) * sizeof(T));                              \
    }                                                                          \
    v->size--;                                                                 \
    memset(&v->data[v->size], 0, sizeof(T));                                   \
  }                                                                            \
                                                                               \
  [[gnu::nonnull(1)]]                                                          \
  static inline void Vector_##T##_shrink_to_fit(Vector_##T *restrict v) {      \
    size_t needed = v->size + 1;                                               \
    if (needed == v->capacity)                                                 \
      return;                                                                  \
    T *new_data = (T *)realloc(v->data, needed * sizeof(T));                   \
    if (new_data) {                                                            \
      v->data = new_data;                                                      \
      v->capacity = needed;                                                    \
    }                                                                          \
  }                                                                            \
                                                                               \
  [[gnu::nonnull(1)]]                                                          \
  static inline void Vector_##T##_clear(Vector_##T *restrict v) {              \
    void (*free_fn)(typeof_unqual(T)) = (void (*)(typeof_unqual(T)))(FREE_FN); \
    if (free_fn) {                                                             \
      for (size_t i = 0; i < v->size; i++)                                     \
        free_fn(v->data[i]);                                                   \
    }                                                                          \
    v->size = 0;                                                               \
    memset(&v->data[0], 0, sizeof(T));                                         \
  }
