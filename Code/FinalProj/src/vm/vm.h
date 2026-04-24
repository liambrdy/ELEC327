#ifndef _VM_H
#define _VM_H

#include "common.h"
#include "codegen/rom.h"

// ---- Sizing constants ----

#define VM_STACK_SIZE   1024   // u32 slots on the operand stack
#define VM_MAX_FRAMES    256   // maximum call depth
#define VM_MEMORY_SIZE  65536  // bytes of flat memory (globals live at ROM_GLOBAL_OFFSET)

// ---- Call-frame saved state ----
// Pushed by CALL, popped by RET.

typedef struct vm_call_frame_t {
    u32 return_ip;    // ip to resume after RET
    u32 frame_base;   // caller's frame_base to restore
} vm_call_frame_t;

// ---- VM execution result ----

typedef enum vm_result_t {
    VM_OK = 0,
    VM_ERR_STACK_OVERFLOW,
    VM_ERR_STACK_UNDERFLOW,
    VM_ERR_CALL_OVERFLOW,
    VM_ERR_CALL_UNDERFLOW,
    VM_ERR_BAD_OPCODE,
    VM_ERR_DIV_ZERO,
    VM_ERR_BAD_MEMORY,
    VM_ERR_NO_HALT,
} vm_result_t;

// ---- VM state ----

typedef struct vm_t {
    // Execution
    u32 ip;           // instruction pointer (byte offset into code[])
    bool halted;

    // Operand stack — grows upward; sp is the index of the next free slot.
    u32 stack[VM_STACK_SIZE];
    u32 sp;

    // Frame base — index into stack[] where slot 0 of the current frame lives.
    u32 frame_base;

    // Call-frame stack
    vm_call_frame_t call_frames[VM_MAX_FRAMES];
    u32 call_depth;

    // Code segment (points into the loaded ROM buffer, not owned)
    u8 *code;
    u32 code_size;

    // Flat memory: address 0 .. VM_MEMORY_SIZE-1.
    // Globals are mapped starting at ROM_GLOBAL_OFFSET.
    u8 memory[VM_MEMORY_SIZE];

    // Exit code: value left on the stack when the outermost RET fires.
    i32 exit_code;

    // Optional host syscall handler (NULL = syscalls are no-ops).
    // Called by OPCODE_SYSCALL with the 1-byte syscall id.
    // Args are at vm->stack[vm->frame_base + 0..n-1].
    // Push a return value onto vm->stack before returning for non-void functions.
    void (*syscall_handler)(struct vm_t *vm, u8 id);
} vm_t;

// ---- API ----

void        VmInit(vm_t *vm, u8 *code, u32 code_size, u32 entry_point);
vm_result_t VmRun(vm_t *vm);
const char *VmResultStr(vm_result_t result);

// Load a ROM file, validate it, and call VmInit.
// Returns a malloc'd buffer containing the ROM (caller must free after VmRun).
// Returns NULL and prints a message on any error.
u8 *VmLoadRom(vm_t *vm, const char *path);

#endif
