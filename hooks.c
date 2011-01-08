#include "rumble.h"
#include "servers.h"

void rumble_hook_function(void* handle, uint32_t flags, ssize_t (*func)(sessionHandle*) ) {
    hookHandle* hook = malloc(sizeof(hookHandle));
    hook->func = func;
    hook->flags = flags;
    hook->module = ((masterHandle*) handle)->readOnly.currentSO;
    #ifdef RUMBLE_DEBUG
    printf("<hooks> Adding hook of type %#lx from %s\n", hook->flags, hook->module);
    #endif
    switch ( flags && RUMBLE_HOOK_STATE_MASK ) {
        case RUMBLE_HOOK_ACCEPT:
            switch ( flags & RUMBLE_HOOK_SVC_MASK ) {
                case RUMBLE_HOOK_SMTP: cvector_add(((masterHandle*) handle)->smtp.init_hooks, hook);break;
                case RUMBLE_HOOK_POP3: cvector_add(((masterHandle*) handle)->pop3.init_hooks, hook);break;
                case RUMBLE_HOOK_IMAP: cvector_add(((masterHandle*) handle)->imap.init_hooks, hook);break;
                default: break;
                } break;
        case RUMBLE_HOOK_COMMAND:
            switch ( flags & RUMBLE_HOOK_SVC_MASK ) {
                case RUMBLE_HOOK_SMTP: cvector_add(((masterHandle*) handle)->smtp.cue_hooks, hook);break;
                case RUMBLE_HOOK_POP3: cvector_add(((masterHandle*) handle)->pop3.cue_hooks, hook);break;
                case RUMBLE_HOOK_IMAP: cvector_add(((masterHandle*) handle)->imap.cue_hooks, hook);break;
                default: break;
                } break;
        case RUMBLE_HOOK_EXIT:
            switch ( flags & RUMBLE_HOOK_SVC_MASK ) {
                case RUMBLE_HOOK_SMTP: cvector_add(((masterHandle*) handle)->smtp.exit_hooks, hook);break;
                case RUMBLE_HOOK_POP3: cvector_add(((masterHandle*) handle)->pop3.exit_hooks, hook);break;
                case RUMBLE_HOOK_IMAP: cvector_add(((masterHandle*) handle)->imap.exit_hooks, hook);break;
                default: break;
                } break;
        default: break;
    }
}


ssize_t rumble_server_execute_hooks(sessionHandle* session, cvector* hooks, uint32_t flags) {
    int g = 0;
    ssize_t rc = EXIT_SUCCESS;
    hookHandle* el;
    #ifdef RUMBLE_DEBUG
    if ( cvector_size(hooks)) printf("<hooks> Running hooks of type %#lx\n", flags);
    #endif
    for (el = (hookHandle*) cvector_first(hooks); el != NULL; el = (hookHandle*) cvector_next(hooks)) {
        if ( el->flags == flags ) {
            g++;
            ssize_t (*hookFunc)(sessionHandle*) = el->func;
            #ifdef RUMBLE_DEBUG
            printf("<hooks> Executing hook %#x from %s\n", hookFunc, el->module);
            #endif
            rc = (*hookFunc)(session);
        }
    }
    return rc;
}

ssize_t rumble_server_schedule_hooks(masterHandle* handle, sessionHandle* session, uint32_t flags) {
    switch ( flags && RUMBLE_HOOK_STATE_MASK ) {
        case RUMBLE_HOOK_ACCEPT:
            switch ( flags & RUMBLE_HOOK_SVC_MASK ) {
                case RUMBLE_HOOK_SMTP: return rumble_server_execute_hooks(session, handle->smtp.init_hooks, flags);
                case RUMBLE_HOOK_POP3: return rumble_server_execute_hooks(session, handle->pop3.init_hooks, flags);
                case RUMBLE_HOOK_IMAP: return rumble_server_execute_hooks(session, handle->imap.init_hooks, flags);
                default: break;
                } break;
        case RUMBLE_HOOK_COMMAND:
            switch ( flags & RUMBLE_HOOK_SVC_MASK ) {
                case RUMBLE_HOOK_SMTP: return rumble_server_execute_hooks(session, handle->smtp.cue_hooks, flags);
                case RUMBLE_HOOK_POP3: return rumble_server_execute_hooks(session, handle->pop3.cue_hooks, flags);
                case RUMBLE_HOOK_IMAP: return rumble_server_execute_hooks(session, handle->imap.cue_hooks, flags);
                default: break;
            } break;
        case RUMBLE_HOOK_EXIT:
            switch ( flags & RUMBLE_HOOK_SVC_MASK ) {
                case RUMBLE_HOOK_SMTP: return rumble_server_execute_hooks(session, handle->smtp.exit_hooks, flags);
                case RUMBLE_HOOK_POP3: return rumble_server_execute_hooks(session, handle->pop3.exit_hooks, flags);
                case RUMBLE_HOOK_IMAP: return rumble_server_execute_hooks(session, handle->imap.exit_hooks, flags);
                default: break;
            } break;
        default: break;
    }
    return EXIT_SUCCESS;
}
