/* 
 * Code for basic C skills diagnostic.
 * Developed for courses 15-213/18-213/15-513 by R. E. Bryant, 2017
 * Modified to store strings, 2018
 */

/*
 * This program implements a queue supporting both FIFO and LIFO
 * operations.
 *
 * It uses a singly-linked list to represent the set of queue elements
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "harness.h"
#include "queue.h"

/*
  Create empty queue.
  Return NULL if could not allocate space.
*/
queue_t *q_new()
{
    queue_t *q = malloc(sizeof(queue_t));
    if (q)
    {
		q->head = NULL;
		q->tail = NULL;
		q->size = 0;
		return q;
    }

    return NULL;
}

/* Free all storage used by queue */
void q_free(queue_t *q)
{
    if (q)
    {
		/* How about freeing the list elements and the strings? */
		while (q->size)
		{
			assert(q_remove_head(q, NULL, 0));
		}
		/* Free queue structure */
		free(q);
    }
}

/*
  Attempt to insert element at head of queue.
  Return true if successful.
  Return false if q is NULL or could not allocate space.
  Argument s points to the string to be stored.
  The function must explicitly allocate space and copy the string into it.
 */
bool q_insert_head(queue_t *q, char *s)
{
    if (!q) return false;

    list_ele_t *new_head;
    /* What should you do if the q is NULL? */
    new_head = malloc(sizeof(list_ele_t));
    if (!new_head) return false;
    
    /* Don't forget to allocate space for the string and copy it */
    /* What if either call to malloc returns NULL? */
    char *new_str = malloc(strlen(s) + 1);
    if (!new_str)
    {
		// IMPORTANT!! have to free this one
		free(new_head);
      	return false;
    }

	strcpy(new_str, s);
	new_head->value = new_str;
    new_head->next = q->head;
    q->head = new_head;

    if (!q->tail)
    {
		assert(q->size == 0);
      	q->tail = new_head;
    }

    q->size += 1;

    return true;
}


/*
  Attempt to insert element at tail of queue.
  Return true if successful.
  Return false if q is NULL or could not allocate space.
  Argument s points to the string to be stored.
  The function must explicitly allocate space and copy the string into it.
 */
bool q_insert_tail(queue_t *q, char *s)
{
    /* You need to write the complete code for this function */
    /* Remember: It should operate in O(1) time */
	if (!q) return false;

	list_ele_t *new_tail;
    new_tail = malloc(sizeof(list_ele_t));
    if (!new_tail) return false;

    char *new_str = malloc(strlen(s) + 1);
    if (!new_str)
    {
		// IMPORTANT!! have to free this one
		free(new_tail);
      	return false;
    }
    
	strcpy(new_str, s);
	new_tail->value = new_str;
	new_tail->next = NULL;
	// if never inserted a ele
	if (!q->tail)
	{
		assert(q->size == 0);
		q->head = new_tail;
		q->tail = new_tail;
	}
	else
	{
		q->tail->next = new_tail;
    	q->tail = new_tail;
	}

	q->size += 1;

	return true;
}

/*
  Attempt to remove element from head of queue.
  Return true if successful.
  Return false if queue is NULL or empty.
  If sp is non-NULL and an element is removed, copy the removed string to *sp
  (up to a maximum of bufsize-1 characters, plus a null terminator.)
  The space used by the list element and the string should be freed.
*/
bool q_remove_head(queue_t *q, char *sp, size_t bufsize)
{
    if (!q || !q->head)
    {
      return false;
    }
	list_ele_t *old_head = q->head, *new_head = q->head->next;
    /* You need to fix up this code. */
	if (sp)
	{
		if (bufsize > 0)
		{
			strncpy(sp, q->head->value, bufsize - 1);
			sp[bufsize - 1] = '\0';
		}
	}
	free(old_head->value);
	free(old_head);
    q->head = new_head;
	q->size -= 1;
	
    return true;
}

/*
  Return number of elements in queue.
  Return 0 if q is NULL or empty
 */
int q_size(queue_t *q)
{
    /* You need to write the code for this function */
    /* Remember: It should operate in O(1) time */
	if (!q)
	{
		return 0;
	}
    return q->size;
}

/*
  Reverse elements in queue
  No effect if q is NULL or empty
  This function should not allocate or free any list elements
  (e.g., by calling q_insert_head, q_insert_tail, or q_remove_head).
  It should rearrange the existing ones.
 */
void q_reverse(queue_t *q)
{
    /* You need to write the code for this function */
	if (q && q->size > 1)
	{
		// better to have initializations like this
		list_ele_t *old_head = q->head, *old_tail = q->tail;
		list_ele_t *cur_node = q->head, *prev_node = NULL, *next_node = NULL;
		while (cur_node)
		{
			// if (cur_node == q->head)
			// {
			// 	q->tail = cur_node;
			// }
			next_node = cur_node->next;
			cur_node->next = prev_node;
			prev_node = cur_node;
			cur_node = next_node;
		}
		// q->head = prev_node;
		q->head = old_tail;
		q->tail = old_head;
	}
}

