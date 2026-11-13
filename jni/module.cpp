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
        load_conf();
    }
    void preAppSpecialize(AppSpecializeArgs *args) override {
        if (!args || !args->nice_name) return;
        std::string pkg(args->nice_name);
        if (g_targets.count(pkg) > 0) {
            g_should_hide = true;
        } else {
            g_should_hide = false;
        }
    }
    void postAppSpecialize(const AppSpecializeArgs *) override {
        if (!g_should_hide) return;
        auto api = getApi();
        if (api) {
            api->pltHookRegister("libc.so", "access", (void*)hooked_access, (void**)&orig_access);
            api->pltHookRegister("libc.so", "stat", (void*)hooked_stat, (void**)&orig_stat);
            api->pltHookRegister("libc.so", "lstat", (void*)hooked_lstat, (void**)&orig_lstat);
            api->pltHookRegister("libc.so", "__system_property_get", (void*)hooked___system_property_get, (void**)&orig___system_property_get);
            api->pltHookCommit();
        }
    }
};

REGISTER_ZYGISK_MODULE(HiderModule)
