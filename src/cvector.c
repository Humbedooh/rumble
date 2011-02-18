/*$6
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

#include "cvector.h"
#include <stdio.h>

/*
 =======================================================================================================================
 =======================================================================================================================
 */
cvector *cvector_init(void) {

    /*~~~~~~~~~~~*/
    cvector *c = 0;
    /*~~~~~~~~~~~*/

    c = (cvector *) malloc(sizeof(cvector));
    if (!c) return (0);
    c->current = NULL;
    c->first = NULL;
    c->last = NULL;
    c->size = 0;
    return (c);
}

#ifdef CVECTOR_THREADED

/*
 =======================================================================================================================
 =======================================================================================================================
 */
cvector *cvector_init_threaded(void) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    cvector *c = calloc(1, sizeof(cvector));
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    c->mutex = PTHREAD_MUTEX_INITIALIZER;
    return (c);
}
#endif

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void cvector_add(cvector *parent, void *object)
{
#ifdef CVECTOR_THREADED
    if (parent->mutex) pthread_mutex_lock(&parent->mutex);
#endif

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    cvector_element *el = (cvector_element *) malloc(sizeof(cvector_element));
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!el) return;
    el->object = object;
    el->previous = parent->last;
    el->next = NULL;
    parent->last = el;
    parent->first = parent->first ? parent->first : el;
    if (el->previous) el->previous->next = el;
    parent->size++;
#ifdef CVECTOR_THREADED
    if (parent->mutex) pthread_mutex_unlock(&parent->mutex);
#endif
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void cvector_delete(cvector *parent)
{
#ifdef CVECTOR_THREADED
    if (parent->mutex) pthread_mutex_lock(&parent->mutex);
#endif

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    cvector_element *currobj = parent->current;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!currobj) return;
    parent->first = (parent->first == currobj) ? currobj->next : parent->first;
    parent->last = (parent->last == currobj) ? currobj->previous : parent->last;
    parent->current = currobj->previous ? currobj->previous : currobj->next;
    if (currobj->previous) currobj->previous->next = currobj->next;
    if (currobj->next) currobj->next->previous = currobj->previous;
    currobj->object = NULL;
    currobj->previous = NULL;
    currobj->next = NULL;
    free(currobj);
    parent->size--;
#ifdef CVECTOR_THREADED
    if (parent->mutex) pthread_mutex_unlock(&parent->mutex);
#endif
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void cvector_delete_at(cvector *parent, cvector_element *el)
{
#ifdef CVECTOR_THREADED
    if (parent->mutex) pthread_mutex_lock(&parent->mutex);
#endif
    if (!el) return;
    parent->first = (parent->first == el) ? el->next : parent->first;
    parent->last = (parent->last == el) ? el->previous : parent->last;
    if (el->previous) el->previous->next = el->next;
    if (el->next) el->next->previous = el->previous;
    free(el);
    parent->size--;
#ifdef CVECTOR_THREADED
    if (parent->mutex) pthread_mutex_unlock(&parent->mutex);
#endif
}

/*
 =======================================================================================================================
    cvector_delete_before: Used for deleting elements using iterators
 =======================================================================================================================
 */
