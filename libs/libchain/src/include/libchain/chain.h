#ifndef CHAIN_H
#define CHAIN_H

typedef void (task_func_t)(void);

extern task_func_t * volatile curtask;

/** @brief Function called on every reboot
 *  @details This function usually initializes hardware, such as GPIO
 *           direction. The application must define this function.
 */
void init();

/** @brief First task to run when the application starts
 *  @details The application must define this function.
 *
 *  TODO: An alternative would be to have a macro that defines
 *        the curtask symbol and initializes it to the entry task. The
 *        application would be required to have a definition using that macro.
 *        An advantage is that the names of the tasks in the application are
 *        not constrained, and the whole thing is less magical when reading app
 *        code, but slightly more verbose.
 */
void entry_task();

/** @brief Declare the first task of the application
 *  @details This macro defines a function with a special name that is
 *           used to initialize the current task pointer.
 *
 *           This does incur the penalty of an extra task transition, but it
 *           happens only once in application lifetime.
 *
 *           The alternatives are to force the user to define functions
 *           with a special name or to define a task pointer symbol outside
 *           of the library.
 */
#define ENTRY_TASK(task) void _entry_task() { transition_to(task); }

/** @brief Declare the function to be called on each boot
 *  @details The same notes apply as for entry task.
 */
#define INIT_FUNC(func) void _init() { func(); }

/**
 * @brief Transfer control to the given task
 * @details Finalize the current task and jump to the given task.
 *          This function does not return.
 */
static inline void transition_to(task_func_t *next_task)
{
    // reset stack pointer
    // update current task pointer
    // jump to next task

    // NOTE: the order of these does not seem to matter, a reboot
    // at any point in this sequence seems to be harmless.
    //
    // NOTE: It might be a bit cleaner to reset the stack and
    // set the current task pointer in the function *prologue* --
    // would need compiler support for this.

    // TODO: re-use the top-of-stack address used in entry point, instead
    //       of hardcoding the address.
    //
    //       Probably need to write a custom entry point in asm, and
    //       use it instead of the C runtime one.
    __asm__ volatile ( // volatile because output operands unused by C
        "mov #0x2400, r1\n"
        "mov %[nt], %[ct]\n"
        "br %[nt]\n"
        : [ct] "=m" (curtask)
        : [nt] "r" (next_task)
    );

    // Alternative:
    // task-function prologue:
    //     mov pc, curtask 
    //     mov #0x2400, sp
    //
    // transition_to(next_task):
    //     br next_task
}

#endif // CHAIN_H
