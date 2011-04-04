/*$I0 */
#include "rumble.h"

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void rumble_test(void) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    cvector     *cvec;
    c_iterator  iter;
    char        *moo = "testing, 1 2 3";
    char        *x;
    int         n = 0,
                rc;
    char        *cmd = "Moo";
    time_t      m;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    cvec = cvector_init();
    for (n = 0; n < 5; n++) {
        printf("<cvector> Testing foreach() with %u element(s)\n", n);
        cforeach((char *), x, cvec, iter) {

            /*
             * if (cvec->size > 3 && c ==1) { cvector_delete(&iter);
             * iter.start = 0;
             * }
             */
            printf("Output: %s, size: %u\n", x, cvec->size);
        }

        x = (char *) calloc(1, 20);
        sprintf(x, "testing, %u", rand());
        cvector_add(cvec, x);
    }

    cvector_flush(cvec);
    for (n = 0; n < 16; n++) cvector_add(cvec, "TESTING");
    cvector_add(cvec, cmd);
    m = time(0);
    moo = "Moo";
    rc = 0;
    for (n = 0; n < 40000000; n++) {
        if (!strcmp(cmd, "LOGOUT")) rc++;
        if (!strcmp(cmd, "NOOP")) rc++;
        if (!strcmp(cmd, "CAPABILITY")) rc++;
        if (!strcmp(cmd, "AUTHENTICATE")) rc++;
        if (!strcmp(cmd, "STARTTLS")) rc++;
        if (!strcmp(cmd, "SELECT")) rc++;
        if (!strcmp(cmd, "EXAMINE")) rc++;
        if (!strcmp(cmd, "CREATE")) rc++;
        if (!strcmp(cmd, "DELETE")) rc++;
        if (!strcmp(cmd, "RENAME")) rc++;
        if (!strcmp(cmd, "SUBSCRIBE")) rc++;
        if (!strcmp(cmd, "UNSUBSCRIBE")) rc++;
        if (!strcmp(cmd, "LIST")) rc++;
        if (!strcmp(cmd, "LSUB")) rc++;
        if (!strcmp(cmd, "STATUS")) rc++;
        if (!strcmp(cmd, moo)) rc++;
        if (!strcmp(cmd, "CHECK")) rc++;
        if (!strcmp(cmd, "CLOSE")) rc++;
        if (!strcmp(cmd, "EXPUNGE")) rc++;
        if (!strcmp(cmd, "SEARCH")) rc++;
        if (!strcmp(cmd, "FETCH")) rc++;
        if (!strcmp(cmd, "STORE")) rc++;
        if (!strcmp(cmd, "COPY")) rc++;
        if (!strcmp(cmd, "Moo")) rc++;
        cmd = "Moo";
    }

    printf("Old took %lu, rc = %d\n", time(0) - m, rc);
    m = time(0);
    for (n = 0; n < 40000000; n++) {
        cforeach((char *), x, cvec, iter) {
            if (!strcmp(cmd, x)) rc++;
        }
    }

    printf("New took %lu, rc = %d\n", time(0) - m, rc);
}
