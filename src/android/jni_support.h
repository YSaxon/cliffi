#ifndef JNI_SUPPORT_H
#define JNI_SUPPORT_H

#ifdef __ANDROID__

// Called by library_manager after every successful dlopen on Android.
// If the library exports JNI_OnLoad, either calls it automatically (when
// autojni is on) or prints a reminder showing the initjni invocation.
void jni_notify_library_loaded(void* lib_handle, const char* lib_path);

// REPL command handler: initjni <library> <vm_var> <env_var> [--real [jvm_opts...]]
//   Default (no flag): FalsoJNI fake environment — works on any Android version.
//   --real            : ART/Dalvik bootstrap via JNI_CreateJavaVM; needs a JVM-
//                       capable device (generally Android < 8 or rooted). Lets
//                       the native lib actually call back into real Java classes.
void parseInitJNI(char* args);

// REPL command handler: autojni [on|off]
//   on  — future library loads auto-call JNI_OnLoad via FalsoJNI
//   off — only print a reminder (default)
//   (no arg) — print current setting
void parseAutoJNI(char* args);

#endif // __ANDROID__
#endif // JNI_SUPPORT_H
