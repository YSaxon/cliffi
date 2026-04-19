// FalsoJNI implementation tables for cliffi.
//
// FalsoJNI dispatches JNI method/field calls (env->CallVoidMethodV, etc.)
// through these tables. The default empty tables mean any Java method the
// native library calls back into will hit a logged stub that returns null/zero.
//
// For deeper RE work where a library makes specific Java callbacks you care
// about, populate these tables following the FalsoJNI README pattern. Example:
//
//   static void my_Log_i(jmethodID id, va_list args) {
//       jstring tag = va_arg(args, jstring);
//       jstring msg = va_arg(args, jstring);
//       printf("[Java/Log.i] %s: %s\n", (char*)tag, (char*)msg);
//   }
//
//   NameToMethodID nameToMethodId[] = {
//       { 1, "i", METHOD_TYPE_VOID },
//   };
//   MethodsVoid methodsVoid[] = {
//       { 1, my_Log_i },
//   };

#ifdef __ANDROID__

#include "FalsoJNI_Impl.h"

NameToMethodID nameToMethodId[] = {};

MethodsBoolean methodsBoolean[] = {};
MethodsByte    methodsByte[]    = {};
MethodsChar    methodsChar[]    = {};
MethodsDouble  methodsDouble[]  = {};
MethodsFloat   methodsFloat[]   = {};
MethodsInt     methodsInt[]     = {};
MethodsLong    methodsLong[]    = {};
MethodsObject  methodsObject[]  = {};
MethodsShort   methodsShort[]   = {};
MethodsVoid    methodsVoid[]    = {};

NameToFieldID  nameToFieldId[]  = {};

FieldsBoolean  fieldsBoolean[]  = {};
FieldsByte     fieldsByte[]     = {};
FieldsChar     fieldsChar[]     = {};
FieldsDouble   fieldsDouble[]   = {};
FieldsFloat    fieldsFloat[]    = {};
FieldsInt      fieldsInt[]      = {};
FieldsObject   fieldsObject[]   = {};
FieldsLong     fieldsLong[]     = {};
FieldsShort    fieldsShort[]    = {};

__FALSOJNI_IMPL_CONTAINER_SIZES

#endif // __ANDROID__
