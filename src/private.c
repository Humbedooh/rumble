#include "rumble.h"
extern masterHandle* master;

void rumble_clean_session(sessionHandle* session) {
    rumble_free_address(&session->sender);
    address* el;
    for ( el = (address*) cvector_first(session->recipients); el != NULL; el = (address*) cvector_next(session->recipients)) {
        rumble_free_address(el);
    }
    cvector_flush(session->recipients);
}

masterHandle* rumble_get_master() {
    return master;
}