void cvector_delete_before(cvector *parent, cvector_element *el) {
    if (el == (citerator) CVECTOR_LAST) el = parent->last;
    cvector_delete_at(parent, el->previous);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void *cvector_shift(cvector *parent)
{
#ifdef CVECTOR_THREADED
    if (parent->mutex) pthread_mutex_lock(&parent->mutex);
#endif

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    cvector_element *currobj = parent->first;
    void            *obj = 0;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (currobj == obj) return (obj);
    if (currobj->next != (cvector_element *) 0) currobj->next->previous = currobj->previous;
    obj = currobj->object;
    free(currobj);
    parent->size--;
    if (parent->size == 0) {
        parent->current = 0;
        parent->first = 0;
        parent->last = 0;
    }

#ifdef CVECTOR_THREADED
    if (parent->mutex) pthread_mutex_unlock(&parent->mutex);
#endif
    return (obj);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void *cvector_pop(cvector *parent)
{
#ifdef CVECTOR_THREADED
    if (parent->mutex) pthread_mutex_lock(&parent->mutex);
#endif

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    cvector_element *currobj = parent->last;
    void            *obj = 0;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!currobj) return (obj);
    if (currobj->previous) currobj->previous->next = NULL;
    obj = currobj->object;
    free(currobj);
    parent->size--;
    if (parent->size == 0) {
        parent->current = 0;
        parent->first = 0;
        parent->last = 0;
    }

#ifdef CVECTOR_THREADED
    if (parent->mutex) pthread_mutex_unlock(&parent->mutex);
#endif
    return (obj);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void *cvector_first(cvector *parent)
{
#ifdef CVECTOR_THREADED
    if (parent->mutex) pthread_mutex_lock(&parent->mutex);
#endif
    parent->current = parent->first;
#ifdef CVECTOR_THREADED
    if (parent->mutex) pthread_mutex_unlock(&parent->mutex);
#endif
    return (parent->current ? parent->current->object : NULL);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void *cvector_next(cvector *parent)
{
#ifdef CVECTOR_THREADED
    if (parent->mutex) pthread_mutex_lock(&parent->mutex);
#endif
    parent->current = parent->current ? parent->current->next : NULL;
#ifdef CVECTOR_THREADED
    if (parent->mutex) pthread_mutex_unlock(&parent->mutex);
#endif
    return (parent->current ? parent->current->object : NULL);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void *cvector_previous(cvector *parent)
{
#ifdef CVECTOR_THREADED
    if (parent->mutex) pthread_mutex_lock(&parent->mutex);
#endif
    parent->current = parent->current ? parent->current->previous : NULL;
#ifdef CVECTOR_THREADED
    if (parent->mutex) pthread_mutex_unlock(&parent->mutex);
#endif
    return (parent->current ? parent->current->object : NULL);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void *cvector_last(cvector *parent)
{
#ifdef CVECTOR_THREADED
    if (parent->mutex) pthread_mutex_lock(&parent->mutex);
#endif
    parent->current = parent->last;
#ifdef CVECTOR_THREADED
    if (parent->mutex) pthread_mutex_unlock(&parent->mutex);
#endif
    return (parent->current ? parent->current->object : NULL);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void *cvector_current(cvector *parent) {
    return (parent->current ? parent->current->object : NULL);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
unsigned int cvector_size(cvector *parent) {
    return (parent->size);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void cvector_flush(cvector *parent) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    cvector_element *el = parent->last;
    cvector_element *oel;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    while (el) {
        oel = el;
        el = el->previous;
        free(oel);
    }

    parent->last = 0;
    parent->first = 0;
    parent->current = 0;
    parent->size = 0;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void *cvector_foreach(cvector *parent, citerator *iter) {

    /*~~~~~~~*/
    void    *o;
    /*~~~~~~~*/

    if (*iter == (citerator) CVECTOR_LAST) return (0);
    if (*iter == (citerator) CVECTOR_FIRST) *iter = parent->first;
    if (*iter == 0) return (0);
    o = (*iter)->object;
    *iter = (*iter)->next;
    if (*iter == 0) *iter = (citerator) CVECTOR_LAST;
    return (o);
}

/*$5
 #######################################################################################################################
    dvector implementation
 #######################################################################################################################
 */

/*
 =======================================================================================================================
 =======================================================================================================================
 */
dvector *dvector_init(void) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    dvector *dvec = (dvector *) malloc(sizeof(dvector));
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!dvec) return (0);
    dvec->size = 0;
    dvec->first = 0;
    dvec->last = 0;
    return (dvec);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void dvector_add(dvector *parent, void *object) {

    /*~~~~~~~~~~~~~~~~*/
    dvector_element *el;
    /*~~~~~~~~~~~~~~~~*/

    if (!parent) return;
    el = (dvector_element *) malloc(sizeof(dvector_element));
    el->object = object;
    el->next = 0;
    el->prev = parent->last;
    if (parent->last) parent->last->next = el;
    else parent->first = el;
    parent->last = el;
    parent->size++;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void dvector_delete(d_iterator *iter) {

    /*~~~~~~~~~~~~~~~~*/
    dvector_element *el;
    /*~~~~~~~~~~~~~~~~*/

    if (!iter || !iter->current) return;
    el = iter->current;
    if (el->next) el->next->prev = el->prev;
    if (el->prev) el->prev->next = el->next;
    if (iter->parent->first == el) iter->parent->first = el->next;
    if (iter->parent->last == el) iter->parent->last = el->prev;
    iter->current = el->prev;
    free(el);
    iter->parent->size--;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void *dvector_foreach(dvector *parent, d_iterator *iter) {
    if (iter->start) {
        iter->start = 0;
        iter->parent = parent;
        iter->current = parent->first;
        iter->next = iter->current ? iter->current->next : 0;
        return (iter->current ? iter->current->object : 0);
    }

    iter->current = iter->current ? iter->current->next : 0;
    if (!iter->current) return (0);
    iter->next = iter->current->next;
    return (iter->current->object);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void dvector_flush(dvector *parent) {

    /*~~~~~~~~~~~~~~~~*/
    dvector_element *el,
                    *nl;
    /*~~~~~~~~~~~~~~~~*/

    for (el = parent->first; el; el = nl) {
        nl = el->next;
        free(el);
    }

    parent->size = 0;
    parent->first = 0;
    parent->last = 0;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void dvector_destroy(dvector *parent) {

    /*~~~~~~~~~~~~~~~~*/
    dvector_element *el,
                    *nl;
    /*~~~~~~~~~~~~~~~~*/

    for (el = parent->first; el; el = nl) {
        nl = el->next;
        free(el);
    }

    free(parent);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void *dvector_pop(dvector *parent) {

    /*~~~~~~~~~~~~~~~~~~~~*/
    dvector_element *el;
    void            *object;
    /*~~~~~~~~~~~~~~~~~~~~*/

    if (!parent) return (0);
    el = parent->last;
    if (el) {
        parent->last = el->prev;
        if (parent->first == el) parent->first = 0;
        parent->size--;
    }

    object = el->object;
    free(el);
    return (object);
}
