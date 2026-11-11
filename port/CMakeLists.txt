cmake_minimum_required(VERSION 3.20)
project(gowa
    VERSION 0.1.0
    LANGUAGES C CXX
    DESCRIPTION "Recompiled PS3 game project (ps3recomp)"
)

# ---------------------------------------------------------------------------
# Standards
# ---------------------------------------------------------------------------
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Prefer Ninja if available
if(NOT CMAKE_GENERATOR MATCHES "Ninja" AND NOT DEFINED ENV{CMAKE_GENERATOR})
    message(STATUS "Tip: use -G Ninja for faster builds")
endif()

# ---------------------------------------------------------------------------
# The two directories this project is built from
# ---------------------------------------------------------------------------
# ENGINE_PACOTE_DIR is the Otavix Engine PS3 package (06/11): the engine comes
# compiled (lib/, include/, bin/, cmake/), downloaded and checked by gerar.ps1
# into <port root>/engine. RECOMP_DIR is where the lifter's output for THIS
# game lives:
#
#   cmake -B build -G Ninja \
#         -DENGINE_PACOTE_DIR=/path/to/engine \
#         -DRECOMP_DIR=/path/to/my_game/recompiled
#
set(ENGINE_PACOTE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../engine" CACHE PATH
    "Otavix Engine PS3 package (extracted): lib/, include/, cmake/")
get_filename_component(ENGINE_PACOTE_DIR "${ENGINE_PACOTE_DIR}" ABSOLUTE)

set(RECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/recompiled" CACHE PATH
    "Directory holding the lifter's output: ppu_recomp.h and the lifted sources")
get_filename_component(RECOMP_DIR "${RECOMP_DIR}" ABSOLUTE)

message(STATUS "Engine package:  ${ENGINE_PACOTE_DIR}")
message(STATUS "Lifted output:   ${RECOMP_DIR}")

if(NOT EXISTS "${RECOMP_DIR}/ppu_recomp.h")
    message(FATAL_ERROR
        "No ppu_recomp.h in ${RECOMP_DIR}.\n"
        "That header and the lifted sources beside it are what "
        "the translator (engine bin/ppu_traduz.exe) writes. Run gerar.ps1, drop its output there, "
        "or point -DRECOMP_DIR= at wherever it already is.")
endif()

# The lifted translation units. A game's lift is hundreds of generated files,
# so they are globbed rather than listed; re-run CMake after a fresh lift.
file(GLOB RECOMP_SOURCES CONFIGURE_DEPENDS
    "${RECOMP_DIR}/*.c"
    "${RECOMP_DIR}/*.cpp"
)
if(NOT RECOMP_SOURCES)
    message(FATAL_ERROR
        "${RECOMP_DIR} has a ppu_recomp.h but no .c or .cpp beside it. The "
        "lifted functions and the function table live in those; without them "
        "there is nothing to run.")
endif()
list(LENGTH RECOMP_SOURCES RECOMP_SOURCE_COUNT)
message(STATUS "Lifted sources:  ${RECOMP_SOURCE_COUNT} file(s)")

# ---------------------------------------------------------------------------
# Otavix Engine PS3 (compiled package)
# ---------------------------------------------------------------------------
# The runtime, the PPU boot scaffold (runtime/ppu/*.cpp, built against this
# game's ppu_recomp.h) and the SPU musttail dispatcher come prebuilt in the
# package, with the options this file used to give them: x86-64-v3 without /GS
# (the old GOWA_RUNTIME_FAST), FFmpeg loaded at run time, and two versions --
# portatil (SPU_SX_RZ, GOWA_SPU_RZ=ON) and nativa. The package's cmake file
# carries the link requirements (Winsock, XInput, COM, bcrypt).
if(NOT EXISTS "${ENGINE_PACOTE_DIR}/cmake/otavix-engine.cmake")
    message(FATAL_ERROR
        "No Otavix Engine PS3 package in ${ENGINE_PACOTE_DIR} (cmake/otavix-engine.cmake). "
        "gerar.ps1 downloads and checks it into <port root>/engine; or pass -DENGINE_PACOTE_DIR=.")
endif()

