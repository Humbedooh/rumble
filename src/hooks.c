#include "rumble.h"
#include "servers.h"

uint32_t rumble_module_check() {
    return RUMBLE_VERSION;
}

void rumble_hook_function(void* handle, uint32_t flags, ssize_t (*func)(sessionHandle*) ) {
    hookHandle* hook = malloc(sizeof(hookHandle));
    rumble_module_check();
    hook->func = func;
    hook->flags = flags;
    hook->module = ((masterHandle*) handle)->readOnly.currentSO;
    hook->modinfo = (rumble_module_info*) cvector_last(((masterHandle*) handle)->readOnly.modules);
    #if RUMBLE_DEBUG & RUMBLE_DEBUG_HOOKS
    printf("<debug :: hooks> Adding hook of type %#x from %s\n", hook->flags, hook->module);
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
    ssize_t rc = RUMBLE_RETURN_OKAY;
    cvector_element* el;
    #if RUMBLE_DEBUG & RUMBLE_DEBUG_HOOKS
    if ( cvector_size(hooks)) printf("<debug :: hooks> Running hooks of type %#x\n", flags);
    #endif
    for (el = hooks->first; el != NULL; el = el->next) {
        hookHandle* hook = (hookHandle*) el->object;
        if ( hook->flags == flags ) {
            g++;
            ssize_t (*hookFunc)(sessionHandle*) = hook->func;
            #if RUMBLE_DEBUG & RUMBLE_DEBUG_HOOKS
            printf("<debug :: hooks> Executing hook %p from %s\n", (void*) hookFunc, hook->module);
            #endif
            rc = (*hookFunc)(session);
            if ( rc == RUMBLE_RETURN_FAILURE ) {
                #if RUMBLE_DEBUG & RUMBLE_DEBUG_HOOKS
                printf("<debug :: hooks> Hook %p claimed failure, aborting connection!\n", (void*) hookFunc);
                #endif
                return RUMBLE_RETURN_FAILURE;
            }
            if ( rc == RUMBLE_RETURN_IGNORE ) {
                #if RUMBLE_DEBUG & RUMBLE_DEBUG_HOOKS
                printf("<debug :: hooks> Hook %p took over, skipping to next command.\n", (void*) hookFunc);
                #endif
                return RUMBLE_RETURN_IGNORE;
            }
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
    return RUMBLE_RETURN_OKAY;
}
