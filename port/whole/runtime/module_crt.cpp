// Linked into every module library. The SN REL entry points (_prolog in
// st2.cpp, ...) run the constructors listed between the linker-script symbols
// _ctors/_dtors. On ELF the dynamic loader already runs a library's static
// constructors in dlopen, so the module-private lists are empty.
extern "C" {
__attribute__((visibility("hidden"))) void (*_ctors[1])(void) = {0};
__attribute__((visibility("hidden"))) void (*_dtors[1])(void) = {0};
}
