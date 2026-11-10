# Otavix: compilar o gowa.exe (Windows x64) no Linux, para rodar pelo Proton/Wine.
# Ferramentas em GOWA_FERR (padrão ~/.local/share/otavix/ferr): llvm/ (clang-cl, lld-link, llvm-lib,
# llvm-rc, llvm-mt), winsdk/ (CRT e Windows SDK do xwin splat). Usado pelo scripts/linux/build_linux.sh.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 10.0)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

set(_f "$ENV{GOWA_FERR}")
if(NOT _f)
    set(_f "$ENV{HOME}/.local/share/otavix/ferr")
endif()
set(_w "${_f}/winsdk")

set(CMAKE_C_COMPILER   "${_f}/llvm/bin/clang-cl")
set(CMAKE_CXX_COMPILER "${_f}/llvm/bin/clang-cl")
set(CMAKE_LINKER       "${_f}/llvm/bin/lld-link")
set(CMAKE_AR           "${_f}/llvm/bin/llvm-lib")
set(CMAKE_RC_COMPILER  "${_f}/llvm/bin/llvm-rc")
set(CMAKE_MT           "${_f}/llvm/bin/llvm-mt")
set(CMAKE_C_COMPILER_TARGET   x86_64-pc-windows-msvc)
set(CMAKE_CXX_COMPILER_TARGET x86_64-pc-windows-msvc)

# os cabeçalhos do CRT e do SDK como de sistema (/imsvc: sem avisos deles)
set(_inc "/imsvc \"${_w}/crt/include\" /imsvc \"${_w}/sdk/include/ucrt\" /imsvc \"${_w}/sdk/include/um\" /imsvc \"${_w}/sdk/include/shared\" /imsvc \"${_w}/sdk/include/winrt\"")
set(CMAKE_C_FLAGS_INIT   "${_inc}")
set(CMAKE_CXX_FLAGS_INIT "${_inc}")
set(CMAKE_RC_FLAGS_INIT  "-I \"${_w}/sdk/include/um\" -I \"${_w}/sdk/include/shared\" -I \"${_w}/crt/include\"")

set(_lib "/libpath:\"${_w}/crt/lib/x86_64\" /libpath:\"${_w}/sdk/lib/um/x86_64\" /libpath:\"${_w}/sdk/lib/ucrt/x86_64\"")
set(CMAKE_EXE_LINKER_FLAGS_INIT    "${_lib}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${_lib}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${_lib}")

# o xwin não traz as bibliotecas de depuração (msvcrtd.lib): os testes do CMake compilam em Release
set(CMAKE_TRY_COMPILE_CONFIGURATION Release)

# nada do Linux entra na busca de bibliotecas e cabeçalhos
set(CMAKE_FIND_ROOT_PATH "${_w}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
