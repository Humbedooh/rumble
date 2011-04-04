/*$6
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

#include "cvector.h"

/*
 =======================================================================================================================
 =======================================================================================================================
 */
cvector *cvector_init(void) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    cvector *cvec = (cvector *) malloc(sizeof(cvector));
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!cvec) return (0);
    cvec->size = 0;
    cvec->objects = (const void **) calloc(33, sizeof(void *));
    cvec->allocated = 32;
    return (cvec);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void cvector_add(cvector *parent, const void *object) {

    /*~~~~~~~~~~~~~~~~~*/
    unsigned int    size;
    /*~~~~~~~~~~~~~~~~~*/

    if (!parent) return;
    if (parent->allocated == parent->size) {
        size = parent->allocated * 2;
        parent->objects = (const void **) realloc((void **) parent->objects, (size + 1) * sizeof(void *));
        parent->allocated = size;
        parent->objects[parent->allocated] = 0;
    }

    parent->objects[parent->size] = object;
    parent->size++;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void cvector_delete(c_iterator *iter) {

    /*~~~~~~~~~~~~~~*/
    unsigned int    n;
    /*~~~~~~~~~~~~~~*/

    if (!iter) return;
    for (n = iter->position; n < iter->parent->size; n++) iter->parent->objects[n] = iter->parent->objects[n + 1];
    iter->parent->objects[iter->parent->size] = 0;
    iter->parent->size--;
    iter->position--;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
const void *cvector_foreach(cvector *parent, c_iterator *iter) {
    if (iter->position == 0) {
        iter->parent = parent;
        iter->position++;
        return (parent->objects[0]);
    }

    iter->position++;
    return (iter->position < parent->size ? parent->objects[iter->position] : 0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void cvector_flush(cvector *parent) {
    free((void **) parent->objects);
    parent->allocated = 32;
    parent->objects = (const void **) calloc(33, sizeof(void *));
    parent->size = 0;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void cvector_destroy(cvector *parent) {
    free((void **) parent->objects);
    free(parent);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
const void *cvector_pop(cvector *parent) {

    /*~~~~~~~~~~~~~~~~*/
    const void  *object;
    /*~~~~~~~~~~~~~~~~*/

    if (!parent) return (0);
    object = parent->objects[parent->size - 1];
    parent->objects[parent->size - 1] = 0;
    parent->size--;
    return (object);
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
    iter->current = el->prev ? el->prev : iter->parent->first;
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
        return (iter->current ? iter->current->object : 0);
    }

    if (!(iter->current = iter->current ? iter->current->next : 0)) return (0);
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
