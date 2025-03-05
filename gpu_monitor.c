#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <pci/pci.h>
#include <nvml.h>
#include <errno.h>
#include <string.h>

#define HOTSPOT_REGISTER_OFFSET 0x0002046C
#define VRAM_REGISTER_OFFSET 0x0000E2A8
#define PG_SZ sysconf(_SC_PAGE_SIZE)
#define MEM_PATH "/dev/mem"

typedef struct {
    nvmlReturn_t result;
    unsigned int device_count;
    int initialized;
    struct pci_access *pacc;
} GpuContext;

typedef struct {
    uint32_t gpu_temp;
    uint32_t junction_temp;
    uint32_t vram_temp;
    char error_message[256];
} GpuTemperatures;

// Function to check if running as root
static int check_root_privileges(void) {
    return geteuid() == 0 ? 0 : -1;
}

// Initialize PCI access
static int init_pci(GpuContext *ctx) {
    ctx->pacc = pci_alloc();
    if (!ctx->pacc) return -1;
    pci_init(ctx->pacc);
    pci_scan_bus(ctx->pacc);
    return 0;
}

// Initialize NVML
static int init_nvml(GpuContext *ctx) {
    ctx->result = nvmlInit();
    if (NVML_SUCCESS != ctx->result) return -1;
    ctx->initialized = 1;
    return 0;
}

// Get device count
static int get_device_count(GpuContext *ctx) {
    ctx->result = nvmlDeviceGetCount(&ctx->device_count);
    if (NVML_SUCCESS != ctx->result || ctx->device_count == 0) return -1;
    return 0;
}

// Read register temperature
static int read_register_temp(struct pci_dev *dev, uint32_t offset, uint32_t *temp) {
    int fd = open(MEM_PATH, O_RDWR | O_SYNC);
    if (fd < 0) return -1;

    uint32_t reg_addr = (dev->base_addr[0] & 0xFFFFFFFF) + offset;
    uint32_t base_offset = reg_addr & ~(PG_SZ-1);
    void *map_base = mmap(0, PG_SZ, PROT_READ, MAP_SHARED, fd, base_offset);

    if (map_base == MAP_FAILED) {
        close(fd);
        return -1;
    }

    uint32_t reg_value = *((uint32_t *)((char *)map_base + (reg_addr - base_offset)));

    if (offset == HOTSPOT_REGISTER_OFFSET) {
        *temp = (reg_value >> 8) & 0xff;
    } else if (offset == VRAM_REGISTER_OFFSET) {
        *temp = (reg_value & 0x00000fff) / 0x20;
    }

    munmap(map_base, PG_SZ);
    close(fd);

    return (*temp < 0x7f) ? 0 : -1;
}

// Exported functions (API)
__attribute__((visibility("default")))
GpuContext* gpu_monitor_init(void) {
    GpuContext *ctx = calloc(1, sizeof(GpuContext));
    if (!ctx) return NULL;

    if (check_root_privileges() < 0 ||
        init_pci(ctx) < 0 ||
        init_nvml(ctx) < 0 ||
        get_device_count(ctx) < 0) {
        free(ctx);
        return NULL;
    }

    return ctx;
}

__attribute__((visibility("default")))
void gpu_monitor_cleanup(GpuContext *ctx) {
    if (!ctx) return;

    if (ctx->initialized) {
        nvmlShutdown();
    }

    if (ctx->pacc) {
        pci_cleanup(ctx->pacc);
    }

    free(ctx);
}

__attribute__((visibility("default")))
int gpu_monitor_get_device_count(GpuContext *ctx) {
    return ctx ? (int)ctx->device_count : -1;
}

__attribute__((visibility("default")))
int gpu_monitor_get_temperatures(GpuContext *ctx, int device_index, GpuTemperatures *temps) {
    if (!ctx || !temps || device_index < 0 || (unsigned int)device_index >= ctx->device_count) {
        return -1;
    }

    nvmlDevice_t device;
    nvmlPciInfo_t pci_info;

    // Get GPU temperature using NVML
    ctx->result = nvmlDeviceGetHandleByIndex((unsigned int)device_index, &device);
    if (NVML_SUCCESS != ctx->result) return -1;

    ctx->result = nvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU, &temps->gpu_temp);
    if (NVML_SUCCESS != ctx->result) return -1;

    ctx->result = nvmlDeviceGetPciInfo(device, &pci_info);
    if (NVML_SUCCESS != ctx->result) return -1;

    // Get junction and VRAM temperatures through PCI
    for (struct pci_dev *dev = ctx->pacc->devices; dev; dev = dev->next) {
        pci_fill_info(dev, PCI_FILL_IDENT | PCI_FILL_BASES);

        if (((unsigned int)(dev->device_id << 16 | dev->vendor_id)) != pci_info.pciDeviceId ||
            (unsigned int)dev->domain != pci_info.domain ||
            dev->bus != pci_info.bus ||
            dev->dev != pci_info.device) {
            continue;
        }

        if (read_register_temp(dev, HOTSPOT_REGISTER_OFFSET, &temps->junction_temp) < 0 ||
            read_register_temp(dev, VRAM_REGISTER_OFFSET, &temps->vram_temp) < 0) {
            return -1;
        }

        return 0;
    }

    return -1;
}