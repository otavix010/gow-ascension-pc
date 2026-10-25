/* GOWA_PGO=gen only. The clang profile runtime calls getenv directly; the CRT
 * import-library member that defines it also defines __imp_getenv, which
 * boot_main_gowa.cpp already provides (its cached getenv), so linking that
 * member is a duplicate symbol. This getenv goes through the same pointer.
 * No <stdlib.h> here: it declares getenv dllimport. */
extern char* (__cdecl* __imp_getenv)(const char*);

char* __cdecl getenv(const char* name)
{
    return __imp_getenv(name);
}