# GOWA_PGO: profile-guided optimization (clang) for the runtime and the lifted
# SPU code only -- the huge lifted PPU TUs stay out (rebuilding them costs tens
# of minutes). "gen" instruments them and links pgo_dump.cpp, which writes the
# profile (LLVM_PROFILE_FILE) after GOWA_PGO_DUMP_S seconds: test runs end by
# killing the process, so the profile runtime's atexit never fires. "use"
# compiles them with GOWA_PGO_PROFILE (llvm-profdata merge). Empty = off.
set(GOWA_PGO "" CACHE STRING "PGO of the runtime and the lifted SPU code: gen | use | empty")
set(GOWA_PGO_PROFILE "${CMAKE_CURRENT_SOURCE_DIR}/pgo/gowa.profdata" CACHE FILEPATH "Profile for GOWA_PGO=use")
set(GOWA_PGO_FLAGS "")
if(GOWA_PGO STREQUAL "gen")
    set(GOWA_PGO_FLAGS "/clang:-fprofile-instr-generate")
elseif(GOWA_PGO STREQUAL "use")
    set(GOWA_PGO_FLAGS "/clang:-fprofile-instr-use=${GOWA_PGO_PROFILE};/clang:-Wno-profile-instr-unprofiled;/clang:-Wno-profile-instr-out-of-date;/clang:-Wno-backend-plugin")
endif()
# (the runtime is prebuilt now: GOWA_PGO reaches the lifted SPU code only)

# ---------------------------------------------------------------------------
# The HLE NID table
# ---------------------------------------------------------------------------
# Maps each firmware import NID onto the handler in the runtime that answers
# it: written beside the lift by gerar.ps1 (engine bin/ppu_nids.exe --all).
if(NOT EXISTS "${RECOMP_DIR}/ppu_hle_nids.cpp")
    message(FATAL_ERROR "No ppu_hle_nids.cpp in ${RECOMP_DIR} (gerar.ps1, step 4_ppu, writes it).")
endif()
set(HLE_NID_TABLE "${RECOMP_DIR}/ppu_hle_nids.cpp")
message(STATUS "HLE NID table:   ${HLE_NID_TABLE}")

# ---------------------------------------------------------------------------
# The game executable
# ---------------------------------------------------------------------------
# The PPU boot scaffold (runtime/ppu/*.cpp) comes compiled in the engine
# package, against this game's ppu_recomp.h (otavix_engine_liga checks it is
# the same one).

# ---------------------------------------------------------------------------
# Lifted SPU images (optional)
# ---------------------------------------------------------------------------
# A title that runs SPU work gets its images lifted into <SPU_DIR>/<image>/ by
# tools/spu_lifter.py, with a registration TU generated by
# tools/build_spu_workloads.py. cellSpurs dispatches them by content
# fingerprint, so the generated table is keyed to the images THIS build ships:
# a table generated from another release registers signatures that can never
# match, which is worse than shipping none. Both the table and the job-module
# registry self-register from constructors and are therefore optional -- a tree
# that has not lifted its SPU images yet configures and links without them.
set(SPU_DIR "${CMAKE_CURRENT_SOURCE_DIR}/spu" CACHE PATH
    "Directory holding the lifted SPU images (<SPU_DIR>/<image>/spu_recomp.c)")
option(PORT_ENABLE_SPU "Compile and register the title's lifted SPU images" ON)

set(PORT_SPU_INPUTS "" CACHE STRING
    "Input files to scan for all embedded SPU ELF programs (semicolon-separated decrypted ELF/PRX/data paths)")
set(SPU_SOURCES "")
if(PORT_ENABLE_SPU AND PORT_SPU_INPUTS)
    message(FATAL_ERROR "PORT_SPU_INPUTS: the SPU images are lifted by gerar.ps1 (step 5_spu), not by CMake")
