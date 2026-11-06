#ifndef TEST_NULL_OSYSTEM
#define TEST_NULL_OSYSTEM 1
namespace Common {
#if defined(POSIX) || defined(WIN32)
void install_null_g_system();
// Acceptance fixtures use the real filesystem save manager in isolated paths.
void install_null_g_system_with_saves();
void uninstall_null_g_system();
#define NULL_OSYSTEM_IS_AVAILABLE 1
#else
#define NULL_OSYSTEM_IS_AVAILABLE 0
#endif
}
#endif
