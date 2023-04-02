#include <ginger.h>

void *writer(void *);
cv_t *cv = __cv_init();
mutex_t *mtx = __mutex_init();

int main(int argc, char *const argv[])
{
    tid_t tid = 0;
    mutex_init(mtx);
    cv_init(cv);
    mutex_lock(mtx);
    thread_create(&tid, writer, NULL);
    mutex_unlock(mtx);
    
    cv_wait(cv);
    return 0;
}

void *writer(void *arg)
{
    mutex_lock(mtx);
    mutex_unlock(mtx);
    cv_broadcast(cv);
    return NULL;
}