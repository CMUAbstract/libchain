#include <chain.h>

/* Variable placement in nonvolatile memory; linker puts this in right place */
#define __fram __attribute__((section(".fram_vars")))

__fram task_func_t * volatile curtask;
