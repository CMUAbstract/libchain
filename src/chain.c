#include <stdarg.h>
#include <string.h>

#ifndef LIBCHAIN_ENABLE_DIAGNOSTICS
#define LIBCHAIN_PRINTF(...)
#else
#include <stdio.h>
#define LIBCHAIN_PRINTF printf
#endif

#include "chain.h"

/* Dummy types for offset calculations */
struct _void_type_t {
    void * x;
};
typedef struct _void_type_t void_type_t;

__nv chain_time_t volatile curtime = 0;

/* To update the context, fill-in the unused one and flip the pointer to it */
__nv context_t context_1 = {0};
__nv context_t context_0 = {
    .task = TASK_REF(_entry_task),
    .time = 0,
    .next_ctx = &context_1,
};

__nv context_t * volatile curctx = &context_0;

// for internal instrumentation purposes
__nv volatile unsigned _numBoots = 0;

/**
 * @brief Function to be invoked at the beginning of every task
 */
void task_prologue()
{
    task_t *curtask = curctx->task;

    // Swaps of the self-channel buffer happen on transitions, not restarts.
    // We detect transitions by comparing the current time with a timestamp.
    if (curctx->time != curtask->last_execute_time) {

        // Minimize FRAM reads
        self_field_meta_t **dirty_self_fields = curtask->dirty_self_fields;

        int i;

        // It is safe to repeat the loop for the same element, because the swap
        // operation clears the dirty bit. We only need to be a little bit careful
        // to decrement the count strictly after the swap.
        while ((i = curtask->num_dirty_self_fields) > 0) {
            self_field_meta_t *self_field = dirty_self_fields[--i];

            if (self_field->idx_pair & SELF_CHAN_IDX_BIT_DIRTY_CURRENT) {
                // Atomically: swap AND clear the dirty bit (by "moving" it over to MSB)
                __asm__ volatile (
                    "SWPB %[idx_pair]\n"
                    : [idx_pair]  "=m" (self_field->idx_pair)
                );
            }

            // Trade-off: either we do one FRAM write after each element, or
            // we do only one write at the end (set to 0) but also not make
            // forward progress if we reboot in the middle of this loop.
            // We opt for making progress.
            curtask->num_dirty_self_fields = i;
        }

        curtask->last_execute_time = curctx->time;
    } else {
        // In this case, swapping that needed to take place after the last
        // transition has run to completion (even if it was restarted) [because
        // the last_execute_time was set]. We get into this clause only
        // because of a restart. We must clear any state that the incomplete
        // execution of the task might have changed.
        curtask->num_dirty_self_fields = 0;
    }
}

/**
 * @brief Transfer control to the given task
 * @details Finalize the current task and jump to the given task.
 *          This function does not return.
 *
 *  TODO: mark this function as bare (i.e. no prologue) for efficiency
 */
void transition_to(task_t *next_task)
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

    next_ctx->next_ctx = curctx;
    curctx = next_ctx;

    task_prologue();

    __asm__ volatile ( // volatile because output operands unused by C
        "mov #0x2400, r1\n"
        "br %[ntask]\n"
        :
        : [ntask] "r" (next_task->func)
    );

    // Alternative:
    // task-function prologue:
    //     mov pc, curtask 
    //     mov #0x2400, sp
    //
    // transition_to(next_task->func):
    //     br next_task
}

/** @brief Sync: return the most recently updated value of a given field
 *  @param field_name   string name of the field, used for diagnostics
 *  @param var_size     size of the 'variable' type (var_meta_t + value type)
 *  @param count        number of channels to sync
 *  @param ...          channel ptr, field offset in corresponding message type
 *  @return Pointer to the value, ready to be cast to final value type pointer
 */
void *chan_in(const char *field_name, size_t var_size, int count, ...)
{
    va_list ap;
    unsigned i;
    unsigned latest_update = 0;
#ifdef LIBCHAIN_ENABLE_DIAGNOSTICS
    unsigned latest_chan_idx = 0;
    char curidx;
#endif

    var_meta_t *var;
    var_meta_t *latest_var = NULL;

    LIBCHAIN_PRINTF("[%u] %s: in: '%s':", curctx->time,
                    curctx->task->name, field_name);

    va_start(ap, count);

    for (i = 0; i < count; ++i) {
        uint8_t *chan = va_arg(ap, uint8_t *);
        size_t field_offset = va_arg(ap, unsigned);

        uint8_t *chan_data = chan + offsetof(CH_TYPE(_sa, _da, _void_type_t), data);
        chan_meta_t *chan_meta = (chan_meta_t *)(chan +
                                    offsetof(CH_TYPE(_sb, _db, _void_type_t), meta));
        uint8_t *field = chan_data + field_offset;

        switch (chan_meta->type) {
            case CHAN_TYPE_SELF: {
                self_field_meta_t *self_field = (self_field_meta_t *)field;

                unsigned var_offset =
                    (self_field->idx_pair & SELF_CHAN_IDX_BIT_CURRENT) ? var_size : 0;

                var = (var_meta_t *)(field +
                        offsetof(SELF_FIELD_TYPE(void_type_t), var) + var_offset);

#ifdef LIBCHAIN_ENABLE_DIAGNOSTICS
                curidx = '0' + self_field->curidx;
#endif
                break;
            }
            default:
                var = (var_meta_t *)(field +
                        offsetof(FIELD_TYPE(void_type_t), var));
#ifdef LIBCHAIN_ENABLE_DIAGNOSTICS
                curidx = ' ';
                break;
#endif
        }

        LIBCHAIN_PRINTF(" {%u} %s->%s:%c c%04x:off%u:v%04x [%u],", i,
               chan_meta->diag.source_name, chan_meta->diag.dest_name,
               curidx, (uint16_t)chan, field_offset,
               (uint16_t)var, var->timestamp);

        if (var->timestamp > latest_update) {
            latest_update = var->timestamp;
            latest_var = var;
#ifdef LIBCHAIN_ENABLE_DIAGNOSTICS
            latest_chan_idx = i;
#endif
        }
    }
    va_end(ap);

    LIBCHAIN_PRINTF(": {latest %u}: ", latest_chan_idx);

    uint8_t *value = (uint8_t *)latest_var + offsetof(VAR_TYPE(void_type_t), value);

#ifdef LIBCHAIN_ENABLE_DIAGNOSTICS
    for (int i = 0; i < var_size - sizeof(var_meta_t); ++i)
        LIBCHAIN_PRINTF("%02x ", value[i]);
    LIBCHAIN_PRINTF("\r\n");
#endif

    // TODO: No two timestamps compared above can be equal.
    //       How can two different sources (i.e. different tasks) write to
    //       this destination at the same logical time? Pretty sure, this is
    //       impossible.
    // ASSERT(latest_field != NULL);

    return (void *)value;
}

