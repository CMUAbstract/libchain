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

/** @brief Pointer to the most recently started but not finished task */
__fram task_func_t * volatile curtask = _entry_task;

/** @brief Logical time, ticks at task boundaries */
__fram chain_time_t volatile curtime = 0;

// for internal instrumentation purposes
__fram volatile unsigned _numBoots = 0;

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
        : [nt] "r" (curtask)
    );

    return 0; // TODO: write our own entry point and get rid of this
}
