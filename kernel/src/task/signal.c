#include <signal.h>

bool signal_fire(pid_t pid, bool group, int sig) { return 0; }
int signal_wait(pid_t pid, const sigset_t *set, int *sig) { return 0; }