elseif(PORT_ENABLE_SPU)
    file(GLOB SPU_LIFTED CONFIGURE_DEPENDS "${SPU_DIR}/*/spu_recomp.c")
    if(SPU_LIFTED)
        list(LENGTH SPU_LIFTED SPU_IMAGE_COUNT)
        message(STATUS "Lifted SPU:      ${SPU_IMAGE_COUNT} image(s)")
        # The musttail indirect-branch fast path (spu_dispatch_mt.c) comes
        # compiled in the engine package, by clang, with the SPU options.
        list(APPEND SPU_SOURCES ${SPU_LIFTED})
        # Generated beside the lift; each is present only once its generator
        # has been run, so each is included only if it is actually here.
        foreach(gen IN ITEMS spu_workloads.c jobmods_register.c taskset_policy_bytes.c)
            if(EXISTS "${SPU_DIR}/${gen}")
                list(APPEND SPU_SOURCES "${SPU_DIR}/${gen}")
            else()
                message(STATUS "  (no ${gen} -- not registered)")
            endif()
        endforeach()
    else()
        message(STATUS "Lifted SPU:      none found in ${SPU_DIR}")
    endif()
endif()

# GOWA: libsre lifted (LLE do cellSpurs/cellSync). O .cpp inclui o próprio
# ppu_recomp.h da libsre, que fica ao lado dele (o #include "..." acha esse antes).
set(LLE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/lle_libsre/libsre_recomp.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/lle_libsre/libsre_exports.c"
)

# GOWA: o runner padrão é boot_main_gowa.cpp (cópia do runtime/ppu/tests/boot_main.cpp
# do toolkit: watchdog de travamento, filtro de crash, cache de getenv). O main.cpp
# do template continua disponível com -DGOWA_TEMPLATE_MAIN=ON.
option(GOWA_TEMPLATE_MAIN "Usar o main.cpp do template em vez do boot harness" OFF)
if(GOWA_TEMPLATE_MAIN)
    set(GOWA_MAIN main.cpp)
else()
    set(GOWA_MAIN boot_main_gowa.cpp)
endif()

add_executable(${PROJECT_NAME}
    ${GOWA_MAIN}
    stubs.cpp
    gowa_offline.cpp
    gowa_ouroboros.cpp                 # Ouroboros: evento do amuleto que a cena espera (soft lock da 2a ponte)
    gowa_confere.cpp                   # confer�ncia dos jobs de SPU (GOWA_CONF / GOWA_CONF_REPLAY)
    otavix_osd.c                       # o medidor padrão dos ports Otavix (F10)
    otavix_legenda.c                   # a legenda das teclas (F1)
    ${LLE_SOURCES}
    ${HLE_NID_TABLE}
    ${RECOMP_SOURCES}
    ${SPU_SOURCES}
)

# The lifted SPU sources use bare #include "spu_context.h". Scope that include
# directory (and their flags) to those TUs alone: adding it to the whole target
# changes the compile command of every lifted PPU TU, and those are large enough
# that the needless rebuild is measured in tens of minutes.
#   SPU_USE_DISPATCH_MT -- route SPU_IB_DISPATCH through the musttail path.
#   SPU_LS_DIRECT       -- GOWA: LS helpers address ctx->ls_store (no raw SPUs
#                          in this title; see spu_context.h)
#   SPU_LS_NO_PROBES    -- GOWA: no diagnostic checks in spu_ls_read128/write128
#                          (four tests in front of every LS access); the
#                          interpreter and the runtime keep them.
#   -O2                 -- the target-wide -O1 exists for the huge PPU TUs; the
#                          SPU sources are small and decode-hot, so they get
#                          full optimization. The last -O on the line wins.
#   SPU_SX_DAZ          -- GOWA: the float helpers leave the denormal-operand
#                          flush to MXCSR.DAZ, which the runtime sets on every
#                          SPU host thread (spu_daz_enter). GOWA_SPU_DAZ.
option(GOWA_SPU_DAZ "Lifted SPU float ops rely on MXCSR.DAZ (set by the runtime)" OFF)
set(GOWA_SPU_DAZ_DEF "")
if(GOWA_SPU_DAZ)
    set(GOWA_SPU_DAZ_DEF ";SPU_SX_DAZ")
endif()
#   SPU_SX_APPROX       -- GOWA: "approximate xfloat" (RPCS3's default): no
#                          exact path for extended-range/saturated float results.
option(GOWA_SPU_APPROX "Lifted SPU float ops without the extended-range exact path" OFF)
if(GOWA_SPU_APPROX)
    set(GOWA_SPU_DAZ_DEF "${GOWA_SPU_DAZ_DEF};SPU_SX_APPROX")
