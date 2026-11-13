#pragma once
#include <jni.h>
#include <sys/types.h> // Bắt buộc phải có dòng này để fix lỗi dev_t

#define ZYGISK_API_VERSION 4

namespace zygisk {
struct Api;
struct AppSpecializeArgs;
struct ServerSpecializeArgs;

class ModuleBase {
public:
    virtual void onLoad(Api *api, JNIEnv *env) {}
    virtual void preAppSpecialize(AppSpecializeArgs *args) {}
    virtual void postAppSpecialize(const AppSpecializeArgs *args) {}
    virtual void preServerSpecialize(ServerSpecializeArgs *args) {}
    virtual void postServerSpecialize(const ServerSpecializeArgs *args) {}
protected:
    Api *getApi() const { return api_; }
private:
    Api *api_ = nullptr;
    friend struct Api;
};

struct AppSpecializeArgs {
    jstring nice_name;
    jstring app_data_dir;
    jint uid;
    jint gid;
    jintArray gids;
    jint runtime_flags;
    jobjectArray rlimits;
    jint mount_external;
    jstring se_info;
    jstring se_name;
    jintArray fds_to_ignore;
    jboolean is_child_zygote;
    jboolean is_top_app;
    jobjectArray pkg_data_info_list;
    jobjectArray whitelisted_data_info_list;
    jboolean mount_data_dirs;
    jboolean mount_storage_dirs;
};

struct ServerSpecializeArgs {
    jint uid;
    jint gid;
    jintArray gids;
    jint runtime_flags;
    jobjectArray rlimits;
    jstring se_info;
    jstring se_name;
    jintArray fds_to_ignore;
};

struct Api {
    int connectCompanion() const { return 0; }
    void pltHookRegister(dev_t dev, ino_t inode, const char *symbol, void *newFunc, void **oldFunc) const {}
    void pltHookCommit() const {}
    friend class ModuleBase;
};
}

#define REGISTER_ZYGISK_MODULE(name) \
    extern "C" [[gnu::visibility("default")]] void zygisk_module_entry(zygisk::Api *api, JNIEnv *env) { \
        static name module; \
        module.onLoad(api, env); \
    } \
    extern "C" [[gnu::visibility("default")]] void zygisk_module_pre_app_specialize(zygisk::AppSpecializeArgs *args) {} \
    extern "C" [[gnu::visibility("default")]] void zygisk_module_post_app_specialize(const zygisk::AppSpecializeArgs *args) {} \
    extern "C" [[gnu::visibility("default")]] void zygisk_module_pre_server_specialize(zygisk::ServerSpecializeArgs *args) {} \
    extern "C" [[gnu::visibility("default")]] void zygisk_module_post_server_specialize(const zygisk::ServerSpecializeArgs *args) {}
