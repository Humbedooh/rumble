#include "rumble.h"
#include "private.h"

#ifndef RUMBLE_MSC
#include <pwd.h>
#include <grp.h>
#endif

void rumble_setup_runas(masterHandle* master) {
#ifndef RUMBLE_MSC
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    struct passwd   *runAsUserEntry;
    struct group    *runAsGroupEntry;
    __uid_t         runAsUID = 999999;
    __uid_t         runAsGUID = 999999;
    const char      *runAsUser, *runAsGroup;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    runAsUser = rhdict(master->_core.conf, "runas") ? rrdict(master->_core.conf, "runas") : "";
    runAsGroup = rhdict(master->_core.conf, "runasgroup") ? rrdict(master->_core.conf, "runasgroup") : "";
    if (strlen(runAsUser)) {
        
        if (!strcmp(runAsUser, "root")) runAsUID = 0;
        else {
            runAsUserEntry = getpwnam(runAsUser);
            if (runAsUserEntry && runAsUserEntry->pw_uid) {
                runAsUID = runAsUserEntry->pw_uid;
            }
        }
        
        if (runAsUID != 999999) {
            rumble_debug(NULL, "core", "Running as user: %s", runAsUser);
            
            if (setresuid(runAsUID,runAsUID,runAsUID)) {
                rumble_debug(NULL, "core", "Error: Could not set process UID to %u!", runAsUserEntry->pw_uid);
                exit(EXIT_FAILURE);
            }
        } else {
            rumble_debug(NULL, "core", "I couldn't find the user to run as: %s", runAsUser);
            exit(EXIT_FAILURE);
        }
        
        if (!strcmp(runAsGroup, "root")) runAsGUID = 0;
        
    } else rumble_debug(NULL, "core", "no run-as directive set, running as root(?)");
#endif
}