// Android JNI support for cliffi.
//
// Two initialization paths are provided:
//
//  FalsoJNI (default): A fake JNI environment (github.com/v-atamanenko/FalsoJNI)
//    that stubs the full JNIEnv/JavaVM vtable. Works on every Android version
//    without any ART dependency. Sufficient for loading a library, triggering
//    JNI_OnLoad so native methods are registered, then calling those methods
//    via cliffi's normal FFI. Java callbacks from the native side will hit
//    logged stubs — good for shallow RE or libs that are mostly self-contained.
//
//  Real ART JVM (--real flag): Bootstraps a genuine ART/Dalvik runtime by
//    dlopening libart.so/libdvm.so and calling JNI_CreateJavaVM. The native
//    lib gets a live JNIEnv and can round-trip into real Java classes. This
//    works reliably only on Android < 8 (or custom/rooted devices); Google
//    progressively restricted JNI_CreateJavaVM in ART from API 24 onward.

#ifdef __ANDROID__

#include "jni_support.h"

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

// FalsoJNI headers — found via target_include_directories(... src/android/FalsoJNI)
#include "FalsoJNI.h"
#include "FalsoJNI_Impl.h"

// cliffi headers — found via target_include_directories(... src/)
#include "exception_handling.h"
#include "library_manager.h"
#include "parse_address.h"
#include "tokenize.h"
#include "var_map.h"

// ── FalsoJNI global state ────────────────────────────────────────────────────

static bool g_fake_jvm_initialized = false;

static void ensure_fake_jvm(void) {
    if (!g_fake_jvm_initialized) {
        jni_init();
        g_fake_jvm_initialized = true;
    }
}

// ── Real ART/Dalvik bootstrap ────────────────────────────────────────────────

#if __aarch64__
#define APEX_LIBART "/apex/com.android.runtime/lib64/libart.so"
#else
#define APEX_LIBART "/apex/com.android.runtime/lib/libart.so"
#endif

typedef int (*JNI_CreateJavaVM_t)(JavaVM**, JNIEnv**, void*);
typedef int (*registerNatives_t)(JNIEnv*, jclass);

// Returns 0 on success. Negative values indicate which step failed.
// Error -1: libart/libdvm not found
// Error -2: JNI_CreateJavaVM symbol not found
// Error -3: libandroid_runtime.so or registerNatives not found
// Error -4: JNI_CreateJavaVM call failed
// Error -5: registerNatives call failed
static int init_real_jvm(JavaVM** p_vm, JNIEnv** p_env,
                          int opt_count, char** opts) {
    // Build JVM options
    JavaVMOption* options = NULL;
    if (opt_count > 0) {
        options = calloc(opt_count, sizeof(JavaVMOption));
        for (int i = 0; i < opt_count; i++)
            options[i].optionString = opts[i];
    }
    JavaVMInitArgs args = {
        .version            = JNI_VERSION_1_6,
        .options            = options,
        .nOptions           = opt_count,
        .ignoreUnrecognized = JNI_FALSE,
    };

    // Try APEX path (Android 10+) then generic path then old Dalvik
    void* libart = dlopen(APEX_LIBART, RTLD_NOW);
    if (!libart) libart = dlopen("libart.so",  RTLD_NOW);
    if (!libart) libart = dlopen("libdvm.so",  RTLD_NOW);

    if (!libart) { free(options); return -1; }

    JNI_CreateJavaVM_t createVM =
        (JNI_CreateJavaVM_t)dlsym(libart, "JNI_CreateJavaVM");
    if (!createVM) { free(options); return -2; }

    void* libruntime = dlopen("libandroid_runtime.so", RTLD_NOW);
    if (!libruntime) { free(options); return -3; }

    // Symbol name changed across Android versions
    registerNatives_t regNatives =
        (registerNatives_t)dlsym(libruntime,
            "Java_com_android_internal_util_WithFramework_registerNatives");
    if (!regNatives)
        regNatives = (registerNatives_t)dlsym(libruntime,
            "registerFrameworkNatives");
    if (!regNatives) { free(options); return -3; }

    if (createVM(p_vm, p_env, &args)) { free(options); return -4; }
    if (regNatives(*p_env, 0))        { free(options); return -5; }

    free(options);
    return 0;
}

// ── Signal chain stubs ───────────────────────────────────────────────────────
// libandroid_runtime.so dlsym-lookups these symbols on the calling binary via
// RTLD_DEFAULT. They must be exported (enforced by -Wl,--export-dynamic in
// CMakeLists). Empty stubs are fine for cliffi's use case.

typedef bool (*SpecialSignalHandlerFn)(int, siginfo_t*, void*);

