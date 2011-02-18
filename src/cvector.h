/* File: cvector.h Author: Humbedooh Created on January 3, 2011, 9:25 PM */
#ifndef CVECTOR_H
#   define CVECTOR_H
#   define CVECTOR_FIRST   0
#   define CVECTOR_LAST    1

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
typedef cvector_element         *citerator;
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
cvector *cvector_init(void);
#   ifdef CVECTOR_THREADED
cvector *cvector_init_threaded(void);
#   endif

/*$2
 -----------------------------------------------------------------------------------------------------------------------
    dvector: Slim implementation of cvector
 -----------------------------------------------------------------------------------------------------------------------
 */

struct _dvector_element
{
    struct _dvector_element *next;
    struct _dvector_element *prev;
    void                    *object;
};
typedef struct _dvector_element dvector_element;
typedef struct _dvector
{
    dvector_element *first;
    dvector_element *last;
    unsigned int    size;
} dvector;
typedef struct _d_iterator
{
    dvector_element *current;
    dvector_element *next;
    dvector         *parent;
    unsigned char   start;
} d_iterator;
void            cvector_add(cvector *parent, void *object);
void            cvector_delete(cvector *parent);
void            cvector_delete_at(cvector *parent, cvector_element *el);
void            cvector_delete_before(cvector *parent, citerator el);
void            *cvector_first(cvector *parent);
void            *cvector_next(cvector *parent);
void            *cvector_last(cvector *parent);
void            *cvector_current(cvector *parent);
void            *cvector_shift(cvector *parent);
void            *cvector_pop(cvector *parent);
unsigned int    cvector_size(cvector *parent);
void            cvector_flush(cvector *parent);
void            *cvector_foreach(cvector *parent, citerator *iter);

/*$2
 -----------------------------------------------------------------------------------------------------------------------
    dvector prototypes
 -----------------------------------------------------------------------------------------------------------------------
 */

void    dvector_add(dvector *parent, void *object);
void    dvector_delete(d_iterator *iter);
void    *dvector_foreach(dvector *parent, d_iterator *iter);
void    dvector_flush(dvector *parent);
void    dvector_destroy(dvector *parent);
void    *dvector_pop(dvector *parent);
dvector *dvector_init(void);

/*$2
 -----------------------------------------------------------------------------------------------------------------------
    Macro for implementing a dvector foreach() block as: For each A in B (as type T), using iterator I do {...}
    example: int myValue, myArray[] = {1,2,3,4,5,6,7,8,9};
    d_iterator iter;
    dforeach(int, myValue, myArray, iter) { printf("I got %d\n", myValue);
    }
 -----------------------------------------------------------------------------------------------------------------------
 */

#   define dforeach(type, element, list, iterator) \
    iterator.start = 1; \
    while ((element = type dvector_foreach(list, &iterator)))
#   ifdef __cplusplus
}
#   endif
#endif /* CVECTOR_H */
