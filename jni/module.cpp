#include "zygisk.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cstdio>
#include <string>
#include <unordered_set>
#include <sys/stat.h>
#include <android/log.h>

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "ZygiskHider", __VA_ARGS__)
#define CONF_PATH "/data/adb/ksu_zygisk_hider/targets.conf"

using namespace zygisk;

static std::unordered_set<std::string> g_targets;
static bool g_should_hide = false;
static Api* g_api = nullptr;

static void load_conf() {
    FILE* f = fopen(CONF_PATH, "r");
    if (!f) return;
    g_targets.clear();
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char* bar = strchr(line, '|');
        if (bar) *bar = '\0';
        size_t n = strlen(line);
        while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r' || line[n-1] == ' ')) line[--n] = '\0';
        if (n > 0) g_targets.insert(std::string(line));
    }
    fclose(f);
}

// Đọc thẳng package từ cmdline, đéo cần JNIEnv
static std::string get_current_pkg() {
    int fd = open("/proc/self/cmdline", O_RDONLY);
    if (fd < 0) return "";
    char buf[256];
    ssize_t r = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (r <= 0) return "";
    buf[r] = '\0';
    char* colon = strchr(buf, ':');
    if (colon) *colon = '\0';
    return std::string(buf);
}

static bool is_root_path(const char* path) {
    if (!path) return false;
    static const char* needles[] = {
        "/su", "magisk", "ksu", "apatch", "superuser",
        "/system/xbin/su", "/system/bin/su", "/sbin/su",
        "/data/adb/ksu", "/data/adb/magisk", "/data/adb/ap",
    };
    for (auto n : needles) if (strstr(path, n)) return true;
    return false;
}

static int (*orig_access)(const char*, int) = nullptr;
static int (*orig_stat)(const char*, struct stat*) = nullptr;
static int (*orig_lstat)(const char*, struct stat*) = nullptr;
static int (*orig___system_property_get)(const char*, char*) = nullptr;

int hooked_access(const char* pathname, int mode) {
    if (g_should_hide && is_root_path(pathname)) return -1;
    return orig_access(pathname, mode);
}
int hooked_stat(const char* pathname, struct stat* buf) {
    if (g_should_hide && is_root_path(pathname)) return -1;
    return orig_stat(pathname, buf);
}
int hooked_lstat(const char* pathname, struct stat* buf) {
    if (g_should_hide && is_root_path(pathname)) return -1;
    return orig_lstat(pathname, buf);
}
int hooked___system_property_get(const char* name, char* value) {
    if (g_should_hide && name) {
        if (!strcmp(name, "ro.debuggable")) { strcpy(value, "0"); return 1; }
        if (!strcmp(name, "ro.secure"))     { strcpy(value, "1"); return 1; }
        if (!strcmp(name, "ro.build.tags")) { strcpy(value, "release-keys"); return 1; }
        if (!strcmp(name, "ro.boot.verifiedbootstate")) { strcpy(value, "green"); return 1; }
        if (!strcmp(name, "ro.boot.flash.locked"))      { strcpy(value, "1"); return 1; }
        if (!strcmp(name, "ro.boot.veritymode"))        { strcpy(value, "enforcing"); return 1; }
    }
    return orig___system_property_get(name, value);
}

class HiderModule : public ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override {
        g_api = api;
        load_conf();
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        std::string pkg = get_current_pkg();
        if (!pkg.empty() && g_targets.count(pkg) > 0) {
            g_should_hide = true;
        } else {
            g_should_hide = false;
        }
    }

    void postAppSpecialize(const AppSpecializeArgs *args) override {
        if (!g_should_hide || !g_api) return;
        
        struct stat sb;
        // Xác định đường dẫn libc theo kiến trúc 64-bit hay 32-bit
        const char* libc_path = (sizeof(void*) == 8) ? "/system/lib64/libc.so" : "/system/lib/libc.so";
        
        if (stat(libc_path, &sb) != 0) {
            LOGD("Failed to stat %s", libc_path);
            return;
        }
        
        // Zygisk yêu cầu truyền dev_t và ino_t của thư viện chứa hàm
        g_api->pltHookRegister(sb.st_dev, sb.st_ino, "access", (void*)hooked_access, (void**)&orig_access);
        g_api->pltHookRegister(sb.st_dev, sb.st_ino, "stat", (void*)hooked_stat, (void**)&orig_stat);
        g_api->pltHookRegister(sb.st_dev, sb.st_ino, "lstat", (void*)hooked_lstat, (void**)&orig_lstat);
        g_api->pltHookRegister(sb.st_dev, sb.st_ino, "__system_property_get", (void*)hooked___system_property_get, (void**)&orig___system_property_get);
        
        g_api->pltHookCommit();
        LOGD("Hooks installed via PLT");
    }
};

REGISTER_ZYGISK_MODULE(HiderModule)