/** @brief Write a value to a field in a channel
 *  @param field_name    string name of the field, used for diagnostics
 *  @param value         pointer to value data
 *  @param var_size      size of the 'variable' type (var_meta_t + value type)
 *  @param count         number of output channels
 *  @param ...           channel ptr, field offset in corresponding message type
 */
void chan_out(const char *field_name, const void *value,
              size_t var_size, int count, ...)
{
    va_list ap;
    int i;
    var_meta_t *var;
#ifdef LIBCHAIN_ENABLE_DIAGNOSTICS
    char curidx;
#endif

    va_start(ap, count);

    for (i = 0; i < count; ++i) {
        uint8_t *chan = va_arg(ap, uint8_t *);
        size_t field_offset = va_arg(ap, unsigned);

        uint8_t *chan_data = chan + offsetof(CH_TYPE(_sa, _da, _void_type_t), data);
        chan_meta_t *chan_meta = (chan_meta_t *)(chan +
                                    offsetof(CH_TYPE(_sb, _db, _void_type_t), meta));
        uint8_t *field = chan_data + field_offset;

        switch (chan_meta->type) {
            case CHAN_TYPE_SELF: {
                self_field_meta_t *self_field = (self_field_meta_t *)field;
                task_t *curtask = curctx->task;

                unsigned var_offset =
                    (self_field->idx_pair & SELF_CHAN_IDX_BIT_NEXT) ? var_size : 0;

                var = (var_meta_t *)(field +
                        offsetof(SELF_FIELD_TYPE(void_type_t), var) + var_offset);

                // "Enqueue" the buffer index to be flipped on next transition:
                //   (1) initialize the dirty bit for next swap, or, in other words,
                //       "finalize" clearing of the dirty bit from the previous
                //       swap, since the swap "clears" the dirty bit by moving
                //       it over from LSB to MSB.
                //   (2) mark the index dirty, which enques the swap
                //   (3) add the field to the list of dirty fields
                //
                // NOTE: these do not have to be atomic, and can be repeated any
                // number of times (idempotent). Counter of the dirty list is
                // reset on in task prologue.
                self_field->idx_pair &= ~(SELF_CHAN_IDX_BIT_DIRTY_NEXT);
                self_field->idx_pair |= SELF_CHAN_IDX_BIT_DIRTY_CURRENT;
                curtask->dirty_self_fields[curtask->num_dirty_self_fields++] = self_field;

#ifdef LIBCHAIN_ENABLE_DIAGNOSTICS
                curidx = '0' + next_self_chan_field_idx;
#endif
                break;
            }
            default:
                var = (var_meta_t *)(field +
                        offsetof(FIELD_TYPE(void_type_t), var));
#ifdef LIBCHAIN_ENABLE_DIAGNOSTICS
                curidx = ' ';
                break;
#endif
        }

#ifdef LIBCHAIN_ENABLE_DIAGNOSTICS
        LIBCHAIN_PRINTF("[%u] %s: out: '%s': %s -> %s:%c c%04x:off%u:v%04x: ",
               curctx->time, curctx->task->name, field_name,
               chan_meta->diag.source_name, chan_meta->diag.dest_name,
               curidx, (uint16_t)chan, field_offset, (uint16_t)var);

        for (int i = 0; i < var_size - sizeof(var_meta_t); ++i)
            LIBCHAIN_PRINTF("%02x ", *((uint8_t *)value + i));
        LIBCHAIN_PRINTF("\r\n");
#endif

        var->timestamp = curctx->time;
        void *var_value = (uint8_t *)var + offsetof(VAR_TYPE(void_type_t), value);
        memcpy(var_value, value, var_size - sizeof(var_meta_t));
    }

    va_end(ap);
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

    task_prologue();

    __asm__ volatile ( // volatile because output operands unused by C
        "br %[nt]\n"
        : /* no outputs */
        : [nt] "r" (curctx->task->func)
    );

    return 0; // TODO: write our own entry point and get rid of this
}
