#ifndef KERN_RW_H
#define KERN_RW_H

#include "common.h"
#include "device.h"
#include "memory.h"

size_t kwrite(uint64_t where, const void *p, size_t size);
void wk32(uint64_t where, uint32_t what);
void wk64(uint64_t where, uint64_t what);
size_t kread(uint64_t where, void *p, size_t size);
uint32_t rk32(uint64_t where);
uint64_t rk64(uint64_t where);

ret_t exploit_pipe_init(struct pipe_exploit_state *state, uint64_t self_proc);
void cleanup_pipe_state(void);
uint64_t pipe_read_64(uint64_t buffer_value);
uint64_t pipe_read_32(uint64_t buffer_value);
uint64_t pipe_write_64(uint64_t buffer_value, uint64_t value);
uint64_t pipe_write_32(uint64_t buffer_value, uint32_t value);

#endif