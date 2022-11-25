/*
*   linked_list.h
*
*   Created on: Sept 12, 2019
*       Author: Jonathan Moriarty
*/

#ifndef _LINKED_LIST_H
#define _LINKED_LIST_H

// Doubly-linked list.
typedef struct node
{
    void* val;
    struct node* next;
    struct node* prev;
} node_t;

typedef struct
{
    node_t* first;
    node_t* last;
    int length;
} llist_t;

// Creating an empty list example.
/*
    llist_t * myList = malloc(sizeof(llist_t));
    myList->first = NULL;
    myList->last = NULL;
    myList->length = 0;
*/

// Iterating through the list example.
/*
    node_t * currentNode = landedTetrads->first;
    while (currentNode != NULL)
    {
        //Perform desired operations with current node.
        currentNode = currentNode->next;
    }
*/

// Add to the end of the list.
void push(llist_t* list, void* val);

// Remove from the end of the list.
void* pop(llist_t* list);

// Add to the front of the list.
void unshift(llist_t* list, void* val);

// Remove from the front of the list.
void* shift(llist_t* list);

// Add at an index in the list.
void add(llist_t* list, void* val, int index);

// Remove at an index in the list.
void* removeIdx(llist_t* list, int index);

// Remove a given entry from the list.
void* removeEntry(llist_t* list, node_t* entry);

// Remove all items from the list.
// NOTE: This frees nodes but does not free anything pointed to by the vals of nodes.
void clear(llist_t* list);

// Exercise the linked list functions
void listTester(void);

#endif
