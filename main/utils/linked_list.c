/*
*   linked_list.c
*
*   Created on: Sept 12, 2019
*       Author: Jonathan Moriarty
*/

#include <stdlib.h>
#include <esp_log.h>

#include "linked_list.h"

/*
#define dbgList(l) do{ \
        ESP_LOGD("LL", "   %s::%d, len:%d, ", __func__, __LINE__, l->length); \
        node_t* currentNode = l->first; \
        while (currentNode != NULL) \
        { \
            ESP_LOGD("LL", "%p ", currentNode); \
            currentNode = currentNode->next; \
        } \
    } while(0)
*/
#define dbgList(l)

// Add to the end of the list.
void push(list_t* list, void* val)
{
    dbgList(list);
    node_t* newLast = malloc(sizeof(node_t));
    newLast->val = val;
    newLast->next = NULL;
    newLast->prev = list->last;

    if (list->length == 0)
    {
        list->first = newLast;
        list->last = newLast;
    }
    else
    {
        list->last->next = newLast;
        list->last = newLast;
    }
    list->length++;
    dbgList(list);
}

// Remove from the end of the list.
void* pop(list_t* list)
{
    dbgList(list);
    void* retval = NULL;

    // Get a direct pointer to the node we're removing.
    node_t* target = list->last;

    // If the list any nodes at all.
    if (target != NULL)
    {

        // Adjust the node before the removed node, then adjust the list's last pointer.
        if (target->prev != NULL)
        {
            target->prev->next = NULL;
            list->last = target->prev;
        }
        // If the list has only one node, clear the first and last pointers.
        else
        {
            list->first = NULL;
            list->last = NULL;
        }

        // Get the last node val, then free it and update length.
        retval = target->val;
        free(target);
        list->length--;
    }

    dbgList(list);
    return retval;
}

// Add to the front of the list.
void unshift(list_t* list, void* val)
{
    dbgList(list);
    node_t* newFirst = malloc(sizeof(node_t));
    newFirst->val = val;
    newFirst->next = list->first;
    newFirst->prev = NULL;

    if (list->length == 0)
    {
        list->first = newFirst;
        list->last = newFirst;
    }
    else
    {
        list->first->prev = newFirst;
        list->first = newFirst;
    }
    list->length++;
    dbgList(list);
}

// Remove from the front of the list.
void* shift(list_t* list)
{
    dbgList(list);
    void* retval = NULL;

    // Get a direct pointer to the node we're removing.
    node_t* target = list->first;

    // If the list any nodes at all.
    if (target != NULL)
    {

        // Adjust the node after the removed node, then adjust the list's first pointer.
        if (target->next != NULL)
        {
            target->next->prev = NULL;
            list->first = target->next;
        }
        // If the list has only one node, clear the first and last pointers.
        else
        {
            list->first = NULL;
            list->last = NULL;
        }

        // Get the first node val, then free it and update length.
        retval = target->val;
        free(target);
        list->length--;
    }

    dbgList(list);
    return retval;
}

//TODO: bool return to check if index is valid?
// Add at an index in the list.
void add(list_t* list, void* val, int index)
{
    dbgList(list);
    // If the index we're trying to add to the start of the list.
    if (index == 0)
    {
        unshift(list, val);
    }
    // Else if the index we're trying to add to is before the end of the list.
    else if (index < list->length - 1)
    {
        node_t* newNode = malloc(sizeof(node_t));
        newNode->val = val;
        newNode->next = NULL;
        newNode->prev = NULL;

        node_t* current = NULL;
        for (int i = 0; i < index; i++)
        {
            current = current == NULL ? list->first : current->next;
        }

        // We need to adjust the newNode, and the nodes before and after it.
        // current is set to the node before it.

        current->next->prev = newNode;
        newNode->next = current->next;

        current->next = newNode;
        newNode->prev = current;

        list->length++;
    }
    // Else just add the node to the end of the list.
    else
    {
        push(list, val);
    }
    dbgList(list);
}

// Remove at an index in the list.
void* removeIdx(list_t* list, int index)
{
    dbgList(list);
    // If the list is null or empty, dont touch it
    if(NULL == list || list->length == 0)
    {
        dbgList(list);
        return NULL;
    }
    // If the index we're trying to remove from is the start of the list.
    else if (index == 0)
    {
        dbgList(list);
        return shift(list);
    }
    // Else if the index we're trying to remove from is before the end of the list.
    else if (index < list->length - 1)
    {
        void* retval = NULL;

        node_t* current = NULL;
        for (int i = 0; i < index; i++)
        {
            current = current == NULL ? list->first : current->next;
        }

        // We need to free the removed node, and adjust the nodes before and after it.
        // current is set to the node before it.

        node_t* target = current->next;
        retval = target->val;

        current->next = target->next;
        current->next->prev = current;

        free(target);
        target = NULL;

        list->length--;
        dbgList(list);
        return retval;
    }
    // Else just remove the node at the end of the list.
    else
    {
        dbgList(list);
        return pop(list);
    }
}

/**
 * Remove a specific entry from the linked list
 * This only removes the first instance of the entry if it is linked multiple
 * times
 *
 * @param list  The list to remove an entry from
 * @param entry The entry to remove
 * @return The void* val associated with the removed entry
 */
void* removeEntry(list_t* list, node_t* entry)
{
    dbgList(list);
    // If the list is null or empty, dont touch it
    if(NULL == list || list->length == 0)
    {
        dbgList(list);
        return NULL;
    }
    // If the entry we're trying to remove is the fist one, shift it
    else if(list->first == entry)
    {
        dbgList(list);
        return shift(list);
    }
    // If the entry we're trying to remove is the last one, pop it
    else if(list->last == entry)
    {
        dbgList(list);
        return pop(list);
    }
    // Otherwise it's somewhere in the middle, or doesn't exist
    else
    {
        // Start at list->first->next because we know the entry isn't at the head
        node_t* prev = list->first;
        node_t* curr = prev->next;
        // Iterate!
        while (curr != NULL)
        {
            // Found the node to remove
            if(entry == curr->next)
            {
                // We need to free the removed node, and adjust the nodes before and after it.
                // current is set to the node before it.

                node_t* target = curr->next;
                void* retval = target->val;

                curr->next = target->next;
                curr->next->prev = curr;

                free(target);
                target = NULL;

                list->length--;
                dbgList(list);
                return retval;
            }

            // Iterate to the next node
            curr = curr->next;
        }
    }
    // Nothing to be removed
    dbgList(list);
    return NULL;
}

// Remove all items from the list.
void clear(list_t* list)
{
    dbgList(list);
    while (list->first != NULL)
    {
        pop(list);
    }
    dbgList(list);
}

