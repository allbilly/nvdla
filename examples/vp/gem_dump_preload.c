#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#define DRM_IOCTL_NVDLA_SUBMIT 0xC0106440UL
#define DRM_IOCTL_NVDLA_GEM_CREATE 0xC0106441UL
#define DRM_IOCTL_NVDLA_GEM_MMAP 0xC0106442UL
#define DRM_IOCTL_PRIME_HANDLE_TO_FD 0xC00C642DUL

struct gem_info {
    uint32_t handle;
    int prime_fd;
    uint64_t size;
    uint64_t map_offset;
    void *vaddr;
};

static struct gem_info gems[128];
static int gem_count;
static int drm_fd = -1;
static int dumped;

static int (*real_ioctl)(int, unsigned long, void *);
static void *(*real_mmap)(void *, size_t, int, int, int, off_t);

static struct gem_info *gem_by_handle(uint32_t handle)
{
    for (int i = 0; i < gem_count; i++)
        if (gems[i].handle == handle)
            return &gems[i];
    if (gem_count >= (int)(sizeof(gems) / sizeof(gems[0])))
        return NULL;
    gems[gem_count].handle = handle;
    gems[gem_count].prime_fd = -1;
    return &gems[gem_count++];
}

static struct gem_info *gem_by_offset(uint64_t offset)
{
    uint64_t low = offset & 0xffffffffULL;
    for (int i = 0; i < gem_count; i++)
        if (gems[i].map_offset == low)
            return &gems[i];
    return NULL;
}

static void dump_gems(void)
{
    if (dumped)
        return;
    dumped = 1;
    FILE *log = fopen("/mnt/vp/gem_dump_preload.log", "w");
    if (!log)
        log = stderr;
    for (int i = 0; i < gem_count; i++) {
        struct gem_info *g = &gems[i];
        size_t size = g->size > 8192 ? 8192 : (size_t)g->size;
        fprintf(log, "GEM_BUFFER handle=%u prime_fd=%d size=%llu dumped=%zu vaddr=%p map_offset=0x%llx data=",
                g->handle, g->prime_fd, (unsigned long long)g->size, size,
                g->vaddr, (unsigned long long)g->map_offset);
        if (g->vaddr) {
            const unsigned char *p = (const unsigned char *)g->vaddr;
            for (size_t j = 0; j < size; j++)
                fprintf(log, "%02x", p[j]);
        }
        fprintf(log, "\n");
    }
    if (log != stderr)
        fclose(log);
}

int ioctl(int fd, unsigned long req, ...)
{
    va_list ap;
    void *arg;
    int ret;
    if (!real_ioctl)
        real_ioctl = dlsym(RTLD_NEXT, "ioctl");
    va_start(ap, req);
    arg = va_arg(ap, void *);
    va_end(ap);

    ret = real_ioctl(fd, req, arg);
    if (ret == 0 && req == DRM_IOCTL_NVDLA_GEM_CREATE && arg) {
        uint32_t *p = arg;
        uint64_t *size = (uint64_t *)((char *)arg + 8);
        struct gem_info *g = gem_by_handle(p[0]);
        if (g) {
            drm_fd = fd;
            g->size = *size;
        }
    } else if (ret == 0 && req == DRM_IOCTL_PRIME_HANDLE_TO_FD && arg) {
        uint32_t *p = arg;
        int *prime_fd = (int *)((char *)arg + 8);
        struct gem_info *g = gem_by_handle(p[0]);
        if (g)
            g->prime_fd = *prime_fd;
    } else if (ret == 0 && req == DRM_IOCTL_NVDLA_GEM_MMAP && arg) {
        uint32_t *p = arg;
        uint64_t *off = (uint64_t *)((char *)arg + 8);
        struct gem_info *g = gem_by_handle(p[0]);
        if (g)
            g->map_offset = *off;
    } else if (req == DRM_IOCTL_NVDLA_SUBMIT) {
        dump_gems();
    }
    return ret;
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    void *ret;
    if (!real_mmap)
        real_mmap = dlsym(RTLD_NEXT, "mmap");
    ret = real_mmap(addr, length, prot, flags, fd, offset);
    if (ret != MAP_FAILED && fd == drm_fd) {
        struct gem_info *g = gem_by_offset((uint64_t)offset);
        if (g)
            g->vaddr = ret;
    }
    return ret;
}
