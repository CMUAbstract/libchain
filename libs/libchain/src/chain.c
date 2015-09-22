#include <chain.h>

/* Variable placement in nonvolatile memory; linker puts this in right place */
#define __fram __attribute__((section(".fram_vars")))

__fram task_func_t * volatile curtask = entry_task;

/** @brief Entry point upon reboot */
int main() {
    init();
    transition_to(curtask);
    return 0; // TODO: write our own entry point and get rid of this
}
