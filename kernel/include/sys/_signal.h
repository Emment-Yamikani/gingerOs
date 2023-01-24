#ifndef _SIGNAL_H
#define _SIGNAL_H

#include <sys/system.h>
#include <locks/cond.h>
#include <lime/assert.h>
#include <locks/spinlock.h>


#define NSIG 32

/* Signal numbers */
#define	SIGHUP	1	/* hangup */
#define	SIGINT	2	/* interrupt */
#define	SIGQUIT	3	/* quit */
#define	SIGILL	4	/* illegal instruction (not reset when caught) */
#define	SIGTRAP	5	/* trace trap (not reset when caught) */
#define	SIGIOT	6	/* IOT instruction */
#define	SIGABRT 6	/* used by abort, replace SIGIOT in the future */
#define	SIGEMT	7	/* EMT instruction */
#define	SIGFPE	8	/* floating point exception */
#define	SIGKILL	9	/* kill (cannot be caught or ignored) */
#define	SIGBUS	10	/* bus error */
#define	SIGSEGV	11	/* segmentation violation */
#define	SIGSYS	12	/* bad argument to system call */
#define	SIGPIPE	13	/* write on a pipe with no one to read it */
#define	SIGALRM	14	/* alarm clock */
#define	SIGTERM	15	/* software termination signal from kill */
#define	SIGURG	16	/* urgent condition on IO channel */
#define	SIGSTOP	17	/* sendable stop signal not from tty */
#define	SIGTSTP	18	/* stop signal from tty */
#define	SIGCONT	19	/* continue a stopped process */
#define	SIGCHLD	20	/* to parent on child stop or exit */
#define	SIGCLD	20	/* System V name for SIGCHLD */
#define	SIGTTIN	21	/* to readers pgrp upon background tty read */
#define	SIGTTOU	22	/* like TTIN for output if (tp->t_local&LTOSTOP) */
#define	SIGIO	23	/* input/output possible signal */
#define	SIGPOLL	SIGIO	/* System V name for SIGIO */
#define	SIGWINCH 24	/* window changed */
#define	SIGUSR1 25	/* user defined signal 1 */
#define	SIGUSR2 26	/* user defined signal 2 */

#define SIGACT_ABORT        1
#define SIGACT_TERMINATE    2
#define SIGACT_IGNORE       3
#define SIGACT_STOP         4
#define SIGACT_CONTINUE     5

/* libc bindings */
#define SIG_DFL ((uintptr_t) 0)  /* Default action */
#define SIG_IGN ((uintptr_t) 1)  /* Ignore action */
#define SIG_ERR ((uintptr_t) -1) /* Error return */

typedef unsigned long sigset_t;
typedef struct sigaction {
    uintptr_t sa_handler;
    sigset_t  sa_mask;
    int       sa_flags;
} sigaction_t;

typedef struct signals
{
    atomic_t nsig_handled;
    sigaction_t sig_action[32];
    cond_t *sig_wait;
    queue_t *sig_queue;
    spinlock_t *sig_lock;
} *SIGNAL;

#define signals_assert(s) assert(s, "no signals");

#define signals_assert_lock(s) {signals_assert(s); spin_assert_lock(s->sig_lock);}

#define signals_lock(s) {signals_assert(s); spin_lock(s->sig_lock);}

#define signals_unlock(s) {signals_assert(s); spin_unlock(s->sig_lock);}

#define signals_lock_queue(s) {signals_assert_lock(s); queue_lock(s->sig_queue);}

#define signals_unlock_queue(s) {signals_assert_lock(s); queue_unlock(s->sig_queue);}

struct proc;
struct pgroup;

#define proc_signals(p) (p->signals) 


extern int sig_default_action[];

void signals_free(SIGNAL);
int signals_alloc(char *, SIGNAL *);
int signal_send(int, int);
int signal_proc_send(struct proc *, int);
int signal_pgrp_send(struct pgroup *pg, int signal, ssize_t *count);

void (*signal(int sig, void (*handler)(int)))(int);
int kill(pid_t pid, int sig);
int pause(void);

int has_pending_signal();

static inline int signal_isvalid(int sig)
{
    return !((sig < 1) || (sig >= NSIG));
}

int handle_signals(trapframe_t *ustack);
int signals_pending(struct proc *);
int signals_next(struct proc *p);

int signals_cancel(struct proc *);
void (*signals_get_handler(SIGNAL, int))(int);

#endif /* ! _SIGNAL_H */
