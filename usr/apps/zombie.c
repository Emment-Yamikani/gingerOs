// Create a zombie process that 
// must be reparented at exit.

#include <ginger.h>

int
main(void)
{
  close(0);
  close(1);
  close(2);
  
  if(fork() == 0)
    sleep(5);  // Let child exit before parent.
  exit(0);
}