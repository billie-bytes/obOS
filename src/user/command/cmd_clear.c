#include "user/command/syscall.h"

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    sys_clear();
    return 0;
}
