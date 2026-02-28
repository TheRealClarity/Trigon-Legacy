//
//  device.c
//  Trigon-Legacy
//

#include "device.h"

static void set_device_family(struct device_info* dev_info) {
    cpu_subtype_t cpuFamily = 0;
    size_t cpuFamilySize = sizeof(cpuFamily);
    sysctlbyname("hw.cpufamily", &cpuFamily, &cpuFamilySize, NULL, 0);
    dev_info->cpu_family = cpuFamily;
}

// thx staturnz
static ret_t set_ios_version(struct device_info* dev_info) {
    char str[64] = {0};
    size_t size = sizeof(str);
    
    if (sysctlbyname("kern.osproductversion", str, &size, 0, 0) != 0 || str[0] == '\0') {
        Class cls = objc_getClass("UIDevice");
        if (cls == NULL) {
            void *handle = dlopen("/System/Library/Frameworks/UIKit.framework/UIKit", RTLD_NOW);
            if (handle == NULL) return RET_ERR;
            cls = objc_getClass("UIDevice");
            dlclose(handle);
            if (cls == NULL) return RET_ERR;
        }
        
        id device = ((id (*)(Class cls, SEL sel))objc_msgSend)(cls, sel_getUid("currentDevice"));
        if (device == NULL) return RET_ERR;
        
        CFStringRef version = ((CFStringRef (*)(id obj, SEL sel))objc_msgSend)(device, sel_getUid("systemVersion"));
        if (version == NULL) return RET_ERR;
        CFStringGetCString(version, str, 64, kCFStringEncodingUTF8);
    }
    unsigned int major = 0, minor = 0, patch = 0;
    sscanf(str, "%u.%u.%u", &major, &minor, &patch);
    dev_info->major = (uint8_t)major;
    dev_info->minor = (uint8_t)minor;
    dev_info->patch = (uint8_t)patch;
    return (dev_info->major <= 6) ? RET_ERR : RET_SUCCESS;
}

static ret_t set_kstruct_offsets(struct device_info* dev_info) {
    // Default values common to iOS 7, 8, 9
    struct kstruct_offsets *offsets = &dev_info->kstruct_offsets;
    offsets->task_prev = 0x38;
    offsets->proc_task = 0x18;
    offsets->proc_pid = 0x10;
    offsets->ipc_port_ip_srights = 0x94;
    offsets->fd_ofiles = 0x0;
    offsets->fp_fglob = 0x8;
    offsets->fg_data = 0x38;
    offsets->pb_buffer = 0x10;

    switch (dev_info->major) {
        case 7:
            offsets->is_table = 0x18;
            offsets->is_task = 0x20;
            offsets->task_itk_self = 0xE0;
            offsets->task_itk_bootstrap = 0x238;
            offsets->task_itk_seatbelt = 0x240;
            offsets->task_itk_space = 0x278;
            offsets->task_bsd_info = 0x2e0;
            offsets->ipc_port_ip_receiver = 0x60;
            offsets->ipc_port_ip_kobject = 0x68;
            offsets->proc_p_fd = 0xF0;
            break;
        case 8:
            offsets->is_table = 0x20;
            offsets->is_task = 0x28;
            offsets->task_itk_self = 0xE8;
            offsets->task_itk_bootstrap = 0x240;
            offsets->task_itk_seatbelt = 0x248;
            offsets->task_itk_space = 0x288;
            offsets->task_bsd_info = 0x2f0;
            offsets->ipc_port_ip_receiver = 0x60;
            offsets->ipc_port_ip_kobject = 0x68;
            offsets->proc_p_fd = 0xF0;
            break;
        case 9:
            offsets->is_table = 0x20;
            offsets->is_task = 0x28;
            offsets->task_itk_self = 0xE8;
            offsets->task_itk_bootstrap = 0x258;
            offsets->task_itk_seatbelt = 0x260;
            offsets->task_itk_space = 0x2a0;
            offsets->task_bsd_info = 0x308;
            offsets->ipc_port_ip_receiver = 0x58;
            offsets->ipc_port_ip_kobject = 0x60;
            
            if (dev_info->minor == 1 || dev_info->minor == 2) {
                offsets->proc_p_fd = 0x108;
            } else if (dev_info->minor == 0) {
                offsets->proc_p_fd = 0xF0;
            } else {
                offsets->proc_p_fd = 0x120; // Default for other iOS 9 versions
            }
            break;
        default:
            ERR("ERROR: failed to init device info, unsupported iOS version");
            return RET_ERR;
    }
    return RET_SUCCESS;
}

