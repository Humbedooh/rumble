#include "rumble.h"
#include "private.h"
#include <signal.h>
#ifdef RUMBLE_MSC
static void signal_windows(int sig) {
    rumble_debug(NULL, "core", "Caught signal %d from system!\n", sig);
    if (sig == SIGBREAK || sig == SIGINT) {
        printf("User stopped the program.\n");
    }

    exit(SIGINT);
}

#else
#   include <execinfo.h>
#   include <errno.h>
#   include <ucontext.h>
#   include <unistd.h>
#   include <limits.h>
#   include <sys/types.h>

/*
 -----------------------------------------------------------------------------------------------------------------------
    This structure mirrors the one found in /usr/include/asm/ucontext.h
 -----------------------------------------------------------------------------------------------------------------------
 */
typedef struct _sig_ucontext
{
    unsigned long       uc_flags;
    struct ucontext     *uc_link;
    stack_t             uc_stack;
    struct sigcontext   uc_mcontext;
    sigset_t            uc_sigmask;
} sig_ucontext_t;
static void         signal_handler(int sig, siginfo_t *info, void *ucontext);
void                init_signals(void);
struct sigaction    sigact;
uint32_t            lastClick = 0;
int                 alreadyDead = 0;

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void signal_handler(int sig, siginfo_t *info, void *ucontext) {
    if (sig == SIGQUIT || sig == SIGHUP) {
        printf("User ended the program - bye bye!\r\n");
        cleanup();
    } else if (sig == SIGPIPE) {
        printf("Client disconnected\n");
    } else if (sig == SIGKILL) {
        printf("Rumble got killed :(\r\n");
        cleanup();
    } else if (sig == SIGTERM) {
        printf("Rumble got killed :(\r\n");
        cleanup();
    } else if (sig == SIGINT) {
        if (time(0) - lastClick < 2) {
            cleanup();
            exit(0);
        }

        printf("Ctrl+C detected. Press it again to exit rumble.\r\n");
        lastClick = time(0);
    } else {
        if (!alreadyDead) {

            /*~~~~~~~~~~~~~~~~~~~*/
            void        *array[50];
            char        **messages;
            int         size,
                        i;
            ucontext_t  *context;
            /*~~~~~~~~~~~~~~~~~~~*/

            alreadyDead++;
            context = (ucontext_t *) ucontext;
            rumble_debug(NULL, "debug", "Caught signal %d (%s), address is %p\n", sig, strsignal(sig), info->si_addr);
            rumble_debug(NULL, "debug", "PID=%d \n", getpid());
            rumble_debug(NULL, "debug", "signo=%d/%s\n", sig, strsignal(sig));
            rumble_debug(NULL, "debug", "code=%d (not always applicable)\n", info->si_code);
            rumble_debug(NULL, "debug", "\nContext: 0x%08lx\n", (unsigned long) ucontext);
            rumble_debug(NULL, "debug", "Register stuff:\n    gs: 0x%08x   fs: 0x%08x   es: 0x%08x   ds: 0x%08x\n"
                         "   edi: 0x%08x  esi: 0x%08x  ebp: 0x%08x  esp: 0x%08x\n""   ebx: 0x%08x  edx: 0x%08x  ecx: 0x%08x  eax: 0x%08x\n"
                     "  trap:   %8u  err: 0x%08x  cs: 0x%08x\n", context->uc_mcontext.gregs[23], context->uc_mcontext.gregs[22],
                         context->uc_mcontext.gregs[24], context->uc_mcontext.gregs[25], context->uc_mcontext.gregs[7],
                         context->uc_mcontext.gregs[6], context->uc_mcontext.gregs[5], context->uc_mcontext.gregs[4],
                         context->uc_mcontext.gregs[3], context->uc_mcontext.gregs[2], context->uc_mcontext.gregs[1],
                         context->uc_mcontext.gregs[0], context->uc_mcontext.gregs[15], context->uc_mcontext.gregs[16],
                         context->uc_mcontext.gregs[18]);
            size = backtrace(array, 50);
            messages = backtrace_symbols(array, size);

            /* skip first stack frame (points here) */
            for (i = 1; i < size && messages != NULL; ++i) {
                rumble_debug(NULL, "debug", "[backtrace]: (%d) %s\n", i, messages[i]);
            }

            cleanup();
        } else exit(0);
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void init_signals(void) {
    sigact.sa_sigaction = signal_handler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = SA_RESTART | SA_SIGINFO;
    sigaction(SIGKILL, &sigact, 0);
    sigaction(SIGINT, &sigact, 0);
    sigaction(SIGSEGV, &sigact, 0);
    sigaction(SIGSTKFLT, &sigact, 0);
    sigaction(SIGHUP, &sigact, 0);
    sigaction(SIGQUIT, &sigact, 0);
    sigaction(SIGPIPE, &sigact, 0);
    sigaction(SIGKILL, &sigact, 0);
}

#endif


void attach_debug() {
    #ifndef RUMBLE_MSC
    init_signals();
#else
    signal(SIGINT, &signal_windows);
    signal(SIGBREAK, &signal_windows);
    signal(SIGSEGV, &signal_windows);
    signal(SIGTERM, &signal_windows);
    signal(SIGABRT, &signal_windows);
    signal(SIGILL, &signal_windows);
    atexit(&cleanup);
#endif
}