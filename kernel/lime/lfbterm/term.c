#include <fs/fs.h>
#include <fs/sysfile.h>
#include <lib/string.h>
#include <locks/sync.h>
#include <lime/string.h>
#include <video/lfbterm.h>
#include <video/color_code.h>

typedef struct lfbtty
{
    int id;
    int pty;
    char *name;
    event_t *ev;
    spinlock_t *lk;
} lfbtty_t;

lfbtty_t tty[NTTY];


int lfb_console_init(void) {
    int err = 0;
    char *tty_name = NULL;
    memset(&tty, 0, sizeof tty);
    for (int id = 0; id < NTTY; ++id) {
        tty[id].id = id;
        err = -ENOMEM;
        if ((tty_name = strcat_num("tty", id, 10)) == NULL)
            goto error;
        if ((err = spinlock_init(NULL, tty_name, &tty[id].lk)))
            goto error;
        if ((err = event_new(tty_name, &tty[id].ev)))
            goto error;
        tty[id].name = tty_name;
    }

    return 0;
error:
    return err;
}