static ret_t set_boot_args(struct device_info* dev_info) {
    struct kernel_config *config = &dev_info->kernel_config;
    switch (dev_info->major) {
        case 7:
        case 8:
            config->boot_args_ver = 2;
            config->boot_args_rev = 1;
            break;
        case 9:
            config->boot_args_ver = 2;
            config->boot_args_rev = 2;
            break;
        default:
            ERR("ERROR: failed to init boot args, unsupported iOS version");
            return RET_ERR;
    }
    return RET_SUCCESS;
}

static int is_a8x(struct device_info* dev_info) {
    if (!strncmp(dev_info->model, "iPad5,3", 7) || !strncmp(dev_info->model, "iPad5,4", 7)) {
        return 1;
    }
    return 0;
}

ret_t init_device_info(struct device_info* dev_info) {
    memset(dev_info, 0, sizeof(struct device_info));
    set_device_family(dev_info);
    if (set_ios_version(dev_info) != RET_SUCCESS) {
        ERR("ERROR: unsupported iOS version");
        return RET_ERR;
    }
    LOG("iOS version: %d.%d.%d", dev_info->major, dev_info->minor, dev_info->patch);
    if (set_kstruct_offsets(dev_info) != RET_SUCCESS) return RET_ERR;
    if (set_boot_args(dev_info) != RET_SUCCESS) return RET_ERR;
    
    memset(dev_info->model, 0, sizeof(dev_info->model));
    size_t model_size = sizeof(dev_info->model);
    sysctlbyname("hw.machine", dev_info->model, &model_size, NULL, 0);

    struct kernel_config *config = &dev_info->kernel_config;
    switch (dev_info->cpu_family) {
        case CPUFAMILY_ARM_CYCLONE:
            config->sleep_token_buffer_base = (dev_info->major <= 8) ? 0x83F7FB000 : 0x83F6FB000;
            config->pagesize = 0x1000;
            config->phys_align = config->pagesize;
            break;

        case CPUFAMILY_ARM_TYPHOON:
            if (is_a8x(dev_info)) {
                config->sleep_token_buffer_base = (dev_info->major <= 8) ? 0x87F5FB000 : 0x87F57B000;
                config->phys_align = 0x4000; // real alignment for A8X
                config->pagesize = 0x1000; // a8x lies
            } else {
                config->sleep_token_buffer_base = (dev_info->major <= 8) ? 0x83F5FB000 : 0x83F57B000;
                config->pagesize = 0x1000;
                config->phys_align = config->pagesize;
            }
            break;

        case CPUFAMILY_ARM_TWISTER:
            if (!strncmp(dev_info->model, "iPad6,7", 7) || !strncmp(dev_info->model, "iPad6,8", 7)) {
                config->sleep_token_buffer_base = 0x8FF3F8000; // 4GB A9X
            } else if (!strncmp(dev_info->model, "iPad6,3", 7) || !strncmp(dev_info->model, "iPad6,4", 7)) {
                config->sleep_token_buffer_base = 0x87F3F8000; // 2GB A9X
            } else {
                config->sleep_token_buffer_base = 0x87E778000; // A9
            }
            config->pagesize = 0x4000;
            config->phys_align = config->pagesize;
            break;

        default:
            ERR("ERROR: failed to init device info, unsupported device family: %d", dev_info->cpu_family);
            return RET_ERR;
    }

    struct pte_info *pteinfo = &dev_info->pteinfo;
    if (config->pagesize == 0x4000) {
        pteinfo->offset_bit = 14;
        pteinfo->offset_mask = 0x3fff;
        pteinfo->table_index = 11;
        pteinfo->table_mask = 0x7ff;
    } else {
        pteinfo->offset_bit = 12;
        pteinfo->offset_mask = 0xfff;
        pteinfo->table_index = 9;
        pteinfo->table_mask = 0x1ff;
    }
    dev_info->kernel_state.tfp0 = MACH_PORT_NULL;
    return RET_SUCCESS;
}
