#include "rumble.h"

void rumble_clean_session(sessionHandle* session) {
    printf("cleaning up...\n");
    //free(session->sender.domain);
    //free(session->sender.user);
    //free(session->sender.raw);
    address* el;
    for ( el = (address*) cvector_first(session->recipients); el != NULL; el = (address*) cvector_next(session->recipients)) {
        free(el->domain);
        free(el->user);
        free(el->raw);
        cvector_delete(session->recipients);
    }
    printf("done!\n");
}