endif()
#   SPU_SX_NOFLUSH      -- GOWA: MEASUREMENT ONLY (not exact): float results
#                          leave without the flush to +0 (cost ceiling).
#   SPU_SX_RZ           -- GOWA (24/10): builds without AVX-512: the SPU host threads
#                          run with MXCSR round-toward-zero and the float helpers use
#                          plain SSE/FMA ops (spu_helpers.h). The runtime is built with
#                          it too: its interpreter runs on the same threads.
option(GOWA_SPU_RZ "SPU float ops with MXCSR round-toward-zero (builds without AVX-512)" OFF)
if(GOWA_SPU_RZ)
    set(GOWA_SPU_DAZ_DEF "${GOWA_SPU_DAZ_DEF};SPU_SX_RZ")
    set(ENGINE_VARIANTE portatil)   # the engine package's runtime built with SPU_SX_RZ
else()
    set(ENGINE_VARIANTE nativa)
endif()
# 11/11: GOWA_ENGINE_VARIANTE=compativel (scripts/build_portatil.cmd x86-64-v2): a engine em x86-64-v2, para
# processadores sem AVX2; vazio = a regra acima (portatil com GOWA_SPU_RZ, nativa sem)
set(GOWA_ENGINE_VARIANTE "" CACHE STRING "variante do pacote da engine (vazio: pela GOWA_SPU_RZ)")
if(GOWA_ENGINE_VARIANTE)
    set(ENGINE_VARIANTE ${GOWA_ENGINE_VARIANTE})
endif()
if(ENGINE_VARIANTE STREQUAL "compativel" AND MSVC)
    # o resto do port (boot, stubs, LLE, PPU traduzido) em SSE4.2 sem AVX; o SPU traduzido vem com a GOWA_SPU_MARCH
    target_compile_options(${PROJECT_NAME} PRIVATE /clang:-march=x86-64-v2)
endif()
include("${ENGINE_PACOTE_DIR}/cmake/otavix-engine.cmake")
option(GOWA_SPU_NOFLUSH "Measurement only: lifted SPU float ops without the +0 flush" OFF)
if(GOWA_SPU_NOFLUSH)
    set(GOWA_SPU_DAZ_DEF "${GOWA_SPU_DAZ_DEF};SPU_SX_NOFLUSH")
endif()
#   SPU_SX_CRU_CHECK    -- GOWA: check build for scripts/flush_preguicoso.py: raw
#                          float results carry a poison denormal, and any LS
#                          store of it is reported ([cru-check]).
option(GOWA_SPU_CRU_CHECK "Check build: poison raw SPU float results, report LS stores of the poison" OFF)
if(GOWA_SPU_CRU_CHECK)
    set(GOWA_SPU_DAZ_DEF "${GOWA_SPU_DAZ_DEF};SPU_SX_CRU_CHECK")
endif()
#   SPU_LS_CONST_CHECK  -- GOWA: the folded constant-pool loads of the job
#                          modules (scripts/ls_const_fold.py) read the LS and
#                          report any difference from the image ([ls-const]).
option(GOWA_LS_CONST_CHECK "Check the folded LS constants against the LS" OFF)
if(GOWA_LS_CONST_CHECK)
    set(GOWA_SPU_DAZ_DEF "${GOWA_SPU_DAZ_DEF};SPU_LS_CONST_CHECK")
