#include "cvector.h"

inline cvector* cvector_init() {
    cvector* c = malloc(sizeof(cvector));
    c->current = NULL;
    c->first = NULL;
    c->last = NULL;
    c->size = 0;
    return c;
}

#ifdef CVECTOR_THREADED
cvector* cvector_init_threaded() {
    cvector* c = calloc(1, sizeof(cvector));
    c->mutex = PTHREAD_MUTEX_INITIALIZER;
    return c;
}
#endif

void cvector_add(cvector* parent, void* object) {
    #ifdef CVECTOR_THREADED
    if (parent->mutex) pthread_mutex_lock(&parent->mutex);
    #endif
    cvector_element* el = cvector_element_init();
    el->object = object;
    el->previous = parent->last;
    el->next = NULL;
    parent->last = el;
    parent->first = parent->first ? parent->first : el;
    if (el->previous) { el->previous->next = el; }
    parent->size++;
    #ifdef CVECTOR_THREADED
    if (parent->mutex) pthread_mutex_unlock(&parent->mutex);
    #endif
}

void cvector_delete(cvector* parent) {
    #ifdef CVECTOR_THREADED
    if (parent->mutex) pthread_mutex_lock(&parent->mutex);
    #endif
    cvector_element* this = parent->current;
    if (!this) return;
    parent->first = (parent->first == this) ? this->next : parent->first;
    parent->last = (parent->last == this) ? this->previous : parent->last;
    parent->current = this->previous ? this->previous : this->next;
    if ( this->previous ) this->previous->next = this->next;
    if ( this->next ) this->next->previous = this->previous;
    this->object = NULL;
    this->previous = NULL;
    this->next = NULL;
    free(this);
    parent->size--;
    #ifdef CVECTOR_THREADED
    if (parent->mutex) pthread_mutex_unlock(&parent->mutex);
    #endif
}

void cvector_delete_at(cvector* parent, cvector_element* el) {
    #ifdef CVECTOR_THREADED
    if (parent->mutex) pthread_mutex_lock(&parent->mutex);
    #endif
    parent->first = ( parent->first == el ) ? el->next : parent->first;
    parent->last = ( parent->last == el ) ? el->previous : parent->last;
    if ( el->previous ) { el->previous->next = el->next; }
    if ( el->next ) {  el->next->previous = el->previous; }
    free(el);
    parent->size--;
    #ifdef CVECTOR_THREADED
    if (parent->mutex) pthread_mutex_unlock(&parent->mutex);
    #endif
}

void* cvector_shift(cvector* parent) {
    #ifdef CVECTOR_THREADED
    if (parent->mutex) pthread_mutex_lock(&parent->mutex);
    #endif
    cvector_element* this = parent->first;
    if (!this) return NULL;
    if ( this->next ) this->next->previous = this->previous;
    void* o = this->object;
    free(this);
    parent->size--;
    #ifdef CVECTOR_THREADED
    if (parent->mutex) pthread_mutex_unlock(&parent->mutex);
    #endif
    return o;
}

void* cvector_pop(cvector* parent) {
    #ifdef CVECTOR_THREADED
    if (parent->mutex) pthread_mutex_lock(&parent->mutex);
    #endif
    cvector_element* this = parent->last;
    if (!this) return NULL;
    if ( this->previous ) this->previous->next = NULL;
    void* o = this->object;
    free(this);
    parent->size--;
    #ifdef CVECTOR_THREADED
    if (parent->mutex) pthread_mutex_unlock(&parent->mutex);
    #endif
    return o;
}


inline void* cvector_first(cvector* parent) {
    #ifdef CVECTOR_THREADED
    if (parent->mutex) pthread_mutex_lock(&parent->mutex);
    #endif
    parent->current = parent->first;
    #ifdef CVECTOR_THREADED
    if (parent->mutex) pthread_mutex_unlock(&parent->mutex);
    #endif
    return parent->current ? parent->current->object : NULL;
}

inline void* cvector_next(cvector* parent) {
    #ifdef CVECTOR_THREADED
    if (parent->mutex) pthread_mutex_lock(&parent->mutex);
    #endif
    parent->current = parent->current ? parent->current->next : NULL;
    #ifdef CVECTOR_THREADED
    if (parent->mutex) pthread_mutex_unlock(&parent->mutex);
    #endif
    return parent->current ? parent->current->object : NULL;
}

inline void* cvector_previous(cvector* parent) {
    #ifdef CVECTOR_THREADED
    if (parent->mutex) pthread_mutex_lock(&parent->mutex);
    #endif
    parent->current = parent->current ? parent->current->previous : NULL;
    #ifdef CVECTOR_THREADED
    if (parent->mutex) pthread_mutex_unlock(&parent->mutex);
    #endif
    return parent->current ? parent->current->object : NULL;
}

inline void* cvector_last(cvector* parent) {
    #ifdef CVECTOR_THREADED
    if (parent->mutex) pthread_mutex_lock(&parent->mutex);
    #endif
    parent->current = parent->last;
    #ifdef CVECTOR_THREADED
    if (parent->mutex) pthread_mutex_unlock(&parent->mutex);
    #endif
    return parent->current ? parent->current->object : NULL;
}

inline void* cvector_current(cvector* parent) {
    return parent->current ? parent->current->object : NULL;
}

inline unsigned int cvector_size(cvector* parent) {
    return parent->size;
}

inline void cvector_flush(cvector* parent) {
    cvector_element* el = parent->last;
    cvector_element* oel;
    while ( el ) {
        oel = el;
        el = el->previous;
        free(oel);
    }
    parent->last = 0;
    parent->first = 0;
    parent->current = 0;
    parent->size = 0;
}