__attribute__((visibility("default"))) void InitializeSignalChain(void)    {}
__attribute__((visibility("default"))) void ClaimSignalChain(void)         {}
__attribute__((visibility("default"))) void UnclaimSignalChain(void)       {}
__attribute__((visibility("default"))) void InvokeUserSignalHandler(void)  {}
__attribute__((visibility("default"))) void EnsureFrontOfChain(void)       {}
__attribute__((visibility("default"))) void AddSpecialSignalHandlerFn(void){}
__attribute__((visibility("default"))) void RemoveSpecialSignalHandlerFn(void){}

// ── Auto-init hook (called by library_manager) ───────────────────────────────

typedef int (*JNI_OnLoadFunc)(void* vm, void* reserved);

void jni_notify_library_loaded(void* lib_handle, const char* lib_path) {
    JNI_OnLoadFunc onLoad =
        (JNI_OnLoadFunc)dlsym(lib_handle, "JNI_OnLoad");
    if (!onLoad) return;

    bool first_init = !g_fake_jvm_initialized;
    ensure_fake_jvm();

    if (first_init) {
        printf("[jni] %s exports JNI_OnLoad — auto-initialized FalsoJNI\n",
               lib_path);
    }

    printf("[jni] Calling JNI_OnLoad for %s\n", lib_path);
    onLoad(&jvm, NULL);
    printf("[jni] JNI_OnLoad complete. Native methods registered.\n"
           "[jni]   vm  = %p  (use: set vm  %p)\n"
           "[jni]   env = %p  (use: set env %p)\n"
           "[jni] Or use 'initjni %s <vm_var> <env_var> [--real]' "
           "to store them in named variables.\n",
           (void*)&jvm, (void*)&jvm,
           (void*)&jni, (void*)&jni,
           lib_path);
}

// ── REPL command: initjni ────────────────────────────────────────────────────

void parseInitJNI(char* args) {
    int argc;
    char** argv;
    tokenize(args, &argc, &argv);

    // initjni <library> <vm_var> <env_var> [--real [jvm_opt ...]]
    if (argc < 3) {
        raiseException(1, "Error: initjni requires: <library> <vm_var> <env_var>"
                          " [--real [jvm_opts...]]\n");
        return;
    }

    char* libraryName = argv[0];
    char* vmStr       = argv[1];
    char* envStr      = argv[2];

    bool use_real = false;
    int  jvm_opts_start = 3;
    if (argc > 3 && strcmp(argv[3], "--real") == 0) {
        use_real = true;
        jvm_opts_start = 4;
    }

    void* lib_handle = getOrLoadLibrary(libraryName);
    if (!lib_handle) {
        raiseException(1, "Error: Could not load library: %s\n", libraryName);
        return;
    }

    JNI_OnLoadFunc onLoad =
        (JNI_OnLoadFunc)dlsym(lib_handle, "JNI_OnLoad");
    if (!onLoad) {
        printf("[jni] Warning: %s has no JNI_OnLoad\n", libraryName);
    }

    JavaVM*  vm  = NULL;
    JNIEnv*  env = NULL;

    if (use_real) {
        printf("[jni] Attempting real ART/Dalvik JVM bootstrap...\n");
        int jvm_opt_count = argc - jvm_opts_start;
        int status = init_real_jvm(&vm, &env,
                                   jvm_opt_count, argv + jvm_opts_start);
        if (status != 0) {
            const char* why;
            switch (status) {
                case -1: why = "libart.so/libdvm.so not found"; break;
                case -2: why = "JNI_CreateJavaVM symbol not exported "
                               "(likely Android 8+ restriction)"; break;
                case -3: why = "libandroid_runtime.so or registerNatives "
                               "not found"; break;
                case -4: why = "JNI_CreateJavaVM() returned error"; break;
                case -5: why = "registerNatives() returned error"; break;
                default: why = "unknown";
            }
            printf("[jni] Real JVM failed (%d: %s). "
                   "Falling back to FalsoJNI.\n", status, why);
            use_real = false;
        } else {
            printf("[jni] Real JVM ready (vm=%p env=%p)\n", (void*)vm, (void*)env);
        }
    }

    if (!use_real) {
        ensure_fake_jvm();
        vm  = &jvm;
        env = &jni;
        printf("[jni] FalsoJNI ready (vm=%p env=%p)\n", (void*)vm, (void*)env);
    }

    if (onLoad) {
        printf("[jni] Calling JNI_OnLoad for %s\n", libraryName);
        onLoad(vm, NULL);
        printf("[jni] JNI_OnLoad complete. Native methods registered.\n");
    }

    // JavaVM and JNIEnv are both pointer typedefs (pointer-to-interface-struct).
    // vm/env here are JavaVM*/JNIEnv* — store those pointer values directly.
    setVar(vmStr,  getPVar((void*)vm));
    setVar(envStr, getPVar((void*)env));
}

#endif // __ANDROID__