endif()
if(SPU_SOURCES)
    set_source_files_properties(${SPU_SOURCES} PROPERTIES
        INCLUDE_DIRECTORIES "${ENGINE_SPU_INCLUDE_DIR}"
        COMPILE_DEFINITIONS "SPU_USE_DISPATCH_MT;SPU_LS_NO_PROBES;SPU_LS_DIRECT${GOWA_SPU_DAZ_DEF}")
    if(NOT MSVC OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # GOWA: -msse4.1 so spu_helpers.h takes its pshufb spu_shufb (and the
        # other SSE4.1 paths) in the lifted SPU code too; the runtime already
        # gets it. As the byte loop, spu_shufb was ~18% of every SPU thread.
        # GOWA_SPU_X86_V3: x86-64-v3 (AVX2/BMI2/LZCNT/MOVBE) for the lifted SPU
        # code -- every LS access is a big-endian load or store (movbe folds
        # the bswap), and clz/cntb become one instruction. Same CPU floor as
        # GOWA_HOST_FMA (Haswell). Contraction stays off: the SPU rounds fm
        # and fa separately.
        option(GOWA_SPU_X86_V3 "Compile the lifted SPU code for x86-64-v3" ON)
        # /GS-: no stack cookie in the lifted SPU functions (a load, an xor
        # and a check on every call of every one of them; the runtime already
        # builds without it, GOWA_RUNTIME_FAST).
        # GOWA_SPU_MARCH: the -march of that build (x86-64-v3 by default). With
        # znver5 / x86-64-v4 the compiler gets AVX-512: 32 vector registers
        # instead of 16 for the ~100 live SPU registers of a hot job function.
        set(GOWA_SPU_MARCH "x86-64-v3" CACHE STRING "-march for the lifted SPU code (GOWA_SPU_X86_V3)")
        if(GOWA_SPU_X86_V3 AND MSVC)
            set(GOWA_SPU_OPTS "-O2;/GS-;/clang:-march=${GOWA_SPU_MARCH};/clang:-ffp-contract=off")
            if(GOWA_PGO_FLAGS)
                list(APPEND GOWA_SPU_OPTS ${GOWA_PGO_FLAGS})
            endif()
            set_source_files_properties(${SPU_SOURCES} PROPERTIES COMPILE_OPTIONS "${GOWA_SPU_OPTS}")
        elseif(GOWA_SPU_X86_V3)
            set_source_files_properties(${SPU_SOURCES} PROPERTIES COMPILE_OPTIONS
                "-O2;-march=${GOWA_SPU_MARCH};-ffp-contract=off")
        else()
            set_source_files_properties(${SPU_SOURCES} PROPERTIES COMPILE_OPTIONS "-O2;-msse4.1")
        endif()
    endif()
endif()

# app.rc carries the icon, the version block and the manifest, and needs a
# resource compiler. Listing it as a source on a host that has none is not a
# link error, it is a "no rule to make" for a file nothing can consume.
if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/app.rc")
    target_sources(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/app.rc")
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    "${RECOMP_DIR}"                            # the generated ppu_recomp.h
    ${ENGINE_INCLUDE_DIRS}                     # the engine package's headers (include, runtime/*, libs/video)
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# the engine: runtime, PPU scaffold (whole), SPU dispatcher, and their Windows imports
otavix_engine_liga(${PROJECT_NAME} "${RECOMP_DIR}")
if(GOWA_PGO STREQUAL "gen")
    # pgo_getenv.c: the profile runtime's getenv, so the CRT member that also
    # defines __imp_getenv (already ours, boot_main_gowa.cpp) is not linked.
    target_sources(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/pgo_dump.cpp"
                                           "${CMAKE_CURRENT_SOURCE_DIR}/pgo_getenv.c")
    get_filename_component(GOWA_LLVM_BIN "${CMAKE_LINKER}" DIRECTORY)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "${GOWA_LLVM_BIN}/../lib/clang/19/lib/windows/clang_rt.profile-x86_64.lib")
endif()

# GOWA: map file (RVA -> symbol) for the host profiler (GOWA_HOSTPROF). Linker
# only: no compile flag changes, so only the link reruns.
if(MSVC OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_link_options(${PROJECT_NAME} PRIVATE "/MAP:${CMAKE_CURRENT_BINARY_DIR}/gowa.map")
endif()

# The runtime is C++, and on Apple also Objective-C. Letting CMake infer the
# link language from the generated C would leave the C++ standard library out.
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)

# Windows system import libraries. dbghelp symbolizes crash-handler addresses
# against the PDB, user32 and gdi32 are what the backend's window and its GDI
# present are made of, and winmm is timeBeginPeriod. None of the four names
# anything a POSIX linker can find; the D3D12 backend's own imports come in
# through the #pragma comment(lib) in libs/video/rsx_d3d12_backend.c.
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        dbghelp
        user32
        gdi32
        winmm
        dxguid      # IID_IDXGIFactory6 (rsx_d3d12_backend.c)
    )
