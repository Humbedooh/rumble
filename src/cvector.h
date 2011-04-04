/* File: cvector.h Author: Humbedooh Created on January 3, 2011, 9:25 PM */
#ifndef CVECTOR_H
#   define CVECTOR_H
#   define CVECTOR_FIRST   0
#   define CVECTOR_LAST    1
#   ifdef __cplusplus
extern "C"
{
#   endif
#   include <stdlib.h>

/*$2
 -----------------------------------------------------------------------------------------------------------------------
    cvector: Chunked vectoring mechanism for ANSI C. Uses alligned memory chunks for accessing an array of arbitrary
    size. This allows for very fast access to the elements (constant time) and adding new values in constant amortized
    time, while deleting values may take a significant amount of linear time. If you have a static array of values, or
    just an array that doesn't change much, cvector is the best choice for fast access to elements.
 -----------------------------------------------------------------------------------------------------------------------
 */

typedef struct _cvector
{
    const void      **objects;
    unsigned int    size;
    unsigned int    allocated;
} cvector;
typedef struct _c_iterator
{
    cvector         *parent;
    unsigned int    position;
} c_iterator;

/*$2
 -----------------------------------------------------------------------------------------------------------------------
    dvector: Dynamic vector mechanism. Unlike cvector, dvector uses mutual referencing to allow for very fast
    insertions and deletions (constant time), while accessing elements is done somewhat slower (linear time). Thus,
    dvectors are optimal for arrays where you have a lot of inserting and deleting compared to indexing.
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
    dvector         *parent;
    unsigned char   start;
} d_iterator;
void        cvector_add(cvector *parent, const void *object);
void        cvector_delete(c_iterator *iter);
const void  *cvector_foreach(cvector *parent, c_iterator *iter);
void        cvector_flush(cvector *parent);
void        cvector_destroy(cvector *parent);
const void  *cvector_pop(cvector *parent);
cvector     *cvector_init(void);

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
    Macro for implementing a (d/c)vector foreach() block as: For each A in B (as type T), using iterator I do {...}
    example: int myValue, myArray[] = {1,2,3,4,5,6,7,8,9};
    d_iterator iter;
    dforeach(int, myValue, myArray, iter) { printf("I got %d\n", myValue);
    }
 -----------------------------------------------------------------------------------------------------------------------
 */

#   define cforeach(type, element, list, iterator) \
    iterator.parent = list; \
    for \
    ( \
        element = type list->objects[(iterator.position = 0)]; \
        iterator.position < list->size; \
        element = type list->objects[++iterator.position] \
    )
#   define dforeach(type, element, list, iterator) \
    iterator.start = 1; \
while ((element = type dvector_foreach(list, &iterator)))
#   ifdef __cplusplus
}
#   endif
#endif /* CVECTOR_H */
