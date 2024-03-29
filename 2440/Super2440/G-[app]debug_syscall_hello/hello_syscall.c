

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
//#include <sysdep.h>

//#if defined(__thumb__) || defined(__ARM_EABI__)
//#define __NR_SYSCALL_BASE	0
//#else
#define __NR_SYSCALL_BASE	0x900000
//#endif

void hello(char *buf, int count)
{
    /* swi */    
    asm ("mov r0, %0\n"   /* save the argment in r0 */
         "mov r1, %1\n"   /* save the argment in r1 */
         "swi %2\n"   /* do the system call */
         :
         : "r"(buf), "r"(count), "i" (__NR_SYSCALL_BASE + 370)
         : "r0", "r1");
}

int main(int argc, char **argv)
{
    printf("Charles.Y[app]: call hello\n");
    hello("ARM LINUX", 9);

    return 0;
}