endif()

# ---------------------------------------------------------------------------
# Compiler-specific flags
# ---------------------------------------------------------------------------
if(MSVC AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # clang-cl: -Wall there means -Weverything, which buries everything. /W3 instead.
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W3 -Wno-unused-parameter
        -fno-strict-aliasing
        -fwrapv
    )
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _CRT_SECURE_NO_WARNINGS
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall -Wextra -Wno-unused-parameter
        -fno-strict-aliasing
        # The lift spells PPC arithmetic as signed C arithmetic, which must wrap;
        # without this, clang exploits overflow UB (e.g. folds "x*x < 0" to false).
        -fwrapv
    )
elseif(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W3 /wd4100 /wd4244 /wd4267
        /Zc:__cplusplus         # report correct __cplusplus value
        # ppu_recomp.h defines PPU_FENCE, the lowering of the guest's
        # sync/lwsync/eieio/isync, and in C that is <stdatomic.h>. The lifter
        # writes C++ by default, but --single-file writes ppu_recomp.c, and a
        # port that took it would not compile without this. The runtime's own
        # build already carries the same flag for the same reason.
        /experimental:c11atomics
    )
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# The lifted translation units are machine-generated and can run to hundreds of
# thousands of lines each, so they get their own flags:
#
#   - warnings off. They are about generated code and nobody will act on them,
#     and at this volume they bury the diagnostics that matter.
#   - no floating-point contraction. arm64 fuses a multiply and an add by
#     default; a PPC fmadd is spelt as one where the lifter means one.
#   - /bigobj on MSVC. A single lifted file routinely passes the 65,536-section
#     limit of the default object format.
if(MSVC AND CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND GOWA_HOST_FMA)
    # GOWA: -mfma so the lift's fmaf()/fma() (vmaddfp, fmadd...) are one
    # vfmadd instead of a call into the UCRT per lane -- same correctly rounded
    # result. Contraction off with it: clang-cl's /fp:precise contracts a*b+c
    # inside an expression, which an FMA-capable target would then fuse where
    # the guest rounds twice. Needs an FMA3 CPU (Haswell / Piledriver or later).
    # GOWA_PPU_MARCH (empty = as before): a -march for the lifted PPU and LLE
    # code, plus /GS-. x86-64-v3 brings MOVBE, so a big-endian guest load is
    # one instruction. Changing it rebuilds every lifted TU (~2 min).
    set(GOWA_PPU_MARCH "" CACHE STRING "-march for the lifted PPU code (empty: none)")
    # GOWA_PGO_PPU (26/10, experiment): GOWA_PGO also on the lifted PPU / LLE code (the game's
    # main thread). Off by default: a profile made without it has no PPU data.
    option(GOWA_PGO_PPU "Apply GOWA_PGO to the lifted PPU code too" OFF)
    set(GOWA_PPU_PGO_OPTS "")
    if(GOWA_PGO_PPU AND GOWA_PGO_FLAGS)
        set(GOWA_PPU_PGO_OPTS ";${GOWA_PGO_FLAGS}")
    endif()
    if(GOWA_PPU_MARCH)
        set_source_files_properties(${RECOMP_SOURCES} ${LLE_SOURCES} PROPERTIES
            COMPILE_OPTIONS "/w;/bigobj;-mfma;/clang:-ffp-contract=off;/GS-;/clang:-march=${GOWA_PPU_MARCH}${GOWA_PPU_PGO_OPTS}")
    else()
    set_source_files_properties(${RECOMP_SOURCES} ${LLE_SOURCES} PROPERTIES
        COMPILE_OPTIONS "/w;/bigobj;-mfma;/clang:-ffp-contract=off")
    endif()
elseif(MSVC)
    set_source_files_properties(${RECOMP_SOURCES} ${LLE_SOURCES} PROPERTIES
        COMPILE_OPTIONS "/w;/bigobj")
else()
    set_source_files_properties(${RECOMP_SOURCES} PROPERTIES
        COMPILE_OPTIONS "-w;-ffp-contract=off")
endif()

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
