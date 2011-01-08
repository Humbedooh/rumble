#include "rumble.h"

void rumble_clean_session(sessionHandle* session) {
    free(session->client->addr);
    free(session->client->client_info);
    free(session->client);
    free(session->sender.domain);
    free(session->sender.user);
    free(session->sender.raw);
    free(session->sender);
    cvector_element* el = session->recipients->first;
    while ( el != NULL ) {
        address* ad = (address*) el->object;
        free(ad->domain);
        free(ad->user);
        free(ad->raw);
        free(ad);
        el = el->next;
        free(el->previous);
    }
}