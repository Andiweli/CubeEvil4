// Linked into every module library. The SN REL entry points (_prolog in
// st2.cpp, ...) run the constructors listed between the linker-script symbols
// _ctors/_dtors. On ELF the dynamic loader already runs a library's static
// constructors in dlopen, so the module-private lists are empty.
extern "C" {
__attribute__((visibility("hidden"))) void (*_ctors[1])(void) = {0};
__attribute__((visibility("hidden"))) void (*_dtors[1])(void) = {0};

// The static destructors of the module, which the GameCube ran from _epilog
// (_dtors); OSUnlink calls this right after the epilog. Destructors registered
// here do not run again when the library is unloaded later.
extern void* __dso_handle;
int __cxa_finalize(void* dso);
void re4_module_finalize(void)
{
    __cxa_finalize(&__dso_handle);
}
}
