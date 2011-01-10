#include "rumble.h"
extern masterHandle* master;

void rumble_clean_session(sessionHandle* session) {
    free(session->sender.domain);
    free(session->sender.user);
    free(session->sender.raw);
    rumble_flush_dictionary(session->sender.flags);
    address* el;
    for ( el = (address*) cvector_first(session->recipients); el != NULL; el = (address*) cvector_next(session->recipients)) {
        free(el->domain);
        free(el->user);
        free(el->raw);
        rumble_flush_dictionary(el->flags);
        cvector_delete(session->recipients);
    }
}

masterHandle* rumble_get_master() {
    return master;
}