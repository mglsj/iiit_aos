#pragma once

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#define LIST_DEFINE(T, FREE_FN)                                                \
  typedef struct List_##T##_Node {                                             \
    T value;                                                                   \
    struct List_##T##_Node *prev;                                              \
    struct List_##T##_Node *next;                                              \
  } List_##T##_Node;                                                           \
                                                                               \
  typedef struct {                                                             \
    List_##T##_Node *head;                                                     \
    List_##T##_Node *tail;                                                     \
    size_t size;                                                               \
  } List_##T;                                                                  \
                                                                               \
  [[nodiscard]] static inline List_##T *List_##T##_new(void) {                 \
    List_##T *l = (List_##T *)malloc(sizeof(List_##T));                        \
    if (!l)                                                                    \
      return nullptr;                                                          \
    l->head = nullptr;                                                         \
    l->tail = nullptr;                                                         \
    l->size = 0;                                                               \
    return l;                                                                  \
  }                                                                            \
                                                                               \
  /* Frees every remaining element (via FREE_FN, if non-nullptr) and resets    \
     the list to empty. Keeps the List_##T struct itself alive. */             \
  static inline void List_##T##_clear(List_##T *l) {                           \
    void (*free_fn)(typeof_unqual(T)) = (void (*)(typeof_unqual(T)))(FREE_FN); \
    List_##T##_Node *node = l->head;                                           \
    while (node) {                                                             \
      List_##T##_Node *next = node->next;                                      \
      if (free_fn)                                                             \
        free_fn(node->value);                                                  \
      free(node);                                                              \
      node = next;                                                             \
    }                                                                          \
    l->head = nullptr;                                                         \
    l->tail = nullptr;                                                         \
    l->size = 0;                                                               \
  }                                                                            \
                                                                               \
  /* Counterpart to List_##T##_new: clears every node, then frees l itself.    \
     Safe to call with l == nullptr. */                                        \
  static inline void List_##T##_delete(List_##T *l) {                          \
    if (!l)                                                                    \
      return;                                                                  \
    List_##T##_clear(l);                                                       \
    free(l);                                                                   \
  }                                                                            \
                                                                               \
  [[nodiscard]] static inline int List_##T##_push_back(List_##T *l, T value) { \
    List_##T##_Node *node =                                                    \
        (List_##T##_Node *)malloc(sizeof(List_##T##_Node));                    \
    if (!node)                                                                 \
      return 0;                                                                \
    node->value = value;                                                       \
    node->next = nullptr;                                                      \
    node->prev = l->tail;                                                      \
    if (l->tail)                                                               \
      l->tail->next = node;                                                    \
    else                                                                       \
      l->head = node;                                                          \
    l->tail = node;                                                            \
    l->size++;                                                                 \
    return 1;                                                                  \
  }                                                                            \
                                                                               \
  /* Internal helper: node at 0-based index, walking from whichever end is     \
     closer. Caller must ensure index < l->size. */                            \
  static inline List_##T##_Node *List_##T##_node_at(const List_##T *l,         \
                                                    size_t index) {            \
    assert(index < l->size);                                                   \
    List_##T##_Node *node;                                                     \
    if (index <= l->size / 2) {                                                \
      node = l->head;                                                          \
      for (size_t i = 0; i < index; i++)                                       \
        node = node->next;                                                     \
    } else {                                                                   \
      node = l->tail;                                                          \
      for (size_t i = l->size - 1; i > index; i--)                             \
        node = node->prev;                                                     \
    }                                                                          \
    return node;                                                               \
  }                                                                            \
                                                                               \
  /* Inserts value so it becomes the element at `index` (0..size); index ==    \
     size behaves like push_back. Returns 0 on allocation failure. */          \
  [[nodiscard]] static inline int List_##T##_insert_at(                        \
      List_##T *l, size_t index, T value) {                                    \
    assert(index <= l->size);                                                  \
    if (index == l->size)                                                      \
      return List_##T##_push_back(l, value);                                   \
    List_##T##_Node *at = List_##T##_node_at(l, index);                        \
    List_##T##_Node *node =                                                    \
        (List_##T##_Node *)malloc(sizeof(List_##T##_Node));                    \
    if (!node)                                                                 \
      return 0;                                                                \
    node->value = value;                                                       \
    node->prev = at->prev;                                                     \
    node->next = at;                                                           \
    if (at->prev)                                                              \
      at->prev->next = node;                                                   \
    else                                                                       \
      l->head = node;                                                          \
    at->prev = node;                                                           \
    l->size++;                                                                 \
    return 1;                                                                  \
  }                                                                            \
                                                                               \
  /* Does NOT call FREE_FN -- see file header comment. No-op if empty. */      \
  static inline void List_##T##_pop_back(List_##T *l) {                        \
    if (!l->tail)                                                              \
      return;                                                                  \
    List_##T##_Node *node = l->tail;                                           \
    l->tail = node->prev;                                                      \
    if (l->tail)                                                               \
      l->tail->next = nullptr;                                                 \
    else                                                                       \
      l->head = nullptr;                                                       \
    free(node);                                                                \
    l->size--;                                                                 \
  }                                                                            \
                                                                               \
  /* O(1) deletion given a direct pointer to a node already in this list.      \
     Frees the element (via FREE_FN, if non-nullptr), unlinks the node,        \
     updating head/tail as needed, and frees the node. `node` MUST belong to   \
     `l` -- there's no cheap way to verify that, and passing a node from a     \
     different list (or an already-removed node) corrupts `l`'s links. */      \
  static inline void List_##T##_delete_node(List_##T *l,                       \
                                            List_##T##_Node *node) {           \
    assert(node != nullptr);                                                   \
    void (*free_fn)(typeof_unqual(T)) = (void (*)(typeof_unqual(T)))(FREE_FN); \
    if (free_fn)                                                               \
      free_fn(node->value);                                                    \
    if (node->prev)                                                            \
      node->prev->next = node->next;                                           \
    else                                                                       \
      l->head = node->next;                                                    \
    if (node->next)                                                            \
      node->next->prev = node->prev;                                           \
    else                                                                       \
      l->tail = node->prev;                                                    \
    free(node);                                                                \
    l->size--;                                                                 \
  }                                                                            \
                                                                               \
  /* Frees the element at index (via FREE_FN, if non-nullptr) and unlinks it.  \
     Equivalent to List_##T##_delete_node(l, List_##T##_node_at(l, index)). */ \
  static inline void List_##T##_delete_at(List_##T *l, size_t index) {         \
    List_##T##_delete_node(l, List_##T##_node_at(l, index));                   \
  }
