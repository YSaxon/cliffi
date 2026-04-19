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

NameToMethodID nameToMethodId[1] = { { 0, "", METHOD_TYPE_UNKNOWN } };

MethodsBoolean methodsBoolean[1] = { { 0, NULL } };
MethodsByte    methodsByte[1]    = { { 0, NULL } };
MethodsChar    methodsChar[1]    = { { 0, NULL } };
MethodsDouble  methodsDouble[1]  = { { 0, NULL } };
MethodsFloat   methodsFloat[1]   = { { 0, NULL } };
MethodsInt     methodsInt[1]     = { { 0, NULL } };
MethodsLong    methodsLong[1]    = { { 0, NULL } };
MethodsObject  methodsObject[1]  = { { 0, NULL } };
MethodsShort   methodsShort[1]   = { { 0, NULL } };
MethodsVoid    methodsVoid[1]    = { { 0, NULL } };

NameToFieldID  nameToFieldId[1]  = { { 0, "", FIELD_TYPE_UNKNOWN } };

FieldsBoolean  fieldsBoolean[1]  = { { 0, NULL } };
FieldsByte     fieldsByte[1]     = { { 0, NULL } };
FieldsChar     fieldsChar[1]     = { { 0, NULL } };
FieldsDouble   fieldsDouble[1]   = { { 0, NULL } };
FieldsFloat    fieldsFloat[1]    = { { 0, NULL } };
FieldsInt      fieldsInt[1]      = { { 0, NULL } };
FieldsObject   fieldsObject[1]   = { { 0, NULL } };
FieldsLong     fieldsLong[1]     = { { 0, NULL } };
FieldsShort    fieldsShort[1]    = { { 0, NULL } };

__FALSOJNI_IMPL_CONTAINER_SIZES

#endif // __ANDROID__
