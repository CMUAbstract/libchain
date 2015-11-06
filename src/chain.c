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
    .self_chan_idx = 0,
    .next_ctx = &context_1,
};

__nv context_t * volatile curctx = &context_0;

// for internal instrumentation purposes
__nv volatile unsigned _numBoots = 0;

/**
 * @brief Transfer control to the given task
 * @details Finalize the current task and jump to the given task.
 *          This function does not return.
 *
 *  TODO: mark this function as bare (i.e. no prologue) for efficiency
 */
void transition_to(const task_t *next_task)
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
    next_ctx->self_chan_idx = curctx->self_chan_idx ^ curctx->task->mask;

    next_ctx->next_ctx = curctx;
    curctx = next_ctx;

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
                var = (var_meta_t *)(field +
                        offsetof(SELF_FIELD_TYPE(void_type_t), var) +
                        var_size * self_field->curidx);
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
 *  @param var_size     size of the 'variable' type (var_meta_t + value type)
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
                self_field->curidx ^= 0x1;
                var = (var_meta_t *)(field +
                        offsetof(SELF_FIELD_TYPE(void_type_t), var) +
                        var_size * self_field->curidx);
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

    __asm__ volatile ( // volatile because output operands unused by C
        "br %[nt]\n"
        : /* no outputs */
        : [nt] "r" (curctx->task->func)
    );

    return 0; // TODO: write our own entry point and get rid of this
}
