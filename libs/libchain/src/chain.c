#include <chain.h>

/* Variable placement in nonvolatile memory; linker puts this in right place */
#define __fram __attribute__((section(".fram_vars")))

__fram task_func_t * volatile curtask = entry_task;

/** @brief Entry point upon reboot */
int main() {
    init();

    // Resume execution at the last task that started but did not finish

    // TODO: using the raw transtion would be possible once the
    //       prologue discussed in chain.h is implemented (requires compiler
    //       support)
    // transition_to(curtask);

    __asm__ volatile ( // volatile because output operands unused by C
        "br %[nt]\n"
        : /* no outputs */
        : [nt] "r" (curtask)
    );

    return 0; // TODO: write our own entry point and get rid of this
}
