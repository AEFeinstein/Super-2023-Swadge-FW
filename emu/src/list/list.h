
//
// list.h
//
// Copyright (c) 2010 TJ Holowaychuk <tj@vision-media.ca>
//

#ifndef __CLIBS_LIST_H__
#define __CLIBS_LIST_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

// Library version

#define LIST_VERSION "0.2.0"

// Memory management macros
#ifdef LIST_CONFIG_H
#define _STR(x) #x
#define STR(x) _STR(x)
#include STR(LIST_CONFIG_H)
#undef _STR
#undef STR
#endif

#ifndef LIST_MALLOC
#define LIST_MALLOC malloc
#endif

#ifndef LIST_FREE
#define LIST_FREE free
#endif

/*
 * linked_list_t iterator direction.
 */

typedef enum {
    LIST_HEAD
  , LIST_TAIL
} list_direction_t;

/*
 * linked_list_t node struct.
 */

typedef struct list_node {
  struct list_node *prev;
  struct list_node *next;
  void *val;
} list_node_t;

/*
 * linked_list_t struct.
 */

typedef struct {
  list_node_t *head;
  list_node_t *tail;
  unsigned int len;
  void (*free)(void *val);
  int (*match)(void *a, void *b);
} linked_list_t;

/*
 * linked_list_t iterator struct.
 */

typedef struct {
  list_node_t *next;
  list_direction_t direction;
} list_iterator_t;

// Node prototypes.

list_node_t *
list_node_new(void *val);

// linked_list_t prototypes.

linked_list_t *
list_new(void);

list_node_t *
list_rpush(linked_list_t *self, list_node_t *node);

list_node_t *
list_lpush(linked_list_t *self, list_node_t *node);

list_node_t *
list_find(linked_list_t *self, void *val);

list_node_t *
list_at(linked_list_t *self, int index);

list_node_t *
list_rpop(linked_list_t *self);

list_node_t *
list_lpop(linked_list_t *self);

void
list_remove(linked_list_t *self, list_node_t *node);

void
list_destroy(linked_list_t *self);

// linked_list_t iterator prototypes.

list_iterator_t *
list_iterator_new(linked_list_t *list, list_direction_t direction);

list_iterator_t *
list_iterator_new_from_node(list_node_t *node, list_direction_t direction);

list_node_t *
list_iterator_next(list_iterator_t *self);

void
list_iterator_destroy(list_iterator_t *self);

#ifdef __cplusplus
}
#endif

#endif /* __CLIBS_LIST_H__ */
