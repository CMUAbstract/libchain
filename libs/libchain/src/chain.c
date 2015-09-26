#include <chain.h>

#include <stdarg.h>

/** @brief Entry task function prototype
 *  @details We rely on the special name of this symbol to initialize the
 *           current task pointer. The entry function is defined in the user
 *           application through a macro provided by our header.
 */
void _entry_task();

/** @brief Init function prototype
 *  @details Same notes apply as for entry task function.
 */
void _init();

__fram task_func_t * volatile curtask = _entry_task;

__fram chain_time_t volatile curtime = 0;

/* To update the context, fill-in the unused one and flip the pointer to it */
__fram context_t context_1 = {0};
__fram context_t context_0 = {
    .task = _entry_task,
    .time = 0,
    .next_ctx = &context_1,
};

__fram context_t * volatile curctx = &context_0;

// for internal instrumentation purposes
__fram volatile unsigned _numBoots = 0;

/**
 * @brief Transfer control to the given task
 * @details Finalize the current task and jump to the given task.
 *          This function does not return.
 *
 *  TODO: mark this function as bare (i.e. no prologue) for efficiency
 */
void transition_to(task_func_t *next_task)
{
    context_t *next_ctx; // this should be in a register for efficiency
                         // (if we really care, write this func in asm)

    // reset stack pointer
    // update current task pointer
    // tick logical time
    // jump to next task

    // NOTE: the order of these does not seem to matter, a reboot
    // at any point in this sequence seems to be harmless.
    //
    // NOTE: It might be a bit cleaner to reset the stack and
    // set the current task pointer in the function *prologue* --
    // would need compiler support for this.
    //
    // NOTE: It is harmless to increment the time even if we fail before
    // transitioning to the next task. The reverse, i.e. failure to increment
    // time while having transitioned to the task, would break the
    // semantics of CHAN_IN (aka. sync), which should get the most recently
    // updated value.
    //
    // NOTE: Storing two pointers (one to next and one to current context)
    // does not seem acceptable, because the two would need to be kept
    // consistent in the face of intermittence. But, could keep one pointer
    // to current context and a pointer to next context inside the context
    // structure. The only reason to do that is if it is more efficient --
    // i.e. avoids XORing the index and getting the actual pointer.

    // TODO: handle overflow of timestamp. Some very raw ideas:
    //          * limit the age of values
    //          * a maintainance task that fixes up stored timestamps
    //          * extra bit to mark timestamps as pre/post overflow

    // TODO: re-use the top-of-stack address used in entry point, instead
    //       of hardcoding the address.
    //
    //       Probably need to write a custom entry point in asm, and
    //       use it instead of the C runtime one.

    next_ctx = curctx->next_ctx;
    next_ctx->task = next_task;
    next_ctx->time = curctx->time + 1;

    curctx->next_ctx = curctx;
    curctx = next_ctx;

    __asm__ volatile ( // volatile because output operands unused by C
        "mov #0x2400, r1\n"
        "br %[ntask]\n"
        :
        : [ntask] "r" (next_task)
    );

    // Alternative:
    // task-function prologue:
    //     mov pc, curtask 
    //     mov #0x2400, sp
    //
    // transition_to(next_task):
    //     br next_task
}

void *chan_in(int count, ...)
{
    va_list ap;
    unsigned i;
    unsigned latest_update;
    chan_field_meta_t *field;
    void *latest_field;

    va_start(ap, count);

    latest_update = 0;
    latest_field = NULL;
    for (i = 0; i < count; ++i) {
        field = va_arg(ap, __typeof__(field));

        if (field->timestamp > latest_update) {
            latest_update = field->timestamp;
            latest_field = field;
        }
    }

    return latest_field;
}

/** @brief Entry point upon reboot */
int main() {
    _init();

    _numBoots++;

    // Resume execution at the last task that started but did not finish

    // TODO: using the raw transtion would be possible once the
    //       prologue discussed in chain.h is implemented (requires compiler
    //       support)
    // transition_to(curtask);

    __asm__ volatile ( // volatile because output operands unused by C
        "br %[nt]\n"
        : /* no outputs */
        : [nt] "r" (curctx->task)
    );

    return 0; // TODO: write our own entry point and get rid of this
}
