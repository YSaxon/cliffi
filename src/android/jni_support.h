#ifndef JNI_SUPPORT_H
#define JNI_SUPPORT_H

#ifdef __ANDROID__

// Called by library_manager after every successful dlopen on Android.
// If the library exports JNI_OnLoad, auto-initializes FalsoJNI (once) and
// calls JNI_OnLoad so native method mappings are registered immediately.
void jni_notify_library_loaded(void* lib_handle, const char* lib_path);

// REPL command handler.
// Syntax: initjni <library> <vm_var> <env_var> [--real [jvm_option ...]]
//   Default (no flag): FalsoJNI fake environment — works on any Android version.
//   --real            : ART/Dalvik bootstrap via JNI_CreateJavaVM; needs a JVM-
//                       capable device (generally Android < 8 or rooted). Lets
//                       the native lib actually call back into real Java classes.
void parseInitJNI(char* args);

#endif // __ANDROID__
#endif // JNI_SUPPORT_H
