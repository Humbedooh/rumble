/*$T cvector.h GC 1.140 02/16/11 21:15:02 */

/* File: cvector.h Author: Humbedooh Created on January 3, 2011, 9:25 PM */
#ifndef CVECTOR_H
#   define CVECTOR_H

/* define CVECTOR_THREADED // Comment out this line to disable threaded support. */
#   ifdef __cplusplus
extern "C"
{
#   endif
#   include <stdlib.h>
#   ifdef CVECTOR_THREADED
#      include <pthread.h>
#   endif
struct _cvector_element
{
    struct _cvector_element *previous;
    struct _cvector_element *next;
    void                    *object;
};
typedef struct _cvector_element cvector_element;
#   define citerator   cvector_element *
typedef struct _cvector
{
    cvector_element *first;
    cvector_element *last;
    cvector_element *current;
    unsigned int    size;
#   ifdef CVECTOR_THREADED
    pthread_mutex_t mutex;
#   endif
} cvector;
cvector         *cvector_init(void);
#   ifdef CVECTOR_THREADED
cvector         *cvector_init_threaded(void);
#   endif
void            cvector_add(cvector *parent, void *object);
void            cvector_delete(cvector *parent);
void            cvector_delete_at(cvector *parent, cvector_element *el);
void            *cvector_first(cvector *parent);
void            *cvector_next(cvector *parent);
void            *cvector_last(cvector *parent);
void            *cvector_current(cvector *parent);
void            *cvector_shift(cvector *parent);
void            *cvector_pop(cvector *parent);
unsigned int    cvector_size(cvector *parent);
void            cvector_flush(cvector *parent);
void            *cvector_foreach(cvector *parent, citerator *iter);
#   ifdef __cplusplus
}
#   endif
#endif /* CVECTOR_H */
