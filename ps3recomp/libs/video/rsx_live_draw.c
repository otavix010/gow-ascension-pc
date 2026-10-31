/*
 * ps3recomp - Track B live NV4097 draw path (implementation)
 *
 * See rsx_live_draw.h. This is the validated capture-replay D3D12 engine
 * (libs/video/tests/replay_main.c) lifted into a runtime module and driven by
 * the live FIFO consumer instead of an .rxs file:
 *   - rsx_dispatch register-file model (shared, unchanged)
 *   - NV40 VP/FP -> HLSL decompilers (shared, unchanged)
 *   - B1 render/sampler state -> D3D12 PSO + dynamic samplers + mip chains
 *
 * Differences from the harness:
 *   - guest memory comes from an injected resolver (the runtime's vm_base map),
 *     not a private arena;
 *   - present goes to a swap chain bound to the runtime's window, not a PPM
 *     readback;
 *   - the whole engine is gated behind RSX_DRAW (default ON; "0" = off).
 *
 * Clean-room: NV40 ISA/register facts from envytools rnndb + Mesa nv30 +
 * psdevwiki; RPCS3 as a read-only fact oracle only.
 */

#include "rsx_live_draw.h"
#include "ps3emu/yz_runtime_config.h"
#include "ps3emu/yz_frontier_trace.h"

#if !defined(_WIN32)

/* Non-Windows: the whole path is a no-op (D3D12 is Windows-only). */
int  rsx_live_draw_enabled(void) { return 0; }
int  rsx_live_draw_init(void* hwnd, u32 w, u32 h, rsx_live_guest_ptr_fn f, void* u)
{ (void)hwnd; (void)w; (void)h; (void)f; (void)u; return 0; }
void rsx_live_draw_seed_registers(const u32* r, u32 n) { (void)r; (void)n; }
void rsx_live_draw_seed_transform_program(const u32* w, u32 n) { (void)w; (void)n; }
void rsx_live_draw_seed_transform_constants(const u32* w, u32 n) { (void)w; (void)n; }
void rsx_live_draw_set_display_buffer(
    u32 b, u32 l, u32 o, u32 p, u32 w, u32 h)
{ (void)b; (void)l; (void)o; (void)p; (void)w; (void)h; }
void rsx_live_draw_method(u32 m, u32 a) { (void)m; (void)a; }
void rsx_live_draw_set_fifo_position(u32 g, u32 p) { (void)g; (void)p; }
void rsx_live_draw_note_inline_transfer(u32 d, u32 o, u32 v)
{ (void)d; (void)o; (void)v; }
int  rsx_live_draw_blit(u32 sl, u32 sa, u32 sp, u32 dl, u32 da, u32 dp, u32 w, u32 h)
{ (void)sl; (void)sa; (void)sp; (void)dl; (void)da; (void)dp; (void)w; (void)h; return 0; }
int  rsx_live_draw_resolve_blit(u32 sl, u32 sa, u32 sp, u32 ds, u32 dt, u32 dl, u32 da,
                                u32 dp, u32 x, u32 y, u32 w, u32 h)
{ (void)sl; (void)sa; (void)sp; (void)ds; (void)dt; (void)dl; (void)da; (void)dp;
  (void)x; (void)y; (void)w; (void)h; return 0; }
void rsx_live_draw_note_resolve(u32 sl, u32 sa, u32 dl, u32 da)
{ (void)sl; (void)sa; (void)dl; (void)da; }
void rsx_live_draw_flush(void) {}
void rsx_live_draw_present(u32 b) { (void)b; }
void rsx_live_draw_set_movie_mode(int on) { (void)on; }
void rsx_live_draw_present_rgba(const uint8_t* r, u32 w, u32 h) { (void)r; (void)w; (void)h; }
u32  rsx_live_draw_get_frames(void) { return 0; }
void* rsx_live_draw_get_present_thread_handle(void) { return NULL; }
u32  rsx_live_draw_get_last_draws(void) { return 0; }
double rsx_live_draw_get_present_fps(void) { return 0.0; }
void rsx_live_draw_dump_present_samples(void) {}
void rsx_live_draw_a010_probe_begin(void) {}
int  rsx_live_draw_a010_probe_active(void) { return 0; }
int  rsx_live_draw_a010_world_ready(void) { return 1; }
int  rsx_live_draw_debug_dump_surface(u32 l, u32 o, const char* p)
{ (void)l; (void)o; (void)p; return -1; }
void rsx_live_draw_set_a010_camera_matrix(const float* m) { (void)m; }
void rsx_live_draw_shutdown(void) {}

#else /* _WIN32 */

#define _CRT_SECURE_NO_WARNINGS

#include "rsx_dispatch.h"
#include "rsx_fp_decompiler.h"
#include "rsx_texture_layout.h"   /* rsx_texture_gamma_mask */
static unsigned long long g_ld_bind_white = 0, g_ld_bind_real = 0, g_ld_bind_surf = 0;
/* Set once PS1 VRAM actually holds a drawn frame (see the PS1_PC heartbeat).
 * Every surface dump before this is an early-boot snapshot of a framebuffer the
 * PS1 has not drawn into yet -- which is what made nonblack=0 look like a broken
 * pipeline when it was the correct reading of an empty source. */
static volatile long g_ld_ps1_vram_ready = 0;
#include "rsx_restart_cuts.h"
#include "rsx_vertex_compact.h"
#include "rsx_vp_decompiler.h"
#include "rsx_edge_luz.h"

#include <ctype.h>
#include <immintrin.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>   /* ld_res_linha */
#include <stdlib.h>
#include <stddef.h>   /* offsetof (GOWA pipeline library) */
#include <wchar.h>    /* swprintf */

#if defined(YZ_PERF_CLEAN)
#define LD_DIAG_ENABLED(name) 0
#else
/* Cached per call site: several of these sit on per-draw paths. */
#define LD_DIAG_ENABLED(name) ({ static int _ld_diag = -1; \
    if (_ld_diag < 0) _ld_diag = getenv(name) != NULL; _ld_diag; })
#endif
#include <string.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>   /* K32EnumProcessModules: [modulos] da tela preta */
#include <initguid.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_4.h>
#include <dxgi1_5.h>   /* GOWA_PRESENT_SYNC=0: DXGI_FEATURE_PRESENT_ALLOW_TEARING */
#include <dxgi1_6.h>   /* GOWA_GPU: EnumAdapterByGpuPreference */
#include <d3dcompiler.h>
#include "rsx_vertex_formats.h"
#include <sys/stat.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

/* Armed by the a010 AUTH repair only after its animation palette is resident. */
extern volatile LONG g_yz_a010_reference_camera_active;
/* Published when the Sunshine Orphanage AUTH root becomes live. */
extern volatile LONG g_yz_a010_root_active;
/* Opt-in headless gameplay-proof handshake, owned jointly with cellPad.c. */
extern volatile LONG g_yz_movement_proof_phase;
extern volatile unsigned long long g_yz_auto_start_tick;

/* ---------------------------------------------------------------------------
 * Engine state (module-static; single live RSX)
 * -----------------------------------------------------------------------*/

/* GOWA: room for up to 4 back buffers (RTV heap layout); the swap chain
 * uses g_ld_swap_count of them -- GOWA_SWAP_BUFFERS (2..4, default 3). With 2,
 * a flip-model Present waits for a free buffer: with frame generation (two
 * presents per guest frame) that was ~16% of the thread that walks the FIFO. */
#define LD_SWAP_BUFFERS  4
static u32 g_ld_swap_count = 3;
#define MAX_SURFACES     64
/* GOWA: 8192 (was 1024). God of War Ascension's first level binds more than
 * 1024 distinct textures; the LRU then evicted and re-decoded thousands of
 * them (7,000+ evictions in a few minutes), a large share of the RSX thread.
 * Lookups go through an index (texture_srv_slot), not a linear scan.
 * 28/10: room for 16384; how many are kept is ld_tex_cap() (by the card's VRAM). */
#define MAX_TEXTURES     16384
#define MAX_RETIRED_TEXTURES 4096
#define MAX_VTEX         64
#define MAX_SAMPLERS     4096   /* GOWA: was 128 -- see sampler_slot */
/*
 * A full live boot carries shader pairs from startup movies, menus, and AUTH
 * into the orphanage.  The old 256-entry cap was already exhausted there,
 * causing 155/328 sampled a010 groups to return NULL from get_pso().  The
 * one-frame reference alone uses 197 pairs.  A full visible boot through
 * orphanage and into Akiyama measured 2,296 distinct requested PSO keys before
 * the first presentation stall, so use the next power of two with headroom.
 */
#define MAX_PSOS         8192
#define FORMER_MAX_SHADER_BLOBS 2048
#define MAX_SHADER_BLOBS 8192
#define MAX_REJECTED_PSO_KEYS 8192
#define UPLOAD_SIZE      (64u * 1024 * 1024)
#define SHADER_DISK_CACHE_MAGIC   0x43535A59u /* "YZSC" */
/* v2: OP_KIL now emits discard (was a dropped TODO) — cached v1 pixel
 * shaders would silently keep the old no-discard codegen. */
#define SHADER_DISK_CACHE_VERSION 2u
#define SHADER_DISK_CACHE_MAX_BLOB (4u * 1024u * 1024u)

#define SRV_WHITE        0
#define SRV_SURFACE_BASE 1
#define SRV_ZDEPTH_BASE  (SRV_SURFACE_BASE + MAX_SURFACES)
#define SRV_TEXTURE_BASE (SRV_ZDEPTH_BASE + MAX_SURFACES)
#define SRV_VTEX_BASE    (SRV_TEXTURE_BASE + MAX_TEXTURES)
#define SRV_HEAP_SLOTS   (SRV_VTEX_BASE + MAX_VTEX)
#define SRV_DEPTH_SOURCE_BASE SRV_HEAP_SLOTS
#define UAV_ZDEPTH_BASE   (SRV_DEPTH_SOURCE_BASE + MAX_SURFACES)
/* Per-surface copy a draw samples when it reads its own render target. */
#define SRV_FEEDBACK_BASE (UAV_ZDEPTH_BASE + MAX_SURFACES)
/* GOWA: source of the scaled present blit (ld_present_blit) */
#define SRV_BLIT_SRC      (SRV_FEEDBACK_BASE + MAX_SURFACES)
/* GOWA: zeta read as ARGB8 (ld_zcolor_convert): SRV and UAV per zeta */
#define SRV_ZCOLOR_BASE   (SRV_BLIT_SRC + 1)
#define UAV_ZCOLOR_BASE   (SRV_ZCOLOR_BASE + MAX_SURFACES)
/* GOWA: an RGBA8 surface read as Y16X16_FLOAT (ld_cast_y16x16) */
#define SRV_CAST_BASE     (UAV_ZCOLOR_BASE + MAX_SURFACES)
#define UAV_CAST_BASE     (SRV_CAST_BASE + MAX_SURFACES * 4u)   /* 4 = LD_CAST_WAYS */
/* GOWA: 1x1x1 white volume for a Texture3D unit with nothing decoded */
#define SRV_WHITE3D       (UAV_CAST_BASE + MAX_SURFACES * 4u)
#define SRV_CPU_HEAP_SLOTS (SRV_WHITE3D + 1u)
#define SRV_TABLE_SIZE   16
#define SRV_RING_TABLES  4096

#define SMP_DEFAULT      0
#define SMP_CACHE_SLOTS  MAX_SAMPLERS
#define SMP_TABLE_SIZE   16
/* A shader-visible SAMPLER heap is hard-capped at 2048 descriptors by D3D12
 * (D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE). SMP_RING_TABLES*SMP_TABLE_SIZE
 * must stay <= 2048, else CreateDescriptorHeap fails. Identical sampler
 * tables share entries, so this is 128 unique tables between GPU flushes. */
#define SMP_RING_TABLES  128

#define CB_VP_BYTES      ((512 + 2) * 16)
#define CB_LUZ_OFS       ((CB_VP_BYTES + 255) & ~255u)   /* GOWA: luzes do EDGE (b2) no fim do bloco */
#define CB_BLOCK_BYTES   (CB_LUZ_OFS + 2048)
#define CB_BLOCK_ALIGNED ((CB_BLOCK_BYTES + 255) & ~255u)
#define CB_RING_BYTES    (CB_BLOCK_ALIGNED * SRV_RING_TABLES)
#define PS_CB_RING_BYTES (CB_LUZ_OFS * SRV_RING_TABLES)
#define LD_VARIANT_SET_CAPACITY (MAX_SHADER_BLOBS * 2u)

#define VERT_STRIDE      (16 * 4 * 4)   /* 16 attrs * float4                  */
#define VERT_BUFFER_SIZE (256u * 1024u * 1024u)
#define INDEX_BUFFER_SIZE (64u * 1024u * 1024u)
#define LD_INVALID_SURFACE 0xFFFFFFFFu

/* gcm texture format bytes (mirror rsx_dispatch.h) */
#define TEX_FMT_B8         0x81
#define TEX_FMT_A1R5G5B5   0x82
#define TEX_FMT_A4R4G4B4   0x83
#define TEX_FMT_R5G6B5     0x84
#define TEX_FMT_A8R8G8B8   0x85
#define TEX_FMT_DXT1       0x86
#define TEX_FMT_DXT23      0x87
#define TEX_FMT_DXT45      0x88
#define TEX_FMT_G8B8       0x8B
#define TEX_FMT_DEPTH24_D8 0x90
#define TEX_FMT_X16        0x94   /* one 16-bit unorm channel */
#define TEX_FMT_LINEAR     0x20
#define TEX_FMT_UNNORM     0x40
#define TEX_FMT_BASE_MASK  0x9F

typedef struct {
    u32 location, offset;
    ID3D12Resource* tex;
    u32 w, h;
    DXGI_FORMAT fmt;
    ID3D12Resource* fb_tex;   /* feedback copy, see surface_feedback_srv */
#if !defined(YZ_PERF_CLEAN)
    u32 resource_serial;
    u32 last_write_generation;
    u32 last_write_kind;
    u32 last_create_generation;
    u32 last_draw_generation;
    u32 last_clear_generation;
    u32 last_copy_generation;
    u32 last_blit_generation;
    u32 last_resolve_generation;
    u32 last_other_generation;
    u32 last_guest_blit_generation;
    u32 last_present_copy_generation;
#endif
} surface_t;
typedef struct {
    u32 location, offset, pitch, width, height;
    int valid;
    int resolve_src;   /* surface slot + 1 a scaled blit resolves into this buffer; 0 = none */
} display_buffer_t;
typedef struct {
    u32 location, offset;
    ID3D12Resource* tex;
    ID3D12Resource* snapshot;
    u32 w, h;
    u32 snapshot_w, snapshot_h;
    D3D12_RESOURCE_STATES snapshot_state;
    int cleared;
    int had_write;
    int snapshot_valid;
    u64 draws;
    u64 depth_test_draws;
    u64 depth_write_draws;
    u64 depth_both_draws;
    int reject_logged;
    /* GOWA: the zeta as the bytes an ARGB8 texture over it reads
     * (ld_zcolor_convert), with the remap of the last bind */
    ID3D12Resource* zcolor;
    u32 zcolor_w, zcolor_h;
    int zcolor_srv;          /* SRV written for the current zcolor */
    u32 snap_gen;            /* bumped each time the snapshot is refreshed */
    u32 zcolor_gen, zcolor_remap;   /* what the ARGB8 view was last built from */
    int zcolor_ok;
} zdepth_t;
typedef struct {
    u32 location, offset, format, width, height, pitch, remap;
    u32 cubemap;
    u32 depth3d;             /* GOWA: slices of a volume texture, 0 = 2D */
    ID3D12Resource* tex;
    u64 content_hash;
    u32 last_hash_frame;
    u64 last_use_serial;
} texcache_t;
typedef struct {
    u32 location, offset, format, width, height, pitch;
    ID3D12Resource* tex;
    u64 content_hash;
    u32 last_hash_frame;
} vtexcache_t;
typedef struct { u64 key; ID3D12PipelineState* pso; u32 last_frame; } psocache_t;   /* GOWA: last_frame = LRU (26/10) */
typedef struct {
    u64 hash;
    u32 source_length;
    char* source;
    ID3DBlob* blob;
} shader_blob_t;
typedef struct {
    shader_blob_t entries[MAX_SHADER_BLOBS];
    u32 count;
    u64 retained_source_bytes;
    u64 retained_blob_bytes;
} shader_blob_cache_t;
typedef struct {
    u32 magic;
    u32 version;
    u32 stage;
    u32 source_length;
    u32 blob_length;
    u32 reserved;
    u64 source_hash;
} shader_disk_cache_header;

typedef struct {
    int              enabled;    /* RSX_DRAW resolved                     */
    int              ready;      /* device + resources up                    */

    ID3D12Device*              dev;
    ID3D12CommandQueue*        queue;
    ID3D12CommandAllocator*    alloc;
    ID3D12GraphicsCommandList* list;
    ID3D12Fence*               fence;
    HANDLE                     fence_event;
    u64                        fence_value;

    IDXGISwapChain3*           swap;
    ID3D12Resource*            backbuf[LD_SWAP_BUFFERS];
    ID3D12DescriptorHeap*      rtv_heap;
    u32                        rtv_step;

    surface_t                  surfaces[MAX_SURFACES];
    u32                        n_surfaces;
    display_buffer_t           display_buffers[8];

    ID3D12DescriptorHeap*      dsv_heap;
    u32                        dsv_step;
    ID3D12Resource*            depth;
    int                        depth_cleared;
    zdepth_t                   zdepths[MAX_SURFACES];
    u32                        n_zdepths;

    ID3D12DescriptorHeap*      srv_cpu_heap;
    ID3D12DescriptorHeap*      srv_heap;
    u32                        srv_step, srv_ring_used;
    ID3D12Resource*            white_tex;
    ID3D12Resource*            white_tex3d;
    texcache_t                 textures[MAX_TEXTURES];
    u32                        n_textures;
    vtexcache_t                vtex[MAX_VTEX];
    u32                        n_vtex;
    ID3D12Resource*            retired_textures[MAX_RETIRED_TEXTURES];
    u32                        n_retired_textures;
    ID3D12Resource*            upload;
    u8*                        upload_mapped;
    u32                        upload_used;

    /* Optional host-movie UI compositor. The guest keeps rendering captions
     * into its ordinary offscreen surface. Each guest flip reads back the
     * latest sparse overlay for blending over each 30 Hz host movie frame. */
    ID3D12Resource*            movie_upload;
    u8*                        movie_upload_mapped;
    ID3D12Resource*            movie_overlay_readback;
    u8*                        movie_overlay_rgba;
    u8*                        movie_overlay_mask;
    u32                        movie_overlay_pitch;
    int                        movie_overlay_valid;
    u64                        movie_overlay_frames;

    ID3D12DescriptorHeap*      smp_cpu_heap;
    ID3D12DescriptorHeap*      smp_heap;
    u32                        smp_step, smp_ring_used;
    u32                        smp_ring_slots[SMP_RING_TABLES][SMP_TABLE_SIZE];
    u64                        smp_keys[SMP_CACHE_SLOTS];
    u16                        smp_hash[2 * SMP_CACHE_SLOTS];   /* GOWA: sampler_slot index */
    u32                        n_samplers;

    ID3D12RootSignature*       rootsig_x;
    ID3D12RootSignature*       depth_snapshot_rootsig;
    ID3D12PipelineState*       depth_snapshot_pso;
    psocache_t                 psos[MAX_PSOS];
    u32                        n_psos;
    shader_blob_cache_t        vs_blobs;
    shader_blob_cache_t        ps_blobs;

    ID3D12Resource*            cb;
    u8*                        cb_mapped;
    u32                        cb_used;

    /* Separate transient b1 arena for buffered fragment constants. It uses
     * the existing constant-buffer ring budget rather than changing any
     * texture or vertex upload-ring capacity. */
    ID3D12Resource*            ps_cb;
    u8*                        ps_cb_mapped;
    u32                        ps_cb_used;
    rsx_fp_constant_block      fp_constants;
    float                      fp_alpha_ref;
    char                       fp_constant_mode;

    ID3D12Resource*            vb;
    u8*                        vb_mapped;
    u32                        vb_used;
    ID3D12Resource*            ib;
    u32*                       ib_mapped;
    u32                        ib_used;

    u32                        width, height;
    /* GOWA_RES_SCALE=N (1..4): internal resolution. Every live surface and
     * zeta is created at N x its guest size; s->w/s->h, viewports in guest
     * units and all the guest-side bookkeeping stay in guest pixels, and
     * LD_PX() converts at the D3D12 boundary (resources, viewport, scissor,
     * copy boxes, present). 720p x 3 = 2160p. */
    u32                        res_scale;
    rsx_live_guest_ptr_fn      guest_ptr;
    void*                      guest_user;

    rsx_dispatch               rsx;
    /*
     * Startup-selected Stage-1 isolation mode.  The hot draw path only reads
     * these bits; environment parsing is completed once in init.
     *
     * L = legacy
     * H = hoisted descriptor/base fetch
     * M = H + masked shader signature/input layout
     * C = M + compact payload/stride
     */
    u32                        vertex_features;
    char                       vertex_mode;
    char                       vertex_diag_dir[MAX_PATH];
} ld_state;

static ld_state g;
#define LD_PX(v) ((v) * g.res_scale)

/* GOWA (24/10): the D3D12 state sink_end_impl last put on g.list, so a draw skips the
 * calls that would set the same thing again (~15 driver calls a draw, ~2,500 draws a
 * frame in chapter 16). Every other place that sets state on g.list and every list
 * Reset calls ld_state_dirty(). GOWA_D3D_FILTER=0 sets everything on every draw. */
static struct {
    int rt_ok, heaps_ok, pso_ok, rs_ok, vpsc_ok, st_ok, topo_ok;
    u32 n_rt, dsv_on;
    D3D12_CPU_DESCRIPTOR_HANDLE rt[4], dsv;
    ID3D12DescriptorHeap* heaps[2];
    ID3D12PipelineState* pso;
    ID3D12RootSignature* rs;
    D3D12_VIEWPORT vp;
    D3D12_RECT sc;
    UINT st;
    /* the last SRV table srv_table built: same slots, no source descriptor rewritten
     * (g_ld_srv_gen) and nothing allocated in the ring since -> the same table */
    int srvt_ok;
    u32 srvt_slots[SRV_TABLE_SIZE], srvt_gen, srvt_used;
    D3D12_GPU_DESCRIPTOR_HANDLE srvt_h;
    /* the descriptor tables bound at root parameters 1 (SRVs) and 2 (samplers); a
     * root signature change forgets them (D3D12 resets root arguments then) */
    int tbl_ok[2];
    UINT64 tbl[2];
} g_ldc;
static u32 g_ld_srv_gen;   /* bumped by every write into the source SRV heap (srv_dst) */
static void ld_state_dirty(void) { memset(&g_ldc, 0, sizeof g_ldc); }

/* GOWA (24/10): the per-slot vertex upload of the draw being recorded (ld_vslots_plan,
 * after draw_ctx): one vertex buffer per layout slot; build_pso/get_pso read the formats */
enum { LD_VS_DEF = 0, LD_VS_RAW = 1, LD_VS_CONV = 2 };
typedef struct { u8 kind, swap; u16 fmt; u32 attr, stride, bytes, sstride, esz; const u8* src; } ld_vslot;
static ld_vslot g_ld_vs[RSX_DSP_NUM_VERTEX_ATTR];
static u32 g_ld_vs_n;
static int g_ld_vs_active;
static int ld_d3d_filter(void)
{
    static int on = -1;
    if (on < 0) { const char* e = getenv("GOWA_D3D_FILTER"); on = !(e && *e == '0'); }
    return on;
}

/* Present with the GOWA_PRESENT_SYNC choice; the time spent inside goes to
 * g_ld_present_qpc ([flush] line of GOWA_FLUSH_STATS). */
static int g_ld_present_sync = 1, g_ld_present_tearing;
/* GOWA (22/10): luz do EDGE na GPU (rsx_edge_luz.h). O anel é escrito pelas threads SPU
 * (gowa_luz_gpu, gowa_offline.cpp) e lido aqui; g_ld_edge_luz marca o desenho atual. */
gowa_luz_slot g_gowa_luz_anel[GOWA_LUZ_ANEL];
volatile long g_gowa_luz_prox;
int g_gowa_luz_gpu;
static int g_ld_edge_luz;
static const gowa_luz_set* g_ld_edge_luz_set;
static u64 g_ld_luz_n[5];   /* desenhos marcados, usados, gen velha, sem atributos 0/4/5, id inválido */
static volatile long long g_ld_present_qpc;
/* GOWA (25/10): medidor de desempenho na tela, como o do MSI Afterburner ou o da
 * Steam. GOWA_OSD=1 liga na partida; F10 liga e desliga. Desligado, custa um teste
 * por apresentação. Vem DESLIGADO por padrão (o launcher liga pela opção dele);
 * GOWA_OSD_TECLA=0 desliga a tecla F10. O desenho é o medidor padrão dos ports Otavix
 * (port/otavix_osd.c): uma janela própria por cima da do jogo, com o texto na resolução
 * real da tela e o tamanho pela referência de 1080p, igual em todos os jogos e nítido
 * qualquer que seja a escala de render. Antes era desenhado aqui no backbuffer, com uma
 * fonte de bitmap 8x14 multiplicada por 1-3, que saía grande e serrilhada. Uso de GPU e
 * VRAM vêm de uma thread que lê a cada segundo os contadores do Windows (os mesmos do
 * Gerenciador de Tarefas) e a DXGI. FPS = quadros que o jogo pediu por segundo (o jogo=
 * do [fps]). */
#include <pdh.h>
#include <pdhmsg.h>   /* PDH_MORE_DATA */
#pragma comment(lib, "pdh")
/* port/otavix_osd.c, o medidor padrão (compilado no executável) */
void otx_osd_iniciar(void);
void otx_osd_texto(const char* linha);
IDXGIAdapter3* g_ld_osd_adapter;
static int s_osd_on = -1, s_osd_f10, s_osd_tecla = -1, s_osd_mostrando;
static char s_osd_txt[192];
static volatile LONG s_osd_gpu_pct = -1, s_osd_vram_mb = -1, s_osd_vram_tot_mb = -1;
static volatile LONG s_osd_temp_cpu = -1, s_osd_temp_gpu = -1, s_osd_temp_cpu_max = -1, s_osd_temp_gpu_max = -1;
LUID g_ld_osd_luid;
/* D3DKMT (gdi32), estruturas de d3dkmthk.h: KMTQAITYPE_ADAPTERPERFDATA = 62, temperatura em décimos de grau */
typedef struct { LUID luid; UINT h; } ld_kmt_abre;
typedef struct { UINT h; int tipo; void* dados; UINT tam; } ld_kmt_consulta;
typedef struct { UINT32 indice; ULONGLONG mf, mmf, mmfoc, mb, pcie; ULONG fan, potencia, temperatura; UCHAR ps; } ld_kmt_perf;
static int ld_osd_temp_gpu(void)
{
    static LONG (APIENTRY *abre)(ld_kmt_abre*); static LONG (APIENTRY *consulta)(const ld_kmt_consulta*);
    static UINT h; static int pronto;
    if (!pronto) {
        pronto = -1;
        HMODULE m = GetModuleHandleA("gdi32.dll");
        if (m) { abre = (LONG (APIENTRY*)(ld_kmt_abre*))GetProcAddress(m, "D3DKMTOpenAdapterFromLuid");
                 consulta = (LONG (APIENTRY*)(const ld_kmt_consulta*))GetProcAddress(m, "D3DKMTQueryAdapterInfo"); }
        ld_kmt_abre a; a.luid = g_ld_osd_luid; a.h = 0;
        if (abre && consulta && (g_ld_osd_luid.LowPart || g_ld_osd_luid.HighPart) && abre(&a) == 0) { h = a.h; pronto = 1; }
    }
    if (pronto != 1) return -1;
    ld_kmt_perf p; memset(&p, 0, sizeof p);
    ld_kmt_consulta q = { h, 62, &p, sizeof p };
    if (consulta(&q) != 0 || !p.temperatura) return -1;
    return (int)(p.temperatura / 10);
}
static DWORD WINAPI ld_osd_amostrador(LPVOID arg)
{
    (void)arg;
    wchar_t cam[128], luid_txt[48];
    swprintf(cam, 128, L"\\GPU Engine(pid_%lu*engtype_3D)\\Utilization Percentage", (unsigned long)GetCurrentProcessId());
    PDH_HQUERY q = NULL; PDH_HCOUNTER c = NULL, ct = NULL; int idade = 0, ct_decimos = 1;
    long ct_ult = -100000; int ct_iguais = 0, ct_parada = 0;
    for (;;) {
        if (!q || ++idade >= 10) {   /* as instâncias por processo aparecem depois: refaz a consulta a cada 10 s */
            if (q) PdhCloseQuery(q);
            q = NULL; c = NULL; ct = NULL; idade = 0;
            if (PdhOpenQueryW(NULL, 0, &q) == ERROR_SUCCESS) {
                if (PdhAddEnglishCounterW(q, cam, 0, &c) != ERROR_SUCCESS) c = NULL;
                /* temperatura da CPU: as zonas térmicas da ACPI (sem driver nem administrador);
                 * em muitos desktops não existem ou ficam paradas, e aí o medidor mostra "--" */
                ct_decimos = 1;
                if (PdhAddEnglishCounterW(q, L"\\Thermal Zone Information(*)\\High Precision Temperature", 0, &ct) != ERROR_SUCCESS) {
                    ct_decimos = 0;
                    if (PdhAddEnglishCounterW(q, L"\\Thermal Zone Information(*)\\Temperature", 0, &ct) != ERROR_SUCCESS) ct = NULL;
                }
                PdhCollectQueryData(q);
            }
        }
        Sleep(1000);
        /* "luid_0x%08X_0x%08X" no nome das instâncias: só os motores do adaptador do jogo
         * (num notebook com duas GPUs, a integrada também aparece para o processo) */
        swprintf(luid_txt, 48, L"luid_0x%08lx_0x%08lx", (unsigned long)g_ld_osd_luid.HighPart, (unsigned long)g_ld_osd_luid.LowPart);
        if (q && PdhCollectQueryData(q) == ERROR_SUCCESS) {
            DWORD tam = 0, n = 0;
            if (c && PdhGetFormattedCounterArrayW(c, PDH_FMT_DOUBLE, &tam, &n, NULL) == PDH_MORE_DATA && tam) {
                PDH_FMT_COUNTERVALUE_ITEM_W* it = (PDH_FMT_COUNTERVALUE_ITEM_W*)malloc(tam);
                if (it && PdhGetFormattedCounterArrayW(c, PDH_FMT_DOUBLE, &tam, &n, it) == ERROR_SUCCESS) {
                    double mx = -1;
                    for (DWORD k = 0; k < n; k++) {
                        wchar_t nome[256]; wcsncpy(nome, it[k].szName ? it[k].szName : L"", 255); nome[255] = 0;
                        for (wchar_t* w = nome; *w; w++) *w = (wchar_t)towlower(*w);
                        if (!g_ld_osd_luid.LowPart && !g_ld_osd_luid.HighPart) { if (it[k].FmtValue.doubleValue > mx) mx = it[k].FmtValue.doubleValue; }
                        else if (wcsstr(nome, luid_txt) && it[k].FmtValue.doubleValue > mx) mx = it[k].FmtValue.doubleValue;
                    }
                    if (mx >= 0) InterlockedExchange(&s_osd_gpu_pct, (LONG)(mx > 100.0 ? 100.0 : mx + 0.5));
                }
                free(it);
            }
            tam = 0; n = 0;
            if (ct && PdhGetFormattedCounterArrayW(ct, PDH_FMT_DOUBLE, &tam, &n, NULL) == PDH_MORE_DATA && tam) {
                PDH_FMT_COUNTERVALUE_ITEM_W* it = (PDH_FMT_COUNTERVALUE_ITEM_W*)malloc(tam);
                if (it && PdhGetFormattedCounterArrayW(ct, PDH_FMT_DOUBLE, &tam, &n, it) == ERROR_SUCCESS) {
                    double mx = -1000;
                    for (DWORD k = 0; k < n; k++) {
                        const double cel = (ct_decimos ? it[k].FmtValue.doubleValue / 10.0 : it[k].FmtValue.doubleValue) - 273.15;
                        if (cel > mx) mx = cel;
                    }
                    /* zona parada: em muitos desktops a ACPI devolve um valor fixo, que não é
                     * o processador (28 graus com a CPU a 70% no PC teste 2). 90 s sem mudar
                     * nem um décimo -> o medidor mostra "--" e o JSON não grava a máxima */
                    if (mx > 5.0 && mx < 125.0 && !ct_parada) {
                        const long dec = (long)(mx * 10.0 + 0.5);
                        if (dec != ct_ult) { ct_ult = dec; ct_iguais = 0; }
                        else if (++ct_iguais >= 90) {
                            ct_parada = 1;
                            InterlockedExchange(&s_osd_temp_cpu, -1);
                            InterlockedExchange(&s_osd_temp_cpu_max, -1);
                        }
                    }
                    if (mx > 5.0 && mx < 125.0 && !ct_parada) {
                        InterlockedExchange(&s_osd_temp_cpu, (LONG)(mx + 0.5));
                        if ((LONG)(mx + 0.5) > s_osd_temp_cpu_max) InterlockedExchange(&s_osd_temp_cpu_max, (LONG)(mx + 0.5));
                    }
                }
                free(it);
            }
        }
        { const int tg = ld_osd_temp_gpu();
          if (tg > 0 && tg < 125) { InterlockedExchange(&s_osd_temp_gpu, tg); if (tg > s_osd_temp_gpu_max) InterlockedExchange(&s_osd_temp_gpu_max, tg); } }
        if (g_ld_osd_adapter) {
            DXGI_QUERY_VIDEO_MEMORY_INFO mi;
            if (SUCCEEDED(g_ld_osd_adapter->lpVtbl->QueryVideoMemoryInfo(g_ld_osd_adapter, 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &mi))) {
                InterlockedExchange(&s_osd_vram_mb, (LONG)(mi.CurrentUsage >> 20));
                InterlockedExchange(&s_osd_vram_tot_mb, (LONG)(mi.Budget >> 20));
            }
        }
    }
}
/* o texto, refeito a cada 500 ms (devolve 1 quando refez): FPS do jogo, tempo de quadro,
 * CPU (sistema), GPU, VRAM, RAM, no formato do medidor padrão */
static int ld_osd_texto(void)
{
    static LONGLONG t0; static u32 f0; static ULONGLONG i0, k0, u0;
    LARGE_INTEGER agora, fr; QueryPerformanceCounter(&agora); QueryPerformanceFrequency(&fr);
    if (t0 && (agora.QuadPart - t0) * 2 < fr.QuadPart) return 0;
    extern u32 cellGcm_flip_request_count(void);
    const u32 f1 = cellGcm_flip_request_count();
    FILETIME fi, fk, fu; ULONGLONG i1 = 0, k1 = 0, u1 = 0;
    if (GetSystemTimes(&fi, &fk, &fu)) {
        i1 = ((ULONGLONG)fi.dwHighDateTime << 32) | fi.dwLowDateTime;
        k1 = ((ULONGLONG)fk.dwHighDateTime << 32) | fk.dwLowDateTime;
        u1 = ((ULONGLONG)fu.dwHighDateTime << 32) | fu.dwLowDateTime;
    }
    if (t0) {
        const double seg = (double)(agora.QuadPart - t0) / (double)fr.QuadPart;
        const double fps = (f1 - f0) / seg;
        const ULONGLONG tot = (k1 - k0) + (u1 - u0), oc = tot - (i1 - i0);
        const int cpu = tot ? (int)(100.0 * (double)oc / (double)tot + 0.5) : 0;
        char gpu[16] = "--", vram[32] = "", tc[12] = "", tg[12] = "";
        if (s_osd_gpu_pct >= 0) snprintf(gpu, sizeof gpu, "%ld%%", (long)s_osd_gpu_pct);
        if (s_osd_vram_mb >= 0) snprintf(vram, sizeof vram, "   VRAM %.1f/%.1f GB", s_osd_vram_mb / 1024.0, s_osd_vram_tot_mb / 1024.0);
        if (s_osd_temp_cpu > 0) snprintf(tc, sizeof tc, " %ld\xC2\xB0" "C", (long)s_osd_temp_cpu);   /* °C em UTF-8 */
        if (s_osd_temp_gpu > 0) snprintf(tg, sizeof tg, " %ld\xC2\xB0" "C", (long)s_osd_temp_gpu);
        char ram[32] = "";   /* RAM do sistema: em uso / total, como o Gerenciador de Tarefas */
        { MEMORYSTATUSEX ms; ms.dwLength = sizeof ms;
          if (GlobalMemoryStatusEx(&ms))
              snprintf(ram, sizeof ram, "   RAM %.1f/%.1f GB", (double)(ms.ullTotalPhys - ms.ullAvailPhys) / 1073741824.0,
                       (double)ms.ullTotalPhys / 1073741824.0); }
        snprintf(s_osd_txt, sizeof s_osd_txt, "FPS %.1f %.1f ms   CPU %d%%%s   GPU %s%s%s%s",
                 fps, fps > 0.05 ? 1000.0 / fps : 0.0, cpu, tc, gpu, tg, vram, ram);
    } else snprintf(s_osd_txt, sizeof s_osd_txt, "FPS --");
    t0 = agora.QuadPart; f0 = f1; i0 = i1; k0 = k1; u0 = u1;
    return 1;
}
static void ld_osd_atualiza(void)
{
    if (s_osd_on < 0) { const char* e = getenv("GOWA_OSD"); s_osd_on = e && *e == '1';
                        e = getenv("GOWA_OSD_TECLA"); s_osd_tecla = !(e && *e == '0'); }
    if (s_osd_tecla) { const int f10 = (GetAsyncKeyState(VK_F10) & 0x8000) != 0;
      HWND w = GetForegroundWindow(); DWORD pid = 0; if (w) GetWindowThreadProcessId(w, &pid);
      if (f10 && !s_osd_f10 && pid == GetCurrentProcessId()) s_osd_on = !s_osd_on;
      s_osd_f10 = f10; }
    if (!s_osd_on) {
        if (s_osd_mostrando) { otx_osd_texto(NULL); s_osd_mostrando = 0; }
        return;
    }
    static int iniciado;
    if (!iniciado) {
        iniciado = 1;
        otx_osd_iniciar();
        CreateThread(NULL, 0, ld_osd_amostrador, NULL, 0, NULL);
        fprintf(stderr, "[osd] medidor na tela pronto (F10 liga e desliga)\n");
    }
    const int refeito = ld_osd_texto();
    if (refeito || !s_osd_mostrando) { otx_osd_texto(s_osd_txt); s_osd_mostrando = 1; }
}
static HRESULT ld_swap_present(void)
{
    ld_osd_atualiza();
    LARGE_INTEGER a, b; QueryPerformanceCounter(&a);
    const HRESULT hr = g.swap->lpVtbl->Present(g.swap, g_ld_present_sync ? 1 : 0,
                                               (!g_ld_present_sync && g_ld_present_tearing) ? DXGI_PRESENT_ALLOW_TEARING : 0);
    QueryPerformanceCounter(&b); g_ld_present_qpc += b.QuadPart - a.QuadPart;
    /* GOWA: non-S_OK presents (occluded, etc.), counted every 5 s */
    { static long n_ok, n_other; static HRESULT last_other; static ULONGLONG t0;
      if (hr == S_OK) n_ok++; else { n_other++; last_other = hr; }
      const ULONGLONG now = GetTickCount64();
      if (!t0) t0 = now;
      if (now - t0 >= 5000) {
          if (n_other) fprintf(stderr, "[present-hr] 5 s: %ld S_OK, %ld outros (ultimo 0x%08lX)\n",
                               n_ok, n_other, (unsigned long)last_other);
          n_ok = n_other = 0; t0 = now;
      } }
    return hr;
}

/* GOWA_LD_PIPE (default on; =0 off): two sets of per-submission resources (allocator, upload /
 * vertex / index / constant rings, shader-visible descriptor heaps) used in
 * turn, so ld_flush does not wait for the GPU to finish the submission it
 * just made -- only for the one before it, which used the other set. The RSX
 * thread records frame N+1 while the GPU draws frame N (it waited for every
 * frame before, ~2 ms at 720p, more at 4K). Flushes whose caller reads the
 * result (readback, guest reference, movie, shutdown) still wait. */
typedef struct {
    ID3D12CommandAllocator* alloc;
    ID3D12Resource *upload, *vb, *ib, *cb, *ps_cb;
    u8 *upload_mapped, *vb_mapped, *cb_mapped, *ps_cb_mapped;
    u32* ib_mapped;
    ID3D12DescriptorHeap *srv_heap, *smp_heap;
    u64 fence;
    ID3D12Resource** retired;
    u32 n_retired;
} ld_ring_set;
static ld_ring_set s_sets[2];
static u32 s_cur_set;
static int s_pipe;      /* 1 when both sets exist */
static void ld_set_save(u32 i)
{
    ld_ring_set* r = &s_sets[i];
    r->alloc = g.alloc; r->upload = g.upload; r->vb = g.vb; r->ib = g.ib; r->cb = g.cb; r->ps_cb = g.ps_cb;
    r->upload_mapped = g.upload_mapped; r->vb_mapped = g.vb_mapped; r->cb_mapped = g.cb_mapped;
    r->ps_cb_mapped = g.ps_cb_mapped; r->ib_mapped = g.ib_mapped;
    r->srv_heap = g.srv_heap; r->smp_heap = g.smp_heap;
}
static void ld_set_load(u32 i)
{
    const ld_ring_set* r = &s_sets[i];
    g.alloc = r->alloc; g.upload = r->upload; g.vb = r->vb; g.ib = r->ib; g.cb = r->cb; g.ps_cb = r->ps_cb;
    g.upload_mapped = r->upload_mapped; g.vb_mapped = r->vb_mapped; g.cb_mapped = r->cb_mapped;
    g.ps_cb_mapped = r->ps_cb_mapped; g.ib_mapped = r->ib_mapped;
    g.srv_heap = r->srv_heap; g.smp_heap = r->smp_heap;
}
static void ld_set_release_retired(u32 i)
{
    ld_ring_set* r = &s_sets[i];
    for (u32 k = 0; k < r->n_retired; k++) r->retired[k]->lpVtbl->Release(r->retired[k]);
    r->n_retired = 0;
}

#define LD_VERTEX_HOIST_FETCH       (1u << 0)
#define LD_VERTEX_MASK_SIGNATURE    (1u << 1)
#define LD_VERTEX_COMPACT_PAYLOAD   (1u << 2)

static int ld_vertex_hoist_fetch(void)
{
    return (g.vertex_features & LD_VERTEX_HOIST_FETCH) != 0;
}

static int ld_vertex_mask_signature(void)
{
    return (g.vertex_features & LD_VERTEX_MASK_SIGNATURE) != 0;
}

static int ld_vertex_compact_payload(void)
{
    return (g.vertex_features & LD_VERTEX_COMPACT_PAYLOAD) != 0;
}

static const char* ld_vertex_mode_name(void)
{
    switch (g.vertex_mode) {
    case 'H': return "H-hoist";
    case 'M': return "M-mask";
    case 'C': return "C-compact";
    default:  return "L-legacy";
    }
}

static int ld_fp_constants_buffered(void)
{
    return g.fp_constant_mode == 'B';
}

static const char* ld_fp_constant_mode_name(void)
{
    return ld_fp_constants_buffered() ? "buffered" : "literal";
}

static int g_ld_dred_dumped = 0;
static u32 g_ld_frames = 0;
/* GOWA_ARM_TEX=<hex>: g_ld_arm_frame = the frame in which texture unit 0 is
 * first pointed at that offset (a scene marker that does not drift between
 * runs, unlike frame numbers); "+n" FROM values count from it. */
static int g_ld_arm_frame = -1;
/* GOWA: getenv once per name (the names are string literals; the environment
 * does not change after boot). ld_from_env runs on every draw (get_pso), and
 * getenv takes the CRT lock and walks the whole environment each time. */
static const char* ld_getenv_once(const char* name)
{
    static const char* volatile s_k[32];
    static const char* s_v[32];
    static volatile LONG s_n;
    const LONG n = s_n < 32 ? s_n : 32;
    for (LONG i = 0; i < n; i++)
        if (s_k[i] == name) return s_v[i];
    const char* v = getenv(name);
    const LONG i = InterlockedIncrement(&s_n) - 1;
    if (i < 32) { s_v[i] = v; MemoryBarrier(); s_k[i] = name; }
    return v;
}
static int ld_from_env(const char* name)
{
    const char* e = ld_getenv_once(name);
    if (!e) return 0;
    if (*e == '+') return g_ld_arm_frame < 0 ? 0x7FFFFFFF : g_ld_arm_frame + atoi(e + 1);
    /* GOWA: "<n>s" = from that many seconds after the first call (menus run
     * at 65-230 FPS, so a frame number lands on a different scene per run) */
    if (strchr(e, 's')) {
        static ULONGLONG t0;
        if (!t0) t0 = GetTickCount64();
        return (double)(GetTickCount64() - t0) >= atof(e) * 1000.0 ? 0 : 0x7FFFFFFF;
    }
    return atoi(e);
}
static ID3D12InfoQueue* g_ld_info_queue = NULL;
static int g_ld_debug_layer_enabled = 0;

typedef struct {
    int valid;
    u64 key;
    u64 vp_hash;
    u32 vp_start, vp_instrs;
    u32 fp_location, fp_offset, fp_size, fp_control;
    u32 cube_mask, vtex_mask, txl_mask;
    u32 input_mask, input_stride;
    u32 packed_offsets;
    u32 shadow_mask;   /* GOWA: depth-compare units (fp_texwin uploaded) */
} ld_pso_metadata;

static ld_pso_metadata g_ld_current_pso;
static u64 g_ld_flip_requested = 0;
static u64 g_ld_flip_consumed = 0;
static u32 g_ld_last_requested_buffer = UINT32_MAX;
static u32 g_ld_last_consumed_buffer = UINT32_MAX;
static u32 g_ld_last_present_target = UINT32_MAX;
static u64 g_ld_last_dump_fingerprint = 0;
#if !defined(YZ_PERF_CLEAN)
static u32 g_ld_surface_generation = 0;
static u32 g_ld_surface_resource_serial = 0;
static u32 g_ld_guest_blit_generation = 0;
static u32 g_ld_present_copy_generation = 0;
static u64 g_ld_vertex_constant_ring_recycles = 0;
static u32 g_ld_frame_vertex_constant_ring_recycles = 0;
static u32 g_ld_fifo_get = 0;
static u32 g_ld_fifo_put = 0;
static volatile LONG g_ld_diag_post_movie_pending = 0;
static volatile LONG g_ld_diag_post_movie_presents = 0;

enum {
    LD_SURFACE_WRITE_NONE = 0,
    LD_SURFACE_WRITE_CREATE = 1,
    LD_SURFACE_WRITE_DRAW = 2,
    LD_SURFACE_WRITE_CLEAR = 3,
    LD_SURFACE_WRITE_COPY = 4,
    LD_SURFACE_WRITE_BLIT = 5,
    LD_SURFACE_WRITE_RESOLVE = 6,
    LD_SURFACE_WRITE_OTHER = 7
};

/* GOWA: main-memory (location 1) render targets written by the GPU since
 * their last write-back -- see rsx_live_draw_sync_mainmem. */
static u8 g_ld_mm_dirty[MAX_SURFACES];
/* GOWA: surfaces that ever received COLOUR (a draw with a colour target
 * enabled and a non-zero colour mask, a colour clear, a copy). A depth-only
 * pass still names a colour offset -- God of War Ascension's scene zeta
 * 0x11B0000 has one, never written -- and a texture over that address must
 * read the zeta, not that black surface (ld_zcolor_convert). */
static u8 g_ld_color_written[MAX_SURFACES];
typedef struct { u32 frame, seq, tgt, cw, ch, sc0, sc1, cmask, tex[3], tw[3], th[3], fmt, pitch, fp, ntri; float v[6]; u64 key; u32 blend, cmask2, vp0, vp1; } ld_chain_ev;
static ld_chain_ev s_chain_ev[48];
static u32 s_chain_n, s_chain_seq;
static int ld_chain_on(void)
{
    static int on = -1;
    if (on < 0) on = getenv("GOWA_CHAIN_RING") ? 1 : 0;
    return on;
}
static int ld_chain_hit(u32 off) { return off == 0x024A0000u || off == 0x02A29800u || off == 0x00B10000u; }
static volatile int g_ld_surf_dump_req;   /* 1 = bad frame, 2 = good one (LD_SURF_DUMP_ON_BAD) */
static void ld_surface_note_write(u32 target, u32 kind)
{
    if (target >= MAX_SURFACES)
        return;
    surface_t* surface = &g.surfaces[target];
    if (kind != LD_SURFACE_WRITE_DRAW && ld_chain_on() && ld_chain_hit(surface->offset)) {
        ld_chain_ev ev; memset(&ev, 0, sizeof ev);
        ev.frame = g_ld_frames; ev.seq = ++s_chain_seq; ev.tgt = surface->offset;
        ev.cw = surface->w; ev.ch = surface->h; ev.fmt = 0xEE000000u | kind;
        s_chain_ev[s_chain_n++ % 48u] = ev;
    }
    if (surface->location == 1 && kind != LD_SURFACE_WRITE_CREATE)
        g_ld_mm_dirty[target] = 1;
    const u32 generation = ++g_ld_surface_generation;
    surface->last_write_generation = generation;
    surface->last_write_kind = kind;
    /* colour arrives by a clear, a copy, a blit or a resolve; a draw marks it
     * itself (it knows the colour mask); a new resource starts without */
    g_ld_color_written[target] = kind == LD_SURFACE_WRITE_CREATE ? 0 :
        (kind == LD_SURFACE_WRITE_DRAW ? g_ld_color_written[target] : 1);
    switch (kind) {
    case LD_SURFACE_WRITE_CREATE:
        surface->last_create_generation = generation;
        break;
    case LD_SURFACE_WRITE_DRAW:
        surface->last_draw_generation = generation;
        break;
    case LD_SURFACE_WRITE_CLEAR:
        surface->last_clear_generation = generation;
        break;
    case LD_SURFACE_WRITE_COPY:
        surface->last_copy_generation = generation;
        break;
    case LD_SURFACE_WRITE_BLIT:
        surface->last_blit_generation = generation;
        break;
    case LD_SURFACE_WRITE_RESOLVE:
        surface->last_resolve_generation = generation;
        break;
    default:
        surface->last_other_generation = generation;
        break;
    }
}
#else
#define ld_surface_note_write(target, kind) ((void)0)
#endif

#define LD_LAYOUT_CACHE_CAP 256u
typedef struct {
    int valid;
    rsx_vertex_layout_plan plan;
} ld_layout_cache_entry;

static ld_layout_cache_entry g_ld_layout_cache[LD_LAYOUT_CACHE_CAP];
static u32 g_ld_layout_cache_count = 0;

static void ld_layout_plan_get(u32 mask, rsx_vertex_layout_plan* out)
{
    mask &= 0xFFFFu;
    for (u32 i = 0; i < g_ld_layout_cache_count; i++) {
        if (g_ld_layout_cache[i].valid &&
            g_ld_layout_cache[i].plan.mask == mask) {
            *out = g_ld_layout_cache[i].plan;
            return;
        }
    }
    rsx_vertex_layout_plan_init(out, mask);
    if (g_ld_layout_cache_count < LD_LAYOUT_CACHE_CAP) {
        ld_layout_cache_entry* entry =
            &g_ld_layout_cache[g_ld_layout_cache_count++];
        entry->valid = 1;
        entry->plan = *out;
    }
}

#define LD_RECENT_DRAW_CAP 128u
typedef struct {
    u64 serial;
    u64 pso_key;
    u64 descriptor_signature;
    u32 frame;
    u32 vp_start, vp_instrs;
    u32 fp_location, fp_offset, fp_size, fp_control;
    u32 cube_mask, vtex_mask, txl_mask, texture_mask;
    u32 primitive, vertices, target, zslot;
    u32 clip_x, clip_y, clip_w, clip_h;
    u32 srv_ring_used, sampler_ring_used, cb_used, vb_used;
} ld_recent_draw;

static ld_recent_draw g_ld_recent_draws[LD_RECENT_DRAW_CAP];
static u64 g_ld_recent_draw_total = 0;

/*
 * Isolated pre-cleanup benchmark measurement. Record only successful
 * swap-chain presents: one QPC call and fixed-ring stores, with no allocation,
 * configuration lookup, sorting, or logging in the presentation path.
 */
#define LD_PRESENT_RING_CAP 16384u
typedef struct {
    u64 present_id;
    u32 guest_frame;
    LONGLONG qpc;
    u64 process_kernel_100ns;
    u64 process_user_100ns;
    u64 present_thread_kernel_100ns;
    u64 present_thread_user_100ns;
    u32 present_thread_id;
} ld_present_sample;
static ld_present_sample g_ld_present_ring[LD_PRESENT_RING_CAP];
static u64 g_ld_present_total = 0;
static LONGLONG g_ld_qpc_frequency = 0;
static int g_ld_present_dumped = 0;
static int g_ld_schedule_diag = 0;
#if defined(YZ_PPU_SAMPLE)
static HANDLE g_ld_present_thread_handle = NULL;
#endif

static void ld_present_measure_dump(void);
#if defined(YZ_PERF_PROFILE)
extern void spu_perf_window_begin(u32 guest_frame);
extern void spu_perf_window_dump(u32 guest_frame);
extern void spu_perf_frame_sample_record(u32 guest_frame);
extern void spu_perf_frame_sample_dump(u32 guest_start, u32 guest_end);
#endif
#if defined(YZ_PPU_SAMPLE)
extern void yz_ppu_perf_window_begin(u32 guest_frame);
extern void yz_ppu_perf_window_dump(u32 guest_frame);
#endif

static void ld_present_measure_init(void)
{
    LARGE_INTEGER frequency;
    memset(g_ld_present_ring, 0, sizeof(g_ld_present_ring));
    g_ld_present_total = 0;
    g_ld_present_dumped = 0;
    g_ld_schedule_diag = getenv("RSX_SCHEDULE_DIAG") != NULL;
    if (QueryPerformanceFrequency(&frequency))
        g_ld_qpc_frequency = frequency.QuadPart;
    else
        g_ld_qpc_frequency = 0;
}

static void ld_present_measure_record(u32 guest_frame)
{
#if defined(YZ_PPU_SAMPLE)
    if (!g_ld_present_thread_handle) {
        HANDLE duplicate = NULL;
        if (DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
                            GetCurrentProcess(), &duplicate, 0, FALSE,
                            DUPLICATE_SAME_ACCESS)) {
            HANDLE previous = (HANDLE)InterlockedCompareExchangePointer(
                (PVOID volatile*)&g_ld_present_thread_handle, duplicate, NULL);
            if (previous)
                CloseHandle(duplicate);
        }
    }
#endif
    LARGE_INTEGER now;
    if (!QueryPerformanceCounter(&now))
        return;
    const u64 present_id = g_ld_present_total + 1u;
    ld_present_sample* sample =
        &g_ld_present_ring[(present_id - 1u) & (LD_PRESENT_RING_CAP - 1u)];
    sample->present_id = present_id;
    sample->guest_frame = guest_frame;
    sample->qpc = now.QuadPart;
    if (g_ld_schedule_diag) {
        FILETIME creation = {0}, exit = {0}, kernel = {0}, user = {0};
        ULARGE_INTEGER value;
        if (GetProcessTimes(GetCurrentProcess(), &creation, &exit,
                            &kernel, &user)) {
            value.LowPart = kernel.dwLowDateTime;
            value.HighPart = kernel.dwHighDateTime;
            sample->process_kernel_100ns = value.QuadPart;
            value.LowPart = user.dwLowDateTime;
            value.HighPart = user.dwHighDateTime;
            sample->process_user_100ns = value.QuadPart;
        }
        if (GetThreadTimes(GetCurrentThread(), &creation, &exit,
                           &kernel, &user)) {
            value.LowPart = kernel.dwLowDateTime;
            value.HighPart = kernel.dwHighDateTime;
            sample->present_thread_kernel_100ns = value.QuadPart;
            value.LowPart = user.dwLowDateTime;
            value.HighPart = user.dwHighDateTime;
            sample->present_thread_user_100ns = value.QuadPart;
        }
        sample->present_thread_id = GetCurrentThreadId();
    }
    g_ld_present_total = present_id;
#if defined(YZ_PPU_SAMPLE)
    {
        static int sample_present = -1;
        static u32 sample_start = 1578u;
        static u32 sample_end = 2178u;
        if (sample_present < 0) {
            sample_present =
                getenv("PPU_SAMPLE_RUN") != NULL &&
                getenv("PPU_SAMPLE_PRESENT") != NULL;
            const char* start_text = getenv("PPU_SAMPLE_START");
            const char* frames_text = getenv("PPU_SAMPLE_FRAMES");
            if (start_text) {
                const unsigned long parsed = strtoul(start_text, NULL, 0);
                if (parsed <= UINT32_MAX)
                    sample_start = (u32)parsed;
            }
            if (frames_text) {
                unsigned long parsed = strtoul(frames_text, NULL, 0);
                if (parsed < 4u) parsed = 4u;
                if (parsed > 600u) parsed = 600u;
                sample_end = sample_start + (u32)parsed;
            }
        }
        const int game_present = guest_frame == g_ld_frames + 1u;
#if defined(YZ_PERF_PROFILE)
        if (sample_present && game_present &&
            guest_frame >= sample_start && guest_frame <= sample_end)
            spu_perf_frame_sample_record(guest_frame);
#endif
        if (sample_present && game_present && guest_frame == sample_start)
            yz_ppu_perf_window_begin(guest_frame);
        else if (sample_present && game_present && guest_frame == sample_end) {
            yz_ppu_perf_window_dump(guest_frame);
#if defined(YZ_PERF_PROFILE)
            spu_perf_frame_sample_dump(sample_start, sample_end);
#endif
        }
    }
#endif
#if defined(YZ_PERF_PROFILE)
    if (guest_frame == 1578u)
        spu_perf_window_begin(guest_frame);
    else if (guest_frame == 1582u)
        spu_perf_window_dump(guest_frame);
#endif
    /*
     * The controlled A010 benchmark ends at guest frame 2293.  Flush the
     * already-recorded fixed ring exactly at that endpoint so an unrelated
     * host-window teardown cannot truncate the authoritative raw samples.
     * This is outside the measured timestamp operation and runs once.
     */
    if (guest_frame == 2293u)
        ld_present_measure_dump();
}

static void ld_present_measure_dump(void)
{
    if (g_ld_present_dumped || g_ld_present_total == 0 ||
        g_ld_qpc_frequency <= 0)
        return;
    g_ld_present_dumped = 1;

    char path[256];
    snprintf(path, sizeof(path), "scratch\\present_qpc_%lu.csv",
             (unsigned long)GetCurrentProcessId());
    FILE* f = fopen(path, "wb");
    if (!f)
        return;

    fprintf(f, "# qpc_frequency=%lld\n", g_ld_qpc_frequency);
    if (g_ld_schedule_diag) {
        fprintf(f, "present_id,guest_frame,qpc,process_kernel_100ns,"
                   "process_user_100ns,present_thread_kernel_100ns,"
                   "present_thread_user_100ns,present_thread_id\n");
    } else {
        fprintf(f, "present_id,guest_frame,qpc\n");
    }
    const u64 first =
        g_ld_present_total > LD_PRESENT_RING_CAP
            ? g_ld_present_total - LD_PRESENT_RING_CAP + 1u : 1u;
    for (u64 present_id = first; present_id <= g_ld_present_total;
         ++present_id) {
        const ld_present_sample* sample =
            &g_ld_present_ring[(present_id - 1u) &
                               (LD_PRESENT_RING_CAP - 1u)];
        if (sample->present_id == present_id) {
            if (g_ld_schedule_diag) {
                fprintf(f, "%llu,%u,%lld,%llu,%llu,%llu,%llu,%u\n",
                        (unsigned long long)sample->present_id,
                        sample->guest_frame, sample->qpc,
                        (unsigned long long)sample->process_kernel_100ns,
                        (unsigned long long)sample->process_user_100ns,
                        (unsigned long long)sample->present_thread_kernel_100ns,
                        (unsigned long long)sample->present_thread_user_100ns,
                        sample->present_thread_id);
            } else {
                fprintf(f, "%llu,%u,%lld\n",
                        (unsigned long long)sample->present_id,
                        sample->guest_frame, sample->qpc);
            }
        }
    }
    fclose(f);
    fprintf(stderr,
            "[present-qpc] preserved %llu successful presents at %lld Hz in %s\n",
            (unsigned long long)(g_ld_present_total - first + 1u),
            g_ld_qpc_frequency, path);
}

static u64 g_ld_texture_cache_full = 0;
static u64 g_ld_texture_use_serial = 0;

static const char* ld_d3d_severity_name(D3D12_MESSAGE_SEVERITY severity)
{
    switch (severity) {
        case D3D12_MESSAGE_SEVERITY_CORRUPTION: return "corruption";
        case D3D12_MESSAGE_SEVERITY_ERROR: return "error";
        case D3D12_MESSAGE_SEVERITY_WARNING: return "warning";
        case D3D12_MESSAGE_SEVERITY_INFO: return "info";
        default: return "message";
    }
}

static void ld_drain_info_queue(const char* where)
{
    if (!g_ld_info_queue) return;
    const u64 count =
        g_ld_info_queue->lpVtbl->GetNumStoredMessages(g_ld_info_queue);
    for (u64 i = 0; i < count; i++) {
        SIZE_T bytes = 0;
        if (FAILED(g_ld_info_queue->lpVtbl->GetMessage(
                g_ld_info_queue, i, NULL, &bytes)) ||
            !bytes || bytes > 1024u * 1024u)
            continue;
        D3D12_MESSAGE* message = (D3D12_MESSAGE*)malloc(bytes);
        if (!message) break;
        if (SUCCEEDED(g_ld_info_queue->lpVtbl->GetMessage(
                g_ld_info_queue, i, message, &bytes))) {
            fprintf(stderr,
                    "[d3d-debug] where=%s frame=%u severity=%s id=%u %s\n",
                    where, g_ld_frames,
                    ld_d3d_severity_name(message->Severity),
                    (unsigned)message->ID,
                    message->pDescription
                        ? message->pDescription : "<no description>");
        }
        free(message);
    }
    if (count)
        g_ld_info_queue->lpVtbl->ClearStoredMessages(g_ld_info_queue);
}

static void ld_dump_recent_draws(void)
{
    const u64 first =
        g_ld_recent_draw_total > LD_RECENT_DRAW_CAP
            ? g_ld_recent_draw_total - LD_RECENT_DRAW_CAP + 1u : 1u;
    fprintf(stderr,
            "[draw-history] total=%llu retained=%llu..%llu\n",
            (unsigned long long)g_ld_recent_draw_total,
            (unsigned long long)first,
            (unsigned long long)g_ld_recent_draw_total);
    for (u64 serial = first; serial <= g_ld_recent_draw_total; serial++) {
        const ld_recent_draw* draw =
            &g_ld_recent_draws[(serial - 1u) & (LD_RECENT_DRAW_CAP - 1u)];
        if (draw->serial != serial) continue;
        fprintf(stderr,
                "[draw-history] serial=%llu frame=%u key=%016llX "
                "vp=%u+%u fp=%u:0x%X+%u ctrl=0x%X "
                "mask{tex=%04X cube=%04X vtex=%04X txl=%04X} "
                "prim=%u verts=%u target=%u z=%u "
                "clip=%u,%u %ux%u desc=%016llX "
                "ring{srv=%u smp=%u cb=%u vb=%u}\n",
                (unsigned long long)draw->serial, draw->frame,
                (unsigned long long)draw->pso_key,
                draw->vp_start, draw->vp_instrs,
                draw->fp_location, draw->fp_offset, draw->fp_size,
                draw->fp_control, draw->texture_mask, draw->cube_mask,
                draw->vtex_mask, draw->txl_mask, draw->primitive,
                draw->vertices, draw->target, draw->zslot,
                draw->clip_x, draw->clip_y, draw->clip_w, draw->clip_h,
                (unsigned long long)draw->descriptor_signature,
                draw->srv_ring_used, draw->sampler_ring_used,
                draw->cb_used, draw->vb_used);
    }
}

static const char* ld_dred_op_name(D3D12_AUTO_BREADCRUMB_OP op)
{
    switch (op) {
        case D3D12_AUTO_BREADCRUMB_OP_DRAWINSTANCED: return "DrawInstanced";
        case D3D12_AUTO_BREADCRUMB_OP_COPYBUFFERREGION: return "CopyBufferRegion";
        case D3D12_AUTO_BREADCRUMB_OP_COPYTEXTUREREGION: return "CopyTextureRegion";
        case D3D12_AUTO_BREADCRUMB_OP_COPYRESOURCE: return "CopyResource";
        case D3D12_AUTO_BREADCRUMB_OP_CLEARRENDERTARGETVIEW: return "ClearRTV";
        case D3D12_AUTO_BREADCRUMB_OP_CLEARDEPTHSTENCILVIEW: return "ClearDSV";
        case D3D12_AUTO_BREADCRUMB_OP_RESOURCEBARRIER: return "ResourceBarrier";
        case D3D12_AUTO_BREADCRUMB_OP_PRESENT: return "Present";
        case D3D12_AUTO_BREADCRUMB_OP_BEGINSUBMISSION: return "BeginSubmission";
        case D3D12_AUTO_BREADCRUMB_OP_ENDSUBMISSION: return "EndSubmission";
        default: return "other";
    }
}

static void ld_enable_dred(void)
{
    ID3D12DeviceRemovedExtendedDataSettings* settings = NULL;
    const HRESULT hr = D3D12GetDebugInterface(
        &IID_ID3D12DeviceRemovedExtendedDataSettings, (void**)&settings);
    if (FAILED(hr) || !settings) {
        fprintf(stderr, "[dred] settings unavailable hr=0x%08lX\n",
                (unsigned long)hr);
        return;
    }
    settings->lpVtbl->SetAutoBreadcrumbsEnablement(
        settings, D3D12_DRED_ENABLEMENT_FORCED_ON);
    settings->lpVtbl->SetPageFaultEnablement(
        settings, D3D12_DRED_ENABLEMENT_FORCED_ON);
    settings->lpVtbl->Release(settings);
    fprintf(stderr, "[dred] auto-breadcrumbs and page-fault tracking enabled\n");
}

static void ld_enable_debug_layer(void)
{
    if (!getenv("RSX_D3D_DEBUG")) return;
    ID3D12Debug* debug = NULL;
    const HRESULT hr = D3D12GetDebugInterface(
        &IID_ID3D12Debug, (void**)&debug);
    if (FAILED(hr) || !debug) {
        fprintf(stderr,
                "[d3d-debug] validation layer unavailable hr=0x%08lX\n",
                (unsigned long)hr);
        return;
    }
    debug->lpVtbl->EnableDebugLayer(debug);
    debug->lpVtbl->Release(debug);
    g_ld_debug_layer_enabled = 1;
    fprintf(stderr, "[d3d-debug] validation layer enabled\n");
}

static void ld_open_info_queue(void)
{
    if (!g_ld_debug_layer_enabled || !g.dev) return;
    const HRESULT hr = g.dev->lpVtbl->QueryInterface(
        g.dev, &IID_ID3D12InfoQueue, (void**)&g_ld_info_queue);
    if (FAILED(hr) || !g_ld_info_queue) {
        fprintf(stderr,
                "[d3d-debug] info queue unavailable hr=0x%08lX\n",
                (unsigned long)hr);
        return;
    }
    D3D12_MESSAGE_SEVERITY severities[] = {
        D3D12_MESSAGE_SEVERITY_CORRUPTION,
        D3D12_MESSAGE_SEVERITY_ERROR,
        D3D12_MESSAGE_SEVERITY_WARNING
    };
    D3D12_INFO_QUEUE_FILTER filter = {0};
    filter.AllowList.NumSeverities =
        (UINT)(sizeof(severities) / sizeof(severities[0]));
    filter.AllowList.pSeverityList = severities;
    g_ld_info_queue->lpVtbl->AddStorageFilterEntries(
        g_ld_info_queue, &filter);
    g_ld_info_queue->lpVtbl->SetMessageCountLimit(
        g_ld_info_queue, 65536u);
    fprintf(stderr,
            "[d3d-debug] info queue attached (warning/error/corruption)\n");
}

static void ld_dump_dred(const char* where, HRESULT trigger_hr)
{
    if (g_ld_dred_dumped++ || !g.dev) return;
    ld_drain_info_queue(where);
    ld_dump_recent_draws();
    const HRESULT removed = g.dev->lpVtbl->GetDeviceRemovedReason(g.dev);
    fprintf(stderr,
            "[dred] trigger=%s hr=0x%08lX removed=0x%08lX frame=%u "
            "surfaces=%u zetas=%u textures=%u psos=%u\n",
            where, (unsigned long)trigger_hr, (unsigned long)removed,
            g_ld_frames, g.n_surfaces, g.n_zdepths, g.n_textures, g.n_psos);

    ID3D12DeviceRemovedExtendedData1* dred = NULL;
    HRESULT hr = g.dev->lpVtbl->QueryInterface(
        g.dev, &IID_ID3D12DeviceRemovedExtendedData1, (void**)&dred);
    if (FAILED(hr) || !dred) {
        fprintf(stderr, "[dred] query failed hr=0x%08lX\n",
                (unsigned long)hr);
        return;
    }

    D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 breadcrumbs = {0};
    hr = dred->lpVtbl->GetAutoBreadcrumbsOutput1(dred, &breadcrumbs);
    fprintf(stderr, "[dred] breadcrumbs hr=0x%08lX\n", (unsigned long)hr);
    if (SUCCEEDED(hr)) {
        const D3D12_AUTO_BREADCRUMB_NODE1* node =
            breadcrumbs.pHeadAutoBreadcrumbNode;
        for (u32 node_index = 0; node && node_index < 16;
             node = node->pNext, node_index++) {
            const u32 last = node->pLastBreadcrumbValue
                ? *node->pLastBreadcrumbValue : 0;
            fprintf(stderr,
                    "[dred] node=%u list=%s queue=%s count=%u last=%u\n",
                    node_index,
                    node->pCommandListDebugNameA
                        ? node->pCommandListDebugNameA : "<unnamed>",
                    node->pCommandQueueDebugNameA
                        ? node->pCommandQueueDebugNameA : "<unnamed>",
                    node->BreadcrumbCount, last);
            if (!node->pCommandHistory || !node->BreadcrumbCount) continue;
            const u32 first = last > 8 ? last - 8 : 0;
            u32 end = last + 8;
            if (end >= node->BreadcrumbCount)
                end = node->BreadcrumbCount - 1;
            for (u32 i = first; i <= end; i++) {
                const D3D12_AUTO_BREADCRUMB_OP op = node->pCommandHistory[i];
                fprintf(stderr,
                        "[dred]   op[%u]%s=%u(%s)\n",
                        i, i == last ? "*" : "",
                        (unsigned)op, ld_dred_op_name(op));
            }
        }
    }

    D3D12_DRED_PAGE_FAULT_OUTPUT1 fault = {0};
    hr = dred->lpVtbl->GetPageFaultAllocationOutput1(dred, &fault);
    fprintf(stderr,
            "[dred] page-fault hr=0x%08lX va=0x%016llX\n",
            (unsigned long)hr,
            (unsigned long long)fault.PageFaultVA);
    if (SUCCEEDED(hr)) {
        const D3D12_DRED_ALLOCATION_NODE1* lists[2] = {
            fault.pHeadExistingAllocationNode,
            fault.pHeadRecentFreedAllocationNode
        };
        const char* labels[2] = {"existing", "recent-freed"};
        for (u32 list = 0; list < 2; list++) {
            const D3D12_DRED_ALLOCATION_NODE1* allocation = lists[list];
            for (u32 i = 0; allocation && i < 16;
                 allocation = allocation->pNext, i++) {
                fprintf(stderr,
                        "[dred] %s[%u] type=%u name=%s object=%p\n",
                        labels[list], i, (unsigned)allocation->AllocationType,
                        allocation->ObjectNameA
                            ? allocation->ObjectNameA : "<unnamed>",
                        (const void*)allocation->pObject);
            }
        }
    }
    dred->lpVtbl->Release(dred);
    fflush(stderr);
}
static u64 g_ld_texture_cache_evictions = 0;
static u64 g_ld_texture_decode_fail = 0;
static u64 g_ld_zdepth_srv_binds = 0;
static u64 g_ld_zdepth_srv_reject_no_write = 0;
static u64 g_ld_vtex_binds = 0;
static u64 g_ld_vtex_uploads = 0;
static u64 g_ld_vtex_refreshes = 0;
static u64 g_ld_vtex_unsupported = 0;
static u64 g_ld_vtex_enabled = 0;
static u64 g_ld_vtex_missing_for_txl = 0;
static u64 g_ld_divider_fetches = 0;
static char g_ld_shader_disk_dir[MAX_PATH];
static int g_ld_shader_disk_ready = -1;
static u64 g_ld_shader_disk_hits[2] = {0, 0};
static u64 g_ld_shader_disk_misses[2] = {0, 0};
static u64 g_ld_shader_disk_writes[2] = {0, 0};
static u64 g_ld_shader_disk_rejects = 0;

/*
 * Profile-lane-only renderer accounting.  These counters deliberately live
 * outside ld_state so the clean benchmark lane keeps the exact same state
 * layout and hot path.  Emit only slow/interesting frames plus a sparse
 * heartbeat; the orphanage window is therefore visible without turning the
 * log itself into a benchmark cost.
 */
typedef enum {
    LD_FLUSH_PRESENT = 0,
    LD_FLUSH_GUEST_REFERENCE,
    LD_FLUSH_VERTEX_RING,
    LD_FLUSH_VERTEX_CONSTANT_RING,
    LD_FLUSH_RETIRE_QUEUE,
    LD_FLUSH_MOVIE,
    LD_FLUSH_MOVIE_PRESENT,
    LD_FLUSH_READBACK,
    LD_FLUSH_PIXEL_CONSTANT_RING,
    LD_FLUSH_DESCRIPTOR_RING,
    LD_FLUSH_SHUTDOWN,
    LD_FLUSH_REASON_COUNT
} ld_flush_reason;
/* GOWA: flushes per reason and the time spent waiting for the GPU in them,
 * printed with each [fps] line (GOWA_FLUSH_STATS=1) */
static volatile long g_ld_flush_n[LD_FLUSH_REASON_COUNT];
static volatile long long g_ld_flush_wait_qpc;
/* GOWA_ENGASGO=<ms> (27/10, report 80): cada quadro acima disso vira uma linha [engasgo] com o
 * que custou nele: shaders compilados (D3DCompile) e PSOs criados fora da biblioteca, com o
 * tempo de cada, esperas da GPU (flush) e texturas novas no cache. */
static long long g_eng_hlsl_qpc, g_eng_pso_qpc;
static u32 g_eng_hlsl_n, g_eng_pso_n;

#if defined(YZ_PERF_PROFILE)
typedef enum {
    LD_REJECT_WORLD = 0,
    LD_REJECT_CHARACTER,
    LD_REJECT_UI,
    LD_REJECT_OTHER,
    LD_REJECT_CLASS_COUNT
} ld_reject_class;

typedef struct {
    u64 pso_lookups;
    u64 pso_hits;
    u64 pso_misses;
    u64 pso_probes;
    u64 pso_full;
    u64 vs_blob_lookups;
    u64 vs_blob_hits;
    u64 vs_blob_misses;
    u64 vs_blob_inserts;
    u64 vs_blob_full_rejects;
    u64 ps_blob_lookups;
    u64 ps_blob_hits;
    u64 ps_blob_misses;
    u64 ps_blob_inserts;
    u64 ps_blob_full_rejects;
    u64 vs_compile_calls;
    u64 ps_compile_calls;
    u64 vs_unique;
    u64 ps_unique;
    u64 vs_post_boundary_distinct;
    u64 vs_post_boundary_repeats;
    u64 ps_post_boundary_distinct;
    u64 ps_post_boundary_repeats;
    u64 create_pso_calls;
    u64 decompile_qpc;
    u64 vs_blob_lookup_qpc;
    u64 ps_blob_lookup_qpc;
    u64 vs_compile_qpc;
    u64 ps_compile_qpc;
    u64 create_pso_qpc;
    u64 flush_qpc;
    u64 fence_wait_qpc;
    u64 flush_reason[LD_FLUSH_REASON_COUNT];
    u64 flush_reason_qpc[LD_FLUSH_REASON_COUNT];
    u64 fence_reason_qpc[LD_FLUSH_REASON_COUNT];
    u64 input_vertices;
    u64 expanded_vertices;
    u64 used_attribute_draws;
    u64 used_attribute_sum;
    u64 legacy_vertex_upload_bytes;
    u64 vertex_upload_bytes;
    u64 vertex_fetch_pack_qpc;
    u64 sink_end_qpc;
    u64 texture_upload_bytes;
    u64 texture_decode_calls;
    u64 texture_decode_qpc;
    u64 ps_constant_allocations;
    u64 ps_constant_upload_bytes;
    u64 ps_constant_upload_qpc;
    u64 ps_constant_capacity_failures;
} ld_profile_counts;

typedef struct {
    ld_profile_counts total;
    ld_profile_counts previous;
    u64 vs_hashes[MAX_SHADER_BLOBS];
    u64 ps_hashes[MAX_SHADER_BLOBS];
    u64 ps_raw_exact_hashes[LD_VARIANT_SET_CAPACITY];
    u64 ps_raw_constant_hashes[LD_VARIANT_SET_CAPACITY];
    u64 ps_exact_source_hashes[LD_VARIANT_SET_CAPACITY];
    u64 ps_constant_specialized_hashes[LD_VARIANT_SET_CAPACITY];
    u64 ps_canonical_hashes[LD_VARIANT_SET_CAPACITY];
    u32 n_ps_raw_exact_hashes;
    u32 n_ps_raw_constant_hashes;
    u32 n_vs_hashes;
    u32 n_ps_hashes;
    u32 n_ps_exact_source_hashes;
    u32 n_ps_constant_specialized_hashes;
    u32 n_ps_canonical_hashes;
    u64 ps_variant_tracking_overflow;
    u32 frame_upload_high;
    u32 frame_vb_high;
    u32 frame_retired_high;
    u32 total_upload_high;
    u32 total_vb_high;
    u32 total_retired_high;
    u32 frame_used_attribute_max;
    u32 total_used_attribute_max;
    u64 measured_frames;
    u64 compile_free_frames;
    double total_frame_ms;
    double compile_free_frame_ms;
    u64 previous_packets;
    u64 previous_groups;
    u64 previous_executed;
    u64 previous_evictions;
    u64 previous_refreshes;
    LONGLONG previous_present_qpc;
    LONGLONG qpc_frequency;
    IDXGIAdapter3* adapter;
    u64 rejected_pso_keys[MAX_REJECTED_PSO_KEYS];
    u8 rejected_pso_class_masks[MAX_REJECTED_PSO_KEYS];
    u32 n_rejected_pso_keys;
    u64 rejected_pso_requests[LD_REJECT_CLASS_COUNT];
    u32 rejected_pso_unique[LD_REJECT_CLASS_COUNT];
    u32 first_pso_full_frame;
} ld_profile_state;

static ld_profile_state g_ld_profile;

static LONGLONG ld_profile_qpc(void)
{
    LARGE_INTEGER value;
    QueryPerformanceCounter(&value);
    return value.QuadPart;
}

static void ld_profile_note_ring_highwater(void)
{
    if (g.upload_used > g_ld_profile.frame_upload_high)
        g_ld_profile.frame_upload_high = g.upload_used;
    if (g.upload_used > g_ld_profile.total_upload_high)
        g_ld_profile.total_upload_high = g.upload_used;
    if (g.vb_used > g_ld_profile.frame_vb_high)
        g_ld_profile.frame_vb_high = g.vb_used;
    if (g.vb_used > g_ld_profile.total_vb_high)
        g_ld_profile.total_vb_high = g.vb_used;
    if (g.n_retired_textures > g_ld_profile.frame_retired_high)
        g_ld_profile.frame_retired_high = g.n_retired_textures;
    if (g.n_retired_textures > g_ld_profile.total_retired_high)
        g_ld_profile.total_retired_high = g.n_retired_textures;
}
#else
static void ld_profile_note_ring_highwater(void) {}
#endif

static volatile LONG g_ld_a010_probe_active = 0;
static u32 g_ld_a010_probe_start_frame = 0;
static u32 g_ld_a010_probe_sample = 0;
static u64 g_ld_a010_probe_touched = 0;
static volatile LONG g_ld_a010_camera_ready = 0;
static volatile LONG g_ld_a010_world_ready = 0;
static u32 g_ld_a010_camera_bits[16];

static void ld_movement_camera_snapshot(LONG phase)
{
    fprintf(stderr,
            "[movement-frontier-rsx] phase=%ld frame=%u root=%ld "
            "reference_active=%ld camera_ready=%ld\n",
            phase, g_ld_frames,
            InterlockedCompareExchange(&g_yz_a010_root_active, 0, 0),
            InterlockedCompareExchange(
                &g_yz_a010_reference_camera_active, 0, 0),
            InterlockedCompareExchange(&g_ld_a010_camera_ready, 0, 0));
    for (u32 row = 0; row < 4u; ++row) {
        fprintf(stderr,
                "[movement-frontier-rsx-camera] phase=%ld row=%u "
                "constant=%08X/%08X/%08X/%08X "
                "fallback=%08X/%08X/%08X/%08X\n",
                phase, row,
                g.rsx.constants[108u + row][0],
                g.rsx.constants[108u + row][1],
                g.rsx.constants[108u + row][2],
                g.rsx.constants[108u + row][3],
                g_ld_a010_camera_bits[row * 4u + 0u],
                g_ld_a010_camera_bits[row * 4u + 1u],
                g_ld_a010_camera_bits[row * 4u + 2u],
                g_ld_a010_camera_bits[row * 4u + 3u]);
    }
    fflush(stderr);
}
/* Host movie presentation and the FIFO consumer live on different threads but
 * share one D3D12 command list. Normal gameplay remains single-producer and
 * bypasses this lock; the active-reader handshake closes the movie-mode
 * transition race without putting every ordinary RSX method behind an SRW
 * lock. Host-frame priority prevents a flood of guest caption commands from
 * starving Present when the window is backgrounded. */
static SRWLOCK g_ld_access_lock = SRWLOCK_INIT;
static volatile LONG g_ld_guest_active = 0;
static volatile LONG g_ld_host_waiting = 0;

static const u8* guest_ptr(u32 location, u32 offset, u32 min_bytes)
{
    if (!g.guest_ptr) return NULL;
    return g.guest_ptr(g.guest_user, location, offset, min_bytes);
}
static u64 fnv1a(const void* data, u32 n, u64 h);
static int ld_mm_bypass(const surface_t* sf);
static u64 ld_dump_surface_ppm(const char* path, const surface_t* surface);
static u64 live_legacy_pso_key(void);
static void ld_vertex_diag_emit(const char* reason, int dump_surface);

/* ---------------------------------------------------------------------------
 * enable gate
 * -----------------------------------------------------------------------*/
int rsx_live_draw_enabled(void)
{
    static int cached = -1;
    if (cached < 0) {
        /* Opt-in in this tree. Upstream (caner's Yakuza fork) defaults this ON
         * because that runner has no other renderer; here rsx_d3d12_backend.c
         * is still the default for every other title, so the live engine has
         * to be asked for. (The YZ_RSX_DRAW alias his scripts used went with
         * the title-hack drain; RSX_LIVE_DRAW is the one spelling.) */
        const char* e = getenv("RSX_LIVE_DRAW");
        cached = (e && e[0] != '0') ? 1 : 0;
    }
    return cached;
}

/* ---------------------------------------------------------------------------
 * B1 render/sampler state decode (identical facts to replay_main.c)
 * -----------------------------------------------------------------------*/
#define M_BLEND_ENABLE       0x0310
#define M_ALPHA_TEST_ENABLE  0x0304
#define M_ALPHA_FUNC         0x0308
#define M_ALPHA_REF          0x030C
#define M_BLEND_SFACTOR      0x0314
#define M_BLEND_DFACTOR      0x0318
#define M_BLEND_EQUATION     0x0320
#define M_DEPTH_FUNC         0x0A6C
#define M_DEPTH_WRITE        0x0A70
#define M_DEPTH_TEST_ENABLE  0x0A74
#define M_CULL_FACE          0x1830
#define M_FRONT_FACE         0x1834
#define M_CULL_FACE_ENABLE   0x183C
#define M_COLOR_MASK         0x0324
/* Stencil + scissor (raw NV4097 offsets, same convention as above; RPCS3
 * gcm_enums.h consulted as a read-only numbering oracle). */
#define M_STENCIL_TEST_ENABLE 0x0328
#define M_STENCIL_MASK        0x032C
#define M_STENCIL_FUNC        0x0330
#define M_STENCIL_FUNC_REF    0x0334
#define M_STENCIL_FUNC_MASK   0x0338
#define M_STENCIL_OP_FAIL     0x033C
#define M_STENCIL_OP_ZFAIL    0x0340
#define M_STENCIL_OP_ZPASS    0x0344
#define M_TWO_SIDED_STENCIL   0x0348
#define M_BACK_STENCIL_FUNC   0x0350
#define M_BACK_STENCIL_OP_FAIL  0x035C
#define M_BACK_STENCIL_OP_ZFAIL 0x0360
#define M_BACK_STENCIL_OP_ZPASS 0x0364
#define M_SCISSOR_HORIZONTAL  0x08C0
#define M_SCISSOR_VERTICAL    0x08C4
#define M_ZSTENCIL_CLEAR      0x1D8C

static D3D12_COMPARISON_FUNC gcm_cmp(u32 f)
{
    switch (f) {
    case 0x0200: return D3D12_COMPARISON_FUNC_NEVER;
    case 0x0201: return D3D12_COMPARISON_FUNC_LESS;
    case 0x0202: return D3D12_COMPARISON_FUNC_EQUAL;
    case 0x0203: return D3D12_COMPARISON_FUNC_LESS_EQUAL;
    case 0x0204: return D3D12_COMPARISON_FUNC_GREATER;
    case 0x0205: return D3D12_COMPARISON_FUNC_NOT_EQUAL;
    case 0x0206: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    default:     return D3D12_COMPARISON_FUNC_ALWAYS;
    }
}
/* GOWA: the device takes D3D12_BLEND_ALPHA_FACTOR (D3D12_OPTIONS13) */
static int g_ld_alpha_factor_ok;
static D3D12_BLEND gcm_blend_factor(u32 f, int alpha)
{
    /* CONSTANT_ALPHA in a colour slot: the factor's alpha in every channel.
     * With ALPHA_FACTOR the blend factor is always the full constant colour
     * (OMSetBlendFactor in the draw), so CONSTANT_COLOR and CONSTANT_ALPHA
     * can share one blend -- God of War Ascension's bloom mix does
     * (src * const.a + dst * const.rgb). */
    if (!alpha && g_ld_alpha_factor_ok) {
        if (f == 0x8003) return D3D12_BLEND_ALPHA_FACTOR;
        if (f == 0x8004) return D3D12_BLEND_INV_ALPHA_FACTOR;
    }
    switch (f) {
    case 0x0000: return D3D12_BLEND_ZERO;
    case 0x0001: return D3D12_BLEND_ONE;
    case 0x0300: return alpha ? D3D12_BLEND_SRC_ALPHA : D3D12_BLEND_SRC_COLOR;
    case 0x0301: return alpha ? D3D12_BLEND_INV_SRC_ALPHA : D3D12_BLEND_INV_SRC_COLOR;
    case 0x0302: return D3D12_BLEND_SRC_ALPHA;
    case 0x0303: return D3D12_BLEND_INV_SRC_ALPHA;
    case 0x0304: return D3D12_BLEND_DEST_ALPHA;
    case 0x0305: return D3D12_BLEND_INV_DEST_ALPHA;
    case 0x0306: return alpha ? D3D12_BLEND_DEST_ALPHA : D3D12_BLEND_DEST_COLOR;
    case 0x0307: return alpha ? D3D12_BLEND_INV_DEST_ALPHA : D3D12_BLEND_INV_DEST_COLOR;
    case 0x0308: return D3D12_BLEND_SRC_ALPHA_SAT;
    case 0x8001: return D3D12_BLEND_BLEND_FACTOR;
    case 0x8002: return D3D12_BLEND_INV_BLEND_FACTOR;
    case 0x8003: return D3D12_BLEND_BLEND_FACTOR;
    case 0x8004: return D3D12_BLEND_INV_BLEND_FACTOR;
    default:     return D3D12_BLEND_ONE;
    }
}
static D3D12_BLEND_OP gcm_blend_op(u32 e)
{
    switch (e) {
    case 0x8007: return D3D12_BLEND_OP_MIN;
    case 0x8008: return D3D12_BLEND_OP_MAX;
    case 0x800A: return D3D12_BLEND_OP_SUBTRACT;
    case 0x800B: return D3D12_BLEND_OP_REV_SUBTRACT;
    default:     return D3D12_BLEND_OP_ADD;
    }
}

/* SURFACE_COLOR_TARGET selector -> number of colour targets written, A first.
 * 0x02 (B alone) and anything unknown draw to A only, as before. */
static u32 ld_rt_count(u32 sel)
{
    switch (sel) {
    case 0x13: return 2;
    case 0x17: return 3;
    case 0x1F: return 4;
    default:   return 1;
    }
}

typedef struct {
    u32 alpha_test_enable, alpha_func, alpha_ref_raw, alpha_ref_format;
    u32 blend_enable, sf_rgb, df_rgb, sf_a, df_a, eq_rgb, eq_a;
    u32 depth_test, depth_write, depth_func;
    u32 cull_enable, cull_face, front_face;
    u32 color_mask;
    /* TM: this title renders HDR into FP16 (SURFACE_FORMAT 0xB) targets;
     * the RTV format is PSO state, so it belongs in the key. */
    u32 rt_dxgi;        /* DXGI format of the colour target(s), ld_color_dxgi */
    /* Colour targets the draw writes (SURFACE_COLOR_TARGET: A, AB, ABC,
     * ABCD). GH3's venue pass writes A and B; the post pass then samples B.
     * With only A bound, B stayed empty and that pass painted the scene
     * black. NumRenderTargets is PSO state, so it is keyed too. */
    u32 rt_count;
    /* Stencil is PSO state in D3D12, so it lives here and feeds the PSO key
     * like every other field. The reference value is dynamic
     * (OMSetStencilRef) and deliberately NOT part of this struct. */
    u32 stencil_enable, stencil_two_sided;
    u32 s_func, s_func_mask, s_write_mask, s_fail, s_zfail, s_zpass;
    u32 bs_func, bs_fail, bs_zfail, bs_zpass;
    /* GOWA: polygon offset (NV4097_SET_POLY_OFFSET_FILL_ENABLE, _SCALE_FACTOR,
     * _BIAS) -- the shadow-map passes rely on it; without it the depth
     * comparison self-shadows (moire acne). Raw float bits. */
    u32 poly_offset_fill, poly_offset_scale, poly_offset_bias;
} render_state_t;

/* Host format of a guest colour surface. F_X32 (0xD) and F_W32Z32Y32X32
 * (0xC) were rendered as RGBA8: clamped to 0..1 and quantized, so God of War
 * Ascension's 32-bit float targets (a depth/CoC chain sampled back as
 * X32_FLOAT) read garbage and the depth of field blurred the whole frame.
 * GOWA_F32_RT=0 keeps them RGBA8. */
static DXGI_FORMAT ld_color_dxgi(u32 color_format)
{
    static int f32 = -1;
    if (f32 < 0) { const char* e = getenv("GOWA_F32_RT"); f32 = !(e && *e == '0'); }
    if (color_format == RSX_SURFACE_FMT_F_W16Z16Y16X16) return DXGI_FORMAT_R16G16B16A16_FLOAT;
    if (f32 && color_format == 0xD) return DXGI_FORMAT_R32_FLOAT;
    if (f32 && color_format == 0xC) return DXGI_FORMAT_R32G32B32A32_FLOAT;
    return DXGI_FORMAT_R8G8B8A8_UNORM;
}

static void decode_render_state(render_state_t* rs)
{
    memset(rs, 0, sizeof(*rs));
    rs->alpha_test_enable = rsx_dsp_reg(&g.rsx, M_ALPHA_TEST_ENABLE) & 1;
    rs->alpha_func = rsx_dsp_reg(&g.rsx, M_ALPHA_FUNC);
    rs->alpha_ref_raw = rsx_dsp_reg(&g.rsx, M_ALPHA_REF);
    rsx_dsp_surface alpha_surface;
    rsx_dsp_get_surface(&g.rsx, &alpha_surface);
    rs->alpha_ref_format = alpha_surface.color_format;
    rs->rt_dxgi = (u32)ld_color_dxgi(alpha_surface.color_format);
    rs->rt_count = ld_rt_count(alpha_surface.color_target);
    rs->blend_enable = rsx_dsp_reg(&g.rsx, M_BLEND_ENABLE) & 1;
    const u32 sf = rsx_dsp_reg(&g.rsx, M_BLEND_SFACTOR);
    const u32 df = rsx_dsp_reg(&g.rsx, M_BLEND_DFACTOR);
    const u32 eq = rsx_dsp_reg(&g.rsx, M_BLEND_EQUATION);
    rs->sf_rgb = sf & 0xFFFF; rs->sf_a = sf >> 16;
    rs->df_rgb = df & 0xFFFF; rs->df_a = df >> 16;
    rs->eq_rgb = eq & 0xFFFF; rs->eq_a = eq >> 16;
    rs->depth_test  = rsx_dsp_reg(&g.rsx, M_DEPTH_TEST_ENABLE) & 1;
    /* LD_NO_DEPTH=1: disable the depth test on every draw. A probe for draws
     * that vanish -- Tornado Outbreak's UI text fails its depth test here (not
     * yet understood), and this is how its menus are readable meanwhile. */
    { static int nd = -1; if (nd < 0) nd = getenv("LD_NO_DEPTH") ? 1 : 0; if (nd) rs->depth_test = 0; }
    rs->depth_write = rsx_dsp_reg(&g.rsx, M_DEPTH_WRITE) & 1;
    rs->depth_func  = rsx_dsp_reg(&g.rsx, M_DEPTH_FUNC);
    rs->cull_enable = rsx_dsp_reg(&g.rsx, M_CULL_FACE_ENABLE) & 1;
    rs->cull_face   = rsx_dsp_reg(&g.rsx, M_CULL_FACE);
    rs->front_face  = rsx_dsp_reg(&g.rsx, M_FRONT_FACE);
    { static int po = -1;
      if (po < 0) { const char* e = getenv("GOWA_POLY_OFFSET"); po = !(e && *e == '0'); }
      rs->poly_offset_fill = po ? (rsx_dsp_reg(&g.rsx, 0x0A68) & 1u) : 0u;
      rs->poly_offset_scale = rs->poly_offset_fill ? rsx_dsp_reg(&g.rsx, 0x0A78) : 0u;
      rs->poly_offset_bias = rs->poly_offset_fill ? rsx_dsp_reg(&g.rsx, 0x0A7C) : 0u;
      /* GOWA_POLY_OFFSET_LOG=1: each distinct (factor, units, colour target) once */
      { static int pl = -1; static u32 seen[64][3]; static int ns;
        if (pl < 0) pl = getenv("GOWA_POLY_OFFSET_LOG") ? 1 : 0;
        if (pl && rs->poly_offset_fill) {
            rsx_dsp_surface ps; rsx_dsp_get_surface(&g.rsx, &ps);
            int f = 0;
            for (int i = 0; i < ns; i++)
                if (seen[i][0] == rs->poly_offset_scale && seen[i][1] == rs->poly_offset_bias && seen[i][2] == ps.color_offset[0]) f = 1;
            if (!f && ns < 64) {
                seen[ns][0] = rs->poly_offset_scale; seen[ns][1] = rs->poly_offset_bias; seen[ns][2] = ps.color_offset[0]; ns++;
                float pf, pu; memcpy(&pf, &rs->poly_offset_scale, 4); memcpy(&pu, &rs->poly_offset_bias, 4);
                fprintf(stderr, "[polyoff] frame %u factor=%g units=%g alvo=0x%X zeta=0x%X dfunc=0x%X dwrite=%u cmask=0x%08X\n",
                        g_ld_frames, pf, pu, ps.color_offset[0], ps.zeta_offset,
                        rsx_dsp_reg(&g.rsx, M_DEPTH_FUNC), rsx_dsp_reg(&g.rsx, 0x0A70) & 1u, rsx_dsp_reg(&g.rsx, M_COLOR_MASK));
            }
        } } }
    /* s31 (scratch/s31_blue_emitter.md): honor the RAW register — 0 is a
     * legitimate game-written "write no color channels" (the character
     * shadow-mask depth-prime pass). rsx_dispatch_init seeds the nv40
     * reset default (0x01010101), so never-written reads as all-on. */
    rs->color_mask  = rsx_dsp_reg(&g.rsx, M_COLOR_MASK);
    rs->stencil_enable    = rsx_dsp_reg(&g.rsx, M_STENCIL_TEST_ENABLE) & 1;
    rs->stencil_two_sided = rsx_dsp_reg(&g.rsx, M_TWO_SIDED_STENCIL) & 1;
    rs->s_func       = rsx_dsp_reg(&g.rsx, M_STENCIL_FUNC);
    rs->s_func_mask  = rsx_dsp_reg(&g.rsx, M_STENCIL_FUNC_MASK) & 0xFF;
    rs->s_write_mask = rsx_dsp_reg(&g.rsx, M_STENCIL_MASK) & 0xFF;
    rs->s_fail       = rsx_dsp_reg(&g.rsx, M_STENCIL_OP_FAIL);
    rs->s_zfail      = rsx_dsp_reg(&g.rsx, M_STENCIL_OP_ZFAIL);
    rs->s_zpass      = rsx_dsp_reg(&g.rsx, M_STENCIL_OP_ZPASS);
    rs->bs_func      = rsx_dsp_reg(&g.rsx, M_BACK_STENCIL_FUNC);
    rs->bs_fail      = rsx_dsp_reg(&g.rsx, M_BACK_STENCIL_OP_FAIL);
    rs->bs_zfail     = rsx_dsp_reg(&g.rsx, M_BACK_STENCIL_OP_ZFAIL);
    rs->bs_zpass     = rsx_dsp_reg(&g.rsx, M_BACK_STENCIL_OP_ZPASS);
}

/* GL stencil-op enums (the NV4097 payload) -> D3D12. Zero (never seeded by
 * the game before first use) and GL_KEEP both map to KEEP. */
static D3D12_STENCIL_OP gcm_stencil_op(u32 op)
{
    switch (op) {
    case 0x0000: return D3D12_STENCIL_OP_ZERO;      /* GL_ZERO */
    case 0x1E01: return D3D12_STENCIL_OP_REPLACE;
    case 0x1E02: return D3D12_STENCIL_OP_INCR_SAT;  /* GL_INCR (clamped) */
    case 0x1E03: return D3D12_STENCIL_OP_DECR_SAT;  /* GL_DECR (clamped) */
    case 0x150A: return D3D12_STENCIL_OP_INVERT;
    case 0x8507: return D3D12_STENCIL_OP_INCR;      /* GL_INCR_WRAP */
    case 0x8508: return D3D12_STENCIL_OP_DECR;      /* GL_DECR_WRAP */
    case 0x1E00: default: return D3D12_STENCIL_OP_KEEP;
    }
}

static void apply_render_state(D3D12_GRAPHICS_PIPELINE_STATE_DESC* pd,
                               const render_state_t* rs)
{
    D3D12_RENDER_TARGET_BLEND_DESC* b = &pd->BlendState.RenderTarget[0];
    /* nv40_3d COLOR_MASK byte layout (Mesa/nouveau nv40_3d.xml.h, MIT/X11):
     * B=[0:7] G=[8:15] R=[16:23] A=[24:31]; any nonzero byte = channel on.
     * (s31: was hardcoded ENABLE_ALL — the blue-character class' live twin.) */
    b->RenderTargetWriteMask =
        (((rs->color_mask       ) & 0xFF) ? D3D12_COLOR_WRITE_ENABLE_BLUE  : 0) |
        (((rs->color_mask >>  8 ) & 0xFF) ? D3D12_COLOR_WRITE_ENABLE_GREEN : 0) |
        (((rs->color_mask >> 16 ) & 0xFF) ? D3D12_COLOR_WRITE_ENABLE_RED   : 0) |
        (((rs->color_mask >> 24 ) & 0xFF) ? D3D12_COLOR_WRITE_ENABLE_ALPHA : 0);
    if (rs->blend_enable) {
        b->BlendEnable   = TRUE;
        b->SrcBlend      = gcm_blend_factor(rs->sf_rgb, 0);
        b->DestBlend     = gcm_blend_factor(rs->df_rgb, 0);
        b->BlendOp       = gcm_blend_op(rs->eq_rgb);
        b->SrcBlendAlpha = gcm_blend_factor(rs->sf_a, 1);
        b->DestBlendAlpha= gcm_blend_factor(rs->df_a, 1);
        b->BlendOpAlpha  = gcm_blend_op(rs->eq_a);
    }
    pd->RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    if (rs->cull_enable && rs->cull_face) {
        const u32 f = rs->cull_face;
        pd->RasterizerState.CullMode = (f == 0x0404) ? D3D12_CULL_MODE_FRONT
                                     : (f == 0x0405) ? D3D12_CULL_MODE_BACK
                                                     : D3D12_CULL_MODE_NONE;
    } else {
        pd->RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    }
    /* Y-negating viewport epilogue mirrors winding; take the front sense
     * straight from the guest value (validated in B1). */
    pd->RasterizerState.FrontCounterClockwise = (rs->front_face == 0x0901);
    /* GOWA: polygon offset. RSX: offset = factor * dz/dxy + units * r (r =
     * 2^-24 for D24); D3D12 float depth: DepthBias * 2^(e - 23), equal to
     * 2^-24 for depths in [0.5, 1) -- units map 1:1 there. */
    if (rs->poly_offset_fill) {
        /* the slope part only; the constant part goes per draw into the
         * VS depth offset (it changes every draw in God of War Ascension's
         * shadow passes, so it cannot be PSO state) */
        float pf;
        memcpy(&pf, &rs->poly_offset_scale, 4);
        if (pf == pf) {
            pd->RasterizerState.SlopeScaledDepthBias = pf;
            pd->RasterizerState.DepthBiasClamp = 0.0f;
        }
    }
    if (g.depth) {
        pd->DSVFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
        pd->DepthStencilState.DepthEnable = rs->depth_test ? TRUE : FALSE;
        pd->DepthStencilState.DepthWriteMask =
            rs->depth_write ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
        pd->DepthStencilState.DepthFunc = gcm_cmp(rs->depth_func);
        /* LD_DEPTH_EQUAL_AS_LEQUAL=1: a colour pass that re-renders the geometry
         * of a Z pre-pass with depth func EQUAL (God of War Ascension) needs the
         * two vertex programs to produce bit-identical depth. On the RSX they do;
         * translated to two HLSL shaders they need not, and EQUAL then rejects
         * every pixel (black scene). LESS_EQUAL against the pre-pass depth passes
         * the same surfaces. */
        { static int eq = -1; if (eq < 0) { const char* e = getenv("LD_DEPTH_EQUAL_AS_LEQUAL"); eq = (e && *e != '0') ? 1 : 0; }
          { static int mode = -1; if (mode < 0) { const char* e2 = getenv("LD_DEPTH_EQUAL_AS_LEQUAL"); mode = e2 ? atoi(e2) : 0; }
            if (eq && rs->depth_func == 0x0202)
                pd->DepthStencilState.DepthFunc = mode == 2 ? D3D12_COMPARISON_FUNC_GREATER_EQUAL
                                                : mode == 3 ? D3D12_COMPARISON_FUNC_ALWAYS
                                                            : D3D12_COMPARISON_FUNC_LESS_EQUAL;
            /* LD_DEPTH_EQUAL_BIAS=<int>[:<slope>]: with mode 1, pull the
             * EQUAL pass toward the camera by that depth bias. */
            static int bset = -1; static INT bias; static float slope;
            if (bset < 0) { const char* e3 = getenv("LD_DEPTH_EQUAL_BIAS"); bset = e3 ? 1 : 0;
                            bias = e3 ? (INT)atoi(e3) : 0; const char* c3 = e3 ? strchr(e3, ':') : NULL; slope = c3 ? (float)atof(c3 + 1) : 0.0f; }
            if (eq && bset && mode != 2 && mode != 3 && rs->depth_func == 0x0202) {
                pd->RasterizerState.DepthBias = bias;
                pd->RasterizerState.SlopeScaledDepthBias = slope;
                pd->RasterizerState.DepthBiasClamp = 0.0f;
            } } }
        if (rs->stencil_enable) {
            pd->DepthStencilState.StencilEnable    = TRUE;
            pd->DepthStencilState.StencilReadMask  = (UINT8)rs->s_func_mask;
            pd->DepthStencilState.StencilWriteMask = (UINT8)rs->s_write_mask;
            pd->DepthStencilState.FrontFace.StencilFunc = gcm_cmp(rs->s_func);
            pd->DepthStencilState.FrontFace.StencilFailOp = gcm_stencil_op(rs->s_fail);
            pd->DepthStencilState.FrontFace.StencilDepthFailOp = gcm_stencil_op(rs->s_zfail);
            pd->DepthStencilState.FrontFace.StencilPassOp = gcm_stencil_op(rs->s_zpass);
            /* Two-sided off: nv40 applies the front state to both faces. */
            if (rs->stencil_two_sided) {
                pd->DepthStencilState.BackFace.StencilFunc = gcm_cmp(rs->bs_func);
                pd->DepthStencilState.BackFace.StencilFailOp = gcm_stencil_op(rs->bs_fail);
                pd->DepthStencilState.BackFace.StencilDepthFailOp = gcm_stencil_op(rs->bs_zfail);
                pd->DepthStencilState.BackFace.StencilPassOp = gcm_stencil_op(rs->bs_zpass);
            } else {
                pd->DepthStencilState.BackFace = pd->DepthStencilState.FrontFace;
            }
        } else {
            pd->DepthStencilState.StencilEnable = FALSE;
        }
    }
}

static D3D12_TEXTURE_ADDRESS_MODE gcm_wrap(u32 w)
{
    switch (w & 0xF) {
    case 1: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    case 2: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
    case 3: return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    case 4: return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    case 5: return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    case 6:
    case 7:
    case 8: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
    default: return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    }
}

static D3D12_SAMPLER_DESC decode_sampler(const rsx_dsp_texture* t)
{
    D3D12_SAMPLER_DESC sd = {0};
    const u32 minf = (t->filter >> 16) & 0x7;
    const u32 magf = (t->filter >> 24) & 0x7;
    const int min_linear = (minf == 2 || minf == 4 || minf == 6);
    const int mag_linear = (magf == 2);
    const int mip_linear = (minf == 5 || minf == 6);
    const int mip_present = (minf >= 3);
    D3D12_FILTER_TYPE mnf = min_linear ? D3D12_FILTER_TYPE_LINEAR : D3D12_FILTER_TYPE_POINT;
    D3D12_FILTER_TYPE mgf = mag_linear ? D3D12_FILTER_TYPE_LINEAR : D3D12_FILTER_TYPE_POINT;
    D3D12_FILTER_TYPE mpf = mip_linear ? D3D12_FILTER_TYPE_LINEAR : D3D12_FILTER_TYPE_POINT;
    sd.Filter = D3D12_ENCODE_BASIC_FILTER(mnf, mgf, mpf, D3D12_FILTER_REDUCTION_TYPE_STANDARD);
    sd.AddressU = gcm_wrap(t->wrap);
    sd.AddressV = gcm_wrap(t->wrap >> 8);
    sd.AddressW = gcm_wrap(t->wrap >> 16);
    const u32 max_lod_fx = (t->control0 >> 7)  & 0xFFF;
    const u32 min_lod_fx = (t->control0 >> 19) & 0xFFF;
    sd.MinLOD = (float)min_lod_fx / 256.0f;
    sd.MaxLOD = mip_present ? (float)max_lod_fx / 256.0f : 0.0f;
    if (sd.MaxLOD < sd.MinLOD) sd.MaxLOD = sd.MinLOD;
    /* GOWA_TEX_MIP0=1 (26/10, diagnosis of the mottled skin of reports 22/24/26): only the base level is sampled.
     * If a lower level were decoded wrong, which level a draw picks by distance (and by GPU) would decide where
     * the wrong colours show; with this on they go away. Sharper and with shimmer far away: a test, not a fix. */
    { static int mip0 = -1;
      if (mip0 < 0) { const char* e = getenv("GOWA_TEX_MIP0"); mip0 = e && *e == '1'; }
      if (mip0) sd.MinLOD = sd.MaxLOD = 0.0f; }
    sd.MaxAnisotropy = 1;
    /* GOWA: TEXTURE_BORDER_COLOR (ARGB8) for BORDER addressing -- the
     * descriptor was left at transparent black, which a depth-compare unit
     * reads as depth 0: everything outside a shadow map came out shadowed.
     * GOWA_BORDER_COLOR=0 restores the black. */
    { static int bc_on = -1;
      if (bc_on < 0) { const char* e = getenv("GOWA_BORDER_COLOR"); bc_on = !(e && *e == '0'); }
      if (bc_on) {
          const u32 bc = t->border_color;
          sd.BorderColor[0] = (float)((bc >> 16) & 0xFFu) / 255.0f;
          sd.BorderColor[1] = (float)((bc >> 8) & 0xFFu) / 255.0f;
          sd.BorderColor[2] = (float)(bc & 0xFFu) / 255.0f;
          sd.BorderColor[3] = (float)(bc >> 24) / 255.0f;
      } }
    return sd;
}
/* Cache key of a sampler: exactly the fields decode_sampler reads, so two states share a descriptor only when
 * they decode alike. The previous key kept wrap bits 0-11 (U and V, none of W at bits 16-19) and shifted the 21-bit
 * LOD field of control0 left by 18, which drops its top seven bits: min LOD (control0 bits 19-30) above bit 20
 * and the W address mode were lost, so states differing only there reused another state's descriptor. */
static u64 sampler_key(const rsx_dsp_texture* t)
{
    const u64 minf    = (t->filter >> 16) & 0x7;
    const u64 magf    = (t->filter >> 24) & 0x7;
    const u64 wrap_u  = t->wrap & 0xF;
    const u64 wrap_v  = (t->wrap >> 8) & 0xF;
    const u64 wrap_w  = (t->wrap >> 16) & 0xF;
    const u64 max_lod = (t->control0 >> 7) & 0xFFF;
    const u64 min_lod = (t->control0 >> 19) & 0xFFF;
    u64 key = minf | (magf << 3) | (wrap_u << 6) | (wrap_v << 10) | (wrap_w << 14) | (max_lod << 18) | (min_lod << 30);
    /* GOWA: the border colour (decode_sampler) when a mode reads it */
    if (wrap_u == 4 || wrap_v == 4 || wrap_w == 4)
        key |= (u64)(((u32)t->border_color * 2654435761u) >> 11) << 43;
    return key;
}

/* ---------------------------------------------------------------------------
 * descriptor heap helpers
 * -----------------------------------------------------------------------*/
static D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle(u32 idx)
{
    D3D12_CPU_DESCRIPTOR_HANDLE h;
    g.rtv_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(g.rtv_heap, &h);
    h.ptr += (size_t)idx * g.rtv_step;
    return h;
}
static D3D12_CPU_DESCRIPTOR_HANDLE srv_cpu(u32 slot)
{
    /* GOWA (24/10): the heap start kept (it was asked for on every call, 16+ a draw) */
    static ID3D12DescriptorHeap* heap;
    static D3D12_CPU_DESCRIPTOR_HANDLE start;
    if (heap != g.srv_cpu_heap) {
        g.srv_cpu_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(g.srv_cpu_heap, &start);
        heap = g.srv_cpu_heap;
    }
    D3D12_CPU_DESCRIPTOR_HANDLE h = start;
    h.ptr += (size_t)slot * g.srv_step;
    return h;
}
/* a descriptor about to be written there: tables built from the old one are stale */
static D3D12_CPU_DESCRIPTOR_HANDLE srv_dst(u32 slot)
{
    g_ld_srv_gen++;
    return srv_cpu(slot);
}
static void srv_write(u32 slot, ID3D12Resource* tex)
{
    g.dev->lpVtbl->CreateShaderResourceView(g.dev, tex, NULL, srv_dst(slot));
}
static void srv_write_zdepth(u32 slot, ID3D12Resource* tex)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC sd = {0};
    sd.Format = DXGI_FORMAT_R32_FLOAT;
    sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    /* The replay-proven depth snapshot broadcasts depth to RGB. Mirror that
     * mapping directly in the native SRV and force alpha to one. */
    sd.Shader4ComponentMapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(
        D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0,
        D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0,
        D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0,
        D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1);
    sd.Texture2D.MipLevels = 1;
    g.dev->lpVtbl->CreateShaderResourceView(
        g.dev, tex, &sd, srv_dst(slot));
}
static void srv_write_depth_source(u32 slot, ID3D12Resource* tex)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC sd = {0};
    sd.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
    sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    sd.Texture2D.MipLevels = 1;
    g.dev->lpVtbl->CreateShaderResourceView(
        g.dev, tex, &sd, srv_dst(slot));
}
static void uav_write_zdepth(u32 slot, ID3D12Resource* tex)
{
    D3D12_UNORDERED_ACCESS_VIEW_DESC ud = {0};
    ud.Format = DXGI_FORMAT_R32_FLOAT;
    ud.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    g.dev->lpVtbl->CreateUnorderedAccessView(
        g.dev, tex, NULL, &ud, srv_dst(slot));
}
static D3D12_GPU_DESCRIPTOR_HANDLE srv_table(const u32 slots[SRV_TABLE_SIZE])
{
    /* GOWA (24/10): consecutive draws of a material bind the same textures -- the
     * previous table again instead of copying 16 descriptors (GOWA_D3D_FILTER=0: always) */
    if (ld_d3d_filter() && g_ldc.srvt_ok && g_ldc.srvt_gen == g_ld_srv_gen &&
        g_ldc.srvt_used == g.srv_ring_used && !memcmp(g_ldc.srvt_slots, slots, sizeof g_ldc.srvt_slots))
        return g_ldc.srvt_h;
    if (g.srv_ring_used >= SRV_RING_TABLES) {
        D3D12_GPU_DESCRIPTOR_HANDLE invalid = {0};
        return invalid;
    }
    const u32 base = g.srv_ring_used++ * SRV_TABLE_SIZE;
    D3D12_CPU_DESCRIPTOR_HANDLE dst;
    D3D12_CPU_DESCRIPTOR_HANDLE src[SRV_TABLE_SIZE];
    const UINT dst_size = SRV_TABLE_SIZE;
    g.srv_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(g.srv_heap, &dst);
    dst.ptr += (size_t)base * g.srv_step;
    for (u32 i = 0; i < SRV_TABLE_SIZE; i++)
        src[i] = srv_cpu(slots[i]);
    g.dev->lpVtbl->CopyDescriptors(
        g.dev, 1, &dst, &dst_size, SRV_TABLE_SIZE, src, NULL,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_GPU_DESCRIPTOR_HANDLE h;
    g.srv_heap->lpVtbl->GetGPUDescriptorHandleForHeapStart(g.srv_heap, &h);
    h.ptr += (u64)base * g.srv_step;
    g_ldc.srvt_ok = 1; g_ldc.srvt_gen = g_ld_srv_gen; g_ldc.srvt_used = g.srv_ring_used; g_ldc.srvt_h = h;
    memcpy(g_ldc.srvt_slots, slots, sizeof g_ldc.srvt_slots);
    return h;
}
static int depth_snapshot_descriptors(
    u32 zindex, D3D12_GPU_DESCRIPTOR_HANDLE* out_source,
    D3D12_GPU_DESCRIPTOR_HANDLE* out_destination)
{
    if (zindex >= MAX_SURFACES || g.srv_ring_used >= SRV_RING_TABLES)
        return 0;
    const u32 base = g.srv_ring_used++ * SRV_TABLE_SIZE;
    D3D12_CPU_DESCRIPTOR_HANDLE cpu;
    g.srv_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(
        g.srv_heap, &cpu);
    cpu.ptr += (size_t)base * g.srv_step;
    g.dev->lpVtbl->CopyDescriptorsSimple(
        g.dev, 1, cpu, srv_cpu(SRV_DEPTH_SOURCE_BASE + zindex),
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    cpu.ptr += g.srv_step;
    g.dev->lpVtbl->CopyDescriptorsSimple(
        g.dev, 1, cpu, srv_cpu(UAV_ZDEPTH_BASE + zindex),
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_GPU_DESCRIPTOR_HANDLE gpu;
    g.srv_heap->lpVtbl->GetGPUDescriptorHandleForHeapStart(
        g.srv_heap, &gpu);
    gpu.ptr += (u64)base * g.srv_step;
    *out_source = gpu;
    gpu.ptr += g.srv_step;
    *out_destination = gpu;
    return 1;
}
static D3D12_CPU_DESCRIPTOR_HANDLE smp_cpu(u32 slot)
{
    D3D12_CPU_DESCRIPTOR_HANDLE h;
    g.smp_cpu_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(g.smp_cpu_heap, &h);
    h.ptr += (size_t)slot * g.smp_step;
    return h;
}
/* GOWA: the cache held 128 sampler states and never evicts; past that every
 * new state got SMP_DEFAULT -- CLAMP addressing. God of War Ascension makes
 * more than 128 (filter x address x LOD range), and the states created late
 * (Kratos' body in the first level: WRAP, texture coordinates in -1..0) were
 * sampled clamped: u < 0 read the atlas' left edge, flat grey, and the body
 * lost its whole diffuse -- the red tattoo with it -- while the head (u > 0)
 * looked right. Now 4096 states, found through a hash index (a linear search
 * over thousands, per unit per draw, would cost more than the draw), and a
 * warning if it ever fills up. */
static u32 sampler_slot(const rsx_dsp_texture* t, u64 key)
{
    const u32 hmask = 2u * SMP_CACHE_SLOTS - 1u;
    u32 h = (u32)((key * 0x9E3779B97F4A7C15ull) >> 40) & hmask;
    for (u32 probe = 0; probe <= hmask; probe++, h = (h + 1u) & hmask) {
        const u32 e = g.smp_hash[h];       /* slot (1 + index), 0 = free */
        if (!e) break;
        if (g.smp_keys[e - 1u] == key) return e;
    }
    if (g.n_samplers >= SMP_CACHE_SLOTS) {
        static int warned;
        if (!warned) { warned = 1; fprintf(stderr, "[smp] cache de samplers cheio (%u): estados novos usam o padrao (CLAMP)\n", (unsigned)SMP_CACHE_SLOTS); }
        return SMP_DEFAULT;
    }
    D3D12_SAMPLER_DESC sd = decode_sampler(t);
    const u32 slot = 1 + g.n_samplers;
    g.dev->lpVtbl->CreateSampler(g.dev, &sd, smp_cpu(slot));
    g.smp_keys[g.n_samplers++] = key;
    g.smp_hash[h] = (u16)slot;
    return slot;
}
static D3D12_GPU_DESCRIPTOR_HANDLE sampler_table(const u32 slots[SMP_TABLE_SIZE])
{
    for (u32 i = 0; i < g.smp_ring_used; i++) {
        if (memcmp(g.smp_ring_slots[i], slots, sizeof(g.smp_ring_slots[i])) == 0) {
            D3D12_GPU_DESCRIPTOR_HANDLE cached;
            g.smp_heap->lpVtbl->GetGPUDescriptorHandleForHeapStart(
                g.smp_heap, &cached);
            cached.ptr += (u64)(i * SMP_TABLE_SIZE) * g.smp_step;
            return cached;
        }
    }
    /* The caller preflights capacity before recording render-state commands,
     * because a command-list flush here would discard that state. */
    if (g.smp_ring_used >= SMP_RING_TABLES) {
        D3D12_GPU_DESCRIPTOR_HANDLE invalid = {0};
        return invalid;
    }
    const u32 base = g.smp_ring_used++ * SMP_TABLE_SIZE;
    memcpy(g.smp_ring_slots[base / SMP_TABLE_SIZE], slots,
           sizeof(g.smp_ring_slots[0]));
    D3D12_CPU_DESCRIPTOR_HANDLE dst;
    D3D12_CPU_DESCRIPTOR_HANDLE src[SMP_TABLE_SIZE];
    const UINT dst_size = SMP_TABLE_SIZE;
    g.smp_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(g.smp_heap, &dst);
    dst.ptr += (size_t)base * g.smp_step;
    for (u32 i = 0; i < SMP_TABLE_SIZE; i++)
        src[i] = smp_cpu(slots[i]);
    g.dev->lpVtbl->CopyDescriptors(
        g.dev, 1, &dst, &dst_size, SMP_TABLE_SIZE, src, NULL,
        D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    D3D12_GPU_DESCRIPTOR_HANDLE h;
    g.smp_heap->lpVtbl->GetGPUDescriptorHandleForHeapStart(g.smp_heap, &h);
    h.ptr += (u64)base * g.smp_step;
    return h;
}

/* ---------------------------------------------------------------------------
 * command list submit/wait (simple synchronous model, like the harness)
 * -----------------------------------------------------------------------*/
static void ld_flush(ld_flush_reason reason)
{
#if defined(YZ_PERF_PROFILE)
    const LONGLONG flush_begin = ld_profile_qpc();
    g_ld_profile.total.flush_reason[reason]++;
    ld_profile_note_ring_highwater();
#else
    (void)reason;
#endif
    if ((unsigned)reason < LD_FLUSH_REASON_COUNT) g_ld_flush_n[reason]++;
    const HRESULT close_hr = g.list->lpVtbl->Close(g.list);
    if (FAILED(close_hr)) {
        fprintf(stderr,
                "[d3d-fail] command-list Close hr=0x%08lX frame=%u\n",
                (unsigned long)close_hr, g_ld_frames);
        ld_dump_dred("Close", close_hr);
        g.ready = 0;
        return;
    }
    ID3D12CommandList* lists[] = {(ID3D12CommandList*)g.list};
    g.queue->lpVtbl->ExecuteCommandLists(g.queue, 1, lists);
    const u64 v = ++g.fence_value;
    const HRESULT signal_hr =
        g.queue->lpVtbl->Signal(g.queue, g.fence, v);
    if (FAILED(signal_hr)) {
        fprintf(stderr,
                "[d3d-fail] queue Signal hr=0x%08lX frame=%u\n",
                (unsigned long)signal_hr, g_ld_frames);
        ld_dump_dred("Signal", signal_hr);
        g.ready = 0;
        return;
    }
    /* GOWA_LD_PIPE: hand this submission to its set and continue on the other
     * one, waiting only for the submission that last used it. */
    /* GOWA_PIPE_SYNC=<mask of LD_FLUSH_* bits> (debug): those reasons wait as
     * without the pipe (bisecting the chapter-16 exposure flash) */
    static int ps_init; static u32 ps_mask;
    if (!ps_init) { const char* e = getenv("GOWA_PIPE_SYNC"); ps_mask = e ? (u32)strtoul(e, 0, 0) : 0u; ps_init = 1; }
    /* The vertex / pixel constant ring flushes come in the middle of a draw,
     * after its vertices are in the ring set: they wait and keep the set (the
     * descriptor ring flush rewrites them instead, see its call). */
    if (s_pipe && !((ps_mask >> reason) & 1u) && reason != LD_FLUSH_READBACK && reason != LD_FLUSH_SHUTDOWN &&
        reason != LD_FLUSH_VERTEX_CONSTANT_RING && reason != LD_FLUSH_PIXEL_CONSTANT_RING &&
        reason != LD_FLUSH_GUEST_REFERENCE && reason != LD_FLUSH_MOVIE &&
        reason != LD_FLUSH_MOVIE_PRESENT && reason != LD_FLUSH_RETIRE_QUEUE) {
        ld_ring_set* done = &s_sets[s_cur_set];
        done->fence = v;
        for (u32 i = 0; i < g.n_retired_textures; i++) done->retired[done->n_retired++] = g.retired_textures[i];
        g.n_retired_textures = 0;
        s_cur_set ^= 1u;
        ld_ring_set* next = &s_sets[s_cur_set];
        if (next->fence && g.fence->lpVtbl->GetCompletedValue(g.fence) < next->fence &&
            SUCCEEDED(g.fence->lpVtbl->SetEventOnCompletion(g.fence, next->fence, g.fence_event))) {
            LARGE_INTEGER w0, w1; QueryPerformanceCounter(&w0);
            WaitForSingleObject(g.fence_event, INFINITE);
            QueryPerformanceCounter(&w1); g_ld_flush_wait_qpc += w1.QuadPart - w0.QuadPart;
        }
        ld_drain_info_queue("flush");
        ld_set_release_retired(s_cur_set);
        ld_set_load(s_cur_set);
        g.alloc->lpVtbl->Reset(g.alloc);
        g.list->lpVtbl->Reset(g.list, g.alloc, NULL); ld_state_dirty();
        g.upload_used = 0;
        g.vb_used = 0; g.ib_used = 0; g.cb_used = 0; g.ps_cb_used = 0;
        g.srv_ring_used = 0; g.smp_ring_used = 0;
        return;
    }
    if (g.fence->lpVtbl->GetCompletedValue(g.fence) < v) {
#if defined(YZ_PERF_PROFILE)
        const LONGLONG wait_begin = ld_profile_qpc();
#endif
        const HRESULT event_hr =
            g.fence->lpVtbl->SetEventOnCompletion(
                g.fence, v, g.fence_event);
        if (FAILED(event_hr)) {
            fprintf(stderr,
                    "[d3d-fail] fence SetEventOnCompletion "
                    "hr=0x%08lX frame=%u\n",
                    (unsigned long)event_hr, g_ld_frames);
            ld_dump_dred("SetEventOnCompletion", event_hr);
            g.ready = 0;
            return;
        }
        { LARGE_INTEGER w0, w1; QueryPerformanceCounter(&w0);
          WaitForSingleObject(g.fence_event, INFINITE);
          QueryPerformanceCounter(&w1); g_ld_flush_wait_qpc += w1.QuadPart - w0.QuadPart; }
#if defined(YZ_PERF_PROFILE)
        const u64 wait_ticks = (u64)(ld_profile_qpc() - wait_begin);
        g_ld_profile.total.fence_wait_qpc += wait_ticks;
        g_ld_profile.total.fence_reason_qpc[reason] += wait_ticks;
#endif
    }
    ld_drain_info_queue("flush");
    /* Dynamic guest textures can replace a cached D3D resource while an
     * earlier draw in this command list still references the old one.  The
     * fence above is the first safe point at which those old resources may be
     * released. */
    for (u32 i = 0; i < g.n_retired_textures; i++)
        g.retired_textures[i]->lpVtbl->Release(g.retired_textures[i]);
    g.n_retired_textures = 0;
    if (s_pipe) {   /* everything submitted is done: both sets' leftovers too */
        ld_set_release_retired(0); ld_set_release_retired(1);
    }
    g.alloc->lpVtbl->Reset(g.alloc);
    g.list->lpVtbl->Reset(g.list, g.alloc, NULL); ld_state_dirty();
    g.upload_used = 0;
#if defined(YZ_PERF_PROFILE)
    const u64 flush_ticks = (u64)(ld_profile_qpc() - flush_begin);
    g_ld_profile.total.flush_qpc += flush_ticks;
    g_ld_profile.total.flush_reason_qpc[reason] += flush_ticks;
#endif
}

/* Public wrapper for the RSX SET_REFERENCE / sync fence: block until the GPU
 * has finished all queued draws (mirrors RPCS3 nv406e::set_reference's sync(),
 * RSXThread.cpp), so the game's REF poll advances only after the GPU has really
 * caught up. Without it our async consumer writes REF instantly and races ahead
 * of real GPU time (measured: ours skips the fence wait RPCS3 performs). Gated
 * at the call site by RSX_FENCE_SYNC. */
void rsx_live_draw_flush(void)
{
    if (g.ready) ld_flush(LD_FLUSH_GUEST_REFERENCE);
}

static void retire_texture(ID3D12Resource* tex)
{
    if (!tex) return;
    if (g.n_retired_textures >= MAX_RETIRED_TEXTURES)
        ld_flush(LD_FLUSH_RETIRE_QUEUE);
    g.retired_textures[g.n_retired_textures++] = tex;
    ld_profile_note_ring_highwater();
}

/* ---------------------------------------------------------------------------
 * texture upload (single-level + mip)
 * -----------------------------------------------------------------------*/
static u32 log2_u32(u32 v) { u32 n = 0; while (v > 1) { v >>= 1; n++; } return n; }
static u32 morton_index(u32 x, u32 y, u32 lw, u32 lh)
{
    u32 idx = 0, shift = 0;
    while (lw || lh) {
        if (lw) { idx |= (x & 1) << shift; x >>= 1; shift++; lw--; }
        if (lh) { idx |= (y & 1) << shift; y >>= 1; shift++; lh--; }
    }
    return idx;
}
static u8 remap_comp(const u8 s[4], u32 remap, u32 comp)
{
    const u32 op  = (remap >> (8 + comp * 2)) & 3;
    const u32 sel = (remap >> (comp * 2)) & 3;
    if (op == 0) return 0;
    if (op == 1) return 255;
    return s[sel];
}
static void decode_texel(u32 base_fmt, const u8* p, u32 remap, u8 d[4])
{
    u8 s[4];
    switch (base_fmt) {
    /* B8 is one channel. Broadcasting it to alpha as well is arguably more
     * faithful (the YUV movie combine shader samples each plane's .w), but
     * DO NOT do it until the USM decode actually fills those planes: while
     * they are empty it only makes the garbage VISIBLE, putting green noise
     * behind the title screen and every other movie-backed UI. Tried
     * 2026-09-01, reverted the same session. See ydkj-live-draw-rebuild. */
    case TEX_FMT_B8: s[0] = 255; s[1] = s[2] = s[3] = p[0]; break;
    case TEX_FMT_A4R4G4B4: {
        const u16 v = (u16)((p[0] << 8) | p[1]);
        s[0] = (u8)(((v >> 12) & 0xF) * 17); s[1] = (u8)(((v >> 8) & 0xF) * 17);
        s[2] = (u8)(((v >> 4) & 0xF) * 17);  s[3] = (u8)((v & 0xF) * 17); break;
    }
    case TEX_FMT_A1R5G5B5: {
        const u16 v = (u16)((p[0] << 8) | p[1]);
        /* LD_FORCE_A1_OPAQUE=1: experiment, not a fix. A PS1 15-bit pixel uses
         * the top bit as a semi-transparency FLAG, and it is clear for ordinary
         * opaque pixels -- so decoding it as A1R5G5B5 alpha gives alpha=0 for
         * essentially the whole framebuffer. If the pipeline then blends on
         * source alpha, every pixel is discarded and the surface reads black,
         * which is exactly what LD_SURF_DUMP reports for all nine surfaces.
         * This flips alpha to opaque so that theory can be tested in one run
         * instead of argued about. */
        { static int fo = -1;
          if (fo < 0) fo = getenv("LD_FORCE_A1_OPAQUE") ? 1 : 0;
          s[0] = fo ? 255 : ((v & 0x8000) ? 255 : 0); }
        s[1] = (u8)(((v >> 10) & 0x1F) * 255 / 31);
        s[2] = (u8)(((v >> 5) & 0x1F) * 255 / 31);
        s[3] = (u8)((v & 0x1F) * 255 / 31); break;
    }
    case TEX_FMT_R5G6B5: {
        const u16 v = (u16)((p[0] << 8) | p[1]);
        s[0] = 255;
        s[1] = (u8)(((v >> 11) & 0x1F) * 255 / 31);
        s[2] = (u8)(((v >> 5) & 0x3F) * 255 / 63);
        s[3] = (u8)((v & 0x1F) * 255 / 31); break;
    }
    case TEX_FMT_G8B8:
        s[0] = 255;
        s[1] = s[2] = p[0];
        s[3] = p[1];
        break;
    case RSX_TEX_FMT_W16Z16Y16X16_FLOAT: {
        /* Four big-endian halves, X Y Z W -> R G B A. Values are linear and
         * may exceed 1.0 (these are render targets), so clamp rather than
         * wrap. Decoded to 8-bit because the cache is RGBA8 throughout. */
        for (int c = 0; c < 4; c++) {
            const u16 hbits = (u16)((p[c * 2] << 8) | p[c * 2 + 1]);
            const int  exp  = (hbits >> 10) & 0x1F;
            const int  man  = hbits & 0x3FF;
            float f;
            if (exp == 0)        f = (float)man * (1.0f / 16384.0f) * (1.0f / 64.0f);
            else if (exp == 31)  f = 1.0f;
            else {
                float m = 1.0f + (float)man / 1024.0f;
                int   e = exp - 15;
                while (e > 0) { m *= 2.0f; e--; }
                while (e < 0) { m *= 0.5f; e++; }
                f = m;
            }
            if (hbits & 0x8000) f = 0.0f;   /* negative -> clamp to zero */
            if (f > 1.0f) f = 1.0f;
            /* s[] is A,R,G,B; the halves arrive X,Y,Z,W = R,G,B,A. */
            const u8 v = (u8)(f * 255.0f + 0.5f);
            if (c == 3) s[0] = v; else s[c + 1] = v;
        }
        break;
    }
    case TEX_FMT_DEPTH24_D8: s[0] = 255; s[1] = s[2] = s[3] = p[0]; break;
    /* X16: one big-endian 16-bit unorm channel, broadcast like B8 (the cache
     * is RGBA8, so the high byte). God of War Ascension's menu binds a 256x256
     * linear X16 texture; unsupported, it decoded to nothing and every bind
     * fell back to white. */
    case TEX_FMT_X16: s[0] = 255; s[1] = s[2] = s[3] = p[0]; break;
    /* Y16X16_FLOAT, 8-bit fallback (cube maps): A,R,G,B = Y,X,Y,X like the
     * float path below, each half clamped to 0..1. */
    case RSX_TEX_FMT_Y16X16_FLOAT: {
        for (int c = 0; c < 2; c++) {
            const u16 hbits = (u16)((p[c * 2] << 8) | p[c * 2 + 1]);
            const int exp = (hbits >> 10) & 0x1F;
            float f = exp == 31 ? 1.0f : ldexpf((float)((hbits & 0x3FF) | (exp ? 0x400 : 0)), (exp ? exp : 1) - 25);
            if (hbits & 0x8000) f = 0.0f;
            if (f > 1.0f) f = 1.0f;
            const u8 v = (u8)(f * 255.0f + 0.5f);
            if (c == 0) s[0] = s[2] = v; else s[1] = s[3] = v;
        }
        break;
    }
    default: s[0] = p[0]; s[1] = p[1]; s[2] = p[2]; s[3] = p[3]; break;
    }
    d[0] = remap_comp(s, remap, 1);
    d[1] = remap_comp(s, remap, 2);
    d[2] = remap_comp(s, remap, 3);
    d[3] = remap_comp(s, remap, 0);
}

/* GOWA: the 16-bit float formats as a real RGBA16F texture. The RGBA8 path
 * above clamps every half to 0..1 and quantizes it to 8 bits, which loses
 * negative and >1 values (HDR targets, offset/normal maps); Y16X16_FLOAT
 * (0x9F) had no decoder at all and every bind fell back to white (>4,000
 * times in the first level). Halves are copied bit for bit, only byte-swapped
 * and remapped. Component order in A,R,G,B, as RPCS3 maps them:
 *   W16Z16Y16X16 : halves X Y Z W -> R G B A
 *   Y16X16       : halves 0 1     -> A=0 R=1 G=0 B=1
 * LD_F16_TEX=0 sends them back through the RGBA8 path. */
static int is_f16_format(u32 base_fmt)
{
    static int on = -1;
    if (on < 0) { const char* e = getenv("LD_F16_TEX"); on = e ? atoi(e) : 1; }
    return on && (base_fmt == RSX_TEX_FMT_W16Z16Y16X16_FLOAT ||
                  base_fmt == RSX_TEX_FMT_Y16X16_FLOAT);
}
static void decode_texel_f16(u32 base_fmt, const u8* p, u32 remap, u16 d[4])
{
    u16 s[4];
#define H16(i) ((u16)((p[(i) * 2] << 8) | p[(i) * 2 + 1]))
    if (base_fmt == RSX_TEX_FMT_Y16X16_FLOAT) {
        s[0] = s[2] = H16(0);
        s[1] = s[3] = H16(1);
    } else {
        /* GOWA_F16_ORDER=1: halves in memory as A,R,G,B (like A8R8G8B8)
         * instead of X,Y,Z,W = R,G,B,A -- under test against God of War
         * Ascension's fog colour texture (1:0x26C7280) */
        static int ord = -1;
        if (ord < 0) { const char* e = getenv("GOWA_F16_ORDER"); ord = e ? atoi(e) : 0; }
        if (ord == 1) { s[0] = H16(0); s[1] = H16(1); s[2] = H16(2); s[3] = H16(3); }
        /* 2: W,Z,Y,X from the first half on -- the name read from the top
         * component down, as in A8R8G8B8 and Y16X16 (X is the last half) */
        else if (ord == 2) { s[0] = H16(0); s[1] = H16(3); s[2] = H16(2); s[3] = H16(1); }
        else { s[0] = H16(3); s[1] = H16(0); s[2] = H16(1); s[3] = H16(2); }
    }
#undef H16
    static const u32 out2comp[4] = { 1, 2, 3, 0 };
    for (u32 o = 0; o < 4; o++) {
        const u32 comp = out2comp[o];
        const u32 op  = (remap >> (8 + comp * 2)) & 3;
        const u32 sel = (remap >> (comp * 2)) & 3;
        d[o] = op == 0 ? 0x0000 : op == 1 ? 0x3C00 : s[sel];
    }
}

typedef struct { u32 w, h; const u8* data; u32 row_bytes, rows; } tex_level_t;

/* GOWA: a texture whose guest content changed is re-uploaded INTO its
 * existing resource when the layout matches (set around the refresh decode in
 * texture_srv_slot): God of War Ascension rewrites its fog volume and other
 * small textures every frame, and a committed resource created and destroyed
 * per change cost ~11% of the RSX thread in DestroyAllocation alone, at the
 * end of each frame. The transition barrier orders the copy after every
 * earlier read of the resource on the queue. GOWA_TEX_REUSE=0 disables. */
static ID3D12Resource* g_ld_reuse_tex;
static int ld_take_reuse(D3D12_RESOURCE_DIMENSION dim, DXGI_FORMAT fmt, u32 w, u32 h, u32 d, u32 mips)
{
    static int on = -1;
    if (on < 0) { const char* e = getenv("GOWA_TEX_REUSE"); on = !(e && *e == '0'); }
    if (!on || !g_ld_reuse_tex) return 0;
    D3D12_RESOURCE_DESC od;
    g_ld_reuse_tex->lpVtbl->GetDesc(g_ld_reuse_tex, &od);
    if (od.Dimension != dim || od.Format != fmt || od.Width != w || od.Height != h ||
        od.DepthOrArraySize != d || od.MipLevels != mips)
        return 0;
    D3D12_RESOURCE_BARRIER b = {0};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; b.Transition.pResource = g_ld_reuse_tex;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    g.list->lpVtbl->ResourceBarrier(g.list, 1, &b);
    return 1;
}
/* GOWA: room for `bytes` more in the upload ring. When this frame's uploads
 * have filled it (the start of a level streams in hundreds of textures), flush
 * -- a full GPU sync, the same flush a full retire queue takes, at the same
 * point of a draw as the descriptor-ring flush: before any of its render state
 * is recorded -- and start the ring over. Before, the texture creators gave up
 * here: create_texture_mipped dropped the remaining mips silently, and
 * create_texture_cube/_3d Released a texture whose first faces' copies were
 * already recorded in the open command list, so the GPU later wrote into a
 * freed resource: DXGI_ERROR_DEVICE_REMOVED, a page fault on a "recent-freed"
 * resource during CopyTextureRegion, at the start of the level (5 runs on
 * 18-19/10, the "decode failed" lines right before each). */
static int ld_upload_room(u64 bytes)
{
    if ((u64)((g.upload_used + 511) & ~511u) + bytes <= UPLOAD_SIZE) return 1;
    if (bytes > UPLOAD_SIZE || !g.upload_used || !g.ready) return 0;
    static u32 n_flush;
    if (n_flush++ < 8 || (n_flush & (n_flush - 1)) == 0)
        fprintf(stderr, "[upload] anel de upload cheio (%u MB usados, +%llu KB): flush #%u\n",
                g.upload_used >> 20, (unsigned long long)(bytes >> 10), n_flush);
    ld_flush(LD_FLUSH_RETIRE_QUEUE);
    return g.ready && (u64)((g.upload_used + 511) & ~511u) + bytes <= UPLOAD_SIZE;
}
static ID3D12Resource* create_texture_mipped(DXGI_FORMAT fmt, const tex_level_t* lv, u32 n)
{
    if (n == 0) return NULL;
    D3D12_HEAP_PROPERTIES hp = {0}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC rd = {0};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rd.Width = lv[0].w; rd.Height = lv[0].h; rd.DepthOrArraySize = 1;
    rd.MipLevels = (u16)n; rd.Format = fmt; rd.SampleDesc.Count = 1;
    ID3D12Resource* tex = NULL;
    if (ld_take_reuse(D3D12_RESOURCE_DIMENSION_TEXTURE2D, fmt, lv[0].w, lv[0].h, 1, n))
        tex = g_ld_reuse_tex;
    else if (FAILED(g.dev->lpVtbl->CreateCommittedResource(g.dev, &hp, D3D12_HEAP_FLAG_NONE, &rd,
                                                      D3D12_RESOURCE_STATE_COPY_DEST, NULL,
                                                      &IID_ID3D12Resource, (void**)&tex)))
        return NULL;
    for (u32 m = 0; m < n; m++) {
        const u32 pitch = (lv[m].row_bytes + 255) & ~255u;
        if (!ld_upload_room((u64)pitch * lv[m].rows)) break;
        const u32 start = (g.upload_used + 511) & ~511u;
        for (u32 y = 0; y < lv[m].rows; y++)
            memcpy(g.upload_mapped + start + (size_t)y * pitch,
                   lv[m].data + (size_t)y * lv[m].row_bytes, lv[m].row_bytes);
        D3D12_TEXTURE_COPY_LOCATION src = {0}, dst = {0};
        src.pResource = g.upload; src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint.Offset = start;
        src.PlacedFootprint.Footprint.Format = fmt;
        src.PlacedFootprint.Footprint.Width = lv[m].w;
        src.PlacedFootprint.Footprint.Height = lv[m].h;
        src.PlacedFootprint.Footprint.Depth = 1;
        src.PlacedFootprint.Footprint.RowPitch = pitch;
        dst.pResource = tex; dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = m;
        g.list->lpVtbl->CopyTextureRegion(g.list, &dst, 0, 0, 0, &src, NULL);
        g.upload_used = start + pitch * lv[m].rows;
#if defined(YZ_PERF_PROFILE)
        g_ld_profile.total.texture_upload_bytes +=
            (u64)pitch * lv[m].rows;
#endif
        ld_profile_note_ring_highwater();
    }
    D3D12_RESOURCE_BARRIER b = {0};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; b.Transition.pResource = tex;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    g.list->lpVtbl->ResourceBarrier(g.list, 1, &b);
    return tex;
}

/* Cubemap faces are stored face-major in guest memory. D3D12 subresources use
 * that same order: face * mip_count + mip. */
static ID3D12Resource* create_texture_cube(
    DXGI_FORMAT fmt, const tex_level_t* lv, u32 n_mips)
{
    if (!n_mips) return NULL;
    D3D12_HEAP_PROPERTIES hp = {0}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC rd = {0};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rd.Width = lv[0].w; rd.Height = lv[0].h; rd.DepthOrArraySize = 6;
    rd.MipLevels = (u16)n_mips; rd.Format = fmt; rd.SampleDesc.Count = 1;
    ID3D12Resource* tex = NULL;
    if (FAILED(g.dev->lpVtbl->CreateCommittedResource(
            g.dev, &hp, D3D12_HEAP_FLAG_NONE, &rd,
            D3D12_RESOURCE_STATE_COPY_DEST, NULL,
            &IID_ID3D12Resource, (void**)&tex)))
        return NULL;
    for (u32 face = 0; face < 6; face++) {
        for (u32 mip = 0; mip < n_mips; mip++) {
            const tex_level_t* level = &lv[face * n_mips + mip];
            const u32 pitch = (level->row_bytes + 255) & ~255u;
            if (!ld_upload_room((u64)pitch * level->rows)) {
                retire_texture(tex);   /* copies into it may already be recorded */
                return NULL;
            }
            const u32 start = (g.upload_used + 511) & ~511u;
            for (u32 y = 0; y < level->rows; y++)
                memcpy(g.upload_mapped + start + (size_t)y * pitch,
                       level->data + (size_t)y * level->row_bytes,
                       level->row_bytes);
            D3D12_TEXTURE_COPY_LOCATION src = {0}, dst = {0};
            src.pResource = g.upload;
            src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            src.PlacedFootprint.Offset = start;
            src.PlacedFootprint.Footprint.Format = fmt;
            src.PlacedFootprint.Footprint.Width = level->w;
            src.PlacedFootprint.Footprint.Height = level->h;
            src.PlacedFootprint.Footprint.Depth = 1;
            src.PlacedFootprint.Footprint.RowPitch = pitch;
            dst.pResource = tex;
            dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dst.SubresourceIndex = face * n_mips + mip;
            g.list->lpVtbl->CopyTextureRegion(
                g.list, &dst, 0, 0, 0, &src, NULL);
            g.upload_used = start + pitch * level->rows;
#if defined(YZ_PERF_PROFILE)
            g_ld_profile.total.texture_upload_bytes +=
                (u64)pitch * level->rows;
#endif
            ld_profile_note_ring_highwater();
        }
    }
    D3D12_RESOURCE_BARRIER b = {0};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = tex;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    g.list->lpVtbl->ResourceBarrier(g.list, 1, &b);
    return tex;
}

static ID3D12Resource* create_texture_rgba(const u8* rgba, u32 w, u32 h)
{
    tex_level_t lv = { w, h, rgba, w * 4, h };
    return create_texture_mipped(DXGI_FORMAT_R8G8B8A8_UNORM, &lv, 1);
}

/* GOWA: a volume texture -- level m is depth[m] slices of lv[m], the slices
 * back to back in lv[m].data. God of War Ascension's sky/fog colour is one
 * (64x32xN RGBA16F); decoded as nothing, its unit read white and the fog
 * covered the whole sky. */
static ID3D12Resource* create_texture_3d(DXGI_FORMAT fmt, const tex_level_t* lv,
                                         const u32* depth, u32 n)
{
    if (n == 0) return NULL;
    D3D12_HEAP_PROPERTIES hp = {0}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC rd = {0};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    rd.Width = lv[0].w; rd.Height = lv[0].h; rd.DepthOrArraySize = (u16)depth[0];
    rd.MipLevels = (u16)n; rd.Format = fmt; rd.SampleDesc.Count = 1;
    ID3D12Resource* tex = NULL;
    int reused = 0;
    if (ld_take_reuse(D3D12_RESOURCE_DIMENSION_TEXTURE3D, fmt, lv[0].w, lv[0].h, depth[0], n)) {
        tex = g_ld_reuse_tex; reused = 1;
    } else if (FAILED(g.dev->lpVtbl->CreateCommittedResource(g.dev, &hp, D3D12_HEAP_FLAG_NONE, &rd,
                                                      D3D12_RESOURCE_STATE_COPY_DEST, NULL,
                                                      &IID_ID3D12Resource, (void**)&tex)))
        return NULL;
    for (u32 m = 0; m < n; m++) {
        const u32 pitch = (lv[m].row_bytes + 255) & ~255u;
        const u64 bytes = (u64)pitch * lv[m].rows * depth[m];
        const int room = ld_upload_room(bytes);
        const u32 start = (g.upload_used + 511) & ~511u;
        if (!room) {
            if (reused) {   /* back to readable, keep the old content */
                D3D12_RESOURCE_BARRIER rb = {0};
                rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; rb.Transition.pResource = tex;
                rb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                rb.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
                rb.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
                g.list->lpVtbl->ResourceBarrier(g.list, 1, &rb);
                return tex;
            }
            retire_texture(tex);   /* copies into it may already be recorded */
            return NULL;
        }
        for (u32 z = 0; z < depth[m]; z++)
            for (u32 y = 0; y < lv[m].rows; y++)
                memcpy(g.upload_mapped + start + ((size_t)z * lv[m].rows + y) * pitch,
                       lv[m].data + ((size_t)z * lv[m].rows + y) * lv[m].row_bytes,
                       lv[m].row_bytes);
        D3D12_TEXTURE_COPY_LOCATION src = {0}, dst = {0};
        src.pResource = g.upload; src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint.Offset = start;
        src.PlacedFootprint.Footprint.Format = fmt;
        src.PlacedFootprint.Footprint.Width = lv[m].w;
        src.PlacedFootprint.Footprint.Height = lv[m].h;
        src.PlacedFootprint.Footprint.Depth = depth[m];
        src.PlacedFootprint.Footprint.RowPitch = pitch;
        dst.pResource = tex; dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = m;
        g.list->lpVtbl->CopyTextureRegion(g.list, &dst, 0, 0, 0, &src, NULL);
        g.upload_used = start + (u32)bytes;
        ld_profile_note_ring_highwater();
    }
    D3D12_RESOURCE_BARRIER b = {0};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; b.Transition.pResource = tex;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    g.list->lpVtbl->ResourceBarrier(g.list, 1, &b);
    return tex;
}

/* GOWA: slices of a volume texture (TEX_SIZE1 depth), 0 for 2D and cube */
static u32 tex_depth3d(const rsx_dsp_texture* t)
{
    return (t->dimension == 3 && !t->cubemap) ? (t->depth ? t->depth : 1u) : 0u;
}
static u32 morton3_index(u32 x, u32 y, u32 z, u32 lw, u32 lh, u32 ld)
{
    u32 idx = 0, shift = 0;
    while (lw || lh || ld) {
        if (lw) { idx |= (x & 1) << shift; x >>= 1; shift++; lw--; }
        if (lh) { idx |= (y & 1) << shift; y >>= 1; shift++; lh--; }
        if (ld) { idx |= (z & 1) << shift; z >>= 1; shift++; ld--; }
    }
    return idx;
}

static u32 texture_source_span(const rsx_dsp_texture* t)
{
    const u32 base_fmt = t->format & TEX_FMT_BASE_MASK & ~(u32)TEX_FMT_UNNORM;
    const int linear = (t->format & TEX_FMT_LINEAR) != 0;
    u32 texel_size = 0, block_size = 0;
    switch (base_fmt) {
    case TEX_FMT_DXT1:  block_size = 8; break;
    case TEX_FMT_DXT23:
    case TEX_FMT_DXT45: block_size = 16; break;
    case TEX_FMT_B8: texel_size = 1; break;
    case TEX_FMT_A4R4G4B4:
    case TEX_FMT_A1R5G5B5:
    case TEX_FMT_R5G6B5:
    case TEX_FMT_G8B8:
    case TEX_FMT_X16: texel_size = 2; break;
    case RSX_TEX_FMT_W16Z16Y16X16_FLOAT: texel_size = 8; break;
    case RSX_TEX_FMT_Y16X16_FLOAT: texel_size = 4; break;
    case TEX_FMT_A8R8G8B8:
    case TEX_FMT_DEPTH24_D8: texel_size = 4; break;
    default: return 0;
    }
    const u32 d3 = tex_depth3d(t);
    if (!t->width || !t->height || t->width > 4096 || t->height > 4096 ||
        (t->dimension != 2 && !d3) || (d3 && block_size))
        return 0;
    u32 n_mips = t->mipmaps ? t->mipmaps : 1;
    if (n_mips > 14) n_mips = 14;
    /* A pitch-linear (LN) texture cannot carry a mip chain -- the RSX samples
     * only its base level and ignores the mipmap count in SET_TEXTURE_FORMAT.
     * Honouring that count instead walks `off += pitch * mh` through whatever
     * guest memory follows the base image and builds every lower level out of
     * it, and a minified draw then samples that garbage.
     *
     * Cherry-picked by hand from flow/live-draw 3f4bdb9, where flOw's EULA
     * showed "banded olive stripes with a ghosted second copy of the page".
     * Twisted Metal binds PS1 VRAM pitch-linear (fmt 0xE2/0xE1 carry
     * TEX_FMT_LINEAR) and shows banded green/magenta stripes, which is the same
     * symptom on the same mechanism. */
    if (linear) n_mips = 1;
    if (t->cubemap && block_size) {
        n_mips = 1;
        for (u32 d = (t->width < t->height ? t->width : t->height) / 4;
             d > 1; d >>= 1)
            n_mips++;
        if (t->mipmaps && n_mips > t->mipmaps)
            n_mips = t->mipmaps;
        if (n_mips > 14) n_mips = 14;
    }
    u32 mw = t->width, mh = t->height, md = d3 ? d3 : 1u, span = 0;
    for (u32 m = 0; m < n_mips; m++) {
        if (block_size)
            span += ((mw + 3) / 4) * block_size * ((mh + 3) / 4);
        else {
            const u32 pitch = (m == 0 && linear && t->pitch)
                ? t->pitch : mw * texel_size;
            span += pitch * mh * md;
        }
        if (mw == 1 && mh == 1 && md == 1) break;
        mw = mw > 1 ? mw >> 1 : 1;
        mh = mh > 1 ? mh >> 1 : 1;
        md = md > 1 ? md >> 1 : 1;
    }
    return t->cubemap ? span * 6 : span;
}

/* GOWA_TEX_WW (22/10): a textura foi escrita desde o quadro `desde`? A reserva da memória do
 * guest tem MEM_WRITE_WATCH (boot_main_gowa.cpp); GetWriteWatch com reset na faixa da textura
 * devolve as páginas escritas desde a última consulta, e elas vão para um mapa global "quadro da
 * última escrita vista" (s_ld_pg) -- assim duas texturas que dividem uma página não perdem a
 * escrita uma da outra. 1 = pode ter mudado (refaz o hash); 0 = intacta. Na dúvida, 1. */
extern uint8_t* vm_base;
extern int g_gowa_vm_ww;
static u32* s_ld_pg;   /* 1M páginas de 4 KB: g_ld_frames + 1 na hora em que a escrita foi vista */
static u64 s_ld_ww_n[3];   /* consultas, intactas, com páginas sujas */
/* Leituras de arquivo direto na memória do guest (fread do cellFs/sys_fs): marca as páginas
 * como escritas agora, sem depender de o Windows ver escritas do kernel no write-watch. */
void gowa_ww_marca(const void* p, size_t n)
{
    if (!g_gowa_vm_ww || !vm_base || !p || !n) return;
    if (!s_ld_pg) { s_ld_pg = (u32*)calloc(1u << 20, sizeof(u32)); if (!s_ld_pg) return; }
    const uintptr_t vb = (uintptr_t)vm_base, a = (uintptr_t)p;
    if (a < vb || a + n > vb + 0x100000000ull) return;
    for (uintptr_t q = (a - vb) >> 12, fim = (a + n - 1 - vb) >> 12; q <= fim; q++) s_ld_pg[q] = g_ld_frames + 1u;
}
static int ld_tex_paginas_sujas(const u8* src, u32 span, u32 desde)
{
    static int on = -1;
    if (on < 0) { on = g_gowa_vm_ww && vm_base;
                  if (on && !s_ld_pg) { s_ld_pg = (u32*)calloc(1u << 20, sizeof(u32)); if (!s_ld_pg) on = 0; } }
    if (!on || !src || !span) return 1;
    const uintptr_t vb = (uintptr_t)vm_base;
    uintptr_t a0 = (uintptr_t)src & ~(uintptr_t)4095, a1 = ((uintptr_t)src + span + 4095) & ~(uintptr_t)4095;
    if (a0 < vb || a1 > vb + 0x100000000ull) return 1;
    s_ld_ww_n[0]++;
    for (int volta = 0; volta < 64; volta++) {
        PVOID pg[256]; ULONG_PTR n = 256; ULONG gran = 0;
        if (GetWriteWatch(WRITE_WATCH_FLAG_RESET, (PVOID)a0, (SIZE_T)(a1 - a0), pg, &n, &gran) != 0) return 1;
        for (ULONG_PTR k = 0; k < n; k++) s_ld_pg[((uintptr_t)pg[k] - vb) >> 12] = g_ld_frames + 1u;
        if (n < 256) break;
    }
    for (uintptr_t a = a0; a < a1; a += 4096)
        if (s_ld_pg[(a - vb) >> 12] > desde) { s_ld_ww_n[2]++; return 1; }
    s_ld_ww_n[1]++;
    return 0;
}

static u64 texture_content_hash_guest(const rsx_dsp_texture* t, int* readable)
{
    const u32 span = texture_source_span(t);
    const u8* src = span ? guest_ptr(t->location, t->offset, span) : NULL;
    if (!src) {
        *readable = 0;
        return 0;
    }
    /* GOWA_TEXPEEK=<offset hex>: when that texture is hashed (once per frame
     * at most), count its 32-bit texels with non-zero colour bytes -- what the
     * guest memory held at the moment the live engine sampled it. */
    { static int tp = -2; static u32 tp_off; static u32 tpn = 0;
      if (tp == -2) { const char* e = getenv("GOWA_TEXPEEK"); tp = e ? 1 : 0; tp_off = e ? (u32)strtoul(e, 0, 16) : 0; }
      if (tp && t->offset == tp_off && (tpn < 8 || (tpn & 31) == 0)) {
          u32 nz = 0, na = 0;
          for (u32 q = 0; q + 3 < span; q += 4) { if (src[q + 1] | src[q + 2] | src[q + 3]) nz++; if (src[q]) na++; }
          fprintf(stderr, "[texpeek] n=%u frame=%u loc=%u off=0x%08X %ux%u fmt=0x%02X remap=0x%08X span=%u rgb_nonzero=%u a_nonzero=%u\n",
                  tpn, g_ld_frames, t->location, t->offset, t->width, t->height, t->format & 0xFF, t->remap, span, nz, na);
      }
      if (tp && t->offset == tp_off) tpn++; }
    /* LD_TEXSRC_DBG=1: is the guest data there AT ALL?
     *
     * "Draws execute, forced green fills the screen, the sampled texture is
     * black" keeps arriving as the same question, and the decoded-RGBA dump
     * cannot answer it: that dump lives on the uncompressed path, so a DXT
     * texture -- which is most of a real game's atlas -- never reaches it.
     * Counting non-zero SOURCE bytes covers every format and separates "the
     * upload never happened" from "decode ate it". */
    { static int ts = -1; static int tsn = 0;
      if (ts < 0) ts = getenv("LD_TEXSRC_DBG") ? 1 : 0;
      if (ts && tsn < 16) {
          u32 nz = 0;
          for (u32 q = 0; q < span; q++) if (src[q]) nz++;
          char head[80]; int hn = 0;
          for (u32 q = 0; q < 16 && q < span; q++)
              hn += snprintf(head + hn, sizeof head - hn, "%02X", src[q]);
          fprintf(stderr, "[tex-src] loc=%u off=0x%08X %ux%u fmt=0x%02X mips=%u"
                          " remap=0x%08X filter=0x%08X head=%s"
                          " span=%u nonzero=%u\n",
                  t->location, t->offset, t->width, t->height,
                  t->format & 0xFF, t->mipmaps, t->remap, t->filter, head,
                  span, nz);
          fflush(stderr);
          tsn++;
      } }

    /* GOWA_TEX_HASH_SAMPLE=<KB> (default 1024; 0 = always the full hash): a
     * texture larger than that is hashed from 256 stripes of 64 bytes spread
     * over its whole span instead of every byte. The full hash of every bound
     * texture once a frame read tens of MB per frame in God of War Ascension
     * (a 2048x4096 DXT5 is 8 MB) -- ~7% of the RSX thread. The sampled hash is
     * always the same function of the same texture, so frame-to-frame
     * comparison stays consistent; the game's texture streaming rewrites whole
     * textures, which moves the samples. */
    /* GOWA (25/10): com o write-watch (GOWA_TEX_WW) só chega aqui a textura que FOI escrita
     * (as intactas pulam o hash antes), e ela tem o hash completo. Por amostra, um jogo que
     * carrega a textura em partes ao longo de vários quadros (disco mais lento) mudava as
     * tiras só na 1a parte: o resto ficava velho, em blocos, e havia materiais sem textura
     * (inimigos pretos, mãos amarelas; relato de jogador, 5600X + RTX 3060 Ti). */
    { static long thr = -1;
      if (thr < 0) { const char* e = getenv("GOWA_TEX_HASH_SAMPLE"); thr = (e ? atol(e) : 1024L) * 1024L; }
      if (thr > 0 && span > (u32)thr && span >= 256u * 64u && !(g_gowa_vm_ww && vm_base)) {
          u64 h = 1469598103934665603ull ^ span;
          const u64 step = ((u64)(span - 64u) << 16) / 255u;   /* 16.16 fixed point */
          for (u32 k = 0; k < 256u; k++) {
              const u32 o = (u32)((step * k) >> 16) & ~7u;
              const u8* q = src + o;
              for (u32 w = 0; w < 64u; w += 8) {
                  u64 v; memcpy(&v, q + w, 8);
                  h = (h ^ v) * 1099511628211ull;
                  h ^= h >> 29;
              }
          }
          *readable = 1;
          return h ? h : 1;
      } }
    /* One hash per cached texture per presented frame.  Word-at-a-time FNV is
     * deliberately cheap; this is a mutation detector, not a content ID. */
    u64 hash = 1469598103934665603ull;
    u32 i = 0;
#if defined(__AVX2__)
    /* GOWA: 128 bytes per step in four 256-bit accumulators (the XXH3
     * accumulate step: (d ^ k).lo32 * (d ^ k).hi32 plus d itself, so every
     * input bit reaches the sum). A mutation detector, ~4x the scalar lanes
     * below; the hash was ~12% of the RSX thread in God of War Ascension. */
    if (span >= 128) {
        const __m256i k0 = _mm256_set_epi64x(0x1CAD21F72C81017Cll, 0xDB979083E96DD4DEll, 0x1F67B3B7A4A44072ll, 0x78E5C0CC4EE679CBll);
        const __m256i k1 = _mm256_set_epi64x(0x2172FFCC7DD05A82ll, 0x8E2443F7744608B8ll, 0x4C263A81E69035E0ll, 0xCB00C391BB52283Cll);
        __m256i a0 = _mm256_set1_epi64x((long long)hash), a1 = _mm256_set1_epi64x(0x9E3779B97F4A7C15ll);
        __m256i a2 = _mm256_set1_epi64x(0xC2B2AE3D27D4EB4Fll), a3 = _mm256_set1_epi64x(0x165667B19E3779F9ll);
#define TCH_ACC(a, d, k) do { const __m256i _dk = _mm256_xor_si256((d), (k)); \
            (a) = _mm256_add_epi64(_mm256_add_epi64((a), (d)), _mm256_mul_epu32(_dk, _mm256_srli_epi64(_dk, 32))); } while (0)
        for (; i + 128 <= span; i += 128) {
            const __m256i d0 = _mm256_loadu_si256((const __m256i*)(src + i));
            const __m256i d1 = _mm256_loadu_si256((const __m256i*)(src + i + 32));
            const __m256i d2 = _mm256_loadu_si256((const __m256i*)(src + i + 64));
            const __m256i d3 = _mm256_loadu_si256((const __m256i*)(src + i + 96));
            TCH_ACC(a0, d0, k0); TCH_ACC(a1, d1, k1); TCH_ACC(a2, d2, k1); TCH_ACC(a3, d3, k0);
        }
#undef TCH_ACC
        u64 l[16];
        _mm256_storeu_si256((__m256i*)l, a0); _mm256_storeu_si256((__m256i*)(l + 4), a1);
        _mm256_storeu_si256((__m256i*)(l + 8), a2); _mm256_storeu_si256((__m256i*)(l + 12), a3);
        for (int q = 0; q < 16; q++) { hash ^= l[q]; hash *= 1099511628211ull; hash ^= hash >> 29; }
    }
#endif
    /* GOWA: four independent lanes, 32 bytes per step. The single chain is
     * bound by the multiply latency (8 bytes per ~4 cycles), and this hash
     * was ~15% of the RSX thread in God of War Ascension's first level. */
    {
        u64 h1 = hash, h2 = hash ^ 0x9E3779B97F4A7C15ull,
            h3 = hash ^ 0xC2B2AE3D27D4EB4Full, h4 = hash ^ 0x165667B19E3779F9ull;
        for (; i + 32 <= span; i += 32) {
            u64 w[4];
            memcpy(w, src + i, sizeof(w));
            h1 = (h1 ^ w[0]) * 1099511628211ull;
            h2 = (h2 ^ w[1]) * 1099511628211ull;
            h3 = (h3 ^ w[2]) * 1099511628211ull;
            h4 = (h4 ^ w[3]) * 1099511628211ull;
        }
        hash = ((h1 * 31 + h2) * 31 + h3) * 31 + h4;
    }
    for (; i + 8 <= span; i += 8) {
        u64 word;
        memcpy(&word, src + i, sizeof(word));
        hash ^= word;
        hash *= 1099511628211ull;
    }
    for (; i < span; i++) {
        hash ^= src[i];
        hash *= 1099511628211ull;
    }
    *readable = 1;
    return hash;
}

/* Report 37: the HUD's PS3 button glyphs (64x64 DXT5 with mips) drawn again with the keys and the mouse of
 * cellPad.c (rsx_teclas.h, from scripts/teclas.py) while keyboard and mouse are the last input used. A glyph is
 * known by the FNV-1a hash of its base level; its content hash changes with the device, so the cached texture is
 * decoded again when the player switches. */
#include "rsx_teclas.h"
int cellPad_teclado_ultimo(void);
static int ld_tecla(const rsx_dsp_texture* t)
{
    if ((t->format & TEX_FMT_BASE_MASK & ~(u32)TEX_FMT_UNNORM) != TEX_FMT_DXT45 ||
        (t->format & TEX_FMT_LINEAR) || t->cubemap || t->width != 64 || t->height != 64)
        return -1;
    const u8* src = guest_ptr(t->location, t->offset, 4096);
    if (!src) return -1;
    u64 h = 0xcbf29ce484222325ull;
    for (u32 i = 0; i < 4096; i++) h = (h ^ src[i]) * 0x100000001b3ull;
    for (int i = 0; i < (int)(sizeof k_teclas / sizeof k_teclas[0]); i++)
        if (k_teclas[i].hash_ps3 == h) return i;
    return -1;
}
/* the keyboard glyph to decode in place of the guest bytes, or NULL */
static const u8* ld_tecla_dados(const rsx_dsp_texture* t, u32 total)
{
    if (!cellPad_teclado_ultimo()) return NULL;
    const int i = ld_tecla(t);
    return i >= 0 && total <= k_teclas[i].tam ? k_teclas[i].dados : NULL;
}
static u64 texture_content_hash(const rsx_dsp_texture* t, int* readable)
{
    u64 h = texture_content_hash_guest(t, readable);
    if (*readable && cellPad_teclado_ultimo() && ld_tecla(t) >= 0) h ^= 0x9E3779B97F4A7C15ull;
    return h;
}

/* GOWA_TEXFAIL_WHY=1: why a texture does not decode (first 32) */
static void ld_texfail_why(const rsx_dsp_texture* t, const char* why)
{
    static int on = -1; static int n;
    if (on < 0) on = getenv("GOWA_TEXFAIL_WHY") ? 1 : 0;
    if (on && n++ < 32)
        fprintf(stderr, "[texfail] %u:0x%08X fmt=0x%02X %ux%u dim=%u mips=%u cube=%u pitch=%u: %s\n",
                t->location, t->offset, t->format, t->width, t->height, t->dimension,
                t->mipmaps, t->cubemap, t->pitch, why);
}
static ID3D12Resource* decode_guest_texture(const rsx_dsp_texture* t, u32 remap)
{
    const u32 base_fmt = t->format & TEX_FMT_BASE_MASK & ~(u32)TEX_FMT_UNNORM;
    const int linear = (t->format & TEX_FMT_LINEAR) != 0;
    const u32 w = t->width, h = t->height;
    if (!w || !h || w > 4096 || h > 4096 || (t->dimension != 2 && !tex_depth3d(t))) {
        ld_texfail_why(t, "size/dimension");
        return NULL;
    }
    u32 n_mips = t->mipmaps ? t->mipmaps : 1;
    if (n_mips > 14) n_mips = 14;
    /* A pitch-linear (LN) texture cannot carry a mip chain -- the RSX samples
     * only its base level and ignores the mipmap count in SET_TEXTURE_FORMAT.
     * Honouring that count instead walks `off += pitch * mh` through whatever
     * guest memory follows the base image and builds every lower level out of
     * it, and a minified draw then samples that garbage.
     *
     * Cherry-picked by hand from flow/live-draw 3f4bdb9, where flOw's EULA
     * showed "banded olive stripes with a ghosted second copy of the page".
     * Twisted Metal binds PS1 VRAM pitch-linear (fmt 0xE2/0xE1 carry
     * TEX_FMT_LINEAR) and shows banded green/magenta stripes, which is the same
     * symptom on the same mechanism. */
    if (linear) n_mips = 1;

    if (base_fmt == TEX_FMT_DXT1 || base_fmt == TEX_FMT_DXT23 ||
        base_fmt == TEX_FMT_DXT45) {
        if (tex_depth3d(t)) { ld_texfail_why(t, "dxt volume"); return NULL; }
        const DXGI_FORMAT dxgi = base_fmt == TEX_FMT_DXT1 ? DXGI_FORMAT_BC1_UNORM
                               : base_fmt == TEX_FMT_DXT23 ? DXGI_FORMAT_BC2_UNORM
                                                           : DXGI_FORMAT_BC3_UNORM;
        const u32 block = base_fmt == TEX_FMT_DXT1 ? 8 : 16;
        const u32 total = texture_source_span(t);
        const u8* src = guest_ptr(t->location, t->offset, total);
        if (!src) return NULL;
        { const u8* kb = ld_tecla_dados(t, total); if (kb) src = kb; }   /* report 37 */
        if (t->cubemap) {
            n_mips = 1;
            for (u32 d = (w < h ? w : h) / 4; d > 1; d >>= 1)
                n_mips++;
            if (t->mipmaps && n_mips > t->mipmaps)
                n_mips = t->mipmaps;
            if (n_mips > 14) n_mips = 14;
            const u32 face_span = total / 6;
            tex_level_t cube_levels[6 * 14];
            u32 level = 0;
            for (u32 face = 0; face < 6; face++) {
                u32 mw = w, mh = h, off = 0;
                for (u32 mip = 0; mip < n_mips; mip++) {
                    const u32 bw = (mw + 3) / 4, bh = (mh + 3) / 4;
                    cube_levels[level].w = (mw + 3) & ~3u;
                    cube_levels[level].h = (mh + 3) & ~3u;
                    cube_levels[level].data =
                        src + (size_t)face * face_span + off;
                    cube_levels[level].row_bytes = bw * block;
                    cube_levels[level].rows = bh;
                    level++;
                    off += bw * block * bh;
                    mw = mw > 1 ? mw >> 1 : 1;
                    mh = mh > 1 ? mh >> 1 : 1;
                }
            }
            return create_texture_cube(dxgi, cube_levels, n_mips);
        }
        tex_level_t levels[14];
        u32 mw = w, mh = h, off = 0, n = 0;
        for (u32 m = 0; m < n_mips; m++) {
            const u32 bw = (mw + 3) / 4, bh = (mh + 3) / 4;
            levels[n].w = (mw + 3) & ~3u;
            levels[n].h = (mh + 3) & ~3u;
            levels[n].data = src + off;
            levels[n].row_bytes = bw * block;
            levels[n].rows = bh;
            n++;
            off += bw * block * bh;
            if (mw == 1 && mh == 1) break;
            mw = mw > 1 ? mw >> 1 : 1;
            mh = mh > 1 ? mh >> 1 : 1;
        }
        return create_texture_mipped(dxgi, levels, n);
    }

    u32 texel_size;
    switch (base_fmt) {
    case TEX_FMT_B8: texel_size = 1; break;
    case TEX_FMT_A4R4G4B4:
    case TEX_FMT_A1R5G5B5:
    case TEX_FMT_R5G6B5:
    case TEX_FMT_G8B8:
    case TEX_FMT_X16: texel_size = 2; break;
    case TEX_FMT_A8R8G8B8:
    case TEX_FMT_DEPTH24_D8: texel_size = 4; break;
    case RSX_TEX_FMT_W16Z16Y16X16_FLOAT: texel_size = 8; break;
    case RSX_TEX_FMT_Y16X16_FLOAT: texel_size = 4; break;
    default: return NULL;
    }
    if (!linear && ((w & (w - 1)) || (h & (h - 1)))) { ld_texfail_why(t, "npot swizzled"); return NULL; }
    const u32 span = texture_source_span(t);
    const u8* src = guest_ptr(t->location, t->offset, span);
    if (!src) { ld_texfail_why(t, span ? "guest_ptr" : "span 0"); return NULL; }

    if (tex_depth3d(t)) {
        /* GOWA: volume texture. Linear slices are pitch*h apart; a swizzled
         * volume interleaves the x, y and z bits (RPCS3
         * convert_linear_swizzle_3d). */
        const int f16v = is_f16_format(base_fmt);
        const u32 obpp = f16v ? 8u : 4u;
        u8* vox[14] = {0};
        tex_level_t vl[14];
        u32 vd[14];
        u32 mw = w, mh = h, md = tex_depth3d(t), voff = 0, vn = 0;
        int voom = 0;
        for (u32 m = 0; m < n_mips; m++) {
            const u32 pitch = (m == 0 && linear && t->pitch) ? t->pitch : mw * texel_size;
            vox[vn] = (u8*)malloc((size_t)mw * mh * md * obpp);
            if (!vox[vn]) { voom = 1; break; }
            const u32 lw = log2_u32(mw), lh = log2_u32(mh), ld = log2_u32(md);
            for (u32 z = 0; z < md; z++)
                for (u32 y = 0; y < mh; y++)
                    for (u32 x = 0; x < mw; x++) {
                        const u8* px = linear
                            ? src + voff + ((size_t)z * mh + y) * pitch + (size_t)x * texel_size
                            : src + voff + (size_t)morton3_index(x, y, z, lw, lh, ld) * texel_size;
                        u8* dst = vox[vn] + (((size_t)z * mh + y) * mw + x) * obpp;
                        if (f16v) decode_texel_f16(base_fmt, px, remap, (u16*)dst);
                        else decode_texel(base_fmt, px, remap, dst);
                    }
            vl[vn].w = mw; vl[vn].h = mh; vl[vn].data = vox[vn];
            vl[vn].row_bytes = mw * obpp; vl[vn].rows = mh;
            vd[vn] = md;
            vn++;
            voff += pitch * mh * md;
            if (mw == 1 && mh == 1 && md == 1) break;
            mw = mw > 1 ? mw >> 1 : 1;
            mh = mh > 1 ? mh >> 1 : 1;
            md = md > 1 ? md >> 1 : 1;
        }
        ID3D12Resource* vres = (!voom && vn)
            ? create_texture_3d(f16v ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM,
                                vl, vd, vn)
            : NULL;
        for (u32 m = 0; m < vn; m++) free(vox[m]);
        if (!vres) ld_texfail_why(t, voom ? "volume oom" : "volume create");
        return vres;
    }

    if (t->cubemap) {
        u32 available_mips = 1;
        for (u32 mw = w, mh = h; mw > 1 || mh > 1; available_mips++) {
            mw = mw > 1 ? mw >> 1 : 1;
            mh = mh > 1 ? mh >> 1 : 1;
        }
        if (n_mips > available_mips) n_mips = available_mips;
        const u32 face_span = span / 6;
        u8* rgba[6 * 14] = {0};
        tex_level_t cube_levels[6 * 14];
        u32 level = 0;
        int oom = 0;
        for (u32 face = 0; face < 6 && !oom; face++) {
            u32 mw = w, mh = h, off = 0;
            for (u32 mip = 0; mip < n_mips; mip++) {
                const u32 pitch = (mip == 0 && linear && t->pitch)
                    ? t->pitch : mw * texel_size;
                rgba[level] = (u8*)malloc((size_t)mw * mh * 4);
                if (!rgba[level]) {
                    oom = 1;
                    break;
                }
                const u32 lw = log2_u32(mw), lh = log2_u32(mh);
                const u8* level_src =
                    src + (size_t)face * face_span + off;
                for (u32 y = 0; y < mh; y++)
                    for (u32 x = 0; x < mw; x++) {
                        const u8* pixel = linear
                            ? level_src + (size_t)y * pitch +
                                (size_t)x * texel_size
                            : level_src +
                                (size_t)morton_index(x, y, lw, lh) *
                                    texel_size;
                        decode_texel(
                            base_fmt, pixel, remap,
                            rgba[level] + ((size_t)y * mw + x) * 4);
                    }
                cube_levels[level].w = mw;
                cube_levels[level].h = mh;
                cube_levels[level].data = rgba[level];
                cube_levels[level].row_bytes = mw * 4;
                cube_levels[level].rows = mh;
                level++;
                off += pitch * mh;
                if (mw == 1 && mh == 1) break;
                mw = mw > 1 ? mw >> 1 : 1;
                mh = mh > 1 ? mh >> 1 : 1;
            }
        }
        ID3D12Resource* resource = (!oom && level == 6 * n_mips)
            ? create_texture_cube(
                DXGI_FORMAT_R8G8B8A8_UNORM, cube_levels, n_mips)
            : NULL;
        for (u32 i = 0; i < level; i++) free(rgba[i]);
        return resource;
    }

    u8* rgba[14] = {0};
    tex_level_t levels[14];
    u32 mw = w, mh = h, off = 0, n = 0;
    int oom = 0;
    const int f16 = is_f16_format(base_fmt);
    const u32 out_bpp = f16 ? 8u : 4u;
    for (u32 m = 0; m < n_mips; m++) {
        const u32 pitch = (m == 0 && linear && t->pitch)
            ? t->pitch : mw * texel_size;
        rgba[n] = (u8*)malloc((size_t)mw * mh * out_bpp);
        if (!rgba[n]) { oom = 1; break; }
        const u32 lw = log2_u32(mw), lh = log2_u32(mh);
        const u8* level_src = src + off;
        /* LD_PLANE_DUMP=1: write the first few big single-channel (B8) planes
         * straight out as PGM, exactly as the guest laid them down. Settles
         * "is the video decode producing an image?" without guessing from the
         * composited frame. ponytail: one-shot debug, costs nothing when off. */
        { static int pd = -1; static int pdn = 0;
          if (pd < 0) { const char* e = getenv("LD_PLANE_DUMP"); pd = e ? atoi(e) : 0; }
          if (pd && (int)g_ld_frames >= pd && m == 0 && base_fmt == TEX_FMT_B8 && mw >= 320 && pdn < 6) {
              char fn[128];
              snprintf(fn, sizeof fn, "scratch/plane_%02d_%ux%u_p%u.pgm", pdn, mw, mh, pitch);
              FILE* pf = fopen(fn, "wb");
              if (pf) {
                  fprintf(pf, "P5\n%u %u\n255\n", mw, mh);
                  for (u32 yy = 0; yy < mh; yy++)
                      fwrite(level_src + (size_t)yy * pitch, 1, mw, pf);
                  fclose(pf);
                  fprintf(stderr, "[plane-dump] %s linear=%d\n", fn, linear);
              }
              pdn++;
          } }
        /* GOWA: A8R8G8B8 whose remap only reorders channels or fills them with
         * 0/1 -- one byte shuffle per 8 pixels (linear) or a table-driven
         * morton gather (swizzled) instead of decode_texel per texel. The
         * 1280x720 frame God of War Ascension hands to its SPU MLAA comes back
         * as such a texture and changes every frame: ~900k decode_texel calls
         * per frame on the RSX thread. LD_FAST_ARGB8=0 disables. */
        static int s_fast_argb8 = -1;
        if (s_fast_argb8 < 0) { const char* e = getenv("LD_FAST_ARGB8"); s_fast_argb8 = e ? atoi(e) : 1; }
        u8 shuf[16], orv[16];
        int fast = !f16 && s_fast_argb8 && base_fmt == TEX_FMT_A8R8G8B8;
        if (fast) {
            static const u32 out2comp[4] = { 1, 2, 3, 0 };
            for (u32 o = 0; o < 4; o++) {
                const u32 comp = out2comp[o];
                const u32 op = (remap >> (8 + comp * 2)) & 3, sel = (remap >> (comp * 2)) & 3;
                for (u32 k = 0; k < 4; k++) {
                    shuf[k * 4 + o] = op >= 2 ? (u8)(k * 4 + sel) : 0x80;
                    orv[k * 4 + o] = op == 1 ? 0xFF : 0x00;
                }
            }
        }
        if (fast && linear) {
            const __m128i c4 = _mm_loadu_si128((const __m128i*)shuf), o4 = _mm_loadu_si128((const __m128i*)orv);
#if defined(__AVX2__)
            const __m256i c8 = _mm256_broadcastsi128_si256(c4), o8 = _mm256_broadcastsi128_si256(o4);
#endif
            for (u32 y = 0; y < mh; y++) {
                const u8* s = level_src + (size_t)y * pitch;
                u8* d = rgba[n] + (size_t)y * mw * 4;
                u32 x = 0;
#if defined(__AVX2__)
                for (; x + 8 <= mw; x += 8)
                    _mm256_storeu_si256((__m256i*)(d + x * 4), _mm256_or_si256(
                        _mm256_shuffle_epi8(_mm256_loadu_si256((const __m256i*)(s + x * 4)), c8), o8));
#endif
                for (; x + 4 <= mw; x += 4)
                    _mm_storeu_si128((__m128i*)(d + x * 4), _mm_or_si128(
                        _mm_shuffle_epi8(_mm_loadu_si128((const __m128i*)(s + x * 4)), c4), o4));
                for (; x < mw; x++) decode_texel(base_fmt, s + x * 4, remap, d + x * 4);
            }
        } else if (fast) {
            /* morton(x, y) = morton(x, 0) | morton(0, y): the two coordinates'
             * bits never overlap, so two small tables replace the bit loop. */
            u32* mx = (u32*)malloc(sizeof(u32) * (mw + mh));
            if (mx) {
                u32* my = mx + mw;
                for (u32 x = 0; x < mw; x++) mx[x] = morton_index(x, 0, lw, lh);
                for (u32 y = 0; y < mh; y++) my[y] = morton_index(0, y, lw, lh);
                for (u32 y = 0; y < mh; y++) {
                    u8* d = rgba[n] + (size_t)y * mw * 4;
                    for (u32 x = 0; x < mw; x++) {
                        const u8* p = level_src + (size_t)(mx[x] | my[y]) * 4;
                        for (u32 o = 0; o < 4; o++)
                            d[x * 4 + o] = (u8)((shuf[o] & 0x80 ? 0 : p[shuf[o]]) | orv[o]);
                    }
                }
                free(mx);
            } else fast = 0;
        }
        /* GOWA (23/10): the one-channel formats (B8, X16, DEPTH24_D8: the byte
         * p[0] as A=255, R=G=B=p[0], then the remap). Per output channel the
         * remap either keeps that byte or forces 0/255, so a pixel is
         * (v * 0x01010101 & M) | K, swizzled through the two morton tables as
         * above. God of War Ascension's phase-1 walk streams 1024x1024 B8
         * textures with 10 mips through decode_texel at 16-18 ms each (up to
         * 2.7 s of the RSX thread per 10 s, the hitches of that part).
         * LD_FAST_B8=0 disables; LD_FAST_B8_CHECK=1 compares every level with
         * the generic loop and counts the differences. */
        static int s_fast_b8 = -1, s_b8_check = -1;
        if (s_fast_b8 < 0) { const char* e = getenv("LD_FAST_B8"); s_fast_b8 = e ? atoi(e) : 1;
                             s_b8_check = getenv("LD_FAST_B8_CHECK") ? 1 : 0; }
        if (!fast && !f16 && s_fast_b8 &&
            (base_fmt == TEX_FMT_B8 || base_fmt == TEX_FMT_X16 || base_fmt == TEX_FMT_DEPTH24_D8)) {
            u32 M = 0, K = 0;
            static const u32 b8_out2comp[4] = { 1, 2, 3, 0 };
            for (u32 o = 0; o < 4; o++) {
                const u32 comp = b8_out2comp[o];
                const u32 op = (remap >> (8 + comp * 2)) & 3, sel = (remap >> (comp * 2)) & 3;
                if (op == 1 || (op >= 2 && sel == 0)) K |= 0xFFu << (o * 8);
                else if (op >= 2) M |= 0xFFu << (o * 8);
            }
            u32* mx = linear ? NULL : (u32*)malloc(sizeof(u32) * (mw + mh));
            if (linear || mx) {
                if (mx) {
                    for (u32 x = 0; x < mw; x++) mx[x] = morton_index(x, 0, lw, lh) * texel_size;
                    for (u32 y = 0; y < mh; y++) mx[mw + y] = morton_index(0, y, lw, lh) * texel_size;
                }
                for (u32 y = 0; y < mh; y++) {
                    u32* d = (u32*)(rgba[n] + (size_t)y * mw * 4);
                    if (linear) {
                        const u8* s = level_src + (size_t)y * pitch;
                        for (u32 x = 0; x < mw; x++) d[x] = ((u32)s[(size_t)x * texel_size] * 0x01010101u & M) | K;
                    } else {
                        const u32 yy = mx[mw + y];
                        for (u32 x = 0; x < mw; x++) d[x] = ((u32)level_src[mx[x] | yy] * 0x01010101u & M) | K;
                    }
                }
                free(mx);
                fast = 1;
                if (s_b8_check) {
                    static volatile long s_b8_n, s_b8_bad;
                    u8 ref[4];
                    long bad = 0;
                    for (u32 y = 0; y < mh; y++)
                        for (u32 x = 0; x < mw; x++) {
                            const u8* pixel = linear ? level_src + (size_t)y * pitch + (size_t)x * texel_size
                                                     : level_src + (size_t)morton_index(x, y, lw, lh) * texel_size;
                            decode_texel(base_fmt, pixel, remap, ref);
                            if (memcmp(ref, rgba[n] + ((size_t)y * mw + x) * 4, 4)) bad++;
                        }
                    const long nn = InterlockedIncrement(&s_b8_n);
                    if (bad) InterlockedExchangeAdd(&s_b8_bad, bad);
                    if (bad || (nn & 255) == 0)
                        fprintf(stderr, "[b8-check] %ld niveis, %ld texels diferentes (este: %ux%u fmt=0x%02X remap=0x%X %ld)\n",
                                nn, s_b8_bad, mw, mh, base_fmt, remap, bad);
                }
            }
        }
        if (!fast)
        for (u32 y = 0; y < mh; y++)
            for (u32 x = 0; x < mw; x++) {
                const u8* pixel = linear
                    ? level_src + (size_t)y * pitch + (size_t)x * texel_size
                    : level_src + (size_t)morton_index(x, y, lw, lh) * texel_size;
                if (f16)
                    decode_texel_f16(base_fmt, pixel, remap,
                                     (u16*)(rgba[n] + ((size_t)y * mw + x) * 8));
                else
                    decode_texel(base_fmt, pixel, remap,
                                 rgba[n] + ((size_t)y * mw + x) * 4);
            }
        /* LD_TEXRGBA_DUMP=1: the DECODED level-0 RGBA, with a non-black count
         * and the remap that produced it.
         *
         * Forcing the fragment shader to green fills every surface completely
         * (368,640 = 720x512 and 921,600 = 1280x720), so the draws reach the
         * GPU and write every pixel -- the only thing left is that the sampled
         * texture comes back black. The guest MEMORY has content (PS1 VRAM
         * carries the BIOS boot screen), so the question is whether decode
         * preserves it. remap is applied here, and a remap that zeroes channels
         * would produce exactly this. */
        { static int td = -1; static int tdn = 0; static u32 tdf = 0;
          /* LD_TEXRGBA_DUMP=<hex fmt>: only that base format -- the 12-dump
           * cap otherwise fills with whatever decodes first. */
          if (td < 0) { const char* e = getenv("LD_TEXRGBA_DUMP");
                        td = e ? 1 : 0; tdf = e ? (u32)strtoul(e, 0, 16) : 0; }
          const int td_this = td && !(tdf > 1 && base_fmt != tdf);
          /* No size floor: the floor was picked for one title's full-screen
           * textures and silently skipped every UI atlas, which is the size
           * a 2D layer is made of. The dump is opt-in; dump what was asked. */
          if (td_this && !f16 && m == 0 && tdn < 12 && mw && mh) {
              u64 nb = 0, na = 0;
              for (u32 q = 0; q < mw * mh; q++) {
                  const u8* px = rgba[n] + (size_t)q * 4;
                  if (px[0] || px[1] || px[2]) nb++;
                  if (px[3] > 0x1A) na++;
              }
              char fn[160];
              snprintf(fn, sizeof fn, "scratch/tex_%02d_%ux%u_f%02X.ppm",
                       tdn, mw, mh, base_fmt);
              FILE* tf = fopen(fn, "wb");
              if (tf) {
                  fprintf(tf, "P6\n%u %u\n255\n", mw, mh);
                  for (u32 q = 0; q < mw * mh; q++)
                      fwrite(rgba[n] + (size_t)q * 4, 1, 3, tf);
                  fclose(tf);
              }
              fprintf(stderr, "[tex-rgba] %s fmt=0x%02X remap=0x%04X linear=%d"
                              " pitch=%u nonblack=%llu/%u alpha>0x1A=%llu\n",
                      fn, base_fmt, remap, linear, pitch,
                      (unsigned long long)nb, mw * mh, (unsigned long long)na);
              tdn++;
          } }
        levels[n].w = mw;
        levels[n].h = mh;
        levels[n].data = rgba[n];
        levels[n].row_bytes = mw * out_bpp;
        levels[n].rows = mh;
        n++;
        off += pitch * mh;
        if (mw == 1 && mh == 1) break;
        mw = mw > 1 ? mw >> 1 : 1;
        mh = mh > 1 ? mh >> 1 : 1;
    }
    ID3D12Resource* resource = (!oom && n)
        ? create_texture_mipped(f16 ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM, levels, n) : NULL;
    if (!resource) ld_texfail_why(t, oom ? "oom" : n ? "create_texture_mipped" : "no level");
    for (u32 m = 0; m < n; m++) free(rgba[m]);
    return resource;
}

static ID3D12Resource* decode_guest_texture_profiled(
    const rsx_dsp_texture* texture, u32 remap)
{
#if defined(YZ_PERF_PROFILE)
    const LONGLONG begin = ld_profile_qpc();
    g_ld_profile.total.texture_decode_calls++;
#endif
    /* GOWA_TEX_LOG=1 (23/10): decodes slower than 2 ms, with format and size,
     * and the count/total per 10 s -- the phase-1 hitches with the PSOs gone */
    static int s_texlog = -1;
    if (s_texlog < 0) s_texlog = getenv("GOWA_TEX_LOG") ? 1 : 0;
    LARGE_INTEGER tl0; if (s_texlog) QueryPerformanceCounter(&tl0);
    ID3D12Resource* result = decode_guest_texture(texture, remap);
    if (s_texlog) {
        LARGE_INTEGER tl1, tf; QueryPerformanceCounter(&tl1); QueryPerformanceFrequency(&tf);
        const double ms = (double)(tl1.QuadPart - tl0.QuadPart) * 1000.0 / (double)tf.QuadPart;
        static double s_tot; static long s_n; static LONGLONG s_t0;
        s_tot += ms; s_n++;
        if (!s_t0) s_t0 = tl1.QuadPart;
        if (ms > 2.0)
            fprintf(stderr, "[tex-lenta] %.1f ms fmt=0x%02X %ux%u mips=%u linear=%d cubo=%d\n", ms,
                    texture->format, texture->width, texture->height, texture->mipmaps,
                    (texture->format & TEX_FMT_LINEAR) != 0, texture->cubemap);
        if (tl1.QuadPart - s_t0 > 10 * tf.QuadPart) {
            fprintf(stderr, "[tex] %ld decodificacoes, %.0f ms em 10 s\n", s_n, s_tot);
            s_tot = 0; s_n = 0; s_t0 = tl1.QuadPart;
        }
    }
#if defined(YZ_PERF_PROFILE)
    g_ld_profile.total.texture_decode_qpc +=
        (u64)(ld_profile_qpc() - begin);
#endif
    return result;
}

static void write_texture_srv(u32 index, const texcache_t* entry)
{
    const u32 base_fmt = entry->format & TEX_FMT_BASE_MASK & ~(u32)TEX_FMT_UNNORM;
    const int compressed = base_fmt == TEX_FMT_DXT1 ||
                           base_fmt == TEX_FMT_DXT23 ||
                           base_fmt == TEX_FMT_DXT45;
    if (entry->cubemap) {
        static const u32 sel2d3d[4] = { 3, 0, 1, 2 };
        static const u32 out2comp[4] = { 1, 2, 3, 0 };
        u32 mapping[4];
        for (u32 out = 0; out < 4; out++) {
            const u32 comp = out2comp[out];
            const u32 op = (entry->remap >> (8 + comp * 2)) & 3;
            const u32 sel = (entry->remap >> (comp * 2)) & 3;
            mapping[out] = op == 0 ? 4 : op == 1 ? 5 : sel2d3d[sel];
        }
        D3D12_SHADER_RESOURCE_VIEW_DESC desc = {0};
        desc.Format = !compressed ? DXGI_FORMAT_R8G8B8A8_UNORM
                    : base_fmt == TEX_FMT_DXT1 ? DXGI_FORMAT_BC1_UNORM
                    : base_fmt == TEX_FMT_DXT23 ? DXGI_FORMAT_BC2_UNORM
                                                : DXGI_FORMAT_BC3_UNORM;
        desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        desc.Shader4ComponentMapping =
            !compressed || entry->remap == 0xAAE4
            ? D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING
            : mapping[0] | (mapping[1] << 3) | (mapping[2] << 6) |
              (mapping[3] << 9) | (1u << 12);
        desc.TextureCube.MipLevels = (UINT)-1;
        g.dev->lpVtbl->CreateShaderResourceView(
            g.dev, entry->tex, &desc, srv_dst(SRV_TEXTURE_BASE + index));
    } else if (compressed && entry->remap != 0xAAE4) {
        static const u32 sel2d3d[4] = { 3, 0, 1, 2 };
        static const u32 out2comp[4] = { 1, 2, 3, 0 };
        u32 mapping[4];
        for (u32 out = 0; out < 4; out++) {
            const u32 comp = out2comp[out];
            const u32 op = (entry->remap >> (8 + comp * 2)) & 3;
            const u32 sel = (entry->remap >> (comp * 2)) & 3;
            mapping[out] = op == 0 ? 4 : op == 1 ? 5 : sel2d3d[sel];
        }
        D3D12_SHADER_RESOURCE_VIEW_DESC desc = {0};
        desc.Format = base_fmt == TEX_FMT_DXT1 ? DXGI_FORMAT_BC1_UNORM
                    : base_fmt == TEX_FMT_DXT23 ? DXGI_FORMAT_BC2_UNORM
                                                : DXGI_FORMAT_BC3_UNORM;
        desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        desc.Shader4ComponentMapping = mapping[0] | (mapping[1] << 3) |
                                       (mapping[2] << 6) | (mapping[3] << 9) |
                                       (1u << 12);
        desc.Texture2D.MipLevels = (UINT)-1;
        g.dev->lpVtbl->CreateShaderResourceView(
            g.dev, entry->tex, &desc, srv_dst(SRV_TEXTURE_BASE + index));
    } else {
        srv_write(SRV_TEXTURE_BASE + index, entry->tex);
    }
}

/* Decode a guest texture descriptor into a cached SRV slot.  Unlike the old
 * descriptor-only cache, re-hash the source once per frame and refresh the
 * D3D resource when the game rewrites the same guest address. */
/* GOWA: open-addressing index over g.textures, keyed like the match test
 * in texture_srv_slot. A slot whose entry no longer carries the key (the
 * entry was replaced by an LRU eviction) is skipped like a tombstone; the
 * index is rebuilt from g.textures when such slots could have piled up. */
#define TEX_INDEX_SIZE 65536u
static int32_t s_tex_index[TEX_INDEX_SIZE];
static u32 s_tex_index_puts;
static int s_tex_index_ready;
static u32 tex_key_slot(u32 location, u32 offset, u32 format, u32 width,
                        u32 height, u32 pitch, u32 remap, u32 cubemap)
{
    u64 k = ((u64)offset << 32) ^ ((u64)format << 24) ^ ((u64)width << 12) ^ height;
    k ^= ((u64)location << 63) ^ ((u64)cubemap << 62) ^ ((u64)remap << 40) ^ ((u64)pitch << 6);
    k ^= k >> 33; k *= 0xff51afd7ed558ccdull; k ^= k >> 33; k *= 0xc4ceb9fe1a85ec53ull; k ^= k >> 33;
    return (u32)k & (TEX_INDEX_SIZE - 1u);
}
static void tex_index_put(u32 i)
{
    const texcache_t* e = &g.textures[i];
    u32 slot = tex_key_slot(e->location, e->offset, e->format, e->width,
                            e->height, e->pitch, e->remap, e->cubemap);
    for (u32 probe = 0; probe < TEX_INDEX_SIZE; probe++, slot = (slot + 1u) & (TEX_INDEX_SIZE - 1u)) {
        const int32_t cur = s_tex_index[slot];
        if (cur < 0 || (u32)cur == i) { s_tex_index[slot] = (int32_t)i; break; }
    }
    s_tex_index_puts++;
}
static void tex_index_rebuild(void)
{
    memset(s_tex_index, 0xFF, sizeof s_tex_index);
    s_tex_index_puts = 0;
    for (u32 i = 0; i < g.n_textures; i++) if (g.textures[i].width) tex_index_put(i);
    s_tex_index_ready = 1;
}
/* GOWA_TEX_DUMP_SEC=<s>[,max]: from <s> seconds after the first texture
 * lookup, the raw guest bytes of each distinct texture bound (up to max,
 * default 400) go to out\texdump\<loc>_<offset>_<fmt>_<w>x<h>_r<remap>.bin --
 * to look at a material's textures offline (scripts\dxt_png.py, base = the
 * offset). One file per texture, written once. */
static void ld_tex_dump(const rsx_dsp_texture* t)
{
    static double s = -2.0; static int maxn = 400, n; static ULONGLONG t0;
    if (s == -2.0) {
        const char* e = getenv("GOWA_TEX_DUMP_SEC"); s = e ? atof(e) : -1.0;
        const char* c = e ? strchr(e, ',') : NULL; if (c) maxn = atoi(c + 1);
        t0 = GetTickCount64();
        if (s >= 0.0) CreateDirectoryA("out\\texdump", NULL);
    }
    if (s < 0.0 || n >= maxn || (double)(GetTickCount64() - t0) < s * 1000.0) return;
    static u32 seen[1024]; static int ns;
    const u32 key = t->offset ^ (t->location << 31) ^ (t->format << 24);
    for (int i = 0; i < ns; i++) if (seen[i] == key) return;
    if (ns < 1024) seen[ns++] = key;
    const u32 span = texture_source_span(t);
    const u8* src = span ? guest_ptr(t->location, t->offset, span) : NULL;
    if (!src) return;
    char fn[MAX_PATH];
    snprintf(fn, sizeof fn, "out\\texdump\\%u_%08X_%02X_%ux%u_r%04X.bin",
             t->location, t->offset, t->format, t->width, t->height, t->remap & 0xFFFFu);
    FILE* f = fopen(fn, "wb");
    if (!f) return;
    fwrite(src, 1, span, f); fclose(f); n++;
    /* GOWA_TEX_DUMP_AROUND=<bytes>: for a texture of 64x64 or less (a button
     * glyph), also the memory around it, to find the glyphs loaded beside it
     * (report 37: scripts/teclas.py got the glyph hashes from these) */
    static long around = -2;
    if (around == -2) { const char* a = getenv("GOWA_TEX_DUMP_AROUND"); around = a ? atol(a) : -1; }
    if (around > 0 && t->width <= 64 && t->height <= 64 && t->offset >= (u32)around) {
        const u8* wide = guest_ptr(t->location, t->offset - (u32)around, 2u * (u32)around);
        if (wide && (f = fopen(strcat(fn, ".viz"), "wb"))) { fwrite(wide, 1, 2u * (u32)around, f); fclose(f); }
    }
}

/* GOWA (28/10): textures kept before the LRU starts evicting. The cache fills in the first
 * minutes of every session (the game streams textures to ever new addresses); in the
 * reports from 12 GB cards it then evicted ~5 a frame and re-decoded what came back.
 * 16384 with 8 GB of VRAM or more (henry: same fps as 8192), 8192 below that (the 4 GB
 * cards). GOWA_TEX_CAP=<n> picks it (256..MAX_TEXTURES). */
extern unsigned g_ld_gpu_vram_mb;
/* GOWA (30/10, reports 434/450: 12 GB cheios, travadinhas e o jogo fechando): o limite também segue a VRAM. Uma vez
 * a cada 60 quadros, o uso da memória de vídeo do processo contra o orçamento que o Windows dá a ele
 * (IDXGIAdapter3::QueryVideoMemoryInfo): acima de 90%, o limite cai para 7/8 das texturas carregadas e o LRU
 * descarta as mais antigas; abaixo de 75%, volta a subir 1/8 por vez até o normal. GOWA_TEX_VRAM=0 desliga. */
static u32 s_tex_cap_vram;   /* 0 = sem pressão */
static u32 s_tex_livres[MAX_TEXTURES], s_n_tex_livres;   /* vagas do cache sem textura (ld_tex_encolher) */
static void ld_tex_vram_olha(u32 cap)
{
    static int on = -1; static IDXGIAdapter3* ad; static u32 ult;
    if (on < 0) {
        const char* e = getenv("GOWA_TEX_VRAM"); on = !(e && *e == '0');
        if (on && g.dev) {
            IDXGIFactory4* f = NULL;
            if (SUCCEEDED(CreateDXGIFactory1(&IID_IDXGIFactory4, (void**)&f)) && f) {
                LUID luid = {0}; g.dev->lpVtbl->GetAdapterLuid(g.dev, &luid);
                f->lpVtbl->EnumAdapterByLuid(f, luid, &IID_IDXGIAdapter3, (void**)&ad);
                f->lpVtbl->Release(f);
            }
        }
        if (!ad) on = 0;
    }
    if (!on || g_ld_frames - ult < 60u) return;
    ult = g_ld_frames;
    DXGI_QUERY_VIDEO_MEMORY_INFO mi;
    if (FAILED(ad->lpVtbl->QueryVideoMemoryInfo(ad, 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &mi)) || !mi.Budget) return;
    { static long teste = -1;   /* GOWA_TEX_VRAM_ORCAMENTO_MB=<MB>: simula um orçamento menor (teste) */
      if (teste < 0) { const char* e = getenv("GOWA_TEX_VRAM_ORCAMENTO_MB"); teste = e ? atol(e) : 0; }
      if (teste > 0) mi.Budget = (UINT64)teste << 20; }
    const double uso = (double)mi.CurrentUsage / (double)mi.Budget;
    const u32 vivas = g.n_textures - s_n_tex_livres;   /* as vagas livres (ld_tex_encolher) não ocupam VRAM */
    if (uso > 0.90 && vivas > 1024u) {
        const u32 novo = vivas - vivas / 8u;
        if (!s_tex_cap_vram || novo < s_tex_cap_vram) {
            s_tex_cap_vram = novo < 1024u ? 1024u : novo;
            static int ditos;
            if (ditos++ < 20)
                fprintf(stderr, "[texture-cache] VRAM em %llu de %llu MB (%.0f%%): limite de texturas cai para %u\n",
                        (unsigned long long)(mi.CurrentUsage >> 20), (unsigned long long)(mi.Budget >> 20), uso * 100.0,
                        s_tex_cap_vram);
        }
    } else if (uso < 0.75 && s_tex_cap_vram) {
        s_tex_cap_vram += s_tex_cap_vram / 8u;
        if (s_tex_cap_vram >= cap) s_tex_cap_vram = 0;
    }
}
/* GOWA (30/10, reports 469/480/491): com o limite baixado pela VRAM as texturas carregadas continuavam as mesmas
 * (o LRU só troca uma pela outra quando o cache está cheio): 16384 carregadas contra o limite 14336 por toda a
 * sessão, e a VRAM não aliviava. Agora as mais antigas acima do limite são descartadas (pela fila do fence, como no
 * LRU) e a vaga fica livre (width 0, nunca casa numa busca) para a próxima textura. */
static u32 ld_tex_vivas(void) { return g.n_textures - s_n_tex_livres; }
static int ld_tex_cmp_serial(const void* a, const void* b)
{
    const u64 x = g.textures[*(const u32*)a].last_use_serial, y = g.textures[*(const u32*)b].last_use_serial;
    return x < y ? -1 : x > y;
}
static void ld_tex_encolher(u32 limite, u32 max_por_vez)
{
    const u32 vivas = ld_tex_vivas();
    if (vivas <= limite) return;
    static u32 ordem[MAX_TEXTURES];
    u32 n = 0;
    for (u32 i = 0; i < g.n_textures; i++) if (g.textures[i].width) ordem[n++] = i;
    qsort(ordem, n, sizeof ordem[0], ld_tex_cmp_serial);
    u32 tirar = vivas - limite;
    if (tirar > max_por_vez) tirar = max_por_vez;
    for (u32 k = 0; k < tirar; k++) {
        texcache_t* e = &g.textures[ordem[k]];
        if (e->tex) retire_texture(e->tex);
        memset(e, 0, sizeof *e);
        e->last_use_serial = ~0ull;   /* o LRU de cache cheio não escolhe uma vaga livre */
        s_tex_livres[s_n_tex_livres++] = ordem[k];
    }
    g_ld_texture_cache_evictions += tirar;
    tex_index_rebuild();
    static int ditos;
    if (ditos++ < 20)
        fprintf(stderr, "[texture-cache] VRAM: %u texturas descartadas (agora %u, limite %u)\n", tirar, ld_tex_vivas(), limite);
}
static u32 ld_tex_cap(void)
{
    static u32 cap;
    if (!cap) {
        const char* e = getenv("GOWA_TEX_CAP");
        cap = e ? (u32)atoi(e) : g_ld_gpu_vram_mb >= 7000u ? 16384u : 8192u;
        if (cap < 256u) cap = 256u;
        if (cap > MAX_TEXTURES) cap = MAX_TEXTURES;
        fprintf(stderr, "[texture-cache] até %u texturas (%u MB de VRAM)\n", cap, g_ld_gpu_vram_mb);
    }
    ld_tex_vram_olha(cap);
    return s_tex_cap_vram && s_tex_cap_vram < cap ? s_tex_cap_vram : cap;
}

static u32 texture_srv_slot(const rsx_dsp_texture* t)
{
    /* report 37: keyboard and mouse came in or went away; the 64x64 textures (the button glyphs among them) are
     * hashed again, which skips the untouched-pages shortcut below and decodes the glyphs for the new device */
    { static int modo = -1; const int m = cellPad_teclado_ultimo();
      if (m != modo) { modo = m;
          for (u32 k = 0; k < g.n_textures; k++)
              if (g.textures[k].width == 64 && g.textures[k].height == 64) g.textures[k].content_hash = 0; } }
    const u32 remap = t->remap & 0xFFFF;
    ld_tex_dump(t);
    static int refresh_enabled = -1;
    if (refresh_enabled < 0)
        refresh_enabled = getenv("RSX_NO_TEX_REFRESH") ? 0 : 1;

    if (!s_tex_index_ready || s_tex_index_puts > TEX_INDEX_SIZE / 2u)
        tex_index_rebuild();
    for (u32 slot = tex_key_slot(t->location, t->offset, t->format, t->width, t->height,
                                 t->pitch, remap, t->cubemap), probe = 0;
         probe < TEX_INDEX_SIZE; probe++, slot = (slot + 1u) & (TEX_INDEX_SIZE - 1u)) {
        if (s_tex_index[slot] < 0) break;
        const u32 i = (u32)s_tex_index[slot];
        if (i >= g.n_textures) continue;
        texcache_t* entry = &g.textures[i];
        if (entry->location != t->location || entry->offset != t->offset ||
            entry->format != t->format || entry->width != t->width ||
            entry->height != t->height || entry->pitch != t->pitch ||
            entry->remap != remap || entry->cubemap != t->cubemap ||
            entry->depth3d != tex_depth3d(t))
            continue;
        if (refresh_enabled && entry->tex &&
            entry->last_hash_frame != g_ld_frames) {
            /* GOWA_TEX_WW: nenhuma página da textura escrita desde o último hash -> o mesmo
             * conteúdo; pula o hash (a contagem sai a cada 512 quadros) */
            { const u32 span = texture_source_span(t);
              const u8* src = span ? guest_ptr(t->location, t->offset, span) : NULL;
              static u32 f0 = 0xFFFFFFFFu, nf;
              if (f0 != g_ld_frames) { f0 = g_ld_frames;
                  if (++nf % 512 == 0 && s_ld_ww_n[0]) {
                      fprintf(stderr, "[texww] 512 quadros: %llu consultas, %llu intactas (hash pulado), %llu com escrita\n",
                              (unsigned long long)s_ld_ww_n[0], (unsigned long long)s_ld_ww_n[1], (unsigned long long)s_ld_ww_n[2]);
                      memset(s_ld_ww_n, 0, sizeof s_ld_ww_n); } }
              if (src && entry->content_hash && !ld_tex_paginas_sujas(src, span, entry->last_hash_frame)) {
                  entry->last_hash_frame = g_ld_frames;
                  goto ld_tex_intacta;
              } }
            int readable = 0;
            const u64 hash = texture_content_hash(t, &readable);
            /* LD_TEX_RACE=1: hash the SAME source twice, back to back. The two
             * reads are microseconds apart, so they can only disagree if a
             * guest thread is writing this texture right now -- i.e. we are
             * decoding a half-written image. Pure observation; it changes
             * nothing. Counts per source so one torn texture is attributable. */
            { static int race = -1;
              if (race < 0) race = getenv("LD_TEX_RACE") ? 1 : 0;
              if (race && readable) {
                  int r2 = 0;
                  const u64 again = texture_content_hash(t, &r2);
                  static unsigned long long checks = 0, torn = 0;
                  checks++;
                  if (r2 && again != hash) torn++;
                  if ((checks % 256) == 0)
                      fprintf(stderr,
                              "[tex-race] src=%u:0x%08X %ux%u  torn %llu/%llu reads"
                              " (%.1f%%)\n",
                              t->location, t->offset, t->width, t->height,
                              (unsigned long long)torn,
                              (unsigned long long)checks,
                              100.0 * (double)torn / (double)checks);
              } }
            /* LD_FBDBG=1: account for the PS1 framebuffer specifically
             * (location 1, offset 0x400000, the 1024x512 fmt 0xE2 texture the
             * PS3 side composites). Its guest memory demonstrably fills up --
             * PS1 VRAM goes from 0 to 75,527 non-zero words -- yet
             * [tex-refresh] fires only 6 times in 90 seconds. Three different
             * things produce that, and they need different fixes: the texture
             * is rarely BOUND, the span is unreadable so `readable` is 0, or
             * the hash genuinely does not change. Count all three. */
            { static int fbd = -1;
              if (fbd < 0) fbd = getenv("LD_FBDBG") ? 1 : 0;
              if (fbd && t->location == 1u && t->offset == 0x400000u) {
                  static unsigned long long checks = 0, unread = 0, changed = 0;
                  checks++;
                  if (!readable) unread++;
                  if (readable && hash != entry->content_hash) changed++;
                  if ((checks % 64) == 0 || checks < 4)
                      fprintf(stderr, "[fbdbg] ps1fb checks=%llu unreadable=%llu"
                                      " changed=%llu span=%u %ux%u pitch=%u\n",
                              checks, unread, changed,
                              texture_source_span(t), t->width, t->height,
                              t->pitch);
              } }
            entry->last_hash_frame = g_ld_frames;
            if (readable && hash != entry->content_hash) {
                g_ld_reuse_tex = entry->cubemap ? NULL : entry->tex;
                ID3D12Resource* replacement =
                    decode_guest_texture_profiled(t, remap);
                g_ld_reuse_tex = NULL;
                if (replacement) {
                    ID3D12Resource* old = entry->tex;
                    entry->tex = replacement;
                    entry->content_hash = hash;
                    if (old != replacement) {
                        write_texture_srv(i, entry);
                        retire_texture(old);
                    }
                    static u32 refresh_count = 0;
                    refresh_count++;
                    if (refresh_count <= 64 || (refresh_count & 255) == 0)
                        fprintf(stderr,
                                "[tex-refresh] n=%u frame=%u unit-src=%u:0x%08X "
                                "fmt=0x%02X %ux%u\n",
                                refresh_count, g_ld_frames, t->location, t->offset,
                                t->format, t->width, t->height);
                }
            }
        }
ld_tex_intacta:
        entry->last_use_serial = ++g_ld_texture_use_serial;
        return entry->tex ? SRV_TEXTURE_BASE + i : SRV_WHITE;
    }

    u32 index;
    ID3D12Resource* evicted = NULL;
    ld_tex_encolher(ld_tex_cap(), 512u);
    const int cache_was_full = ld_tex_vivas() >= ld_tex_cap();
    if (!cache_was_full) {
        index = s_n_tex_livres ? s_tex_livres[--s_n_tex_livres] : g.n_textures++;
    } else {
        /*
         * A full boot plus a010 binds far more than 128 distinct guest
         * texture descriptors. Returning SRV_WHITE after the fixed cache
         * filled made the recovered orphanage render as flat green/black
         * geometry. Reuse the least-recently-used descriptor while keeping
         * its resource alive until the open command list reaches a fence.
         * The shader-visible draw table already contains a descriptor copy,
         * so rewriting this CPU cache slot cannot alter earlier draws.
         */
        g_ld_texture_cache_full++;
        index = 0;
        for (u32 i = 1; i < g.n_textures; i++)
            if (g.textures[i].last_use_serial <
                g.textures[index].last_use_serial)
                index = i;
        evicted = g.textures[index].tex;
        g_ld_texture_cache_evictions++;
        if (g_ld_texture_cache_full <= 16 ||
            (g_ld_texture_cache_full & (g_ld_texture_cache_full - 1)) == 0)
            fprintf(stderr,
                    "[texture-cache] FULL cap=%u lru-evictions=%llu\n",
                    ld_tex_cap(),
                    (unsigned long long)g_ld_texture_cache_evictions);
    }

    texcache_t replacement;
    memset(&replacement, 0, sizeof(replacement));
    replacement.location = t->location;
    replacement.offset = t->offset;
    replacement.format = t->format;
    replacement.width = t->width;
    replacement.height = t->height;
    replacement.pitch = t->pitch;
    replacement.remap = remap;
    replacement.cubemap = t->cubemap;
    replacement.depth3d = tex_depth3d(t);
    replacement.last_hash_frame = g_ld_frames;
    replacement.last_use_serial = ++g_ld_texture_use_serial;
    {
        int readable = 0;
        replacement.content_hash = texture_content_hash(t, &readable);
    }
    replacement.tex = decode_guest_texture_profiled(t, remap);
    if (replacement.tex) {
        texcache_t* entry = &g.textures[index];
        if (evicted)
            retire_texture(evicted);
        *entry = replacement;
        write_texture_srv(index, entry);
        tex_index_put(index);
    } else {
        if (!cache_was_full) {
            g.textures[index] = replacement;
            tex_index_put(index);
        }
        g_ld_texture_decode_fail++;
        if (g_ld_texture_decode_fail <= 16 ||
            (g_ld_texture_decode_fail & (g_ld_texture_decode_fail - 1)) == 0)
            fprintf(stderr,
                    "[texture-cache] decode failed n=%llu src=%u:0x%08X "
                    "fmt=0x%02X %ux%u pitch=%u\n",
                    (unsigned long long)g_ld_texture_decode_fail,
                    t->location, t->offset, t->format, t->width, t->height,
                    t->pitch);
    }
    return replacement.tex ? SRV_TEXTURE_BASE + index : SRV_WHITE;
}

typedef struct {
    DXGI_FORMAT dxgi;
    u32 bytes_per_texel;
    u32 component_bytes;
} vertex_texture_format_t;

static int vertex_texture_format(
    const rsx_dsp_vertex_texture* vt, vertex_texture_format_t* out)
{
    if (!vt || vt->dimension != 2 || vt->cubemap)
        return 0;
    vertex_texture_format_t format = {DXGI_FORMAT_UNKNOWN, 0, 0};
    switch (vt->format & TEX_FMT_BASE_MASK) {
    case RSX_TEX_FMT_W16Z16Y16X16_FLOAT:
        format.dxgi = DXGI_FORMAT_R16G16B16A16_FLOAT;
        format.bytes_per_texel = 8;
        format.component_bytes = 2;
        break;
    case RSX_TEX_FMT_W32Z32Y32X32_FLOAT:
        format.dxgi = DXGI_FORMAT_R32G32B32A32_FLOAT;
        format.bytes_per_texel = 16;
        format.component_bytes = 4;
        break;
    case RSX_TEX_FMT_X32_FLOAT:
        format.dxgi = DXGI_FORMAT_R32_FLOAT;
        format.bytes_per_texel = 4;
        format.component_bytes = 4;
        break;
    case RSX_TEX_FMT_Y16X16_FLOAT:
        format.dxgi = DXGI_FORMAT_R16G16_FLOAT;
        format.bytes_per_texel = 4;
        format.component_bytes = 2;
        break;
    default:
        return 0;
    }
    if (out)
        *out = format;
    return 1;
}

static int vertex_texture_trace_enabled(void)
{
    static int enabled = -1;
    if (enabled < 0)
        enabled = getenv("RSX_VTEX_TRACE") ? 1 : 0;
    return enabled;
}

static u32 vertex_texture_mask(void)
{
    u32 mask = 0;
    for (u32 u = 0; u < RSX_DSP_NUM_VERTEX_TEXTURES; u++) {
        rsx_dsp_vertex_texture vt;
        rsx_dsp_get_vertex_texture(&g.rsx, u, &vt);
        if (!vt.enabled) continue;
        g_ld_vtex_enabled++;
        if (vertex_texture_format(&vt, NULL)) {
            mask |= 1u << u;
        } else {
            g_ld_vtex_unsupported++;
            static u32 warned = 0;
            if (warned++ < 16 && vertex_texture_trace_enabled())
                fprintf(stderr,
                        "[vtex] enabled but unsupported unit=%u off=0x%08X "
                        "fmt=0x%02X dim=%u cube=%u %ux%u pitch=%u ctl=0x%08X\n",
                        u, vt.offset, vt.format, vt.dimension, vt.cubemap,
                        vt.width, vt.height, vt.pitch, vt.control0);
        }
    }
    return mask;
}

static u64 vertex_texture_hash(const rsx_dsp_vertex_texture* vt,
                               const u8** out_src, u32* out_pitch)
{
    vertex_texture_format_t format;
    if (!vertex_texture_format(vt, &format) || !vt->width || !vt->height ||
        vt->width > 4096 || vt->height > 4096)
        return 0;
    const u64 row_bytes64 = (u64)vt->width * format.bytes_per_texel;
    const u64 pitch64 = vt->pitch ? vt->pitch : row_bytes64;
    const u64 span64 = pitch64 * vt->height;
    if (row_bytes64 > UINT32_MAX || pitch64 < row_bytes64 ||
        pitch64 > UINT32_MAX || span64 > UINT32_MAX)
        return 0;
    const u32 pitch = (u32)pitch64;
    const u32 span = (u32)span64;
    const u8* src = span ? guest_ptr(vt->location, vt->offset, span) : NULL;
    if (out_src) *out_src = src;
    if (out_pitch) *out_pitch = pitch;
    return src ? fnv1a(src, span, 1469598103934665603ull) : 0;
}

static ID3D12Resource* decode_vertex_texture(
    const rsx_dsp_vertex_texture* vt, u64* out_hash)
{
    vertex_texture_format_t format;
    if (!vertex_texture_format(vt, &format) || !vt->width || !vt->height ||
        vt->width > 4096 || vt->height > 4096)
        return NULL;
    const u8* src = NULL;
    u32 pitch = 0;
    const u64 hash = vertex_texture_hash(vt, &src, &pitch);
    if (!src) return NULL;
    const u32 row_bytes = vt->width * format.bytes_per_texel;
    if (pitch < row_bytes)
        return NULL;
    u8* staging = (u8*)malloc((size_t)row_bytes * vt->height);
    if (!staging) return NULL;
    for (u32 y = 0; y < vt->height; y++) {
        const u8* srow = src + (size_t)y * pitch;
        u8* drow = staging + (size_t)y * row_bytes;
        const u32 components = row_bytes / format.component_bytes;
        for (u32 c = 0; c < components; c++) {
            const u8* source =
                srow + (size_t)c * format.component_bytes;
            u8* destination =
                drow + (size_t)c * format.component_bytes;
            for (u32 byte = 0; byte < format.component_bytes; byte++)
                destination[byte] =
                    source[format.component_bytes - 1u - byte];
        }
    }
    tex_level_t level = {
        vt->width, vt->height, staging, row_bytes, vt->height
    };
    ID3D12Resource* tex = create_texture_mipped(format.dxgi, &level, 1);
    free(staging);
    if (tex) {
        D3D12_RESOURCE_BARRIER b = {0};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition.pResource = tex;
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        b.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        g.list->lpVtbl->ResourceBarrier(g.list, 1, &b);
        if (out_hash) *out_hash = hash;
    }
    return tex;
}

static void write_vertex_texture_srv(u32 index, ID3D12Resource* tex)
{
    rsx_dsp_vertex_texture descriptor = {0};
    vertex_texture_format_t format;
    if (index >= g.n_vtex)
        return;
    descriptor.format = g.vtex[index].format;
    descriptor.dimension = 2;
    if (!vertex_texture_format(&descriptor, &format))
        return;
    D3D12_SHADER_RESOURCE_VIEW_DESC sd = {0};
    sd.Format = format.dxgi;
    sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    sd.Texture2D.MipLevels = 1;
    g.dev->lpVtbl->CreateShaderResourceView(
        g.dev, tex, &sd, srv_dst(SRV_VTEX_BASE + index));
}

static u32 vertex_texture_srv_slot(const rsx_dsp_vertex_texture* vt)
{
    for (u32 i = 0; i < g.n_vtex; i++) {
        vtexcache_t* e = &g.vtex[i];
        if (e->location != vt->location || e->offset != vt->offset ||
            e->format != vt->format || e->width != vt->width ||
            e->height != vt->height || e->pitch != vt->pitch)
            continue;
        if (e->tex && e->last_hash_frame != g_ld_frames) {
            const u64 hash = vertex_texture_hash(vt, NULL, NULL);
            e->last_hash_frame = g_ld_frames;
            if (hash && hash != e->content_hash) {
                u64 replacement_hash = 0;
                ID3D12Resource* replacement =
                    decode_vertex_texture(vt, &replacement_hash);
                if (replacement) {
                    ID3D12Resource* old = e->tex;
                    e->tex = replacement;
                    e->content_hash = replacement_hash;
                    write_vertex_texture_srv(i, e->tex);
                    retire_texture(old);
                    g_ld_vtex_refreshes++;
                }
            }
        }
        return e->tex ? SRV_VTEX_BASE + i : SRV_WHITE;
    }
    if (g.n_vtex >= MAX_VTEX) return SRV_WHITE;
    const u32 index = g.n_vtex++;
    vtexcache_t* e = &g.vtex[index];
    memset(e, 0, sizeof(*e));
    e->location = vt->location;
    e->offset = vt->offset;
    e->format = vt->format;
    e->width = vt->width;
    e->height = vt->height;
    e->pitch = vt->pitch;
    e->last_hash_frame = g_ld_frames;
    e->tex = decode_vertex_texture(vt, &e->content_hash);
    if (e->tex) {
        write_vertex_texture_srv(index, e->tex);
        g_ld_vtex_uploads++;
        if (vertex_texture_trace_enabled())
            fprintf(stderr,
                    "[vtex] upload unit-data %u:0x%08X fmt=0x%02X %ux%u pitch=%u\n",
                    vt->location, vt->offset, vt->format,
                    vt->width, vt->height, vt->pitch);
    }
    return e->tex ? SRV_VTEX_BASE + index : SRV_WHITE;
}

/* ---------------------------------------------------------------------------
 * surfaces (color RTs keyed by location/offset), rendered into then presented
 * -----------------------------------------------------------------------*/
/* fp constants are stored as raw bits; the shader reads them as floats. */
static float ld_c2f(u32 bits) { float f; memcpy(&f, &bits, sizeof f); return f; }

static u32 surface_get(u32 location, u32 offset, u32 want_w, u32 want_h,
                       DXGI_FORMAT want_fmt)
{
    if (!want_w) want_w = g.width;
    if (!want_h) want_h = g.height;
    /* Key on the DIMENSIONS as well as location/offset.
     *
     * Keying on location/offset alone meant a redeclaration of the same offset
     * at a different size STOLE the slot and recreated the D3D12 resource,
     * throwing the old contents away. ps1_netemu renders two passes at two
     * sizes -- the PS1 composite at 720x512 and the PS3 output at 1280x720 --
     * and it alternates between them, so that path fired 9,573 times in a
     * 90-second run:
     *
     *     [surfsz] live surface 0x0 redeclared 720x512 -> 1280x720 (content dropped)
     *     [surfsz] live surface 0x1401C00 redeclared 1280x720 -> 720x512 (content dropped)
     *
     * Every frame of both passes was being destroyed by the other. Giving each
     * (offset, size) its own surface lets both keep their contents.
     *
     * ponytail: two surfaces over the same guest memory do not alias, so a pass
     * that renders at one size and samples at the other will not see the
     * other's pixels. That is strictly better than the old behaviour, which saw
     * NOBODY's pixels, and it is the smaller change. If a title ever needs true
     * aliasing, the fix is one surface plus a copy on redeclare -- not going
     * back to destroying content. */
    u32 slot = MAX_SURFACES;
    for (u32 i = 0; i < g.n_surfaces; i++)
        if (g.surfaces[i].location == location && g.surfaces[i].offset == offset &&
            g.surfaces[i].w == want_w && g.surfaces[i].h == want_h &&
            g.surfaces[i].fmt == want_fmt)
            return i;
    /* GOWA: a surface no draw ever wrote colour to (depth-only passes still
     * name a colour offset) can host any clip that fits: God of War
     * Ascension's shadow map changes its clip every frame and each size was a
     * new surface. GOWA_SURF_SHARE=0 keys every size again. */
    { static int sh = -1; if (sh < 0) { const char* e = getenv("GOWA_SURF_SHARE"); sh = !(e && *e == '0'); }
      if (sh)
          for (u32 i = 0; i < g.n_surfaces; i++)
              if (g.surfaces[i].location == location && g.surfaces[i].offset == offset &&
                  g.surfaces[i].fmt == want_fmt && !g_ld_color_written[i] &&
                  g.surfaces[i].w >= want_w && g.surfaces[i].h >= want_h)
                  return i; }
    /* Never destroy a usable render target because a malformed live command
     * briefly decoded a guest pointer as clip dimensions.  The known-good
     * orphanage stream never exceeds 1280x1024; D3D12 rejects the observed
     * 1280x16452 declaration and the old path then dereferenced a null
     * resource.  Preserve the prior surface so diagnostics can capture the
     * actual world pass while the bad command link is isolated. */
    if (want_w > 8192u || want_h > 8192u) {
        static u32 invalid_surface_logs = 0;
        if (invalid_surface_logs++ < 32u)
            fprintf(stderr,
                    "[surface-guard] rejected implausible surface 0x%X "
                    "%ux%u; preserving slot=%s\n",
                    offset, want_w, want_h,
                    slot < MAX_SURFACES ? "existing" : "none");
        return slot < MAX_SURFACES ? slot : LD_INVALID_SURFACE;
    }
    if (g.n_surfaces >= MAX_SURFACES) {
        /* GOWA: reuse the slot of the surface written longest ago (never a
         * display buffer's) instead of dropping the new one. God of War
         * Ascension's shadow map changes its clip size every frame, and each
         * size is a surface (sizes are part of the key, see above): 38 of
         * them filled the table and the shadow passes were dropped.
         * GOWA_SURF_EVICT=0 drops again. */
        static int ev = -1; if (ev < 0) { const char* e = getenv("GOWA_SURF_EVICT"); ev = !(e && *e == '0'); }
        u32 victim = MAX_SURFACES, best = 0xFFFFFFFFu;
        if (ev)
            for (u32 i = 0; i < g.n_surfaces; i++) {
                const surface_t* c = &g.surfaces[i];
                int disp = 0;
                for (u32 b = 0; b < 8; b++)
                    if (g.display_buffers[b].valid && g.display_buffers[b].location == c->location &&
                        g.display_buffers[b].offset == c->offset) { disp = 1; break; }
                if (disp || i == g_ld_last_present_target) continue;
                if (c->last_write_generation < best) { best = c->last_write_generation; victim = i; }
            }
        static u32 full_logs = 0;
        if (victim == MAX_SURFACES) {
            if (full_logs++ < 8)
                fprintf(stderr, "[surfsz] surface table full (%u); dropping "
                                "%u:0x%X %ux%u\n",
                        (unsigned)MAX_SURFACES, location, offset, want_w, want_h);
            return LD_INVALID_SURFACE;
        }
        if (full_logs++ < 8)
            fprintf(stderr, "[surfsz] surface table full (%u); evicting slot %u %u:0x%X %ux%u for %u:0x%X %ux%u\n",
                    (unsigned)MAX_SURFACES, victim, g.surfaces[victim].location, g.surfaces[victim].offset,
                    g.surfaces[victim].w, g.surfaces[victim].h, location, offset, want_w, want_h);
        slot = victim;
    } else
    slot = g.n_surfaces;
    { static u32 new_logs = 0;
      if (new_logs++ < 24)
          fprintf(stderr, "[surfsz] new live surface %u:0x%X %ux%u (slot %u)\n",
                  location, offset, want_w, want_h, slot); }
    D3D12_HEAP_PROPERTIES hp = {0}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC rd = {0};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rd.Width = LD_PX(want_w); rd.Height = LD_PX(want_h); rd.DepthOrArraySize = 1;
    rd.MipLevels = 1; rd.Format = want_fmt; rd.SampleDesc.Count = 1;
    rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    D3D12_CLEAR_VALUE cv = {0}; cv.Format = rd.Format;
    surface_t* s = &g.surfaces[slot];
    ID3D12Resource* replacement = NULL;
    const HRESULT create_hr = g.dev->lpVtbl->CreateCommittedResource(
        g.dev, &hp, D3D12_HEAP_FLAG_NONE, &rd,
        D3D12_RESOURCE_STATE_RENDER_TARGET, &cv,
        &IID_ID3D12Resource, (void**)&replacement);
    if (FAILED(create_hr)) {
        static u32 surface_fail_logs = 0;
        if (surface_fail_logs++ < 32) {
            const HRESULT removed = g.dev->lpVtbl->GetDeviceRemovedReason(g.dev);
            fprintf(stderr,
                    "[surface-fail] color %u:0x%X %ux%u hr=0x%08lX "
                    "removed=0x%08lX slot=%u count=%u\n",
                    location, offset, want_w, want_h,
                    (unsigned long)create_hr, (unsigned long)removed,
                    slot, g.n_surfaces);
        }
        /* A resize failure must not poison an existing usable slot. */
        return slot < g.n_surfaces && s->tex ? slot : LD_INVALID_SURFACE;
    }
    /* Draws already recorded in the open command list can still reference
     * the old render target.  Release it only after ld_flush fences that list,
     * just like dynamic texture and zeta replacements. */
    if (s->tex)
        retire_texture(s->tex);
    if (s->fb_tex) {                 /* sized for the old resource */
        retire_texture(s->fb_tex);
        s->fb_tex = NULL;
    }
    s->tex = replacement;
    s->location = location; s->offset = offset; s->w = want_w; s->h = want_h;
    s->fmt = want_fmt;
#if !defined(YZ_PERF_CLEAN)
    s->resource_serial = ++g_ld_surface_resource_serial;
#endif
    ld_surface_note_write(slot, LD_SURFACE_WRITE_CREATE);
    /* RTVs for surfaces live above the swap-chain backbuffer RTVs */
    g.dev->lpVtbl->CreateRenderTargetView(g.dev, s->tex,
        NULL, rtv_handle(LD_SWAP_BUFFERS + slot));
    srv_write(SRV_SURFACE_BASE + slot, s->tex);
    if (slot == g.n_surfaces) g.n_surfaces++;
    return slot;
}

/* A draw that samples its own render target. RSX reads and writes the same
 * surface in one pass (GH3's post pass reads the scene at 0x200000 while
 * drawing into 0x200000); D3D12 cannot, and the lookup used to fall back to
 * guest VRAM -- empty, so the pass painted the scene black. Snapshot the
 * target into a per-surface copy and sample that. Returns the SRV slot, or
 * SRV_WHITE if the copy cannot be made. */
static u32 surface_feedback_srv(u32 slot)
{
    surface_t* s = &g.surfaces[slot];
    if (!s->tex) return SRV_WHITE;
    D3D12_RESOURCE_BARRIER b[2] = {0};
    for (int k = 0; k < 2; k++) {
        b[k].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b[k].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    }
    int fresh = 0;
    if (!s->fb_tex) {
        D3D12_RESOURCE_DESC rd;
        s->tex->lpVtbl->GetDesc(s->tex, &rd);
        rd.Flags = D3D12_RESOURCE_FLAG_NONE;
        D3D12_HEAP_PROPERTIES hp = {0};
        hp.Type = D3D12_HEAP_TYPE_DEFAULT;
        if (FAILED(g.dev->lpVtbl->CreateCommittedResource(
                g.dev, &hp, D3D12_HEAP_FLAG_NONE, &rd,
                D3D12_RESOURCE_STATE_COPY_DEST, NULL,
                &IID_ID3D12Resource, (void**)&s->fb_tex))) {
            s->fb_tex = NULL;
            return SRV_WHITE;
        }
        srv_write(SRV_FEEDBACK_BASE + slot, s->fb_tex);
        fresh = 1;
    }
    b[0].Transition.pResource = s->tex;
    b[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    b[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    b[1].Transition.pResource = s->fb_tex;
    b[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    b[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    g.list->lpVtbl->ResourceBarrier(g.list, fresh ? 1 : 2, b);
    g.list->lpVtbl->CopyResource(g.list, s->fb_tex, s->tex);
    b[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    b[0].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    b[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    b[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    g.list->lpVtbl->ResourceBarrier(g.list, 2, b);
    return SRV_FEEDBACK_BASE + slot;
}

/* Find the live surface whose guest rows (at `pitch`) contain byte `abs`, with
 * room for a w x h rect there. Returns the slot and the rect's origin. */
static int surface_containing(u32 location, u32 abs, u32 pitch, u32 w, u32 h,
                              u32* ox, u32* oy)
{
    for (u32 i = 0; i < g.n_surfaces; i++) {
        const surface_t* s = &g.surfaces[i];
        if (!s->tex || s->location != location || abs < s->offset) continue;
        const u32 d = abs - s->offset, y = d / pitch, x = (d % pitch) / 4u;
        if (x + w <= s->w && y + h <= s->h) { *ox = x; *oy = y; return (int)i; }
    }
    return -1;
}

/* NV3089 scaled-image blit whose SOURCE is a live surface: the pixels exist
 * only on the GPU, so the guest-memory copy in cellGcmSys moves black. GH3
 * copies its 1040x592 scene out to 0x1651500 every frame, runs its post pass
 * sampling that copy, and copies it back -- all black, so attract mode showed
 * only its overlay. Do the copy on the GPU, creating a live surface for the
 * destination so later texture lookups at that address find it. 1:1 only.
 * Returns 1 when handled, <= 0 when not (reason codes in ld_blit_gpu). */
static FILE* s_csv_file;       /* defined with live_draw_csv_emit */
static int   s_csv_gate_open;

static int ld_blit_gpu(u32 src_loc, u32 src_abs, u32 src_pitch,
                       u32 dst_loc, u32 dst_abs, u32 dst_pitch, u32 w, u32 h);
int rsx_live_draw_blit(u32 src_loc, u32 src_abs, u32 src_pitch,
                       u32 dst_loc, u32 dst_abs, u32 dst_pitch, u32 w, u32 h)
{
    const int r = ld_blit_gpu(src_loc, src_abs, src_pitch, dst_loc, dst_abs, dst_pitch, w, h);
    /* GOWA_BLIT_LOG=<n>[,<seconds>]: the first n blits (after that many
     * seconds), with the GPU path's result */
    { static int bl = -2; static int bn; static double bs; static ULONGLONG b0;
      if (bl == -2) { const char* e = getenv("GOWA_BLIT_LOG"); bl = e ? atoi(e) : 0;
                      const char* c = e ? strchr(e, ',') : NULL; bs = c ? atof(c + 1) : 0.0; b0 = GetTickCount64(); }
      if (bl > 0 && bn < bl && (double)(GetTickCount64() - b0) >= bs * 1000.0) {
          bn++;
          fprintf(stderr, "[blit] frame %u %u:0x%X/%u -> %u:0x%X/%u %ux%u gpu=%d\n", g_ld_frames,
                  src_loc, src_abs, src_pitch, dst_loc, dst_abs, dst_pitch, w, h, r);
      } }
    if (s_csv_file && s_csv_gate_open) {
        fprintf(s_csv_file, "#blit,%u,%u:0x%X/%u,%u:0x%X/%u,%ux%u,gpu=%d\n", g_ld_frames,
                src_loc, src_abs, src_pitch, dst_loc, dst_abs, dst_pitch, w, h, r);
        fflush(s_csv_file);
    }
    /* LD_BLIT_DUMP=<dir>: while the CSV gate is open, read back the first four
     * blits' destination surfaces right after the copy (one frame's worth). */
    { static const char* dir = (const char*)-1; static u32 n;
      if (dir == (const char*)-1) dir = getenv("LD_BLIT_DUMP");
      if (!(s_csv_file && s_csv_gate_open)) n = 0;
      else if (dir && r == 1 && n < 4) {
          u32 dx, dy;
          const int di = surface_containing(dst_loc, dst_abs, dst_pitch, w, h, &dx, &dy);
          if (di >= 0) {
              char path[MAX_PATH * 2];
              snprintf(path, sizeof(path), "%s\\blit_f%06u_%u_dst%08X.ppm", dir, g_ld_frames, n,
                       g.surfaces[di].offset);
              ld_dump_surface_ppm(path, &g.surfaces[di]);
          }
          n++;
      } }
    return r;
}

/* A SCALED NV3089 blit out of a live surface cannot be a CopyTextureRegion.
 * The one that matters is a supersampled scene resolved into a display buffer
 * (Tornado Outbreak renders 2x2 and downsamples into its scanout in 512x512
 * tiles): the guest-memory copy moves black and the flip presented whatever
 * surface happened to be current. When such a blit lands on a display buffer's
 * origin, remember the source surface; the flip presents it (at the host
 * surface's size) when the buffer has no live surface of its own.
 * ponytail: scanout alias, not a real scaled copy -- a title that samples the
 * resolved buffer as a texture still sees black; add a draw-based scaled blit
 * then. */
/* The copy half of a scaled resolve, done on the GPU. The source is a live
 * surface the host renders at display size although the guest laid it out
 * supersampled (Tornado Outbreak: pitch 10240 = 2560 guest pixels, host
 * surface 1280x720), so a ds x dt guest downsample is a 1:1 copy in host
 * pixels. Each tile lands in a live surface at the display buffer it writes
 * into, so the flip presents a snapshot taken at resolve time -- presenting
 * the source live (the alias below) showed whatever the title rendered into
 * it next, and the menu flickered to black on two frames in three.
 * Returns 1 when copied. */
int rsx_live_draw_resolve_blit(u32 src_loc, u32 src_abs, u32 src_pitch, u32 ds, u32 dt,
                               u32 dst_loc, u32 dst_abs, u32 dst_pitch,
                               u32 out_x, u32 out_y, u32 out_w, u32 out_h)
{
    if (!g.ready || !src_pitch || !dst_pitch || ds != dt || ds < 0x100000u) return 0;
    /* Source surface and the rect's guest position inside it. */
    /* The CLOSEST base at or below the address: surfaces' guest ranges
     * overlap (a stale surface at 0x0 "contains" 0x12804 too), and taking the
     * first match copied every 0x10000 tile out of that black surface. */
    int si = -1; u32 gx = 0, gy = 0;
    for (u32 i = 0; i < g.n_surfaces; i++) {
        const surface_t* s = &g.surfaces[i];
        if (!s->tex || s->location != src_loc || src_abs < s->offset) continue;
        const u32 d = src_abs - s->offset;
        if (d / src_pitch >= 2u * s->h) continue;
        if (si >= 0 && g.surfaces[si].offset > s->offset) continue;
        si = (int)i; gx = (d % src_pitch) / 4u; gy = d / src_pitch;
    }
    if (si < 0) return 0;
    /* Destination: the display buffer this tile writes into. */
    int bi = -1;
    for (u32 b = 0; b < 8; b++) {
        const display_buffer_t* d = &g.display_buffers[b];
        if (d->valid && d->location == dst_loc && dst_abs >= d->offset &&
            dst_abs < d->offset + d->pitch * d->height) { bi = (int)b; break; }
    }
    if (bi < 0) return 0;
    const display_buffer_t* db = &g.display_buffers[bi];
    const u32 dx = (dst_abs - db->offset) % db->pitch / 4u + out_x;
    const u32 dy = (dst_abs - db->offset) / db->pitch + out_y;
    const u32 slot = surface_get(db->location, db->offset, db->width, db->height, g.surfaces[si].fmt);
    if (slot == LD_INVALID_SURFACE || (int)slot == si) return 0;
    surface_t* dst = &g.surfaces[slot];
    if (dst->fmt != g.surfaces[si].fmt) return 0;
    /* Host source position: guest position over the downsample factor. */
    const u32 k = ds >> 20;                       /* 20.12 fixed -> integer factor */
    u32 sx = gx / (k ? k : 1), sy = gy / (k ? k : 1);
    u32 w = out_w, h = out_h;
    if (dx >= dst->w || dy >= dst->h || sx >= g.surfaces[si].w || sy >= g.surfaces[si].h) return 0;
    if (dx + w > dst->w) w = dst->w - dx;
    if (dy + h > dst->h) h = dst->h - dy;
    if (sx + w > g.surfaces[si].w) w = g.surfaces[si].w - sx;
    if (sy + h > g.surfaces[si].h) h = g.surfaces[si].h - sy;
    ID3D12Resource* s = g.surfaces[si].tex;
    D3D12_RESOURCE_BARRIER b[2] = {0};
    for (int q = 0; q < 2; q++) {
        b[q].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b[q].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b[q].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }
    b[0].Transition.pResource = s;        b[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    b[1].Transition.pResource = dst->tex; b[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    g.list->lpVtbl->ResourceBarrier(g.list, 2, b);
    D3D12_TEXTURE_COPY_LOCATION sl = {0}, dl = {0};
    sl.pResource = s;        sl.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dl.pResource = dst->tex; dl.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    D3D12_BOX box = { LD_PX(sx), LD_PX(sy), 0, LD_PX(sx + w), LD_PX(sy + h), 1 };
    g.list->lpVtbl->CopyTextureRegion(g.list, &dl, LD_PX(dx), LD_PX(dy), 0, &sl, &box);
    for (int q = 0; q < 2; q++) {
        b[q].Transition.StateBefore = b[q].Transition.StateAfter;
        b[q].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }
    g.list->lpVtbl->ResourceBarrier(g.list, 2, b);
    ld_surface_note_write(slot, LD_SURFACE_WRITE_COPY);
    if (s_csv_file && s_csv_gate_open) {
        fprintf(s_csv_file, "#resolve,%u,%u:0x%X,%u:0x%X,%u,%u,%ux%u\n", g_ld_frames,
                src_loc, src_abs, dst_loc, dst_abs, sx, sy, w, h);
        fflush(s_csv_file);
    }
    return 1;
}

void rsx_live_draw_note_resolve(u32 src_loc, u32 src_abs, u32 dst_loc, u32 dst_abs)
{
    if (!g.ready) return;
    for (u32 b = 0; b < 8; b++) {
        display_buffer_t* d = &g.display_buffers[b];
        if (!d->valid || d->location != dst_loc || d->offset != dst_abs) continue;
        for (u32 i = 0; i < g.n_surfaces; i++) {
            const surface_t* s = &g.surfaces[i];
            if (s->tex && s->location == src_loc && s->offset == src_abs) {
                if (d->resolve_src != (int)i + 1)
                    fprintf(stderr, "[live-draw] display buffer %u resolves from surface %u (0x%X)\n",
                            b, i, s->offset);
                d->resolve_src = (int)i + 1;
                return;
            }
        }
    }
}

static int ld_blit_gpu(u32 src_loc, u32 src_abs, u32 src_pitch,
                       u32 dst_loc, u32 dst_abs, u32 dst_pitch, u32 w, u32 h)
{
    if (!g.ready || !src_pitch || !dst_pitch || !w || !h) return 0;
    u32 sx, sy, dx = 0, dy = 0;
    const int si = surface_containing(src_loc, src_abs, src_pitch, w, h, &sx, &sy);
    if (si < 0) return -1;            /* source is not a live surface */
    int di = surface_containing(dst_loc, dst_abs, dst_pitch, w, h, &dx, &dy);
    if (di < 0) {
        const u32 slot = surface_get(dst_loc, dst_abs, dst_pitch / 4u, h,
                                     g.surfaces[si].fmt);
        if (slot == LD_INVALID_SURFACE) return -2;
        di = (int)slot;
        if (w > g.surfaces[di].w || h > g.surfaces[di].h) return -3;
    }
    if (di == si) return -4;
    if (g.surfaces[di].fmt != g.surfaces[si].fmt) return -5;
    ID3D12Resource* src = g.surfaces[si].tex;
    ID3D12Resource* dst = g.surfaces[di].tex;
    D3D12_RESOURCE_BARRIER b[2] = {0};
    for (int k = 0; k < 2; k++) {
        b[k].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b[k].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b[k].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }
    b[0].Transition.pResource = src; b[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    b[1].Transition.pResource = dst; b[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    g.list->lpVtbl->ResourceBarrier(g.list, 2, b);
    D3D12_TEXTURE_COPY_LOCATION s = {0}, d = {0};
    s.pResource = src; s.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    d.pResource = dst; d.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    D3D12_BOX box = { LD_PX(sx), LD_PX(sy), 0, LD_PX(sx + w), LD_PX(sy + h), 1 };
    g.list->lpVtbl->CopyTextureRegion(g.list, &d, LD_PX(dx), LD_PX(dy), 0, &s, &box);
    for (int k = 0; k < 2; k++) {
        b[k].Transition.StateBefore = b[k].Transition.StateAfter;
        b[k].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }
    g.list->lpVtbl->ResourceBarrier(g.list, 2, b);
    ld_surface_note_write((u32)di, LD_SURFACE_WRITE_COPY);
    return 1;
}

static u32 current_surface(void)
{
    rsx_dsp_surface sf;
    rsx_dsp_get_surface(&g.rsx, &sf);
    /* GOWA_SURF_FMT_LOG=1: each distinct (offset, guest colour format) */
    { static int fl = -1; if (fl < 0) fl = getenv("GOWA_SURF_FMT_LOG") ? 1 : 0;
      if (fl) { static u32 seen[128][2]; static int ns; int f = 0;
          for (int q = 0; q < ns; q++) if (seen[q][0] == sf.color_offset[0] && seen[q][1] == sf.color_format) { f = 1; break; }
          if (!f && ns < 128) { seen[ns][0] = sf.color_offset[0]; seen[ns][1] = sf.color_format; ns++;
              fprintf(stderr, "[surf-fmt] %u:0x%08X fmt=0x%X %ux%u pitch=%u rt=0x%X zeta=0x%X frame=%u\n",
                      sf.color_location[0], sf.color_offset[0], sf.color_format, sf.clip_w, sf.clip_h,
                      sf.color_pitch[0], sf.color_target, sf.zeta_offset, g_ld_frames); } } }
    return surface_get(sf.color_location[0], sf.color_offset[0],
                       sf.clip_w, sf.clip_h, ld_color_dxgi(sf.color_format));
}

/* On RSX each zeta (depth) address is distinct memory. The offline renderer's
 * s31 fix proved that sharing one D3D depth resource across all of this game's
 * shadow, scene and post-processing passes cross-contaminates later depth
 * tests. Keep the legacy shared target only as a bounded allocation fallback. */
static int honor_zeta_track(void)
{
    static int initialized = 0;
    static int enabled = 1;
    if (!initialized) {
        initialized = 1;
        enabled = getenv("RSX_NO_ZETA_TRACK") ? 0 : 1;
        fprintf(stderr, "[zetatrack] live per-zeta depth %s\n",
                enabled ? "ON" : "OFF (legacy shared target)");
    }
    return enabled;
}

static D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle(u32 slot)
{
    D3D12_CPU_DESCRIPTOR_HANDLE h;
    g.dsv_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(g.dsv_heap, &h);
    h.ptr += (size_t)slot * g.dsv_step;
    return h;
}

/* The R32F snapshot is sized from the clip of the first pass that bound the
 * zeta. A title that first binds its full-screen zeta with a smaller clip
 * (a 512x512 pass into a 1280x720 depth buffer) kept a 512x512 snapshot, and
 * every later full-size read of that depth saw 0 outside the top-left
 * corner. Grow the snapshot to the largest clip bound since, within the
 * depth resource. */
static void zdepth_grow_snapshot(u32 i, u32 rt_w, u32 rt_h)
{
    zdepth_t* z = &g.zdepths[i];
    if (!z->snapshot) return;
    u32 nw = rt_w < z->w ? rt_w : z->w, nh = rt_h < z->h ? rt_h : z->h;
    if (nw < z->snapshot_w) nw = z->snapshot_w;
    if (nh < z->snapshot_h) nh = z->snapshot_h;
    if (nw == z->snapshot_w && nh == z->snapshot_h) return;
    D3D12_HEAP_PROPERTIES hp = {0};
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC rd = {0};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rd.Width = LD_PX(nw);
    rd.Height = LD_PX(nh);
    rd.DepthOrArraySize = 1;
    rd.MipLevels = 1;
    rd.Format = DXGI_FORMAT_R32_FLOAT;
    rd.SampleDesc.Count = 1;
    rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    ID3D12Resource* snap = NULL;
    if (FAILED(g.dev->lpVtbl->CreateCommittedResource(
            g.dev, &hp, D3D12_HEAP_FLAG_NONE, &rd,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, NULL,
            &IID_ID3D12Resource, (void**)&snap))) {
        fprintf(stderr, "[zetatrack] snapshot grow failed %u:0x%X %ux%u\n",
                z->location, z->offset, nw, nh);
        return;
    }
    fprintf(stderr, "[zetatrack] snapshot %u:0x%X grown %ux%u -> %ux%u\n",
            z->location, z->offset, z->snapshot_w, z->snapshot_h, nw, nh);
    /* The open command list may still read the old one. */
    retire_texture(z->snapshot);
    z->snapshot = snap;
    z->snapshot_w = nw;
    z->snapshot_h = nh;
    z->snapshot_state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    srv_write_zdepth(SRV_ZDEPTH_BASE + i, z->snapshot);
    uav_write_zdepth(UAV_ZDEPTH_BASE + i, z->snapshot);
    /* The depth resource still holds what the old snapshot held (a clear
     * would have invalidated both): resolve it again at the next read. */
    if (z->snapshot_valid) z->had_write = 1;
    z->snapshot_valid = 0;
}

/* Returns DSV slot 1+i. Slot 0 is the legacy shared depth fallback. */
static u32 zdepth_get(u32 location, u32 offset, u32 rt_w, u32 rt_h)
{
    if (!honor_zeta_track()) return 0;
    /* The DSV must cover the complete live framebuffer/viewport it is bound
     * with.  Some early passes declare a smaller clip than the active canvas;
     * allocating only that clip causes the D3D device to reject later
     * surface/PSO creation.  Sampling a declared-size window from this padded
     * backing remains a separate SRV concern. */
    u32 want_w = rt_w > g.width ? rt_w : g.width;
    u32 want_h = rt_h > g.height ? rt_h : g.height;
    u32 slot = MAX_SURFACES;
    for (u32 i = 0; i < g.n_zdepths; i++) {
        zdepth_t* z = &g.zdepths[i];
        if (z->location == location && z->offset == offset) {
            if (z->w >= want_w && z->h >= want_h) {
                zdepth_grow_snapshot(i, rt_w, rt_h);
                return 1 + i;
            }
            slot = i;
            break;
        }
    }
    if (slot == MAX_SURFACES) {
        if (g.n_zdepths >= MAX_SURFACES) {
            fprintf(stderr, "[zetatrack] live cache full; shared fallback\n");
            return 0;
        }
        slot = g.n_zdepths;
    } else {
        zdepth_t* old = &g.zdepths[slot];
        if (want_w < old->w) want_w = old->w;
        if (want_h < old->h) want_h = old->h;
        fprintf(stderr,
                "[zetatrack] live zeta %u:0x%X outgrown %ux%u -> %ux%u\n",
                location, offset, old->w, old->h, want_w, want_h);
        if (old->tex) {
            /* Draws already recorded in the open command list may still
             * reference this DSV.  D3D12 command lists do not retain resource
             * lifetimes for the application, so release it only after the
             * next submit/fence completes. */
            retire_texture(old->tex);
            old->tex = NULL;
        }
        if (old->snapshot) {
            retire_texture(old->snapshot);
            old->snapshot = NULL;
        }
    }

    D3D12_HEAP_PROPERTIES hp = {0};
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC rd = {0};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rd.Width = LD_PX(want_w);
    rd.Height = LD_PX(want_h);
    rd.DepthOrArraySize = 1;
    rd.MipLevels = 1;
    rd.Format = DXGI_FORMAT_R32G8X24_TYPELESS;
    rd.SampleDesc.Count = 1;
    rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    D3D12_CLEAR_VALUE cv = {0};
    /* Optimized clear values must use the typed DSV format even though the
     * resource itself is typeless so it can also be exposed through an SRV. */
    cv.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
    cv.DepthStencil.Depth = 1.0f;
    zdepth_t* z = &g.zdepths[slot];
    HRESULT create_hr = g.dev->lpVtbl->CreateCommittedResource(
            g.dev, &hp, D3D12_HEAP_FLAG_NONE, &rd,
            D3D12_RESOURCE_STATE_DEPTH_WRITE, &cv,
            &IID_ID3D12Resource, (void**)&z->tex);
    if (FAILED(create_hr)) {
        fprintf(stderr,
                "[zetatrack] live create failed %u:0x%X hr=0x%08lX; shared fallback\n",
                location, offset, (unsigned long)create_hr);
        z->tex = NULL;
        return 0;
    }
    z->location = location;
    z->offset = offset;
    z->w = want_w;
    z->h = want_h;
    z->had_write = 0;
    z->snapshot_valid = 0;
    z->snapshot_w = rt_w ? rt_w : want_w;
    z->snapshot_h = rt_h ? rt_h : want_h;
    if (z->snapshot_w > want_w) z->snapshot_w = want_w;
    if (z->snapshot_h > want_h) z->snapshot_h = want_h;
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvd = {0};
    dsvd.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
    dsvd.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    g.dev->lpVtbl->CreateDepthStencilView(
        g.dev, z->tex, &dsvd, dsv_handle(1 + slot));

    /* D3D12 forbids a partial CopyTextureRegion from a depth/stencil
     * resource.  Keep the replay-proven logical dimensions and resolve the
     * depth plane with a compute shader instead; that is a normal SRV read,
     * not a depth-resource copy operation. */
    D3D12_RESOURCE_DESC snapshot_rd = {0};
    snapshot_rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    snapshot_rd.Width = LD_PX(z->snapshot_w);
    snapshot_rd.Height = LD_PX(z->snapshot_h);
    snapshot_rd.DepthOrArraySize = 1;
    snapshot_rd.MipLevels = 1;
    snapshot_rd.Format = DXGI_FORMAT_R32_FLOAT;
    snapshot_rd.SampleDesc.Count = 1;
    snapshot_rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    HRESULT snapshot_hr = g.dev->lpVtbl->CreateCommittedResource(
            g.dev, &hp, D3D12_HEAP_FLAG_NONE, &snapshot_rd,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, NULL,
            &IID_ID3D12Resource, (void**)&z->snapshot);
    if (FAILED(snapshot_hr)) {
        fprintf(stderr,
                "[zetatrack] live snapshot create failed %u:0x%X "
                "%ux%u hr=0x%08lX; guest fallback\n",
                location, offset, z->snapshot_w, z->snapshot_h,
                (unsigned long)snapshot_hr);
        z->snapshot = NULL;
    } else {
        z->snapshot_state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        srv_write_depth_source(SRV_DEPTH_SOURCE_BASE + slot, z->tex);
        srv_write_zdepth(SRV_ZDEPTH_BASE + slot, z->snapshot);
        uav_write_zdepth(UAV_ZDEPTH_BASE + slot, z->snapshot);
    }
    g.list->lpVtbl->ClearDepthStencilView(
        g.list, dsv_handle(1 + slot),
        D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
        1.0f, 0, 0, NULL);
    z->cleared = 1;
    fprintf(stderr,
            "[zetatrack] live new #%u %u:0x%X backing=%ux%u "
            "clip=%ux%u canvas=%ux%u\n",
            slot, location, offset, want_w, want_h,
            rt_w, rt_h, g.width, g.height);
    if (slot == g.n_zdepths) g.n_zdepths++;
    return 1 + slot;
}

/* Preserve the last completed depth-writing pass before the guest clears or
 * reuses its live zeta.  The replay renderer gets these bytes from its
 * captured cross-frame memory image; live rendering must explicitly retain
 * the equivalent GPU result because the guest never reads it back to CPU
 * VRAM. */
static int zdepth_snapshot(u32 slot)
{
    if (!slot) return 0;
    zdepth_t* z = &g.zdepths[slot - 1];
    if (!z->snapshot || !g.depth_snapshot_rootsig ||
        !g.depth_snapshot_pso)
        return 0;
    if (!z->had_write) return z->snapshot_valid;

    D3D12_GPU_DESCRIPTOR_HANDLE source_table, destination_table;
    if (!depth_snapshot_descriptors(
            slot - 1u, &source_table, &destination_table)) {
        fprintf(stderr,
                "[zetatrack] depth snapshot descriptor ring exhausted "
                "at frame %u\n",
                g_ld_frames);
        return z->snapshot_valid;
    }

    D3D12_RESOURCE_BARRIER bars[2] = {0};
    u32 nbar = 0;
    bars[nbar].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    bars[nbar].Transition.pResource = z->tex;
    bars[nbar].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    bars[nbar].Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    bars[nbar].Transition.StateAfter =
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    nbar++;
    if (z->snapshot_state != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
        bars[nbar].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        bars[nbar].Transition.pResource = z->snapshot;
        bars[nbar].Transition.Subresource =
            D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        bars[nbar].Transition.StateBefore = z->snapshot_state;
        bars[nbar].Transition.StateAfter =
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        nbar++;
    }
    g.list->lpVtbl->ResourceBarrier(g.list, nbar, bars);

    ID3D12DescriptorHeap* heaps[] = {g.srv_heap, g.smp_heap};
    g.list->lpVtbl->SetDescriptorHeaps(g.list, 2, heaps); ld_state_dirty();
    g.list->lpVtbl->SetPipelineState(g.list, g.depth_snapshot_pso);
    g.list->lpVtbl->SetComputeRootSignature(
        g.list, g.depth_snapshot_rootsig);
    g.list->lpVtbl->SetComputeRootDescriptorTable(
        g.list, 0, source_table);
    g.list->lpVtbl->SetComputeRootDescriptorTable(
        g.list, 1, destination_table);
    g.list->lpVtbl->Dispatch(
        g.list, (LD_PX(z->snapshot_w) + 7u) / 8u,
        (LD_PX(z->snapshot_h) + 7u) / 8u, 1);

    D3D12_RESOURCE_BARRIER uav_done = {0};
    uav_done.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uav_done.UAV.pResource = z->snapshot;
    g.list->lpVtbl->ResourceBarrier(g.list, 1, &uav_done);

    D3D12_RESOURCE_BARRIER done[2] = {0};
    done[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    done[0].Transition.pResource = z->tex;
    done[0].Transition.Subresource =
        D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    done[0].Transition.StateBefore =
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    done[0].Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    done[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    done[1].Transition.pResource = z->snapshot;
    done[1].Transition.Subresource =
        D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    done[1].Transition.StateBefore =
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    done[1].Transition.StateAfter =
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    g.list->lpVtbl->ResourceBarrier(g.list, 2, done);
    z->snapshot_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    z->snapshot_valid = 1;
    z->had_write = 0;
    z->snap_gen++;
    return 1;
}

/* GOWA: a zeta read through an ARGB8 texture. RSX keeps a Z24S8 zeta as
 * big-endian words depth[23:0] | stencil[7:0], so an A8R8G8B8 view of the
 * same memory reads A = depth[23:16], R = depth[15:8], G = depth[7:0],
 * B = stencil, and post-processing shaders (God of War Ascension's depth of
 * field) rebuild the depth from those bytes. The live renderer keeps the
 * zeta on the GPU only and sampled guest VRAM instead -- stale -- so the
 * whole frame came out blurred. A compute pass turns the R32F snapshot into
 * those bytes, through the texture's component remap exactly as
 * decode_texel applies it (s = A,R,G,B; out = s[sel] / 0 / 1 per channel).
 * Stencil reads as 0. GOWA_ZCOLOR=0 falls back to guest VRAM. */
static ID3D12RootSignature* s_zcolor_rs;
static ID3D12PipelineState* s_zcolor_pso;
static int ld_zcolor_pipeline(void)
{
    static int failed;
    if (s_zcolor_pso) return 1;
    if (failed) return 0;
    failed = 1;
    static const char src[] =
        "Texture2D<float> zsrc : register(t0);\n"
        "RWTexture2D<unorm float4> dst : register(u0);\n"
        "cbuffer C : register(b0) { uint remap; uint order; };\n"
        "float comp(uint4 s, uint c) {\n"
        "    uint op = (remap >> (8 + c * 2)) & 3u, sel = (remap >> (c * 2)) & 3u;\n"
        "    return op == 0u ? 0.0 : (op == 1u ? 1.0 : (float)s[sel] / 255.0); }\n"
        "[numthreads(8, 8, 1)]\n"
        "void main(uint3 id : SV_DispatchThreadID) {\n"
        "    uint w, h; dst.GetDimensions(w, h);\n"
        "    if (id.x >= w || id.y >= h) return;\n"
        "    uint z = (uint)round(saturate(zsrc.Load(int3(id.xy, 0))) * 16777215.0);\n"
        "    uint d2 = (z >> 16) & 255u, d1 = (z >> 8) & 255u, d0 = z & 255u;\n"
        /* s = A,R,G,B; GOWA_ZCOLOR_ORDER picks the byte layout (0 = big-endian
         * word depth|stencil, 1 = little-endian, 2 = stencil|depth BE, 3 = depth
         * BE in A,R,G with stencil 0xFF) */
        "    uint4 s = order == 1u ? uint4(0u, d0, d1, d2) : order == 2u ? uint4(0u, d2, d1, d0) :\n"
        "              order == 3u ? uint4(d2, d1, d0, 255u) : uint4(d2, d1, d0, 0u);\n"
        "    dst[id.xy] = float4(comp(s, 1u), comp(s, 2u), comp(s, 3u), comp(s, 0u));\n"
        "}\n";
    ID3DBlob *cs = NULL, *err = NULL;
    if (FAILED(D3DCompile(src, sizeof src - 1, "ld_zcolor", NULL, NULL, "main", "cs_5_0",
                          D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &cs, &err))) {
        if (err) { fprintf(stderr, "[zcolor] cs: %s\n", (const char*)err->lpVtbl->GetBufferPointer(err)); err->lpVtbl->Release(err); }
        return 0;
    }
    D3D12_DESCRIPTOR_RANGE r[2] = {0};
    r[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; r[0].NumDescriptors = 1;
    r[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; r[1].NumDescriptors = 1;
    D3D12_ROOT_PARAMETER rp[3] = {0};
    for (u32 i = 0; i < 2; i++) {
        rp[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rp[i].DescriptorTable.NumDescriptorRanges = 1;
        rp[i].DescriptorTable.pDescriptorRanges = &r[i];
        rp[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    }
    rp[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rp[2].Constants.ShaderRegister = 0;
    rp[2].Constants.Num32BitValues = 2;
    rp[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    D3D12_ROOT_SIGNATURE_DESC rsd = {0};
    rsd.NumParameters = 3; rsd.pParameters = rp;
    ID3DBlob* sig = NULL;
    if (FAILED(D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err))) {
        if (err) err->lpVtbl->Release(err);
        cs->lpVtbl->Release(cs);
        return 0;
    }
    HRESULT hr = g.dev->lpVtbl->CreateRootSignature(g.dev, 0, sig->lpVtbl->GetBufferPointer(sig),
                                                    sig->lpVtbl->GetBufferSize(sig),
                                                    &IID_ID3D12RootSignature, (void**)&s_zcolor_rs);
    sig->lpVtbl->Release(sig);
    if (FAILED(hr)) { cs->lpVtbl->Release(cs); return 0; }
    D3D12_COMPUTE_PIPELINE_STATE_DESC pd = {0};
    pd.pRootSignature = s_zcolor_rs;
    pd.CS.pShaderBytecode = cs->lpVtbl->GetBufferPointer(cs);
    pd.CS.BytecodeLength = cs->lpVtbl->GetBufferSize(cs);
    hr = g.dev->lpVtbl->CreateComputePipelineState(g.dev, &pd, &IID_ID3D12PipelineState, (void**)&s_zcolor_pso);
    cs->lpVtbl->Release(cs);
    if (FAILED(hr)) { fprintf(stderr, "[zcolor] PSO hr=0x%08lX\n", (unsigned long)hr); return 0; }
    failed = 0;
    return 1;
}
/* Fills zeta i's ARGB8 view from its snapshot (which the caller has just
 * brought up to date with zdepth_snapshot) and returns its SRV slot, or
 * SRV_WHITE when it cannot. The view stays in PIXEL_SHADER_RESOURCE. */
static u32 ld_zcolor_convert(u32 i, u32 remap)
{
    zdepth_t* z = &g.zdepths[i];
    if (!z->snapshot || !ld_zcolor_pipeline() || g.srv_ring_used >= SRV_RING_TABLES) return SRV_WHITE;
    const u32 w = LD_PX(z->snapshot_w), h = LD_PX(z->snapshot_h);
    int fresh = 0;
    if (z->zcolor && (z->zcolor_w != w || z->zcolor_h != h)) { retire_texture(z->zcolor); z->zcolor = NULL; }
    /* unchanged since the last build: reuse it (it stays a pixel-shader resource) */
    static int nocache = -1; if (nocache < 0) nocache = getenv("GOWA_IMG_NOCACHE") ? 1 : 0;   /* test */
    if (!nocache && z->zcolor && z->zcolor_ok && z->zcolor_gen == z->snap_gen && z->zcolor_remap == remap)
        return SRV_ZCOLOR_BASE + i;
    if (!z->zcolor) {
        D3D12_HEAP_PROPERTIES hp = {0}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC rd = {0};
        rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        rd.Width = w; rd.Height = h; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
        rd.Format = DXGI_FORMAT_R8G8B8A8_UNORM; rd.SampleDesc.Count = 1;
        rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        if (FAILED(g.dev->lpVtbl->CreateCommittedResource(g.dev, &hp, D3D12_HEAP_FLAG_NONE, &rd,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS, NULL, &IID_ID3D12Resource, (void**)&z->zcolor)))
            { z->zcolor = NULL; return SRV_WHITE; }
        z->zcolor_w = w; z->zcolor_h = h;
        srv_write(SRV_ZCOLOR_BASE + i, z->zcolor);
        D3D12_UNORDERED_ACCESS_VIEW_DESC ud = {0};
        ud.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        ud.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        g.dev->lpVtbl->CreateUnorderedAccessView(g.dev, z->zcolor, NULL, &ud, srv_dst(UAV_ZCOLOR_BASE + i));
        fresh = 1;
        fprintf(stderr, "[zcolor] zeta #%u %u:0x%X lida como ARGB8: vista %ux%u\n", i, z->location, z->offset, w, h);
    }
    /* snapshot: readable by the compute pass too; view: UAV */
    D3D12_RESOURCE_BARRIER b[2] = {0};
    u32 nb = 0;
    const D3D12_RESOURCE_STATES snap_read = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
                                            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    if (z->snapshot_state != snap_read) {
        b[nb].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b[nb].Transition.pResource = z->snapshot;
        b[nb].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b[nb].Transition.StateBefore = z->snapshot_state;
        b[nb].Transition.StateAfter = snap_read;
        nb++;
        z->snapshot_state = snap_read;
    }
    if (!fresh) {
        b[nb].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b[nb].Transition.pResource = z->zcolor;
        b[nb].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b[nb].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        b[nb].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        nb++;
    }
    if (nb) g.list->lpVtbl->ResourceBarrier(g.list, nb, b);
    /* descriptors: snapshot SRV + view UAV, side by side in the ring */
    const u32 base = g.srv_ring_used++ * SRV_TABLE_SIZE;
    D3D12_CPU_DESCRIPTOR_HANDLE cpu;
    g.srv_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(g.srv_heap, &cpu);
    cpu.ptr += (size_t)base * g.srv_step;
    g.dev->lpVtbl->CopyDescriptorsSimple(g.dev, 1, cpu, srv_cpu(SRV_ZDEPTH_BASE + i),
                                         D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    cpu.ptr += g.srv_step;
    g.dev->lpVtbl->CopyDescriptorsSimple(g.dev, 1, cpu, srv_cpu(UAV_ZCOLOR_BASE + i),
                                         D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_GPU_DESCRIPTOR_HANDLE gpu;
    g.srv_heap->lpVtbl->GetGPUDescriptorHandleForHeapStart(g.srv_heap, &gpu);
    gpu.ptr += (u64)base * g.srv_step;
    ID3D12DescriptorHeap* heaps[] = {g.srv_heap, g.smp_heap};
    g.list->lpVtbl->SetDescriptorHeaps(g.list, 2, heaps); ld_state_dirty();
    g.list->lpVtbl->SetPipelineState(g.list, s_zcolor_pso);
    g.list->lpVtbl->SetComputeRootSignature(g.list, s_zcolor_rs);
    g.list->lpVtbl->SetComputeRootDescriptorTable(g.list, 0, gpu);
    gpu.ptr += g.srv_step;
    g.list->lpVtbl->SetComputeRootDescriptorTable(g.list, 1, gpu);
    static int s_order = -1;
    if (s_order < 0) { const char* e = getenv("GOWA_ZCOLOR_ORDER"); s_order = e ? atoi(e) & 3 : 0; }
    const u32 consts[2] = { remap, (u32)s_order };
    g.list->lpVtbl->SetComputeRoot32BitConstants(g.list, 2, 2, consts, 0);
    g.list->lpVtbl->Dispatch(g.list, (w + 7u) / 8u, (h + 7u) / 8u, 1);
    D3D12_RESOURCE_BARRIER a = {0};
    a.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    a.Transition.pResource = z->zcolor;
    a.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    a.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    a.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    g.list->lpVtbl->ResourceBarrier(g.list, 1, &a);
    z->zcolor_ok = 1; z->zcolor_gen = z->snap_gen; z->zcolor_remap = remap;
    return SRV_ZCOLOR_BASE + i;
}

/* GOWA_CHAIN_RING=1 (debug): the last 32 draws that write or read the auto-
 * exposure chain's surfaces (0x24A0000, 0x2A29800, 0xB10000 in God of War
 * Ascension), dumped when GOWA_CAST_CHECK finds a corrupt rebuilt texture --
 * the order of the passes on the frames that flash. */
static void ld_chain_dump(const char* why)
{
    fprintf(stderr, "[chain] %s -- ultimos desenhos da cadeia (antigo -> novo):\n", why);
    const u32 n = s_chain_n < 48u ? s_chain_n : 48u;
    for (u32 i = s_chain_n - n; i < s_chain_n; i++) {
        const ld_chain_ev* e = &s_chain_ev[i % 48u];
        fprintf(stderr, "[chain]   f%u #%u alvo 0x%X %ux%u fmt=%08X z=0x%X fp=0x%X n=%u sc=%08X,%08X zfunc|zw=%08X tex 0x%X(%ux%u) 0x%X(%ux%u) v0 %g,%g %g,%g %g,%g\n",
                e->frame, e->seq, e->tgt, e->cw, e->ch, e->fmt, e->pitch, e->fp, e->ntri, e->sc0, e->sc1, e->cmask,
                e->tex[0], e->tw[0], e->th[0], e->tex[1], e->tw[1], e->th[1],
                e->v[0], e->v[1], e->v[2], e->v[3], e->v[4], e->v[5]);
        if (e->key)
            fprintf(stderr, "[chain]      pso=%016llx blend=%08X cmask=%08X vp=%08X,%08X\n",
                    (unsigned long long)e->key, e->blend, e->cmask2, e->vp0, e->vp1);
    }
}
/* GOWA: an A8R8G8B8 surface read as a Y16X16_FLOAT texture. RSX keeps the
 * surface as bytes A,R,G,B, so the two big-endian halves are A<<8|R and
 * G<<8|B; God of War Ascension's luminance chain writes them that way
 * (pk2h then up4ub into an RGBA8 target) and reads them back as halves for
 * the auto exposure. Sampling the RGBA8 surface gave bytes / 255 instead
 * and the exposure blew the fogged sky to white. A compute pass rebuilds
 * the halves into an RGBA16F texture of the TEXTURE's size (the reads take
 * the top-left sub-rect of a larger surface), through the remap as
 * decode_texel_f16 applies it for Y16X16 (s = h0, h1, h0, h1).
 * GOWA_CAST_Y16=0 samples the RGBA8 surface again. */
static ID3D12RootSignature* s_cast_rs;
static ID3D12PipelineState* s_cast_pso;
#define LD_CAST_WAYS 4u   /* sizes kept per surface: reads alternate 640x360 / 40x45 */
static struct { ID3D12Resource* tex; u32 w, h, used, gen, remap, ok; } s_cast[MAX_SURFACES][LD_CAST_WAYS];
static u32 s_cast_clock;
static int ld_cast_pipeline(void)
{
    static int failed;
    if (s_cast_pso) return 1;
    if (failed) return 0;
    failed = 1;
    static const char src[] =
        "Texture2D<float4> srcs : register(t0);\n"
        "RWTexture2D<float4> dst : register(u0);\n"
        "cbuffer C : register(b0) { uint remap; uint pad0; };\n"
        "float comp(float4 s, uint c) {\n"
        "    uint op = (remap >> (8 + c * 2)) & 3u, sel = (remap >> (c * 2)) & 3u;\n"
        "    return op == 0u ? 0.0 : (op == 1u ? 1.0 : s[sel]); }\n"
        "[numthreads(8, 8, 1)]\n"
        "void main(uint3 id : SV_DispatchThreadID) {\n"
        "    uint w, h; dst.GetDimensions(w, h);\n"
        "    if (id.x >= w || id.y >= h) return;\n"
        "    uint4 b = (uint4)round(saturate(srcs.Load(int3(id.xy, 0))) * 255.0);\n"   /* r,g,b,a */
        "    float h0 = f16tof32((b.w << 8) | b.x), h1 = f16tof32((b.y << 8) | b.z);\n"
        "    float4 s = float4(h0, h1, h0, h1);\n"                                       /* A,R,G,B */
        "    dst[id.xy] = float4(comp(s, 1u), comp(s, 2u), comp(s, 3u), comp(s, 0u));\n"
        "}\n";
    ID3DBlob *cs = NULL, *err = NULL;
    if (FAILED(D3DCompile(src, sizeof src - 1, "ld_cast", NULL, NULL, "main", "cs_5_0",
                          D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &cs, &err))) {
        if (err) { fprintf(stderr, "[cast] cs: %s\n", (const char*)err->lpVtbl->GetBufferPointer(err)); err->lpVtbl->Release(err); }
        return 0;
    }
    D3D12_DESCRIPTOR_RANGE r[2] = {0};
    r[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; r[0].NumDescriptors = 1;
    r[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; r[1].NumDescriptors = 1;
    D3D12_ROOT_PARAMETER rp[3] = {0};
    for (u32 i = 0; i < 2; i++) {
        rp[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rp[i].DescriptorTable.NumDescriptorRanges = 1;
        rp[i].DescriptorTable.pDescriptorRanges = &r[i];
        rp[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    }
    rp[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rp[2].Constants.Num32BitValues = 2;
    rp[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    D3D12_ROOT_SIGNATURE_DESC rsd = {0};
    rsd.NumParameters = 3; rsd.pParameters = rp;
    ID3DBlob* sig = NULL;
    if (FAILED(D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err))) {
        if (err) err->lpVtbl->Release(err);
        cs->lpVtbl->Release(cs);
        return 0;
    }
    HRESULT hr = g.dev->lpVtbl->CreateRootSignature(g.dev, 0, sig->lpVtbl->GetBufferPointer(sig),
                                                    sig->lpVtbl->GetBufferSize(sig),
                                                    &IID_ID3D12RootSignature, (void**)&s_cast_rs);
    sig->lpVtbl->Release(sig);
    if (FAILED(hr)) { cs->lpVtbl->Release(cs); return 0; }
    D3D12_COMPUTE_PIPELINE_STATE_DESC pd = {0};
    pd.pRootSignature = s_cast_rs;
    pd.CS.pShaderBytecode = cs->lpVtbl->GetBufferPointer(cs);
    pd.CS.BytecodeLength = cs->lpVtbl->GetBufferSize(cs);
    hr = g.dev->lpVtbl->CreateComputePipelineState(g.dev, &pd, &IID_ID3D12PipelineState, (void**)&s_cast_pso);
    cs->lpVtbl->Release(cs);
    if (FAILED(hr)) { fprintf(stderr, "[cast] PSO hr=0x%08lX\n", (unsigned long)hr); return 0; }
    failed = 0;
    return 1;
}
/* Surface `si` (RGBA8, in RENDER_TARGET) read as a tw x th Y16X16_FLOAT
 * texture: returns the SRV slot of its rebuilt halves, or SRV_WHITE. */
/* GOWA_CHAIN_RING: a Y16X16 rebuild (reused = 1: the cached one) */
static void ld_chain_cast_ev(const surface_t* sf, u32 w, u32 h, int reused)
{
    if (!ld_chain_on() || !ld_chain_hit(sf->offset)) return;
    ld_chain_ev ev; memset(&ev, 0, sizeof ev);
    ev.frame = g_ld_frames; ev.seq = ++s_chain_seq; ev.tgt = sf->offset; ev.cw = w; ev.ch = h;
    ev.fmt = 0xCA000000u | (u32)reused; ev.pitch = sf->last_write_generation; ev.ntri = sf->last_write_kind;
    ev.fp = (u32)(sf - g.surfaces);   /* surface index */
    s_chain_ev[s_chain_n++ % 48u] = ev;
}
static u32 ld_cast_y16x16(u32 si, u32 tw, u32 th, u32 remap)
{
    surface_t* sf = &g.surfaces[si];
    if (!sf->tex || si >= MAX_SURFACES || !ld_cast_pipeline() || g.srv_ring_used >= SRV_RING_TABLES)
        return SRV_WHITE;
    if (tw > sf->w) tw = sf->w;
    if (th > sf->h) th = sf->h;
    const u32 w = LD_PX(tw), h = LD_PX(th);
    int fresh = 0;
    u32 way = LD_CAST_WAYS, old = 0;
    for (u32 k = 0; k < LD_CAST_WAYS; k++)
        if (s_cast[si][k].tex && s_cast[si][k].w == w && s_cast[si][k].h == h) { way = k; break; }
    if (way == LD_CAST_WAYS) {   /* a free way, else the least recently used */
        for (u32 k = 0; k < LD_CAST_WAYS; k++) {
            if (!s_cast[si][k].tex) { way = k; break; }
            if (s_cast[si][k].used < s_cast[si][old].used) old = k;
        }
        if (way == LD_CAST_WAYS) { way = old; retire_texture(s_cast[si][way].tex); s_cast[si][way].tex = NULL; }
    }
    s_cast[si][way].used = ++s_cast_clock;
    const u32 srv_slot = SRV_CAST_BASE + si * LD_CAST_WAYS + way, uav_slot = UAV_CAST_BASE + si * LD_CAST_WAYS + way;
    /* the surface not written since this way was built: reuse it */
    static int nocache = -1; if (nocache < 0) nocache = getenv("GOWA_IMG_NOCACHE") ? 1 : 0;   /* test */
    if (!nocache && s_cast[si][way].tex && s_cast[si][way].ok && s_cast[si][way].gen == sf->last_write_generation &&
        s_cast[si][way].remap == remap) {
        ld_chain_cast_ev(sf, w, h, 1);
        return srv_slot;
    }
    ld_chain_cast_ev(sf, w, h, 0);
    if (!s_cast[si][way].tex) {
        D3D12_HEAP_PROPERTIES hp = {0}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC rd = {0};
        rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        rd.Width = w; rd.Height = h; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
        rd.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; rd.SampleDesc.Count = 1;
        rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        if (FAILED(g.dev->lpVtbl->CreateCommittedResource(g.dev, &hp, D3D12_HEAP_FLAG_NONE, &rd,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS, NULL, &IID_ID3D12Resource, (void**)&s_cast[si][way].tex)))
            { s_cast[si][way].tex = NULL; return SRV_WHITE; }
        s_cast[si][way].w = w; s_cast[si][way].h = h;
        srv_write(srv_slot, s_cast[si][way].tex);
        D3D12_UNORDERED_ACCESS_VIEW_DESC ud = {0};
        ud.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        ud.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        g.dev->lpVtbl->CreateUnorderedAccessView(g.dev, s_cast[si][way].tex, NULL, &ud, srv_dst(uav_slot));
        fresh = 1;
        static u32 logs;
        if (logs++ < 8)
            fprintf(stderr, "[cast] superficie %u 0x%X (%ux%u) lida como Y16X16_FLOAT %ux%u\n",
                    si, sf->offset, sf->w, sf->h, tw, th);
    }
    D3D12_RESOURCE_BARRIER b[2] = {0};
    u32 nb = 0;
    b[nb].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b[nb].Transition.pResource = sf->tex;
    b[nb].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b[nb].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    b[nb].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    nb++;
    if (!fresh) {
        b[nb].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b[nb].Transition.pResource = s_cast[si][way].tex;
        b[nb].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b[nb].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        b[nb].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        nb++;
    }
    g.list->lpVtbl->ResourceBarrier(g.list, nb, b);
    const u32 base = g.srv_ring_used++ * SRV_TABLE_SIZE;
    D3D12_CPU_DESCRIPTOR_HANDLE cpu;
    g.srv_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(g.srv_heap, &cpu);
    cpu.ptr += (size_t)base * g.srv_step;
    g.dev->lpVtbl->CopyDescriptorsSimple(g.dev, 1, cpu, srv_cpu(SRV_SURFACE_BASE + si),
                                         D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    cpu.ptr += g.srv_step;
    g.dev->lpVtbl->CopyDescriptorsSimple(g.dev, 1, cpu, srv_cpu(uav_slot),
                                         D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_GPU_DESCRIPTOR_HANDLE gpu;
    g.srv_heap->lpVtbl->GetGPUDescriptorHandleForHeapStart(g.srv_heap, &gpu);
    gpu.ptr += (u64)base * g.srv_step;
    ID3D12DescriptorHeap* heaps[] = {g.srv_heap, g.smp_heap};
    g.list->lpVtbl->SetDescriptorHeaps(g.list, 2, heaps); ld_state_dirty();
    g.list->lpVtbl->SetPipelineState(g.list, s_cast_pso);
    g.list->lpVtbl->SetComputeRootSignature(g.list, s_cast_rs);
    g.list->lpVtbl->SetComputeRootDescriptorTable(g.list, 0, gpu);
    gpu.ptr += g.srv_step;
    g.list->lpVtbl->SetComputeRootDescriptorTable(g.list, 1, gpu);
    const u32 consts[2] = { remap, 0 };
    g.list->lpVtbl->SetComputeRoot32BitConstants(g.list, 2, 2, consts, 0);
    g.list->lpVtbl->Dispatch(g.list, (w + 7u) / 8u, (h + 7u) / 8u, 1);
    D3D12_RESOURCE_BARRIER a[2] = {0};
    for (int q = 0; q < 2; q++) {
        a[q].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        a[q].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    }
    a[0].Transition.pResource = sf->tex;
    a[0].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    a[0].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    a[1].Transition.pResource = s_cast[si][way].tex;
    a[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    a[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    g.list->lpVtbl->ResourceBarrier(g.list, 2, a);
    s_cast[si][way].ok = 1; s_cast[si][way].gen = sf->last_write_generation; s_cast[si][way].remap = remap;
    /* GOWA_CAST_CHECK=1 (debug, synchronous): read back every small rebuilt
     * texture (<= 256 texels) and report non-finite or huge halves -- the
     * input of the 1x1 auto-exposure pass */
    /* GOWA_CAST_CHECK=<from frame>,<frames>: every size up to 640x360, and one
     * line per rebuilt texture in that window (bad or not) */
    { static int cc = -1; static long cc_from, cc_len;
      if (cc < 0) { const char* e = getenv("GOWA_CAST_CHECK"); cc = e ? 1 : 0;
                    if (e && strchr(e, ',')) { cc = 2; cc_from = atol(e); cc_len = atol(strchr(e, ',') + 1); } }
      const int win = cc == 2 && (long)g_ld_frames >= cc_from && (long)g_ld_frames < cc_from + cc_len;
      if ((cc == 1 && w * h <= 256u) || (win && w <= 640u && h <= 360u)) {
          static ID3D12Resource* rb; const UINT64 need = 5120u * 360u;
          const u32 rp = (w * 8u + 255u) & ~255u;
          if (!rb) { D3D12_HEAP_PROPERTIES hp = {0}; hp.Type = D3D12_HEAP_TYPE_READBACK;
                     D3D12_RESOURCE_DESC bd = {0}; bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = need;
                     bd.Height = 1; bd.DepthOrArraySize = 1; bd.MipLevels = 1; bd.SampleDesc.Count = 1;
                     bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
                     if (FAILED(g.dev->lpVtbl->CreateCommittedResource(g.dev, &hp, D3D12_HEAP_FLAG_NONE, &bd,
                             D3D12_RESOURCE_STATE_COPY_DEST, NULL, &IID_ID3D12Resource, (void**)&rb))) rb = NULL; }
          if (rb) {
              D3D12_RESOURCE_BARRIER c = {0}; c.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
              c.Transition.pResource = s_cast[si][way].tex; c.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
              c.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE; c.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
              g.list->lpVtbl->ResourceBarrier(g.list, 1, &c);
              D3D12_TEXTURE_COPY_LOCATION src = {0}, dl = {0};
              src.pResource = s_cast[si][way].tex; src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
              dl.pResource = rb; dl.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
              dl.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
              dl.PlacedFootprint.Footprint.Width = w; dl.PlacedFootprint.Footprint.Height = h;
              dl.PlacedFootprint.Footprint.Depth = 1; dl.PlacedFootprint.Footprint.RowPitch = rp;
              g.list->lpVtbl->CopyTextureRegion(g.list, &dl, 0, 0, 0, &src, NULL);
              c.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE; c.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
              g.list->lpVtbl->ResourceBarrier(g.list, 1, &c);
              ld_flush(LD_FLUSH_READBACK);
              const u8* px = NULL; D3D12_RANGE rr = {0, (SIZE_T)need};
              if (SUCCEEDED(rb->lpVtbl->Map(rb, 0, &rr, (void**)&px))) {
                  u32 bad = 0, big = 0, bx = 0, by = 0; u16 first = 0; float mx = 0.0f; double sum[2] = {0, 0};
                  for (u32 y = 0; y < h; y++) for (u32 x = 0; x < w; x++) for (u32 k = 0; k < 2; k++) {
                      u16 hv; memcpy(&hv, px + y * rp + x * 8u + k * 2u, 2);
                      if ((hv & 0x7C00u) == 0x7C00u) { if (!bad) { first = hv; bx = x; by = y; } bad++; continue; }
                      const u32 e = (hv >> 10) & 0x1Fu;
                      const float v = e ? (float)(1u << e) / 32768.0f * (1.0f + (hv & 0x3FFu) / 1024.0f) : 0.0f;
                      if (v > mx) mx = v;
                      if (v > 1000.0f) big++;
                      sum[k] += v;
                  }
                  if (win) {
                      static u32 nw;
                      if (nw++ < 4000)
                          fprintf(stderr, "[cast-win] frame=%u sup 0x%X %ux%u: ruins %u (1o 0x%04X em %u,%u) >1000 %u max %.3g media %.4g / %.4g\n",
                                  g_ld_frames, sf->offset, w, h, bad, first, bx, by, big, mx, sum[0] / (w * h), sum[1] / (w * h));
                      /* raw halves of the first good 160x180 and of the bad
                       * ones, for out/castbin_*.bin (rows of w * 8 bytes) */
                      static int nbin, ngood;
                      if (w == 160u && ((bad && nbin < 6) || (!bad && !ngood))) {
                          char path[MAX_PATH];
                          snprintf(path, sizeof path, "out\\castbin_%s_f%u_%ux%u.bin",
                                   bad ? "ruim" : "bom", g_ld_frames, w, h);
                          FILE* fb = fopen(path, "wb");
                          if (fb) { for (u32 y = 0; y < h; y++) fwrite(px + y * rp, 1, w * 8u, fb); fclose(fb); }
                          if (bad) nbin++; else ngood++;
                      }
                      if (w == 160u && ld_chain_on()) {
                          static int nd, ndg;
                          if (bad && nd++ < 4) ld_chain_dump("160x180 corrompida");
                          else if (!bad && ndg++ == 20) ld_chain_dump("160x180 boa (referencia)");
                      }
                  }
                  static u32 n;
                  if (cc == 1 && (bad || big) && n++ < 200) {
                      fprintf(stderr, "[cast-check] frame=%u sup 0x%X %ux%u: %u fp16 inf/NaN (1o 0x%04X), %u > 1000, max %.3g\n",
                              g_ld_frames, sf->offset, w, h, bad, first, big, mx);
                      if (ld_chain_on() && n <= 6) ld_chain_dump("textura reconstruida corrompida");
                      g_ld_surf_dump_req = 1;
                  } else if (cc == 1) {
                      static u32 good; if (good++ == 300) { if (ld_chain_on()) ld_chain_dump("quadro bom (referencia)"); g_ld_surf_dump_req = 2; }
                  }
                  D3D12_RANGE wr = {0, 0}; rb->lpVtbl->Unmap(rb, 0, &wr);
              }
          }
      } }
    return srv_slot;
}

/* ---------------------------------------------------------------------------
 * PSO cache (VP+FP+render-state keyed)
 * -----------------------------------------------------------------------*/
static u64 fnv1a(const void* data, u32 n, u64 h)
{
    const u8* p = (const u8*)data;
    for (u32 i = 0; i < n; i++) { h ^= p[i]; h *= 1099511628211ull; }
    return h;
}

static int sampler_table_needs_flush(const u32 slots[SMP_TABLE_SIZE])
{
    if (g.smp_ring_used < SMP_RING_TABLES) return 0;
    for (u32 i = 0; i < g.smp_ring_used; i++)
        if (memcmp(g.smp_ring_slots[i], slots,
                   sizeof(g.smp_ring_slots[i])) == 0)
            return 0;
    return 1;
}

static u32 shader_disk_cache_stage_index(u32 stage)
{
    return stage == 'V' ? 0u : 1u;
}

static void shader_disk_cache_progress(u64 value)
{
    if (value > 4u && (value & (value - 1u)) != 0u)
        return;
    fprintf(stderr,
            "[shader-disk-cache] hits{vs=%llu ps=%llu} "
            "misses{vs=%llu ps=%llu} writes{vs=%llu ps=%llu} "
            "rejects=%llu\n",
            (unsigned long long)g_ld_shader_disk_hits[0],
            (unsigned long long)g_ld_shader_disk_hits[1],
            (unsigned long long)g_ld_shader_disk_misses[0],
            (unsigned long long)g_ld_shader_disk_misses[1],
            (unsigned long long)g_ld_shader_disk_writes[0],
            (unsigned long long)g_ld_shader_disk_writes[1],
            (unsigned long long)g_ld_shader_disk_rejects);
}

/* GOWA (27/10): where the shader cache and the PSO library live. By default
 * scratch\ beside the exe (the working directory), so each build variant
 * (build_native, build_v3rz) kept its own and started empty after a switch;
 * GOWA_CACHE_DIR (the launcher sets <port>\cache) keeps one for all of them. */
static const char* gowa_cache_base(void)
{
    static char base[MAX_PATH];
    if (!base[0]) {
        const char* e = getenv("GOWA_CACHE_DIR");
        if (!e || !e[0] || strlen(e) >= sizeof base - 64u)
            e = "scratch";
        snprintf(base, sizeof base, "%s", e);
        CreateDirectoryA(base, NULL);
    }
    return base;
}

static int shader_disk_cache_prepare(void)
{
    if (g_ld_shader_disk_ready >= 0)
        return g_ld_shader_disk_ready;
    g_ld_shader_disk_ready = 0;
    if (getenv("RSX_NO_SHADER_DISK_CACHE")) {
        fprintf(stderr, "[shader-disk-cache] disabled\n");
        return 0;
    }

    const char* override_dir = getenv("RSX_SHADER_CACHE_DIR");
    if (override_dir && override_dir[0]) {
        if (snprintf(g_ld_shader_disk_dir, sizeof(g_ld_shader_disk_dir),
                     "%s", override_dir) < 0 ||
            strlen(g_ld_shader_disk_dir) >= sizeof(g_ld_shader_disk_dir) - 1u)
            return 0;
    } else {
        snprintf(g_ld_shader_disk_dir, sizeof(g_ld_shader_disk_dir),
                 "%s\\rsx_shader_cache_v%u", gowa_cache_base(),
                 SHADER_DISK_CACHE_VERSION);
    }

    if (!CreateDirectoryA(g_ld_shader_disk_dir, NULL) &&
        GetLastError() != ERROR_ALREADY_EXISTS) {
        fprintf(stderr,
                "[shader-disk-cache] cannot create '%s' error=%lu\n",
                g_ld_shader_disk_dir, (unsigned long)GetLastError());
        return 0;
    }
    const DWORD attributes = GetFileAttributesA(g_ld_shader_disk_dir);
    if (attributes == INVALID_FILE_ATTRIBUTES ||
        !(attributes & FILE_ATTRIBUTE_DIRECTORY))
        return 0;

    g_ld_shader_disk_ready = 1;
    fprintf(stderr, "[shader-disk-cache] enabled dir='%s' version=%u\n",
            g_ld_shader_disk_dir, SHADER_DISK_CACHE_VERSION);
    return 1;
}

static int shader_disk_cache_path(
    char path[MAX_PATH], u32 stage, u64 hash, u32 source_length)
{
    if (!shader_disk_cache_prepare())
        return 0;
    const int length = snprintf(
        path, MAX_PATH, "%s\\%c_%016llX_%08X.bin",
        g_ld_shader_disk_dir, (char)stage,
        (unsigned long long)hash, source_length);
    return length > 0 && length < MAX_PATH;
}

static ID3DBlob* shader_disk_cache_load(
    u32 stage, const char* source, u32 source_length, u64 hash)
{
    char path[MAX_PATH];
    const u32 stage_index = shader_disk_cache_stage_index(stage);
    if (!shader_disk_cache_path(path, stage, hash, source_length))
        return NULL;

    FILE* file = fopen(path, "rb");
    if (!file) {
        const u64 value = ++g_ld_shader_disk_misses[stage_index];
        shader_disk_cache_progress(value);
        return NULL;
    }

    shader_disk_cache_header header;
    memset(&header, 0, sizeof(header));
    int valid = fread(&header, sizeof(header), 1, file) == 1 &&
        header.magic == SHADER_DISK_CACHE_MAGIC &&
        header.version == SHADER_DISK_CACHE_VERSION &&
        header.stage == stage && header.source_hash == hash &&
        header.source_length == source_length && source_length <= 256u * 1024u &&
        header.blob_length > 0 &&
        header.blob_length <= SHADER_DISK_CACHE_MAX_BLOB;
    char* stored_source = NULL;
    ID3DBlob* blob = NULL;
    if (valid) {
        stored_source = (char*)malloc(source_length ? source_length : 1u);
        valid = stored_source != NULL &&
            fread(stored_source, 1, source_length, file) == source_length &&
            memcmp(stored_source, source, source_length) == 0;
    }
    if (valid) {
        valid = SUCCEEDED(D3DCreateBlob(header.blob_length, &blob)) && blob &&
            fread(blob->lpVtbl->GetBufferPointer(blob), 1,
                  header.blob_length, file) == header.blob_length;
    }
    free(stored_source);
    fclose(file);

    if (!valid) {
        if (blob)
            blob->lpVtbl->Release(blob);
        DeleteFileA(path);
        g_ld_shader_disk_rejects++;
        const u64 value = ++g_ld_shader_disk_misses[stage_index];
        shader_disk_cache_progress(value);
        return NULL;
    }

    const u64 value = ++g_ld_shader_disk_hits[stage_index];
    shader_disk_cache_progress(value);
    return blob;
}

static void shader_disk_cache_store(
    u32 stage, const char* source, u32 source_length, u64 hash,
    ID3DBlob* blob)
{
    if (!blob || source_length > 256u * 1024u)
        return;
    const size_t blob_length = blob->lpVtbl->GetBufferSize(blob);
    if (!blob_length || blob_length > SHADER_DISK_CACHE_MAX_BLOB)
        return;

    char path[MAX_PATH], temporary[MAX_PATH];
    if (!shader_disk_cache_path(path, stage, hash, source_length))
        return;
    const int temporary_length = snprintf(
        temporary, sizeof(temporary), "%s.tmp.%lu", path,
        (unsigned long)GetCurrentProcessId());
    if (temporary_length <= 0 || temporary_length >= sizeof(temporary))
        return;

    shader_disk_cache_header header;
    memset(&header, 0, sizeof(header));
    header.magic = SHADER_DISK_CACHE_MAGIC;
    header.version = SHADER_DISK_CACHE_VERSION;
    header.stage = stage;
    header.source_length = source_length;
    header.blob_length = (u32)blob_length;
    header.source_hash = hash;

    FILE* file = fopen(temporary, "wb");
    if (!file)
        return;
    const int wrote = fwrite(&header, sizeof(header), 1, file) == 1 &&
        fwrite(source, 1, source_length, file) == source_length &&
        fwrite(blob->lpVtbl->GetBufferPointer(blob), 1,
               blob_length, file) == blob_length &&
        fflush(file) == 0;
    fclose(file);
    if (!wrote || !MoveFileExA(
            temporary, path,
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileA(temporary);
        return;
    }

    const u32 stage_index = shader_disk_cache_stage_index(stage);
    const u64 value = ++g_ld_shader_disk_writes[stage_index];
    shader_disk_cache_progress(value);
}

static u64 ld_hash_structural_render_state(
    const render_state_t* state, u64 hash)
{
#define LD_HASH_RENDER_FIELD(name) \
    hash = fnv1a(&state->name, sizeof(state->name), hash)
    /* Alpha payload is deliberately absent. Enable and compare mode still
     * select distinct shader/PSO behavior. Every other D3D12 render-state
     * field is listed explicitly so struct padding can never become identity. */
    LD_HASH_RENDER_FIELD(alpha_test_enable);
    LD_HASH_RENDER_FIELD(alpha_func);
    LD_HASH_RENDER_FIELD(blend_enable);
    LD_HASH_RENDER_FIELD(sf_rgb);
    LD_HASH_RENDER_FIELD(df_rgb);
    LD_HASH_RENDER_FIELD(sf_a);
    LD_HASH_RENDER_FIELD(df_a);
    LD_HASH_RENDER_FIELD(eq_rgb);
    LD_HASH_RENDER_FIELD(eq_a);
    LD_HASH_RENDER_FIELD(depth_test);
    LD_HASH_RENDER_FIELD(depth_write);
    LD_HASH_RENDER_FIELD(depth_func);
    LD_HASH_RENDER_FIELD(cull_enable);
    LD_HASH_RENDER_FIELD(cull_face);
    LD_HASH_RENDER_FIELD(front_face);
    LD_HASH_RENDER_FIELD(color_mask);
    LD_HASH_RENDER_FIELD(rt_dxgi);
    LD_HASH_RENDER_FIELD(rt_count);
    LD_HASH_RENDER_FIELD(stencil_enable);
    LD_HASH_RENDER_FIELD(stencil_two_sided);
    LD_HASH_RENDER_FIELD(s_func);
    LD_HASH_RENDER_FIELD(s_func_mask);
    LD_HASH_RENDER_FIELD(s_write_mask);
    LD_HASH_RENDER_FIELD(s_fail);
    LD_HASH_RENDER_FIELD(s_zfail);
    LD_HASH_RENDER_FIELD(s_zpass);
    LD_HASH_RENDER_FIELD(bs_func);
    LD_HASH_RENDER_FIELD(bs_fail);
    LD_HASH_RENDER_FIELD(bs_zfail);
    LD_HASH_RENDER_FIELD(bs_zpass);
    LD_HASH_RENDER_FIELD(poly_offset_fill);
    LD_HASH_RENDER_FIELD(poly_offset_scale);
    /* poly_offset_bias deliberately absent: applied per draw in the VS
     * viewport offset (xf[6]), see the draw */
#undef LD_HASH_RENDER_FIELD
    return hash;
}

/*
 * PSO identity includes render state, but D3DCompile only consumes the final
 * generated shader text.  Keep those cache boundaries separate: one exact
 * HLSL program is compiled once and its bytecode is reused by every PSO state
 * variant that references it.  The source bytes are retained for collision-
 * safe equality; the 64-bit hash is only an index accelerator.
 */
static ID3DBlob* shader_blob_cache_find(
    shader_blob_cache_t* cache, const char* source, u32 source_length,
    u64 hash, int* post_boundary_hit, int* hash_seen)
{
    if (post_boundary_hit)
        *post_boundary_hit = 0;
    if (hash_seen)
        *hash_seen = 0;
    for (u32 i = 0; i < cache->count; i++) {
        shader_blob_t* entry = &cache->entries[i];
        if (entry->hash != hash)
            continue;
        if (hash_seen)
            *hash_seen = 1;
        if (
            entry->source_length != source_length ||
            memcmp(entry->source, source, source_length) != 0)
            continue;
        if (post_boundary_hit && i >= FORMER_MAX_SHADER_BLOBS)
            *post_boundary_hit = 1;
        entry->blob->lpVtbl->AddRef(entry->blob);
        return entry->blob;
    }
    return NULL;
}

typedef enum {
    SHADER_BLOB_INSERT_SKIPPED = 0,
    SHADER_BLOB_INSERTED,
    SHADER_BLOB_INSERT_FULL
} shader_blob_insert_result;

static shader_blob_insert_result shader_blob_cache_insert(
    shader_blob_cache_t* cache, const char* source, u32 source_length,
    u64 hash, ID3DBlob* blob)
{
    if (!blob)
        return SHADER_BLOB_INSERT_SKIPPED;
    if (cache->count >= MAX_SHADER_BLOBS)
        return SHADER_BLOB_INSERT_FULL;
    char* source_copy = (char*)malloc((size_t)source_length + 1u);
    if (!source_copy)
        return SHADER_BLOB_INSERT_SKIPPED;
    memcpy(source_copy, source, source_length);
    source_copy[source_length] = '\0';
    shader_blob_t* entry = &cache->entries[cache->count++];
    entry->hash = hash;
    entry->source_length = source_length;
    entry->source = source_copy;
    entry->blob = blob;
    blob->lpVtbl->AddRef(blob);
    cache->retained_source_bytes += source_length;
    cache->retained_blob_bytes += blob->lpVtbl->GetBufferSize(blob);
    return SHADER_BLOB_INSERTED;
}

static void shader_blob_cache_insert_accounted(
    shader_blob_cache_t* cache, const char* source, u32 source_length,
    u64 hash, ID3DBlob* blob, u32 stage, int hash_seen)
{
    const u32 count_before = cache->count;
    const shader_blob_insert_result result = shader_blob_cache_insert(
        cache, source, source_length, hash, blob);
#if defined(YZ_PERF_PROFILE)
    if (stage == 'V') {
        if (result == SHADER_BLOB_INSERTED) {
            g_ld_profile.total.vs_blob_inserts++;
            if (count_before >= FORMER_MAX_SHADER_BLOBS && !hash_seen)
                g_ld_profile.total.vs_post_boundary_distinct++;
        } else if (result == SHADER_BLOB_INSERT_FULL) {
            g_ld_profile.total.vs_blob_full_rejects++;
        }
    } else {
        if (result == SHADER_BLOB_INSERTED) {
            g_ld_profile.total.ps_blob_inserts++;
            if (count_before >= FORMER_MAX_SHADER_BLOBS && !hash_seen)
                g_ld_profile.total.ps_post_boundary_distinct++;
        } else if (result == SHADER_BLOB_INSERT_FULL) {
            g_ld_profile.total.ps_blob_full_rejects++;
        }
    }
#else
    (void)count_before;
    (void)result;
    (void)stage;
    (void)hash_seen;
#endif
}

static void shader_blob_cache_release(shader_blob_cache_t* cache)
{
    for (u32 i = 0; i < cache->count; i++) {
        shader_blob_t* entry = &cache->entries[i];
        if (entry->blob)
            entry->blob->lpVtbl->Release(entry->blob);
        free(entry->source);
    }
    memset(cache, 0, sizeof(*cache));
}

#if defined(YZ_PERF_PROFILE)
static void ld_profile_note_hlsl(
    const char* hlsl, u64 hashes[MAX_SHADER_BLOBS], u32* count, u64* unique)
{
    const size_t length = strlen(hlsl);
    const u64 hash = fnv1a(
        hlsl, (u32)length, 1469598103934665603ull);
    for (u32 i = 0; i < *count; i++)
        if (hashes[i] == hash)
            return;
    if (*count < MAX_SHADER_BLOBS)
        hashes[(*count)++] = hash;
    (*unique)++;
}

static int ld_profile_note_hash(
    u64 hash, u64 hashes[LD_VARIANT_SET_CAPACITY], u32* count)
{
    if (!hash)
        hash = 1;
    u32 slot = (u32)hash & (LD_VARIANT_SET_CAPACITY - 1u);
    for (u32 probe = 0; probe < LD_VARIANT_SET_CAPACITY; ++probe) {
        if (hashes[slot] == hash)
            return 0;
        if (hashes[slot] == 0) {
            hashes[slot] = hash;
            (*count)++;
            return 1;
        }
        slot = (slot + 1u) & (LD_VARIANT_SET_CAPACITY - 1u);
    }
    g_ld_profile.ps_variant_tracking_overflow++;
    return 0;
}

static void ld_profile_note_fp_variants(
    const u8* fp_uc, u32 fp_size, u32 fp_ctrl_key, u32 cube_mask,
    const render_state_t* state)
{
    const u64 seed = 1469598103934665603ull;
    u64 raw_constant = fnv1a(fp_uc, fp_size, seed);
    u64 raw_exact = raw_constant;
    u64 canonical = rsx_fp_structural_hash(fp_uc, fp_size, seed);
    if (!canonical)
        return;

    const u32 alpha_enable = state->alpha_test_enable ? 1u : 0u;
    const u32 alpha_func = alpha_enable ? state->alpha_func : 0u;
#define LD_HASH_FP_SOURCE_FIELD(value) do { \
    const u32 _field = (value); \
    raw_exact = fnv1a(&_field, sizeof(_field), raw_exact); \
    raw_constant = fnv1a( \
        &_field, sizeof(_field), raw_constant); \
    canonical = fnv1a(&_field, sizeof(_field), canonical); \
} while (0)
    LD_HASH_FP_SOURCE_FIELD(fp_ctrl_key);
    LD_HASH_FP_SOURCE_FIELD(cube_mask);
    LD_HASH_FP_SOURCE_FIELD(alpha_enable);
    LD_HASH_FP_SOURCE_FIELD(alpha_func);
#undef LD_HASH_FP_SOURCE_FIELD

    float alpha_ref = 0.0f;
    if (alpha_enable && alpha_func != 0x0207u) {
        alpha_ref = rsx_fp_alpha_ref(
            state->alpha_ref_raw, state->alpha_ref_format);
        raw_exact = fnv1a(
            &alpha_ref, sizeof(alpha_ref), raw_exact);
    }

    const int new_raw_constant = ld_profile_note_hash(
        raw_constant, g_ld_profile.ps_raw_constant_hashes,
        &g_ld_profile.n_ps_raw_constant_hashes);
    const int new_raw_exact = ld_profile_note_hash(
        raw_exact, g_ld_profile.ps_raw_exact_hashes,
        &g_ld_profile.n_ps_raw_exact_hashes);
    if (new_raw_constant || new_raw_exact) {
        u64 literal = rsx_fp_literal_source_hash(
            fp_uc, fp_size, seed);
        literal = fnv1a(
            &fp_ctrl_key, sizeof(fp_ctrl_key), literal);
        literal = fnv1a(
            &cube_mask, sizeof(cube_mask), literal);
        literal = fnv1a(
            &alpha_enable, sizeof(alpha_enable), literal);
        literal = fnv1a(
            &alpha_func, sizeof(alpha_func), literal);
        if (new_raw_constant)
            ld_profile_note_hash(
                literal,
                g_ld_profile.ps_constant_specialized_hashes,
                &g_ld_profile.n_ps_constant_specialized_hashes);
        if (new_raw_exact) {
            u64 exact_source = literal;
            if (alpha_enable && alpha_func != 0x0207u) {
                char alpha_literal[64];
                const int length = snprintf(
                    alpha_literal, sizeof(alpha_literal), "%.9g",
                    (double)alpha_ref);
                if (length > 0 &&
                    (u32)length < sizeof(alpha_literal))
                    exact_source = fnv1a(
                        alpha_literal, (u32)length, exact_source);
            }
            ld_profile_note_hash(
                exact_source, g_ld_profile.ps_exact_source_hashes,
                &g_ld_profile.n_ps_exact_source_hashes);
        }
    }
    ld_profile_note_hash(
        canonical, g_ld_profile.ps_canonical_hashes,
        &g_ld_profile.n_ps_canonical_hashes);
}
#endif

static u32 vp_txl_unit_mask(const u8* ucode, u32 instrs)
{
    u32 mask = 0;
    for (u32 i = 0; i < instrs; i++) {
        const u8* p = ucode + i * 16;
        const u32 d1 = (u32)p[4] | ((u32)p[5] << 8) |
                       ((u32)p[6] << 16) | ((u32)p[7] << 24);
        if (((d1 >> 22) & 0x1Fu) != 0x19u) continue;
        const u32 d2 = (u32)p[8] | ((u32)p[9] << 8) |
                       ((u32)p[10] << 16) | ((u32)p[11] << 24);
        mask |= 1u << ((d2 >> 8) & 3u);
    }
    return mask;
}

/* GOWA (23/10): pipeline library on disk (scratch\rsx_pso_lib_v<N>.bin). Each
 * CreateGraphicsPipelineState took up to 42-62 ms in the driver, on the RSX
 * thread -- 32-253 new PSOs per 10 s in the heavy part of the phase-1 walk,
 * the frames of 33-65 ms that kept the average under 60 there (the shader
 * disk cache above skips only D3DCompile). A PSO is named by a hash of its
 * whole description (VS/PS bytecode, states, input layout, formats); from the
 * second run on it comes ready out of the library (LoadGraphicsPipeline, which
 * also checks the description). New ones are stored and the library is
 * written back after 3 s without a new PSO (temp file + rename).
 * GOWA_PSO_LIB=0 turns it off. */
#define GOWA_PLIB_VERSION 1u
static ID3D12PipelineLibrary* s_plib;
static void* s_plib_blob;        /* must outlive the library */
static int s_plib_state = -1;    /* -1 not started, 0 off, 1 on */
static int s_plib_dirty;
static LARGE_INTEGER s_plib_last;
static char s_plib_path[MAX_PATH];
static void gowa_plib_init(void)
{
    s_plib_state = 0;
    const char* e = getenv("GOWA_PSO_LIB");
    if (e && *e == '0') return;
    ID3D12Device1* d1 = NULL;
    if (FAILED(g.dev->lpVtbl->QueryInterface(g.dev, &IID_ID3D12Device1, (void**)&d1)) || !d1) return;
    snprintf(s_plib_path, sizeof s_plib_path, "%s\\rsx_pso_lib_v%u_%u.bin", gowa_cache_base(),
             SHADER_DISK_CACHE_VERSION, GOWA_PLIB_VERSION);
    SIZE_T n = 0;
    FILE* f = fopen(s_plib_path, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        const long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz > 0 && (s_plib_blob = malloc((size_t)sz)) != NULL && fread(s_plib_blob, 1, (size_t)sz, f) == (size_t)sz)
            n = (SIZE_T)sz;
        fclose(f);
    }
    HRESULT hr = n ? d1->lpVtbl->CreatePipelineLibrary(d1, s_plib_blob, n, &IID_ID3D12PipelineLibrary, (void**)&s_plib)
                   : E_FAIL;
    const int loaded = SUCCEEDED(hr) && s_plib;
    if (!loaded) {
        s_plib = NULL;
        hr = d1->lpVtbl->CreatePipelineLibrary(d1, NULL, 0, &IID_ID3D12PipelineLibrary, (void**)&s_plib);
    }
    d1->lpVtbl->Release(d1);
    if (FAILED(hr) || !s_plib) {
        fprintf(stderr, "[pso-lib] indisponivel hr=0x%08lX\n", (unsigned long)hr);
        s_plib = NULL;
        return;
    }
    s_plib_state = 1;
    fprintf(stderr, "[pso-lib] %s '%s' (%llu bytes)\n", loaded ? "carregada" : "nova", s_plib_path,
            (unsigned long long)(loaded ? n : 0));
}
static u64 gowa_plib_h(u64 h, const void* p, size_t n)
{
    const u8* b = (const u8*)p;
    for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ull; }
    return h;
}
static void gowa_plib_name(const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pd, WCHAR name[40])
{
    u64 h1 = 1469598103934665603ull, h2 = 0x84222325CBF29CE4ull;
    h1 = gowa_plib_h(h1, pd->VS.pShaderBytecode, pd->VS.BytecodeLength);
    h2 = gowa_plib_h(h2, pd->PS.pShaderBytecode, pd->PS.BytecodeLength);
    for (UINT i = 0; i < pd->InputLayout.NumElements; i++) {
        const D3D12_INPUT_ELEMENT_DESC* el = &pd->InputLayout.pInputElementDescs[i];
        h1 = gowa_plib_h(h1, el->SemanticName, strlen(el->SemanticName));
        h1 = gowa_plib_h(h1, &el->SemanticIndex, sizeof(*el) - offsetof(D3D12_INPUT_ELEMENT_DESC, SemanticIndex));
    }
    h1 = gowa_plib_h(h1, &pd->InputLayout.NumElements, sizeof pd->InputLayout.NumElements);
    h2 = gowa_plib_h(h2, &pd->BlendState, sizeof pd->BlendState);
    h2 = gowa_plib_h(h2, &pd->SampleMask, sizeof pd->SampleMask);
    h2 = gowa_plib_h(h2, &pd->RasterizerState, sizeof pd->RasterizerState);
    h2 = gowa_plib_h(h2, &pd->DepthStencilState, sizeof pd->DepthStencilState);
    h2 = gowa_plib_h(h2, &pd->IBStripCutValue, sizeof pd->IBStripCutValue);
    h2 = gowa_plib_h(h2, &pd->PrimitiveTopologyType, sizeof pd->PrimitiveTopologyType);
    h2 = gowa_plib_h(h2, &pd->NumRenderTargets, sizeof pd->NumRenderTargets);
    h2 = gowa_plib_h(h2, pd->RTVFormats, sizeof pd->RTVFormats);
    h2 = gowa_plib_h(h2, &pd->DSVFormat, sizeof pd->DSVFormat);
    h2 = gowa_plib_h(h2, &pd->SampleDesc, sizeof pd->SampleDesc);
    h2 = gowa_plib_h(h2, &pd->Flags, sizeof pd->Flags);
    swprintf(name, 40, L"%016llX%016llX", (unsigned long long)h1, (unsigned long long)h2);
}
/* write the library back once no new PSO came for 3 s (RSX thread) */
static void gowa_plib_tick(void)
{
    if (!s_plib_dirty) return;
    LARGE_INTEGER now, f;
    QueryPerformanceCounter(&now);
    QueryPerformanceFrequency(&f);
    if (now.QuadPart - s_plib_last.QuadPart < 3 * f.QuadPart) return;
    s_plib_dirty = 0;
    const SIZE_T n = s_plib->lpVtbl->GetSerializedSize(s_plib);
    void* buf = n ? malloc(n) : NULL;
    if (buf && SUCCEEDED(s_plib->lpVtbl->Serialize(s_plib, buf, n))) {
        char tmp[MAX_PATH + 8];
        snprintf(tmp, sizeof tmp, "%s.tmp", s_plib_path);
        FILE* fo = fopen(tmp, "wb");
        if (fo) {
            const size_t w = fwrite(buf, 1, n, fo);
            fclose(fo);
            if (w == n && MoveFileExA(tmp, s_plib_path, MOVEFILE_REPLACE_EXISTING))
                fprintf(stderr, "[pso-lib] gravada (%llu bytes)\n", (unsigned long long)n);
        }
    }
    free(buf);
}

/* GOWA (27/10, report 80 -- "congela ao abrir uma porta"): compilação dos shaders fora da
 * thread de desenho. Entrar numa área nova pedia dezenas de D3DCompile seguidos no meio do
 * quadro (~40-75 ms cada; 66 num quadro só = 5,5 s parado no i5-11400F sem cache). Agora um
 * shader que não está no cache (memória ou disco) vai para a fila de threads de trabalho e o
 * desenho que precisa dele é pulado (o PSO não entra no cache: o próximo desenho com a mesma
 * chave tenta de novo) até a compilação terminar -- o jogo segue, e o objeto aparece um ou
 * dois quadros depois. O DXBC é o mesmo da compilação na hora (mesmas opções) e vai para o
 * cache de disco como antes. GOWA_SHADER_ASYNC=0 volta a compilar na hora;
 * GOWA_SHADER_ASYNC=<n> = threads (padrão: 1 por 4 lógicas, de 2 a 6). */
typedef struct ld_cjob {
    struct ld_cjob* next;      /* fila */
    struct ld_cjob* tnext;     /* tabela */
    char stage; u32 len; u64 hash;   /* stage 'S' = PSO: hash da descrição */
    char* src;
    volatile LONG done;
    HRESULT hr; ID3DBlob* blob; ID3DBlob* err;
    /* PSO: a descrição copiada, o layout e os blobs seguros pelo trabalho */
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pd; D3D12_INPUT_ELEMENT_DESC il[16];
    ID3DBlob *vs, *ps; ID3D12PipelineState* pso;
} ld_cjob;
static SRWLOCK s_cj_lock = SRWLOCK_INIT;
static CONDITION_VARIABLE s_cj_cv = CONDITION_VARIABLE_INIT;
static ld_cjob *s_cj_head, *s_cj_tail, *s_cj_tab;
static volatile LONG s_cj_pend;
static int s_cj_on = -1;
static int g_ld_pso_pendente;
static DWORD WINAPI ld_cjob_worker(LPVOID a)
{
    (void)a;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    for (;;) {
        AcquireSRWLockExclusive(&s_cj_lock);
        while (!s_cj_head) SleepConditionVariableSRW(&s_cj_cv, &s_cj_lock, INFINITE, 0);
        ld_cjob* j = s_cj_head;
        s_cj_head = j->next;
        if (!s_cj_head) s_cj_tail = NULL;
        ReleaseSRWLockExclusive(&s_cj_lock);
        if (j->stage == 'S')
            j->hr = g.dev->lpVtbl->CreateGraphicsPipelineState(g.dev, &j->pd, &IID_ID3D12PipelineState, (void**)&j->pso);
        else
        j->hr = D3DCompile(j->src, j->len, j->stage == 'V' ? "xvs" : "xps", NULL, NULL, "main",
                           j->stage == 'V' ? "vs_5_0" : "ps_5_0", 0, 0, &j->blob, &j->err);
        InterlockedExchange(&j->done, 1);
    }
    return 0;
}
static int ld_cjob_on(void)
{
    if (s_cj_on < 0) {
        const char* e = getenv("GOWA_SHADER_ASYNC");
        int n = e ? atoi(e) : -1;
        if (e && *e == '0') n = 0;
        if (n < 0) { SYSTEM_INFO si; GetSystemInfo(&si); n = (int)si.dwNumberOfProcessors / 4; if (n < 2) n = 2; if (n > 6) n = 6; }
        for (int i = 0; i < n; i++) CreateThread(NULL, 0, ld_cjob_worker, NULL, 0, NULL);
        s_cj_on = n > 0;
        fprintf(stderr, "[shader-async] %d threads de compilação\n", n);
    }
    return s_cj_on;
}
/* O shader pronto (*hr = resultado do D3DCompile, como antes) ou NULL com *hr = E_PENDING
 * (posto na fila agora ou ainda compilando). */
static ID3DBlob* ld_cjob_take(char stage, const char* src, u32 len, u64 hash, HRESULT* hr, ID3DBlob** err)
{
    ID3DBlob* blob = NULL;
    *hr = E_PENDING;
    AcquireSRWLockExclusive(&s_cj_lock);
    ld_cjob** pp = &s_cj_tab;
    while (*pp && !((*pp)->stage == stage && (*pp)->hash == hash && (*pp)->len == len)) pp = &(*pp)->tnext;
    ld_cjob* j = *pp;
    if (!j) {
        j = (ld_cjob*)calloc(1, sizeof *j);
        if (j) j->src = (char*)malloc(len ? len : 1);
        if (!j || !j->src) { free(j); ReleaseSRWLockExclusive(&s_cj_lock); *hr = E_OUTOFMEMORY; return NULL; }
        memcpy(j->src, src, len);
        j->stage = stage; j->len = len; j->hash = hash;
        j->tnext = s_cj_tab; s_cj_tab = j;
        if (s_cj_tail) s_cj_tail->next = j; else s_cj_head = j;
        s_cj_tail = j;
        InterlockedIncrement(&s_cj_pend);
        WakeConditionVariable(&s_cj_cv);
    } else if (j->done) {
        *pp = j->tnext;
        *hr = j->hr; blob = j->blob; *err = j->err;
        free(j->src); free(j);
        InterlockedDecrement(&s_cj_pend);
    }
    ReleaseSRWLockExclusive(&s_cj_lock);
    return blob;
}
/* O PSO pronto (*hr do CreateGraphicsPipelineState) ou NULL com *hr = E_PENDING. Chave: a
 * descrição sem os ponteiros, o layout e o DXBC dos dois estágios. */
static ID3D12PipelineState* ld_pjob_take(const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pd, ID3DBlob* vs, ID3DBlob* ps, HRESULT* hr)
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC k = *pd;
    k.pRootSignature = NULL; k.VS.pShaderBytecode = k.PS.pShaderBytecode = NULL; k.InputLayout.pInputElementDescs = NULL;
    u64 h = fnv1a(&k, sizeof k, 1469598103934665603ull);
    for (u32 i = 0; i < pd->InputLayout.NumElements && i < 16; i++) {
        D3D12_INPUT_ELEMENT_DESC e = pd->InputLayout.pInputElementDescs[i]; e.SemanticName = NULL;
        h = fnv1a(&e, sizeof e, h);
    }
    h = fnv1a(pd->VS.pShaderBytecode, (u32)pd->VS.BytecodeLength, h);
    h = fnv1a(pd->PS.pShaderBytecode, (u32)pd->PS.BytecodeLength, h);
    ID3D12PipelineState* pso = NULL;
    *hr = E_PENDING;
    AcquireSRWLockExclusive(&s_cj_lock);
    ld_cjob** pp = &s_cj_tab;
    while (*pp && !((*pp)->stage == 'S' && (*pp)->hash == h)) pp = &(*pp)->tnext;
    ld_cjob* j = *pp;
    if (!j) {
        j = (ld_cjob*)calloc(1, sizeof *j);
        if (!j || pd->InputLayout.NumElements > 16) { free(j); ReleaseSRWLockExclusive(&s_cj_lock); *hr = E_OUTOFMEMORY; return NULL; }
        j->stage = 'S'; j->hash = h; j->pd = *pd;
        for (u32 i = 0; i < pd->InputLayout.NumElements; i++) j->il[i] = pd->InputLayout.pInputElementDescs[i];
        j->pd.InputLayout.pInputElementDescs = pd->InputLayout.NumElements ? j->il : NULL;
        vs->lpVtbl->AddRef(vs); ps->lpVtbl->AddRef(ps); j->vs = vs; j->ps = ps;
        j->tnext = s_cj_tab; s_cj_tab = j;
        if (s_cj_tail) s_cj_tail->next = j; else s_cj_head = j;
        s_cj_tail = j;
        InterlockedIncrement(&s_cj_pend);
        WakeConditionVariable(&s_cj_cv);
    } else if (j->done) {
        *pp = j->tnext;
        *hr = j->hr; pso = j->pso;
        j->vs->lpVtbl->Release(j->vs); j->ps->lpVtbl->Release(j->ps);
        free(j);
        InterlockedDecrement(&s_cj_pend);
    }
    ReleaseSRWLockExclusive(&s_cj_lock);
    return pso;
}
static ID3D12PipelineState* build_pso(
    const char* vs_hlsl, const char* ps_hlsl, const render_state_t* rs,
    const rsx_vertex_layout_plan* masked_layout, int packed_offsets)
{
    ID3DBlob *vs = NULL, *ps = NULL, *err = NULL;
    static u32 compile_fail_logs = 0;
    static u32 create_fail_logs = 0;
    const u32 vs_length = (u32)strlen(vs_hlsl);
    const u32 ps_length = (u32)strlen(ps_hlsl);
    const u64 vs_hash = fnv1a(
        vs_hlsl, vs_length, 1469598103934665603ull);
    const u64 ps_hash = fnv1a(
        ps_hlsl, ps_length, 1469598103934665603ull);
#if defined(YZ_PERF_PROFILE)
    ld_profile_note_hlsl(
        vs_hlsl, g_ld_profile.vs_hashes, &g_ld_profile.n_vs_hashes,
        &g_ld_profile.total.vs_unique);
    ld_profile_note_hlsl(
        ps_hlsl, g_ld_profile.ps_hashes, &g_ld_profile.n_ps_hashes,
        &g_ld_profile.total.ps_unique);
    g_ld_profile.total.vs_blob_lookups++;
    const LONGLONG vs_lookup_begin = ld_profile_qpc();
#endif
    int vs_post_boundary_hit = 0;
    int vs_hash_seen = 0;
    vs = shader_blob_cache_find(
        &g.vs_blobs, vs_hlsl, vs_length, vs_hash,
        &vs_post_boundary_hit, &vs_hash_seen);
#if defined(YZ_PERF_PROFILE)
    g_ld_profile.total.vs_blob_lookup_qpc +=
        (u64)(ld_profile_qpc() - vs_lookup_begin);
#endif
    HRESULT vs_hr = S_OK;
    if (vs) {
#if defined(YZ_PERF_PROFILE)
        g_ld_profile.total.vs_blob_hits++;
        if (vs_post_boundary_hit)
            g_ld_profile.total.vs_post_boundary_repeats++;
#endif
    } else {
#if defined(YZ_PERF_PROFILE)
        g_ld_profile.total.vs_blob_misses++;
#endif
        vs = shader_disk_cache_load('V', vs_hlsl, vs_length, vs_hash);
        if (vs) {
            shader_blob_cache_insert_accounted(
                &g.vs_blobs, vs_hlsl, vs_length, vs_hash, vs,
                'V', vs_hash_seen);
        } else {
#if defined(YZ_PERF_PROFILE)
            g_ld_profile.total.vs_compile_calls++;
            const LONGLONG compile_begin = ld_profile_qpc();
#endif
            { LARGE_INTEGER e0, e1; QueryPerformanceCounter(&e0);
            if (ld_cjob_on()) vs = ld_cjob_take('V', vs_hlsl, vs_length, vs_hash, &vs_hr, &err);
            else
            vs_hr = D3DCompile(
                vs_hlsl, vs_length, "xvs", NULL, NULL, "main",
                "vs_5_0", 0, 0, &vs, &err);
              QueryPerformanceCounter(&e1); g_eng_hlsl_qpc += e1.QuadPart - e0.QuadPart; g_eng_hlsl_n++; }
#if defined(YZ_PERF_PROFILE)
            g_ld_profile.total.vs_compile_qpc +=
                (u64)(ld_profile_qpc() - compile_begin);
#endif
            if (SUCCEEDED(vs_hr)) {
                shader_disk_cache_store(
                    'V', vs_hlsl, vs_length, vs_hash, vs);
                shader_blob_cache_insert_accounted(
                    &g.vs_blobs, vs_hlsl, vs_length, vs_hash, vs,
                    'V', vs_hash_seen);
            }
        }
    }
    const int vs_pendente = vs_hr == E_PENDING;
    if (vs_pendente) vs_hr = S_OK;
    if (FAILED(vs_hr)) {
        if (compile_fail_logs++ < 32) {
            const char* msg = err ? (const char*)err->lpVtbl->GetBufferPointer(err)
                                  : "no compiler diagnostic";
            fprintf(stderr, "[pso-fail] vertex compile: %.768s\n", msg);
        }
        if (err) err->lpVtbl->Release(err); return NULL;
    }
    if (err) {
        err->lpVtbl->Release(err);
        err = NULL;
    }
#if defined(YZ_PERF_PROFILE)
    g_ld_profile.total.ps_blob_lookups++;
    const LONGLONG ps_lookup_begin = ld_profile_qpc();
#endif
    int ps_post_boundary_hit = 0;
    int ps_hash_seen = 0;
    ps = shader_blob_cache_find(
        &g.ps_blobs, ps_hlsl, ps_length, ps_hash,
        &ps_post_boundary_hit, &ps_hash_seen);
#if defined(YZ_PERF_PROFILE)
    g_ld_profile.total.ps_blob_lookup_qpc +=
        (u64)(ld_profile_qpc() - ps_lookup_begin);
#endif
    HRESULT ps_hr = S_OK;
    if (ps) {
#if defined(YZ_PERF_PROFILE)
        g_ld_profile.total.ps_blob_hits++;
        if (ps_post_boundary_hit)
            g_ld_profile.total.ps_post_boundary_repeats++;
#endif
    } else {
#if defined(YZ_PERF_PROFILE)
        g_ld_profile.total.ps_blob_misses++;
#endif
        ps = shader_disk_cache_load('P', ps_hlsl, ps_length, ps_hash);
        if (ps) {
            shader_blob_cache_insert_accounted(
                &g.ps_blobs, ps_hlsl, ps_length, ps_hash, ps,
                'P', ps_hash_seen);
        } else {
#if defined(YZ_PERF_PROFILE)
            g_ld_profile.total.ps_compile_calls++;
            const LONGLONG compile_begin = ld_profile_qpc();
#endif
            { LARGE_INTEGER e0, e1; QueryPerformanceCounter(&e0);
            if (ld_cjob_on()) ps = ld_cjob_take('P', ps_hlsl, ps_length, ps_hash, &ps_hr, &err);
            else
            ps_hr = D3DCompile(
                ps_hlsl, ps_length, "xps", NULL, NULL, "main",
                "ps_5_0", 0, 0, &ps, &err);
              QueryPerformanceCounter(&e1); g_eng_hlsl_qpc += e1.QuadPart - e0.QuadPart; g_eng_hlsl_n++; }
#if defined(YZ_PERF_PROFILE)
            g_ld_profile.total.ps_compile_qpc +=
                (u64)(ld_profile_qpc() - compile_begin);
#endif
            if (SUCCEEDED(ps_hr)) {
                shader_disk_cache_store(
                    'P', ps_hlsl, ps_length, ps_hash, ps);
                shader_blob_cache_insert_accounted(
                    &g.ps_blobs, ps_hlsl, ps_length, ps_hash, ps,
                    'P', ps_hash_seen);
            }
        }
    }
    if (ps_hr == E_PENDING || vs_pendente) {   /* na fila: este desenho fica de fora */
        if (ps) ps->lpVtbl->Release(ps);
        if (vs) vs->lpVtbl->Release(vs);
        if (err) err->lpVtbl->Release(err);
        g_ld_pso_pendente = 1;
        return NULL;
    }
    if (FAILED(ps_hr)) {
        if (compile_fail_logs++ < 32) {
            const char* msg = err ? (const char*)err->lpVtbl->GetBufferPointer(err)
                                  : "no compiler diagnostic";
            fprintf(stderr, "[pso-fail] pixel compile: %.768s\n", msg);
        }
        if (err) err->lpVtbl->Release(err);
        vs->lpVtbl->Release(vs); return NULL;
    }
    if (err) {
        err->lpVtbl->Release(err);
        err = NULL;
    }
    D3D12_INPUT_ELEMENT_DESC il[16];
    const u32 input_count =
        masked_layout ? masked_layout->count : RSX_DSP_NUM_VERTEX_ATTR;
    for (u32 slot = 0; slot < input_count; slot++) {
        const u32 attr =
            masked_layout ? masked_layout->attrs[slot] : slot;
        D3D12_INPUT_ELEMENT_DESC e = {
            "ATTR", attr, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
            masked_layout && !packed_offsets ? attr * 16u : slot * 16u,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
        };
        if (g_ld_vs_active && masked_layout && slot < g_ld_vs_n) {   /* GOWA (24/10): own buffer per slot */
            e.Format = (DXGI_FORMAT)g_ld_vs[slot].fmt;
            e.InputSlot = slot;
            e.AlignedByteOffset = 0;
        }
        il[slot] = e;
    }
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pd = {0};
    pd.pRootSignature = g.rootsig_x;
    pd.VS.pShaderBytecode = vs->lpVtbl->GetBufferPointer(vs);
    pd.VS.BytecodeLength = vs->lpVtbl->GetBufferSize(vs);
    pd.PS.pShaderBytecode = ps->lpVtbl->GetBufferPointer(ps);
    pd.PS.BytecodeLength = ps->lpVtbl->GetBufferSize(ps);
    pd.InputLayout.pInputElementDescs = input_count ? il : NULL;
    pd.InputLayout.NumElements = input_count;
    apply_render_state(&pd, rs);
    pd.SampleMask = 0xFFFFFFFFu;
    pd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pd.NumRenderTargets = rs->rt_count ? rs->rt_count : 1;
    for (u32 r = 0; r < pd.NumRenderTargets; r++)
        pd.RTVFormats[r] = (DXGI_FORMAT)rs->rt_dxgi;
    pd.SampleDesc.Count = 1;
    ID3D12PipelineState* pso = NULL;
#if defined(YZ_PERF_PROFILE)
    g_ld_profile.total.create_pso_calls++;
    const LONGLONG create_begin = ld_profile_qpc();
#endif
    /* GOWA_PSO_LOG=1 (23/10): every PSO created, with how long the driver took
     * and when -- the hitches of the phase-1 walk are frames where the RSX
     * thread is busy for 40-65 ms. */
    static int s_psolog = -1;
    if (s_psolog < 0) s_psolog = getenv("GOWA_PSO_LOG") ? 1 : 0;
    LARGE_INTEGER pso_t0; if (s_psolog) QueryPerformanceCounter(&pso_t0);
    if (s_plib_state < 0) gowa_plib_init();
    WCHAR plib_name[40];
    HRESULT hr = E_FAIL;
    int from_lib = 0;
    if (s_plib_state == 1) {
        gowa_plib_name(&pd, plib_name);
        hr = s_plib->lpVtbl->LoadGraphicsPipeline(s_plib, plib_name, &pd, &IID_ID3D12PipelineState, (void**)&pso);
        from_lib = SUCCEEDED(hr) && pso;
        if (!from_lib) pso = NULL;
    }
    if (!from_lib && ld_cjob_on()) {   /* GOWA_SHADER_ASYNC: o PSO também fora da thread de desenho */
        pso = ld_pjob_take(&pd, vs, ps, &hr);
        if (hr == E_PENDING) {
            vs->lpVtbl->Release(vs); ps->lpVtbl->Release(ps);
            g_ld_pso_pendente = 1;
            return NULL;
        }
        if (SUCCEEDED(hr) && pso && s_plib_state == 1 &&
            SUCCEEDED(s_plib->lpVtbl->StorePipeline(s_plib, plib_name, pso))) {
            s_plib_dirty = 1;
            QueryPerformanceCounter(&s_plib_last);
        }
    } else if (!from_lib) {
        LARGE_INTEGER e0, e1; QueryPerformanceCounter(&e0);
        hr = g.dev->lpVtbl->CreateGraphicsPipelineState(
            g.dev, &pd, &IID_ID3D12PipelineState, (void**)&pso);
        QueryPerformanceCounter(&e1); g_eng_pso_qpc += e1.QuadPart - e0.QuadPart; g_eng_pso_n++;
        if (SUCCEEDED(hr) && pso && s_plib_state == 1 &&
            SUCCEEDED(s_plib->lpVtbl->StorePipeline(s_plib, plib_name, pso))) {
            s_plib_dirty = 1;
            QueryPerformanceCounter(&s_plib_last);
        }
    }
    if (s_psolog) {
        LARGE_INTEGER t1, f; QueryPerformanceCounter(&t1); QueryPerformanceFrequency(&f);
        static volatile long s_npso;
        extern unsigned long long ps3_ms_now(void);
        fprintf(stderr, "[pso-new] n=%ld t=%llu ms criacao=%.2f ms%s\n", InterlockedIncrement(&s_npso),
                ps3_ms_now(), (double)(t1.QuadPart - pso_t0.QuadPart) * 1000.0 / (double)f.QuadPart,
                from_lib ? " (biblioteca)" : "");
    }
#if defined(YZ_PERF_PROFILE)
    g_ld_profile.total.create_pso_qpc +=
        (u64)(ld_profile_qpc() - create_begin);
#endif
    if (FAILED(hr)) {
        const HRESULT removed = g.dev->lpVtbl->GetDeviceRemovedReason(g.dev);
        if (create_fail_logs++ < 32)
            fprintf(stderr,
                    "[pso-fail] create hr=0x%08lX removed=0x%08lX "
                    "depth{test=%u write=%u func=%u} blend=%u "
                    "cull{enable=%u face=%u}\n",
                    (unsigned long)hr, (unsigned long)removed,
                    rs->depth_test, rs->depth_write, rs->depth_func,
                    rs->blend_enable, rs->cull_enable, rs->cull_face);
        if (removed != S_OK)
            ld_dump_dred("CreateGraphicsPipelineState", hr);
    }
    vs->lpVtbl->Release(vs); ps->lpVtbl->Release(ps);
    return SUCCEEDED(hr) ? pso : NULL;
}

static int ld_current_vertex_layout(rsx_vertex_layout_plan* layout)
{
    const u32 start = rsx_dsp_vp_start(&g.rsx);
    if (start >= RSX_DSP_VP_INSTR) {
        ld_layout_plan_get(0xFFFFu, layout);
        return 0;
    }
    const u8* vp_uc = (const u8*)(g.rsx.vp + start * 4);
    const u32 vp_instrs = rsx_vp_program_size_instrs(
        vp_uc, (RSX_DSP_VP_INSTR - start) * 16);
    rsx_vp_input_analysis analysis = {0xFFFFu, 0};
    if (!vp_instrs ||
        rsx_vp_analyze_inputs(
            vp_uc, vp_instrs * 16, &analysis) != (int)vp_instrs ||
        !analysis.exact)
        analysis.input_mask = 0xFFFFu;
    ld_layout_plan_get(analysis.input_mask, layout);
    return vp_instrs != 0;
}

/* LD_PSO_DBG=1: name the first bail-out of each kind in get_pso(). Every exit
 * there returns NULL silently and the caller only counts drop{pso=N}, so a
 * title whose draws ALL die in this function has no way to say which gate
 * rejected them. One line per distinct reason, so a stuck title prints a
 * handful of lines rather than one per draw. */
static void ld_pso_bail(int* said, const char* reason)
{
    static int dbg = -1;
    if (dbg < 0) dbg = getenv("LD_PSO_DBG") ? 1 : 0;
    if (!dbg || *said) return;
    *said = 1;
    fprintf(stderr, "[pso-bail] %s\n", reason);
    fflush(stderr);
}
#define LD_PSO_BAIL(reason) \
    do { static int _said_ = 0; ld_pso_bail(&_said_, (reason)); } while (0)

/* GOWA: PSO cache index (see get_pso) */
#define PSO_IDX_N (MAX_PSOS * 2u)
static u32 s_pso_idx[PSO_IDX_N];
static u32 s_pso_idx_n;   /* g.psos entries already indexed */
static u32 pso_idx_slot(u64 key) { key ^= key >> 31; key *= 0x9E3779B97F4A7C15ull; return (u32)(key >> 40) & (PSO_IDX_N - 1u); }
static void pso_idx_put(u64 key, u32 pos)
{
    for (u32 h = pso_idx_slot(key), probe = 0; probe < PSO_IDX_N; probe++, h = (h + 1u) & (PSO_IDX_N - 1u))
        if (!s_pso_idx[h]) { s_pso_idx[h] = pos + 1u; return; }
}
/* GOWA (26/10): the cache used to stop at MAX_PSOS -- every PSO after the 8192nd
 * distinct one returned NULL and its draws were dropped (drop{pso=N} climbing from
 * ~16 minutes of play on a 5600X: "shaders stop loading after a while", fixed by
 * restarting). Now a full cache reuses the least recently used entry that no draw
 * of this frame used; its PSO goes through the same retire queue as replaced
 * textures (released after the fence), and a PSO that comes back is rebuilt from
 * the pipeline library. Index slots of a reused position stay behind as stale
 * entries (the lookup compares the key), so the index is rebuilt when they pile
 * up. GOWA_PSO_MAX=<n> lowers the limit (tests the eviction in a short run). */
static u32 s_pso_idx_stale;
static u32 s_pso_cap;
static u64 s_pso_evictions;
static void retire_texture(ID3D12Resource* tex);
static int pso_evict_lru(u64 key, ID3D12PipelineState* pso)
{
    u32 best = MAX_PSOS, best_frame = 0xFFFFFFFFu;
    for (u32 i = 0; i < g.n_psos; i++)
        if (g.psos[i].last_frame < best_frame) { best_frame = g.psos[i].last_frame; best = i; }
    if (best == MAX_PSOS || best_frame >= g_ld_frames) return 0;   /* all used this frame */
    /* ID3D12PipelineState is an IUnknown like ID3D12Resource: the retire queue only Releases */
    retire_texture((ID3D12Resource*)(void*)g.psos[best].pso);
    g.psos[best].key = key;
    g.psos[best].pso = pso;
    g.psos[best].last_frame = g_ld_frames;
    s_pso_evictions++;
    if (++s_pso_idx_stale > PSO_IDX_N / 4u) {
        memset(s_pso_idx, 0, sizeof s_pso_idx);
        for (u32 i = 0; i < g.n_psos; i++) pso_idx_put(g.psos[i].key, i);
        s_pso_idx_stale = 0;
    } else {
        pso_idx_put(key, best);
    }
    if ((s_pso_evictions & (s_pso_evictions - 1u)) == 0)
        fprintf(stderr, "[pso-cache] cheio (%u): %llu despejos LRU\n", s_pso_cap, (unsigned long long)s_pso_evictions);
    return 1;
}

/* GOWA (24/10): the 16 texture units as get_pso decoded them for the draw being
 * recorded (sink_end_impl binds from the same table) */
static rsx_dsp_texture g_ld_tx[RSX_DSP_NUM_TEXTURES];
/* GOWA (24/10): the vertex constant slots the current program reads, set by get_pso */
static u32 g_ld_vpc_lo = 0, g_ld_vpc_hi = RSX_DSP_NUM_CONSTANTS - 1;

static ID3D12PipelineState* get_pso(
    const rsx_vertex_layout_plan* masked_layout, int packed_payload)
{
    gowa_plib_tick();
    memset(&g_ld_current_pso, 0, sizeof(g_ld_current_pso));
    const u32 start = rsx_dsp_vp_start(&g.rsx);
    if (start >= RSX_DSP_VP_INSTR) { LD_PSO_BAIL("no vertex program uploaded (vp start out of range)"); return NULL; }
    const u8* vp_uc = (const u8*)(g.rsx.vp + start * 4);
    const u32 vp_instrs = rsx_vp_program_size_instrs(vp_uc, (RSX_DSP_VP_INSTR - start) * 16);
    if (!vp_instrs) { LD_PSO_BAIL("vertex program has zero instructions"); return NULL; }

    u32 fp_loc = 0;
    const u32 fp_off = rsx_dsp_fragment_program(&g.rsx, &fp_loc);
    const u8* fp_uc = guest_ptr(fp_loc, fp_off, 16);
    if (!fp_uc) { LD_PSO_BAIL("fragment program address does not resolve to guest memory"); return NULL; }
    const u32 fp_size = rsx_fp_program_size(fp_uc, 0x10000);
    if (!fp_size) {
        /* No END-bit instruction in 64 KB: the bytes at (location, offset) are
         * not a fragment program. Almost always the LOCATION is wrong, not the
         * offset -- the same class of bug caner hit in Yakuza with textures --
         * so report the register the guest actually wrote and the head of what
         * we read, which is what says local-vs-main. */
        static int said = 0;
        if (!said && getenv("LD_PSO_DBG")) {
            said = 1;
            fprintf(stderr,
                    "[pso-bail] fragment program size reads as zero: "
                    "reg=0x%08X -> loc=%u off=0x%08X, first words "
                    "%08X %08X %08X %08X\n",
                    rsx_dsp_reg(&g.rsx, 0x08E4 /* M_FP_ACTIVE_PROGRAM */), fp_loc, fp_off,
                    rsx_fp_read_word(fp_uc + 0), rsx_fp_read_word(fp_uc + 4),
                    rsx_fp_read_word(fp_uc + 8), rsx_fp_read_word(fp_uc + 12));
            /* All-zero here has two very different causes: the offset resolved
             * into the wrong memory, or nothing ever wrote the buffer. Scan
             * forward for the first non-zero byte -- a hit a few hundred bytes
             * on means the region IS populated and the offset is wrong; no hit
             * across 64 KB means the producer never ran. */
            {
                const u8* q = guest_ptr(fp_loc, fp_off, 0x10000);
                u32 i = 0;
                if (q) { while (i < 0x10000u && !q[i]) i++; }
                if (!q)
                    fprintf(stderr, "[pso-bail]   (64 KB window not mapped)\n");
                else if (i == 0x10000u)
                    fprintf(stderr, "[pso-bail]   64 KB from here is entirely "
                                    "zero -- nothing ever wrote this buffer\n");
                else
                    fprintf(stderr, "[pso-bail]   first non-zero byte at "
                                    "+0x%X -- the region is populated, the "
                                    "offset is wrong\n", i);
            }
            fflush(stderr);
        }
        return NULL;
    }
    /* re-resolve with the true size to validate the whole program is mapped */
    fp_uc = guest_ptr(fp_loc, fp_off, fp_size);
    if (!fp_uc) { LD_PSO_BAIL("fragment program is not fully mapped in guest memory"); return NULL; }

    /* Fragment output register mode (fp16 h0 vs fp32 r0) is driven by the
     * SHADER_CONTROL word bit 0x40 (same fix as the replay harness — the
     * AUTO heuristic returned stale fp32 scratch for h0-writing materials);
     * fold the deciding bit into the cache key so a program reused under a
     * different export mode gets its own PSO. Kill-switch RSX_FP_CTRL_AUTO=1
     * restores the old heuristic for the A/B. */
    static int ctrl_auto = -1;
    if (ctrl_auto < 0) ctrl_auto = getenv("RSX_FP_CTRL_AUTO") ? 1 : 0;
    const u32 fp_ctrl = ctrl_auto ? RSX_FP_CTRL_AUTO : rsx_dsp_shader_control(&g.rsx);
    /* GOWA (24/10): the 16 texture units decoded once here -- the loops below
     * called rsx_dsp_get_texture (out of line) ~80 times a draw */
    rsx_dsp_texture* const ld_tx = g_ld_tx;
    for (u32 unit = 0; unit < RSX_DSP_NUM_TEXTURES; unit++)
        rsx_dsp_get_texture(&g.rsx, unit, &ld_tx[unit]);
    u32 cube_mask = 0;
    for (u32 unit = 0; unit < RSX_DSP_NUM_TEXTURES; unit++) {
        rsx_dsp_texture texture = ld_tx[unit];
        if (texture.enabled && texture.cubemap)
            cube_mask |= 1u << unit;
        else if (texture.enabled && tex_depth3d(&texture))
            cube_mask |= 1u << (16u + unit);
    }
    const u32 vtex_mask = vertex_texture_mask();
    const u32 txl_mask = vp_txl_unit_mask(vp_uc, vp_instrs);
    if (txl_mask && !(txl_mask & vtex_mask)) {
        g_ld_vtex_missing_for_txl++;
        static u32 warned = 0;
        if (warned++ < 32 && vertex_texture_trace_enabled()) {
            fprintf(stderr,
                    "[vtex] TXL shader has no supported binding txl=0x%X "
                    "bound=0x%X start=%u instrs=%u\n",
                    txl_mask, vtex_mask, start, vp_instrs);
            for (u32 u = 0; u < RSX_DSP_NUM_VERTEX_TEXTURES; u++) {
                if (!((txl_mask >> u) & 1u)) continue;
                const u32 base = 0x0900 + u * 0x20;
                fprintf(stderr,
                        "[vtex] raw u%u off=%08X fmt=%08X wrap=%08X "
                        "ctl0=%08X ctl3=%08X filter=%08X rect=%08X\n",
                        u, rsx_dsp_reg(&g.rsx, base),
                        rsx_dsp_reg(&g.rsx, base + 4),
                        rsx_dsp_reg(&g.rsx, base + 8),
                        rsx_dsp_reg(&g.rsx, base + 12),
                        rsx_dsp_reg(&g.rsx, base + 16),
                        rsx_dsp_reg(&g.rsx, base + 20),
                        rsx_dsp_reg(&g.rsx, base + 24));
            }
        }
    }

    render_state_t rs;
    decode_render_state(&rs);
    if (rsx_fp_collect_constants(
            fp_uc, fp_size, &g.fp_constants) < 0) {
        LD_PSO_BAIL("fragment program constant collection failed");
        return NULL;
    }
    g.fp_alpha_ref = rsx_fp_alpha_ref(
        rs.alpha_ref_raw, rs.alpha_ref_format);
    /* GOWA_FP_UCODE_DUMP=<offset hex>: write that fragment program's ucode
     * (as fetched, fp_size bytes) to out/fp_<offset>.bin once. */
    { static u32 du = 0xFFFFFFFFu; static int done;
      if (du == 0xFFFFFFFFu) { const char* e = getenv("GOWA_FP_UCODE_DUMP"); du = e ? (u32)strtoul(e, 0, 16) : 0; }
      if (du && !done && fp_off == du && fp_uc && fp_size) {
          done = 1; char fn[MAX_PATH];
          snprintf(fn, sizeof fn, "out/fp_%08X.bin", du);
          FILE* f = fopen(fn, "wb");
          if (f) { fwrite(fp_uc, 1, fp_size, f); fclose(f); fprintf(stderr, "[fp-dump] %s (%u bytes)\n", fn, fp_size); }
      } }
    /* GOWA_FPCONST_COUNT=<n>: print the constants of the first draws whose
     * fragment program carries exactly n of them (one program of interest). */
    { static int want = -2; static int shown = 0;
      if (want == -2) { const char* e = getenv("GOWA_FPCONST_COUNT"); want = e ? atoi(e) : -1; }
      const int from = ld_from_env("GOWA_FPCONST_FROM");
      /* GOWA_FPCONST_IF_TEX=<offset hex>: instead, the draws whose texture
       * unit 0 is at that offset (any constant count). */
      static int ftx = -2; static u32 ftx_off;
      if (ftx == -2) { const char* e = getenv("GOWA_FPCONST_IF_TEX"); ftx = e ? 1 : 0; ftx_off = e ? (u32)strtoul(e, 0, 16) : 0; }
      /* GOWA_FPCONST_IF_TARGET=<offset hex>: draws into that colour target
       * (AND the count, when GOWA_FPCONST_COUNT is also set); 24 lines. */
      static int ftg = -2; static u32 ftg_off;
      if (ftg == -2) { const char* e = getenv("GOWA_FPCONST_IF_TARGET"); ftg = e ? 1 : 0; ftg_off = e ? (u32)strtoul(e, 0, 16) : 0; }
      int pick = want > 0 && (int)g.fp_constants.count == want;
      if (ftx) { rsx_dsp_texture t0f; rsx_dsp_get_texture(&g.rsx, 0, &t0f); pick = t0f.enabled && t0f.offset == ftx_off; }
      if (ftg) { rsx_dsp_surface sft; rsx_dsp_get_surface(&g.rsx, &sft);
                 pick = sft.color_offset[0] == ftg_off && (want <= 0 || (int)g.fp_constants.count == want) &&
                        (!ftx || pick); }   /* GOWA: AND the texture filter when both are set */
      if (pick && shown < (ftg ? 24 : 6) && (int)g_ld_frames >= from) {
          shown++;
          fprintf(stderr, "[fpconst] frame %u fp=%u:0x%X count %u:", g_ld_frames, fp_loc, fp_off, g.fp_constants.count);
          for (u32 c = 0; c < g.fp_constants.count; c++)
              fprintf(stderr, " c%u=(%g %g %g %g)", c, ld_c2f(g.fp_constants.values[c][0]), ld_c2f(g.fp_constants.values[c][1]),
                      ld_c2f(g.fp_constants.values[c][2]), ld_c2f(g.fp_constants.values[c][3]));
          fprintf(stderr, "\n");
      } }

    /* GOWA: the vertex program by 8-byte words (fnv1a is one multiply per byte:
     * ~1-2 KB of microcode on every draw) -- the key is only the in-memory PSO
     * cache key */
    u64 key = 1469598103934665603ull;
    for (u32 i = 0; i + 8 <= vp_instrs * 16; i += 8) {
        u64 w; memcpy(&w, vp_uc + i, 8);
        key ^= w * 0x9E3779B97F4A7C15ull; key = (key << 31) | (key >> 33); key *= 0xC2B2AE3D27D4EB4Full;
    }
    key = fnv1a(&vp_instrs, sizeof vp_instrs, key);
    const u64 vp_key_only = key;
    /* GOWA (24/10): the constant slots this vertex program can read
     * (rsx_vp_constant_range), remembered per program hash -- sink_end_impl
     * uploads only that part of the 8 KB bank (it copied all of it on every
     * draw, ~2,900 a frame in chapter 16). GOWA_VPC_RANGE=0: the whole bank. */
    {
        static int on = -1;
        if (on < 0) { const char* e = getenv("GOWA_VPC_RANGE"); on = !(e && *e == '0'); }
        static struct { u64 key; u16 lo, hi; } s_vpc[512];
        const u32 h = (u32)(vp_key_only ^ (vp_key_only >> 29)) & 511u;
        if (!on) { g_ld_vpc_lo = 0; g_ld_vpc_hi = RSX_DSP_NUM_CONSTANTS - 1; }
        else {
            if (s_vpc[h].key != vp_key_only) {
                u32 lo, hi;
                if (!rsx_vp_constant_range(vp_uc, vp_instrs * 16, &lo, &hi)) { lo = 0; hi = RSX_DSP_NUM_CONSTANTS - 1; }
                s_vpc[h].key = vp_key_only; s_vpc[h].lo = (u16)lo; s_vpc[h].hi = (u16)hi;
            }
            g_ld_vpc_lo = s_vpc[h].lo; g_ld_vpc_hi = s_vpc[h].hi;
        }
    }
    if (ld_fp_constants_buffered())
        key = rsx_fp_structural_hash(fp_uc, fp_size, key);
    else
        key = fnv1a(fp_uc, fp_size, key);
    if (!key) {
        LD_PSO_BAIL("shader hash collapsed to zero");
        return NULL;
    }
    const u32 fp_ctrl_key = fp_ctrl & 0x4Eu;   /* 32-bit exports, depth export */
    key = fnv1a(&fp_ctrl_key, sizeof(fp_ctrl_key), key);
    key = fnv1a(&cube_mask, sizeof(cube_mask), key);
    key = fnv1a(&vtex_mask, sizeof(vtex_mask), key);
    /* Texel-addressed (RSX_TEX_FMT_UNNORM) units need their coordinates divided
     * by the texture size in the shader -- D3D12 has no unnormalised addressing
     * mode. The divisor is baked into the HLSL, so the sizes are part of the
     * PSO identity and must be in the key. */
    u32 unnorm_mask = 0;
    u32 unnorm_dim[16][2] = {{0}};
    for (u32 uu = 0; uu < 16; uu++) {
        rsx_dsp_texture ut = ld_tx[uu];
        if (!ut.enabled || !(ut.format & TEX_FMT_UNNORM)) continue;
        if (!ut.width || !ut.height) continue;
        unnorm_mask |= 1u << uu;
        unnorm_dim[uu][0] = ut.width;
        unnorm_dim[uu][1] = ut.height;
        /* A depth texture bound to a zeta's snapshot reads that host
         * resource, which is as large as the largest pass that used the zeta
         * (bounded by the zeta), not as large as the texture. Texel
         * coordinates address the resource, so divide by its size: a texture
         * smaller than the zeta then reads its top-left corner, as it does in
         * VRAM, instead of a stretched copy of the whole depth buffer. */
        if ((ut.format & TEX_FMT_BASE_MASK & ~(u32)TEX_FMT_UNNORM) ==
                TEX_FMT_DEPTH24_D8) {
            for (u32 zi = 0; zi < g.n_zdepths; zi++) {
                const zdepth_t* z = &g.zdepths[zi];
                if (z->location != ut.location || z->offset != ut.offset)
                    continue;
                /* Only when the bind will take the snapshot (it has, or
                 * will copy, a depth-writing pass); a zeta that was only
                 * cleared is read from guest memory at texture size. */
                if (z->snapshot && (z->had_write || z->snapshot_valid) &&
                    z->snapshot_w >= ut.width &&
                    z->snapshot_h >= ut.height) {
                    unnorm_dim[uu][0] = z->snapshot_w;
                    unnorm_dim[uu][1] = z->snapshot_h;
                }
                break;
            }
        }
    }
    key = fnv1a(&unnorm_mask, sizeof(unnorm_mask), key);
    if (unnorm_mask) key = fnv1a(unnorm_dim, sizeof(unnorm_dim), key);
    /* GOWA: the RSX's sRGB pair, as the replay engine (rsx_draw_engine.c)
     * already does it:
     *  - texture gamma (TEX_ADDRESS bits 20-23, per channel): the sampler
     *    decodes sRGB to linear on read, for the 8-bit normalized formats
     *    (rsx_texture_gamma_mask);
     *  - framebuffer sRGB (NV4097_SET_SHADER_PACKER, 0x1FEC): the colour
     *    outputs are encoded linear-to-sRGB on write.
     * The live engine did neither: God of War Ascension's post-processing
     * (bloom, motion blur, depth of field, exposure) reads its scene copies
     * with gamma 0x7 and writes with the packer on. Both are part of the PSO
     * identity. GOWA_TEX_GAMMA=0 turns both off. */
    u8 gamma_masks[16] = {0};
    u32 gamma_any = 0, srgb_output = 0;
    {
        static int tg = -1;
        if (tg < 0) { const char* e = getenv("GOWA_TEX_GAMMA"); tg = !(e && *e == '0'); }
        if (tg) {
            for (u32 uu = 0; uu < 16; uu++) {
                rsx_dsp_texture ut = ld_tx[uu];
                if (ut.enabled) gamma_masks[uu] = rsx_texture_gamma_mask(ut.format, ut.remap, ut.filter, ut.wrap);
            }
            const u32 gu = rsx_fp_texture_gamma_units(fp_uc, fp_size, gamma_masks);
            for (u32 uu = 0; uu < 16; uu++) if (!(gu & (1u << uu))) gamma_masks[uu] = 0;
            gamma_any = gu != 0;
            srgb_output = (u32)rsx_fp_srgb_output_enabled(rsx_dsp_reg(&g.rsx, 0x1FECu), fp_ctrl,
                                                          rsx_dsp_reg(&g.rsx, 0x0208u) & 0x1Fu);
        }
    }
    if (gamma_any) key = fnv1a(gamma_masks, sizeof(gamma_masks), key);
    /* GOWA: EXP_TEX expansion needs the unit formats (rsx_fp_decompiler.c) */
    u32 exp_units = 0;
    {
        static int ex = -1;
        if (ex < 0) { const char* e = getenv("GOWA_EXP_TEX"); ex = !(e && *e == '0'); }
        for (u32 uu = 0; ex && uu < 16; uu++) {
            rsx_dsp_texture ut = ld_tx[uu];
            if (!ut.enabled) continue;
            switch (ut.format & 0x9Fu) {
            case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: case 0x86: case 0x87:
            case 0x88: case 0x8B: case 0x8D: case 0x8E: case 0x8F: case 0x92: case 0x93:
            case 0x97: case 0x98: case 0x9D: case 0x9E:
                exp_units |= 1u << uu; break;
            default: break;
            }
        }
    }
    g_rsx_fp_exp_units = exp_units;
    key = fnv1a(&exp_units, sizeof(exp_units), key);
    /* GOWA: depth-compare ("shadow") units. A depth texture whose TEX_ADDRESS
     * zfunc (bits 28-31) is set returns the comparison, not the depth: the
     * live engine never passed this to the decompiler, so God of War
     * Ascension's screen-space shadow mask (0x1D90000, MIN-blended per light
     * and tile) summed raw depths -- wrong shadows, whole 160x90 tiles dark.
     * RSX compares texel OP reference (RPCS3 reverses the func for the
     * sampler); the helper writes reference OP texel, so the func is mirrored
     * (GOWA_SHADOW_REV=0 keeps it as is). The lights reuse one shadow buffer
     * and declare their window's size: the host snapshot is the largest one,
     * so the coordinates are scaled to that window -- per draw, through
     * fp_texwin in the constant buffer (ld_upload_pixel_constants): the
     * windows change size every frame, a PSO per size compiled hundreds of
     * shaders a frame. GOWA_SHADOW_CMP=0 turns it all off. */
    rsx_fp_shadow_desc shadow[16];
    u32 shadow_any = 0;
    memset(shadow, 0, sizeof shadow);
    {
        static int sh = -1, rev = -1;
        if (sh < 0) { const char* e = getenv("GOWA_SHADOW_CMP"); sh = !(e && *e == '0'); }
        if (rev < 0) { const char* e = getenv("GOWA_SHADOW_REV"); rev = !(e && *e == '0'); }
        for (u32 uu = 0; sh && uu < 16; uu++) {
            rsx_dsp_texture ut = ld_tx[uu];
            if (!ut.enabled) continue;
            rsx_fp_shadow_desc d;
            rsx_fp_shadow_desc_from_texture(&d, ut.format, ut.dimension, ut.cubemap,
                                            ut.mipmaps, ut.filter, ut.wrap, ut.control0);
            if (d.enabled != 1) continue;
            int zi = -1;
            for (u32 i = 0; i < g.n_zdepths; i++)
                if (g.zdepths[i].location == ut.location && g.zdepths[i].offset == ut.offset) { zi = (int)i; break; }
            if (zi < 0) continue;
            if (rev) { static const u8 mirror[8] = { 0, 4, 2, 6, 1, 5, 3, 7 }; d.func = mirror[d.func & 7u]; }
            shadow[uu] = d;
            shadow_any |= 1u << uu;
        }
    }
    if (shadow_any) key = fnv1a(shadow, sizeof shadow, key);
    g_rsx_fp_shadow_window = shadow_any != 0 && ld_fp_constants_buffered();
    if (srgb_output) key = fnv1a(&srgb_output, sizeof(srgb_output), key);
    if (masked_layout) {
        static const u32 masked_layout_tag = 0x314B534Du; /* "MSK1" */
        const u32 payload_stride =
            packed_payload ? masked_layout->stride : VERT_STRIDE;
        key = fnv1a(
            &masked_layout_tag, sizeof(masked_layout_tag), key);
        key = fnv1a(
            &masked_layout->mask, sizeof(masked_layout->mask), key);
        key = fnv1a(
            &payload_stride, sizeof(payload_stride), key);
        key = fnv1a(
            &packed_payload, sizeof(packed_payload), key);
        if (g_ld_vs_active) {   /* GOWA (24/10): per-slot buffers, each in its own format */
            u8 sig[1 + RSX_DSP_NUM_VERTEX_ATTR];
            sig[0] = (u8)(0x80u | g_ld_vs_n);
            for (u32 s = 0; s < g_ld_vs_n; s++) sig[1 + s] = (u8)g_ld_vs[s].fmt;
            key = fnv1a(sig, 1 + g_ld_vs_n, key);
        }
    }
    if (ld_fp_constants_buffered())
        key = ld_hash_structural_render_state(&rs, key);
    else
        key = fnv1a(&rs, sizeof(rs), key);
#if defined(YZ_PERF_PROFILE)
    ld_profile_note_fp_variants(
        fp_uc, fp_size, fp_ctrl_key, cube_mask, &rs);
#endif
    if (g_ld_edge_luz) { static const u32 luz_tag = 0x314C4445u; /* "EDL1" */ key = fnv1a(&luz_tag, sizeof luz_tag, key); }
    g_ld_current_pso.valid = 1;
    g_ld_current_pso.key = key;
    g_ld_current_pso.vp_hash = vp_key_only;   /* GOWA: the word hash above (only logged) */
    g_ld_current_pso.vp_start = start;
    g_ld_current_pso.vp_instrs = vp_instrs;
    g_ld_current_pso.fp_location = fp_loc;
    g_ld_current_pso.fp_offset = fp_off;
    g_ld_current_pso.fp_size = fp_size;
    g_ld_current_pso.fp_control = fp_ctrl;
    g_ld_current_pso.cube_mask = cube_mask;
    g_ld_current_pso.shadow_mask = shadow_any;
    g_ld_current_pso.vtex_mask = vtex_mask;
    g_ld_current_pso.txl_mask = txl_mask;
    g_ld_current_pso.input_mask =
        masked_layout ? masked_layout->mask : 0xFFFFu;
    g_ld_current_pso.input_stride =
        masked_layout && packed_payload
            ? masked_layout->stride : VERT_STRIDE;
    g_ld_current_pso.packed_offsets =
        masked_layout && packed_payload;

#if defined(YZ_PERF_PROFILE)
    g_ld_profile.total.pso_lookups++;
#endif
    /* GOWA (21/10): hash index over g.psos (append-only) instead of a linear scan of every
     * cached PSO on every draw -- that loop was ~20% of get_pso in chapter 16 (~2,900 draws a
     * frame). Index entries are position + 1; 0 = empty. */
    if (g.n_psos < s_pso_idx_n) {   /* the cache was reset (device loss): rebuild */
        memset(s_pso_idx, 0, sizeof s_pso_idx);
        s_pso_idx_n = 0;
    }
    for (; s_pso_idx_n < g.n_psos; s_pso_idx_n++)
        pso_idx_put(g.psos[s_pso_idx_n].key, s_pso_idx_n);
    for (u32 h = pso_idx_slot(key), probe = 0; probe < PSO_IDX_N; probe++, h = (h + 1u) & (PSO_IDX_N - 1u)) {
        const u32 e = s_pso_idx[h];
        if (!e) break;
#if defined(YZ_PERF_PROFILE)
        g_ld_profile.total.pso_probes++;
#endif
        if (g.psos[e - 1u].key == key) {
#if defined(YZ_PERF_PROFILE)
            g_ld_profile.total.pso_hits++;
#endif
            g.psos[e - 1u].last_frame = g_ld_frames;
            return g.psos[e - 1u].pso;
        }
    }
#if defined(YZ_PERF_PROFILE)
    g_ld_profile.total.pso_misses++;
#endif
    if (!s_pso_cap) {
        const char* e = getenv("GOWA_PSO_MAX");
        s_pso_cap = e && atoi(e) > 16 && (u32)atoi(e) < MAX_PSOS ? (u32)atoi(e) : MAX_PSOS;
    }
    /* full and nothing old enough to reuse: as before (the draw is dropped) */
    if (g.n_psos >= s_pso_cap) {
        u32 best_frame = 0xFFFFFFFFu;
        for (u32 i = 0; i < g.n_psos; i++) if (g.psos[i].last_frame < best_frame) best_frame = g.psos[i].last_frame;
        if (best_frame >= g_ld_frames) {
#if defined(YZ_PERF_PROFILE)
            g_ld_profile.total.pso_full++;
#endif
            LD_PSO_BAIL("PSO cache full (MAX_PSOS)");
            return NULL;
        }
    }

    static char vs_hlsl[256 * 1024];
    static char ps_hlsl[256 * 1024];
    ID3D12PipelineState* pso = NULL;
#if defined(YZ_PERF_PROFILE)
    const LONGLONG decompile_begin = ld_profile_qpc();
#endif
    rsx_vp_set_branch_base(rsx_dsp_vp_start(&g.rsx));
    { extern int g_rsx_vp_edge_luz; g_rsx_vp_edge_luz = g_ld_edge_luz; }
    const int vi = masked_layout
        ? rsx_vp_decompile_compact_ex(
            vp_uc, vp_instrs * 16, vtex_mask, masked_layout->mask,
            vs_hlsl, sizeof(vs_hlsl))
        : rsx_vp_decompile_ex(
            vp_uc, vp_instrs * 16, vtex_mask, vs_hlsl, sizeof(vs_hlsl));
    { extern int g_rsx_vp_edge_luz; g_rsx_vp_edge_luz = 0; }
    u32 decompiled_constant_count = 0;
    int fi;
    if (ld_fp_constants_buffered()) {
        fi = shadow_any
            ? rsx_fp_decompile_buffered_shadow_color_ex(
                fp_uc, fp_size, fp_ctrl, cube_mask, shadow, gamma_masks, ps_hlsl,
                sizeof(ps_hlsl), &decompiled_constant_count)
            : gamma_any
            ? rsx_fp_decompile_buffered_color_ex(
                fp_uc, fp_size, fp_ctrl, cube_mask, gamma_masks, ps_hlsl,
                sizeof(ps_hlsl), &decompiled_constant_count)
            : rsx_fp_decompile_buffered_ex(
                fp_uc, fp_size, fp_ctrl, cube_mask, ps_hlsl,
                sizeof(ps_hlsl), &decompiled_constant_count);
        if (fi > 0 &&
            decompiled_constant_count != g.fp_constants.count)
            fi = -1;
        if (fi > 0 && rs.alpha_test_enable &&
            rsx_fp_apply_alpha_test_buffered(
                ps_hlsl, sizeof(ps_hlsl), rs.alpha_func) < 0)
            fi = -1;
        if (fi > 0 && srgb_output &&
            rsx_fp_apply_srgb_output(ps_hlsl, sizeof(ps_hlsl)) < 0)
            fi = -1;
        if (fi > 0) {
            const int np = rsx_fp_apply_unnorm_scale(
                ps_hlsl, sizeof(ps_hlsl), unnorm_mask, unnorm_dim, cube_mask);
            { static int n = 0;
              if (n++ < 8)
                  fprintf(stderr, "[unnorm] mask=0x%X cube=0x%X dim0=%ux%u"
                                  " patched=%d\n",
                          unnorm_mask, cube_mask, unnorm_dim[0][0],
                          unnorm_dim[0][1], np); }
            if (np < 0) fi = -1;
        }
    } else {
        fi = rsx_fp_decompile_ex(
            fp_uc, fp_size, fp_ctrl, cube_mask,
            ps_hlsl, sizeof(ps_hlsl));
        if (fi > 0 && rs.alpha_test_enable &&
            rsx_fp_apply_alpha_test(
                ps_hlsl, sizeof(ps_hlsl), rs.alpha_func,
                g.fp_alpha_ref) < 0)
            fi = -1;
        if (fi > 0 && rsx_fp_apply_unnorm_scale(
                ps_hlsl, sizeof(ps_hlsl), unnorm_mask,
                unnorm_dim, cube_mask) < 0)
            fi = -1;
    }
#if defined(YZ_PERF_PROFILE)
    g_ld_profile.total.decompile_qpc +=
        (u64)(ld_profile_qpc() - decompile_begin);
#endif
    /* LD_FORCE_GREEN=1: make every fragment program return solid green.
     *
     * The same trick as LD_CLEAR_TEST, applied one stage later. Clears are
     * visible on these surfaces and draws are not, while the engine reports
     * 18,452 groups executed with zero drops and the GPU state cannot discard
     * anything (viewport 0,0 720x512, scissor full, depth test off, colour mask
     * all channels, blend off, cull off). Two possibilities remain and they need
     * opposite fixes: the draws reach the GPU and the SAMPLED DATA is black, or
     * they never reach it and the accounting is wrong. Green separates them. */
    { static int fg = -1;
      /* LD_FORCE_GREEN=<pso key hex> limits it to one PSO. */
      static u64 gk = 0;
      if (fg < 0) { const char* e = getenv("LD_FORCE_GREEN"); fg = e ? 1 : 0;
                    gk = e ? _strtoui64(e, 0, 16) : 0; if (gk == 1) gk = 0; }
      if (fg && fi > 0 && (!gk || gk == live_legacy_pso_key())) {
          /* The emulated alpha test discards before the return, so replacing
           * only the return still showed nothing for a failing alpha. */
          for (char* d = strstr(ps_hlsl, "discard;"); d; d = strstr(d, "discard;"))
              memcpy(d, "        ", 8);
          char* r = strstr(ps_hlsl, "    return ");
          if (r) {
              char* semi = strchr(r, ';');
              if (semi) {
                  const char rep[] = "    return float4(0,1,0,1);";
                  const size_t rl = sizeof(rep) - 1;
                  const size_t old = (size_t)(semi + 1 - r);
                  const size_t used = strlen(ps_hlsl);
                  if (used - old + rl + 1 <= sizeof(ps_hlsl)) {
                      memmove(r + rl, semi + 1, used - (size_t)(semi + 1 - ps_hlsl) + 1);
                      memcpy(r, rep, rl);
                  }
              }
          }
      } }
    /* GOWA_PS_SHOW=<HLSL expr> [GOWA_PS_SHOW_KEY=<pso key hex>]: colour target 0
     * of that pixel shader becomes float4((expr).xyz, 1) -- an intermediate
     * value (a texture sample, an interpolant, a register) made visible. The
     * FP_SHOW/FP_TEX hooks of the old backend look for "_po.c0 = ", which the
     * live engine's HLSL does not have. */
    { static int sh = -1; static const char* expr; static u64 sk;
      if (sh < 0) { expr = getenv("GOWA_PS_SHOW"); sh = expr ? 1 : 0;
                    const char* k = getenv("GOWA_PS_SHOW_KEY"); sk = k ? _strtoui64(k, 0, 16) : 0; }
      static int stx = -2; static u32 stx_off[4];
      if (stx == -2) { const char* t = getenv("GOWA_PS_SHOW_IF_TEX"); stx = 0;
                       while (t && *t && stx < 4) { char* e2; stx_off[stx++] = (u32)strtoul(t, &e2, 16); t = *e2 == ',' ? e2 + 1 : NULL; } }
      int tex_ok = 1;
      if (stx) { rsx_dsp_texture t0; rsx_dsp_get_texture(&g.rsx, 0, &t0); tex_ok = 0;
                 for (int q = 0; q < stx; q++) if (t0.enabled && t0.offset == stx_off[q]) tex_ok = 1; }
      /* GOWA_PS_DUMP_IF_TEX=<offset hex>: write the PS HLSL of PSOs built while
       * texture unit 0 is at that offset (to read a pass without its key). */
      /* GOWA_PS_REPLACE_KEY=<pso key hex> + GOWA_PS_REPLACE_FROM=<text> +
       * GOWA_PS_REPLACE_TO=<text>: in that PSO's pixel shader, the first
       * occurrence of FROM becomes TO -- one intermediate forced (e.g. an
       * effect's blend factor to 0.0) to see what it does to the image. */
      { static int pr = -2; static u64 prk; static const char *pf, *pt;
        if (pr == -2) { const char* k = getenv("GOWA_PS_REPLACE_KEY"); pf = getenv("GOWA_PS_REPLACE_FROM");
                        pt = getenv("GOWA_PS_REPLACE_TO"); pr = (k && pf && pt) ? 1 : 0; prk = k ? _strtoui64(k, 0, 16) : 0; }
        if (pr && fi > 0 && key == prk) {
            char* at = strstr(ps_hlsl, pf);
            const size_t fl = strlen(pf), tl = strlen(pt), used = strlen(ps_hlsl);
            if (at && used - fl + tl + 1 <= sizeof(ps_hlsl)) {
                memmove(at + tl, at + fl, used - (size_t)(at + fl - ps_hlsl) + 1);
                memcpy(at, pt, tl);
                fprintf(stderr, "[ps-replace] pso %016llx: '%s' -> '%s'\n", (unsigned long long)key, pf, pt);
            } else fprintf(stderr, "[ps-replace] pso %016llx: '%s' NAO ACHADO\n", (unsigned long long)key, pf);
        } }
      /* GOWA_PS_DUMP_KEY=<pso key hex>: write the PS HLSL of that structural
       * key (out\ps_key_<key>.hlsl) -- a PSO is built once, at first use, so
       * the texture-based filters can miss it. */
      { static int dk = -2; static u64 dkeys[8]; static int ndk;   /* up to 8 keys, comma-separated */
        if (dk == -2) { const char* t = getenv("GOWA_PS_DUMP_KEY"); dk = t ? 1 : 0;
                        while (t && *t && ndk < 8) { char* q; dkeys[ndk++] = _strtoui64(t, &q, 16);
                                                     if (q == t) break; t = (*q == ',') ? q + 1 : q; } }
        int dhit = 0; for (int q = 0; q < ndk; q++) if (key == dkeys[q]) dhit = 1;
        if (dk && fi > 0 && dhit) { char fn[260];
          snprintf(fn, sizeof fn, "out/ps_key_%016llx.hlsl", (unsigned long long)key);
          FILE* f = fopen(fn, "wb"); if (f) { fputs(ps_hlsl, f); fclose(f); }
          /* the vertex shader too (out\vs_key_<key>.hlsl) */
          snprintf(fn, sizeof fn, "out/vs_key_%016llx.hlsl", (unsigned long long)key);
          f = fopen(fn, "wb"); if (f) { fputs(vs_hlsl, f); fclose(f); }
          /* and the fragment program's ucode, as fetched (scripts/fp_emula.py) */
          if (fp_uc && fp_size) {
              snprintf(fn, sizeof fn, "out/ps_key_%016llx.fp.bin", (unsigned long long)key);
              f = fopen(fn, "wb"); if (f) { fwrite(fp_uc, 1, fp_size, f); fclose(f); }
          } } }
      { static int dtx = -2; static u32 dtx_off;
        if (dtx == -2) { const char* t = getenv("GOWA_PS_DUMP_IF_TEX"); dtx = t ? 1 : 0; dtx_off = t ? (u32)strtoul(t, 0, 16) : 0; }
        if (dtx && fi > 0) { rsx_dsp_texture t0d; rsx_dsp_get_texture(&g.rsx, 0, &t0d);
          if (t0d.enabled && t0d.offset == dtx_off) { char fn[260];
            snprintf(fn, sizeof fn, "out/ps_iftex_%016llx.hlsl", (unsigned long long)live_legacy_pso_key());
            FILE* f = fopen(fn, "wb"); if (f) { fputs(ps_hlsl, f); fclose(f); } } } }
      /* GOWA_PS_SHOW_UNIT_WH=<w>x<h>: in every PSO that samples a texture of
       * that size, t0 becomes that unit's sample at tc0 (the expression is
       * built per PSO; GOWA_PS_SHOW/_KEY are not needed) -- a material's
       * diffuse shown alone across all its passes, whatever unit it is on. */
      static int swh = -2; static u32 sw_w, sw_h; static char sw_expr[96];
      if (swh == -2) { const char* e = getenv("GOWA_PS_SHOW_UNIT_WH"); swh = 0;
                       if (e) { sw_w = (u32)atoi(e); const char* x = strchr(e, 'x'); sw_h = x ? (u32)atoi(x + 1) : 0; swh = 1; } }
      int sw_hit = 0;
      if (swh && fi > 0)
          for (u32 uu = 0; uu < 16; uu++) {
              rsx_dsp_texture ut = ld_tx[uu];
              if (ut.enabled && ut.width == sw_w && ut.height == sw_h) {
                  snprintf(sw_expr, sizeof sw_expr, "rsx_tex%u.Sample(rsx_samp[%u], input.tc0.xy)", uu, uu);
                  sw_hit = 1; break;
              }
          }
      if (sw_hit) { sh = 1; expr = sw_expr; }
      /* GOWA_PS_SHOW_KEYCOLOR=1: every PSO's t0 becomes a flat colour made
       * from its structural key, logged as "[ps-key] pso=<key> rgb=r,g,b" --
       * read a pixel of a dumped frame to know which PSO drew it. */
      static int skc = -1;
      if (skc < 0) skc = getenv("GOWA_PS_SHOW_KEYCOLOR") ? 1 : 0;
      if (skc && fi > 0) {
          const u64 hk = key * 0x9E3779B97F4A7C15ull;
          const u32 cr = 40u + (u32)((hk >> 8) % 200u), cg = 40u + (u32)((hk >> 24) % 200u), cb = 40u + (u32)((hk >> 40) % 200u);
          snprintf(sw_expr, sizeof sw_expr, "float3(%u.0/255.0, %u.0/255.0, %u.0/255.0)", cr, cg, cb);
          fprintf(stderr, "[ps-key] pso=%016llx rgb=%u,%u,%u\n", (unsigned long long)key, cr, cg, cb);
          sw_hit = 1; sh = 1; expr = sw_expr;
      }
      if (sh && fi > 0 && (sw_hit || (!swh && tex_ok && (!sk || sk == live_legacy_pso_key() || sk == key)))) {   /* GOWA: or the pso= key */
          char* r = strstr(ps_hlsl, "    return _out;");
          /* GOWA: with sRGB output / MRT the shader ends in _rsx_color_out */
          if (!r) r = strstr(ps_hlsl, "    PSOutput _rsx_color_out = _out;");
          int single = 0;
          if (!r) { r = strstr(ps_hlsl, "    return (_o == _o)"); single = r != NULL; }
          if (!r) { r = strstr(ps_hlsl, "    return ((asuint(_o)"); single = r != NULL; }   /* the NaN guard on the bits */
          if (!r) { r = strstr(ps_hlsl, "    float4 _rsx_color_out = (_o == _o)"); single = r != NULL; }
          if (!r) { r = strstr(ps_hlsl, "    float4 _rsx_color_out = ((asuint(_o)"); single = r != NULL; }
          /* GOWA_PS_SHOW_A=<alpha> (default 1): an RGBM scene target reads
           * alpha as the multiplier -- 0 keeps the value as is */
          static const char* sa = (const char*)-1;
          if (sa == (const char*)-1) sa = getenv("GOWA_PS_SHOW_A");
          if (r) {
              char ins[600];
              const int il = snprintf(ins, sizeof ins, single ? "    _o = float4((%s).xyz, %s);\n" : "    _out.t0 = float4((%s).xyz, %s);\n",
                                      expr, sa ? sa : "1.0");
              const size_t used = strlen(ps_hlsl);
              if (il > 0 && used + (size_t)il + 1 <= sizeof(ps_hlsl)) {
                  memmove(r + il, r, used - (size_t)(r - ps_hlsl) + 1);
                  memcpy(r, ins, (size_t)il);
                  static int said = 0;
                  if (said++ < 4) fprintf(stderr, "[ps-show] pso %016llx: t0 = %s\n",
                                          (unsigned long long)live_legacy_pso_key(), expr);
              }
          }
      } }
    /* LD_FORCE_COL0=1: every vertex program outputs white COLOR0 -- separates
     * "the VS computes a wrong colour" from everything downstream of it. */
    /* LD_FORCE_COL0=<pso key hex> limits it to one PSO. */
    { static int fc = -1; static u64 fk = 0;
      if (fc < 0) { const char* e = getenv("LD_FORCE_COL0"); fc = e ? 1 : 0;
                    fk = e ? _strtoui64(e, 0, 16) : 0; if (fk == 1) fk = 0; }
      char* c0 = fc && vi > 0 && (!fk || fk == live_legacy_pso_key())
                 ? strstr(vs_hlsl, "Out.col0 = o[1];") : NULL;
      if (c0) memcpy(c0, "Out.col0=1+0*_p;", 16); }   /* same length, in place */
    /* GOWA_HLSL_X32=<dir>: the pixel shader of every new PSO whose draw samples
     * an X32_FLOAT texture (0x9C), with the bound units' formats -- the passes
     * that read the linear depth (fog, depth of field). */
    { static const char* xd = (const char*)-1; static int xn; static u32 xfmt = 0x9Cu, xoff = 0, xw = 0, xh = 0; static long xfrom = -1;
      static double xsec = -1.0; static ULONGLONG xt0;
      if (xd == (const char*)-1) { xd = getenv("GOWA_HLSL_X32");
          const char* fe = getenv("GOWA_HLSL_FMT"); if (fe) xfmt = (u32)strtoul(fe, 0, 16);
          /* GOWA_HLSL_OFF=<hex>: instead, the passes that sample that offset */
          fe = getenv("GOWA_HLSL_OFF"); if (fe) xoff = (u32)strtoul(fe, 0, 16);
          /* GOWA_HLSL_FROM=<frame>: instead, every PSO built from that frame on */
          fe = getenv("GOWA_HLSL_FROM"); if (fe) xfrom = atol(fe);
          /* "<n>s": seconds after the first PSO instead of a frame */
          if (fe && strchr(fe, 's')) { xsec = atof(fe); xfrom = 0; }
          /* GOWA_HLSL_WH=<w>x<h>: instead, the passes that sample a texture of
           * that size (texture addresses move between runs; a 2048x4096 one is
           * Kratos's body in God of War Ascension) */
          fe = getenv("GOWA_HLSL_WH"); if (fe) { xw = (u32)atoi(fe); const char* x = strchr(fe, 'x'); xh = x ? (u32)atoi(x + 1) : 0; }
          xt0 = GetTickCount64(); }
      if (xsec >= 0.0 && (double)(GetTickCount64() - xt0) < xsec * 1000.0) xfrom = 0x7FFFFFFF;
      else if (xsec >= 0.0) xfrom = 0;
      if (xd && fi > 0) {
          int has = 0; char units[4096]; int up = 0; units[0] = 0;
          for (u32 uu = 0; uu < 16; uu++) {
              rsx_dsp_texture ut = ld_tx[uu];
              if (!ut.enabled) continue;
              if (xw ? ut.width == xw && ut.height == xh :
                  xfrom >= 0 ? (long)g_ld_frames >= xfrom : xoff ? ut.offset == xoff : (ut.format & 0x9Fu) == xfmt) has = 1;
              up += snprintf(units + up, sizeof units - up, "// u%u %u:0x%08X fmt=0x%02X %ux%u remap=0x%X addr=0x%08X filter=0x%08X\n",
                             uu, ut.location, ut.offset, ut.format, ut.width, ut.height, ut.remap, ut.wrap, ut.filter);
          }
          if (has && xn < (xfrom >= 0 ? 200 : 40)) {
              char fn[MAX_PATH]; snprintf(fn, sizeof fn, "%s/x32_%02d.ps.txt", xd, xn++);
              FILE* f = fopen(fn, "wb");
              if (f) { rsx_dsp_surface us; rsx_dsp_get_surface(&g.rsx, &us);
                       fprintf(f, "// alvo 0x%08X fmt=0x%X frame=%u pso=%016llx\n%s", us.color_offset[0], us.color_format, g_ld_frames, (unsigned long long)key, units);
                       fputs(ps_hlsl, f); fclose(f); }
          }
      } }
    /* LD_HLSL_DUMP=<dir>: files named by the PSO key the draw CSV prints. */
    { static const char* hd = (const char*)-1;
      if (hd == (const char*)-1) hd = getenv("LD_HLSL_DUMP");
      if (hd) {
        char fn[MAX_PATH]; FILE* f;
        snprintf(fn, sizeof fn, "%s/hlsl_%016llx.vs.txt", hd, (unsigned long long)live_legacy_pso_key());
        f = fopen(fn, "wb"); if (f) { fputs(vi > 0 ? vs_hlsl : "<vp fail>", f); fclose(f); }
        snprintf(fn, sizeof fn, "%s/hlsl_%016llx.ps.txt", hd, (unsigned long long)live_legacy_pso_key());
        f = fopen(fn, "wb"); if (f) { fputs(fi > 0 ? ps_hlsl : "<fp fail>", f); fclose(f); }
      } }
    if (vi > 0 && fi > 0)
        pso = build_pso(
            vs_hlsl, ps_hlsl, &rs, masked_layout, packed_payload);
    if (g_ld_pso_pendente) {   /* shader na fila de compilação: não guarda o PSO nulo */
        g_ld_pso_pendente = 0;
        return NULL;
    }

    if (g.n_psos >= s_pso_cap) {   /* full: reuse the LRU position (checked above) */
        if (!pso_evict_lru(key, pso)) { if (pso) pso->lpVtbl->Release(pso); return NULL; }
        return pso;
    }
    g.psos[g.n_psos].key = key;
    g.psos[g.n_psos].pso = pso;
    g.psos[g.n_psos].last_frame = g_ld_frames;
    g.n_psos++;
    return pso;
}

static int ld_upload_pixel_constants(u32* out_offset)
{
    if (out_offset)
        *out_offset = 0;
    if (!ld_fp_constants_buffered())
        return 1;
    if (!g.ps_cb || !g.ps_cb_mapped)
        return 0;

    /* HLSL uses a one-element placeholder array for programs with no inline
     * constants so fp_alpha retains a valid 16-byte slot. */
    const u32 value_slots =
        g.fp_constants.count ? g.fp_constants.count : 1u;
    const u32 shadow_mask = g_ld_current_pso.valid ? g_ld_current_pso.shadow_mask : 0u;
    const u32 data_bytes = (value_slots + 1u + (shadow_mask ? 16u : 0u)) * 16u;
    u32 offset = 0;
    u32 allocation_bytes = 0;
    int plan = rsx_fp_constant_ring_plan(
        g.ps_cb_used, PS_CB_RING_BYTES, data_bytes,
        &offset, &allocation_bytes);
    if (plan < 0) {
#if defined(YZ_PERF_PROFILE)
        g_ld_profile.total.ps_constant_capacity_failures++;
#endif
        return 0;
    }
    if (plan == 0) {
        ld_flush(LD_FLUSH_PIXEL_CONSTANT_RING);
        if (!g.ready)
            return 0;
        g.ps_cb_used = 0;
        plan = rsx_fp_constant_ring_plan(
            g.ps_cb_used, PS_CB_RING_BYTES, data_bytes,
            &offset, &allocation_bytes);
        if (plan != 1) {
#if defined(YZ_PERF_PROFILE)
            g_ld_profile.total.ps_constant_capacity_failures++;
#endif
            return 0;
        }
    }

#if defined(YZ_PERF_PROFILE)
    const LONGLONG upload_begin = ld_profile_qpc();
#endif
    u8* destination = g.ps_cb_mapped + offset;
    memset(destination, 0, data_bytes);
    if (g.fp_constants.count)
        memcpy(
            destination, g.fp_constants.values,
            (size_t)g.fp_constants.count * 16u);
    memcpy(
        destination + value_slots * 16u,
        &g.fp_alpha_ref, sizeof(g.fp_alpha_ref));
    /* fp_alpha.y/.z: window height and "origin at the bottom" for WPOS
     * (rsx_fp_decompiler.c _rsx_wpos). GOWA_WPOS_FLIP=0 leaves WPOS as
     * SV_Position, the old behaviour. */
    {
        static int on = -1;
        if (on < 0) { const char* e = getenv("GOWA_WPOS_FLIP"); on = !(e && e[0] == '0'); }
        const u32 sw = rsx_dsp_reg(&g.rsx, 0x1D88);
        const float wp[3] = { (float)(sw & 0xFFFu), (on && ((sw >> 12) & 0xFu)) ? 1.0f : 0.0f,
                              1.0f / (float)g.res_scale };   /* fp_alpha.w: see _rsx_wpos */
        memcpy(destination + value_slots * 16u + 4u, wp, sizeof(wp));
    }
    /* GOWA: fp_texwin -- each depth-compare unit's declared window inside
     * its host snapshot (see the shadow block of the PSO build). */
    if (shadow_mask) {
        /* per unit: (window u, window v, border depth, BORDER addressing) */
        float win[16][4];
        for (u32 u = 0; u < 16; u++) { win[u][0] = 1.0f; win[u][1] = 1.0f; win[u][2] = 1.0f; win[u][3] = 0.0f; }
        for (u32 u = 0; u < 16; u++) {
            if (!((shadow_mask >> u) & 1u)) continue;
            rsx_dsp_texture ut; rsx_dsp_get_texture(&g.rsx, u, &ut);
            win[u][2] = (float)((ut.border_color >> 16) & 0xFFu) / 255.0f;   /* ARGB8 border, R = depth */
            win[u][3] = ((ut.wrap & 0xFu) == 4u || ((ut.wrap >> 8) & 0xFu) == 4u) ? 1.0f : 0.0f;
            for (u32 i = 0; i < g.n_zdepths; i++) {
                const zdepth_t* z = &g.zdepths[i];
                if (z->location != ut.location || z->offset != ut.offset) continue;
                if (z->snapshot_w && z->snapshot_h) {
                    win[u][0] = (float)ut.width / (float)z->snapshot_w;
                    win[u][1] = (float)ut.height / (float)z->snapshot_h;
                }
                break;
            }
        }
        memcpy(destination + (value_slots + 1u) * 16u, win, sizeof win);
    }
    g.ps_cb_used += allocation_bytes;
#if defined(YZ_PERF_PROFILE)
    g_ld_profile.total.ps_constant_allocations++;
    g_ld_profile.total.ps_constant_upload_bytes += data_bytes;
    g_ld_profile.total.ps_constant_upload_qpc +=
        (u64)(ld_profile_qpc() - upload_begin);
#endif
    if (out_offset)
        *out_offset = offset;
    return 1;
}

/* ---------------------------------------------------------------------------
 * Draw accumulation sink (mirrors the harness sink)
 * -----------------------------------------------------------------------*/
typedef struct { float a[16][4]; } vtx_t;
typedef struct { u32 first, count; } batch_t;

typedef struct {
    /* INLINE_ARRAY source for this primitive, or NULL. The stream is the
     * vertices themselves, in guest byte order, so the fetch path reads it
     * with the same big-endian decoders it uses for a vertex array -- only
     * the base pointer differs. inl_off[i] is attribute i's byte offset
     * within one inline vertex; the attributes are packed in ascending
     * index order, which is what makes the offsets derivable at all (an
     * inline stream carries no per-array offset registers). */
    const u8* inl; u32 inl_bytes, inl_stride; u32 inl_off[16];
    batch_t arr[256]; u32 n_arr;
    batch_t idx[256]; u32 n_idx;
    u32     n_packets;
    vtx_t*  verts; u32 n_verts, cap_verts;
    rsx_vertex_ref* refs; u32 n_refs, cap_refs;
    u32     n_source_refs;
    int     refs_remapped;
    rsx_vertex_remap ref_remap;
    u8* compact_verts; u64 compact_capacity;
    rsx_vertex_layout_plan layout;
    rsx_vertex_fetch_plan fetch_plan;
    int     fetch_ok;
    int     slot_mode;   /* GOWA (24/10): vertices go up slot by slot (ld_vslots_*), not in compact_verts */
    int     idx_fast;    /* GOWA (24/10): indices = guest u16 - idx_lo (ld_idx_fast_decode) */
    const u8* idx_src; u32 idx_lo;
    /* Primitive-restart cut points (s25 port of the replay-harness fix):
     * a guest index equal to the RSX restart sentinel is a cut marker, not
     * a vertex reference (RPCS3 RSXThread.cpp:398); fetch_batches records
     * the n_verts position at each one and the STRIP/FAN expansion must
     * not bridge across a cut. */
    u32*    cuts; u32 n_cuts, cap_cuts;
} draw_ctx;

static draw_ctx dc;

/* GOWA (24/10): per-slot vertex upload. The compact path converted every used attribute
 * to a float4 per vertex into compact_verts (rsx_vertex_fetch_all: 12% of the RSX thread
 * in chapter 16, ~2,500 draws and ~856 thousand vertices a frame) and then copied all of
 * it into the vertex ring. Here each layout slot is its own vertex buffer in the ring,
 * written once:
 *   raw      the guest bytes of the vertex range, byte-swapped, read by the input
 *            assembler in their own format (float 1-4, half / snorm16 / unorm8 1, 2, 4)
 *            -- the references must be one consecutive range (the dense remap);
 *   conv     the same float4 conversion as before, straight into the ring;
 *   default  one float4 read with stride 0.
 * Indexed triangle lists (the index buffer is the compact path's). GOWA_VTX_SLOTS=0
 * keeps compact_verts. */
static D3D12_VERTEX_BUFFER_VIEW g_ld_vs_view[RSX_DSP_NUM_VERTEX_ATTR];

static int ld_vtx_slots_on(void)
{
    static int on = -1;
    if (on < 0) {
        const char* e = getenv("GOWA_VTX_SLOTS");
        on = !(e && *e == '0') && !getenv("RSX_DRAW_CSV") && !getenv("RSX_A010_GEOM") &&
             !getenv("GOWA_ATTR_DUMP_KEY") && !getenv("GOWA_VCACHE_STATS") && !getenv("GOWA_VRAW_STATS");
    }
    return on;
}

/* DXGI format the input assembler reads a raw attribute as (0: none) and the byte-swap
 * unit, which is also the component size */
static u16 ld_vs_raw_fmt(u32 type, u32 size, u8* swap)
{
    switch (type) {
    case RSX_VTX_TYPE_FLOAT:
        *swap = 4;
        return (u16)(size == 1 ? DXGI_FORMAT_R32_FLOAT : size == 2 ? DXGI_FORMAT_R32G32_FLOAT :
                     size == 3 ? DXGI_FORMAT_R32G32B32_FLOAT : size == 4 ? DXGI_FORMAT_R32G32B32A32_FLOAT : 0);
    case RSX_VTX_TYPE_HALF:
        *swap = 2;
        return (u16)(size == 1 ? DXGI_FORMAT_R16_FLOAT : size == 2 ? DXGI_FORMAT_R16G16_FLOAT :
                     size == 4 ? DXGI_FORMAT_R16G16B16A16_FLOAT : 0);
    case RSX_VTX_TYPE_SNORM16:
        *swap = 2;
        return (u16)(size == 1 ? DXGI_FORMAT_R16_SNORM : size == 2 ? DXGI_FORMAT_R16G16_SNORM :
                     size == 4 ? DXGI_FORMAT_R16G16B16A16_SNORM : 0);
    case RSX_VTX_TYPE_UNORM8:
        *swap = 1;
        return (u16)(size == 1 ? DXGI_FORMAT_R8_UNORM : size == 2 ? DXGI_FORMAT_R8G8_UNORM :
                     size == 4 ? DXGI_FORMAT_R8G8B8A8_UNORM : 0);
    default:
        return 0;
    }
}

/* After the remap and the fetch plan: can this draw go up slot by slot? Fills g_ld_vs. */
static int ld_vslots_plan(const rsx_vertex_layout_plan* layout)
{
    if (!ld_vtx_slots_on() || !ld_vertex_mask_signature() || dc.n_arr || !dc.n_idx ||
        g.rsx.current_primitive != 5u /* TRIANGLES */ || !dc.refs_remapped ||
        dc.fetch_plan.inline_data || !layout->count || !dc.n_refs)
        return 0;
    const u32 n = dc.n_refs;
    const int dense = g_rsx_remap_dense;
    const u32 e0 = (dc.refs[0].vertex_id + dc.refs[0].base_index) & 0x000FFFFFu;
    for (u32 slot = 0; slot < layout->count; slot++) {
        const u32 attr = layout->attrs[slot];
        const rsx_vertex_fetch_attr* f = &dc.fetch_plan.attr[attr];
        ld_vslot* s = &g_ld_vs[slot];
        s->attr = attr; s->src = NULL; s->swap = 1;
        if (!f->desc.type || !f->desc.size) {
            s->kind = LD_VS_DEF; s->fmt = (u16)DXGI_FORMAT_R32G32B32A32_FLOAT; s->stride = 0; s->bytes = 16;
            continue;
        }
        if (!f->stable_base)
            return 0;   /* rsx_vertex_fetch_one per vertex: the compact path */
        u8 sw = 1;
        const u16 fmt = ld_vs_raw_fmt(f->desc.type, f->desc.size, &sw);
        const u32 esz = f->desc.size * sw;
        if (fmt && dense && f->desc.frequency <= 1 && f->stride >= esz && f->stride <= 2048u &&
            f->stride % sw == 0 && e0 >= f->stable_first && (u64)e0 + n - 1 <= f->stable_last &&
            (u64)e0 + n - 1 <= 0x000FFFFFu) {
            s->kind = LD_VS_RAW; s->fmt = fmt; s->swap = sw; s->stride = f->stride;
            s->src = f->stable_base + (u64)(e0 - f->stable_first) * f->stride;
            s->bytes = (n - 1) * f->stride + esz;
            s->sstride = f->stride; s->esz = esz;
            /* GOWA (27/10): EDGE's skinned vertices (Kratos, the Fury) come with
             * strides of 54/58 bytes, so their SNORM16x4 and UNORM8x4 elements sat
             * off a 4-byte boundary. Radeon drivers fetch those wrong (NVIDIA does
             * not): dark and light patches on the skin, reports 26/35/71. Such an
             * attribute goes up alone, each element at a 4-byte aligned stride.
             * GOWA_VS_ALIGN4=0 goes back to the guest stride (A/B). */
            static int al4 = -1;
            if (al4 < 0) { const char* e = getenv("GOWA_VS_ALIGN4"); al4 = !(e && *e == '0'); }
            if (al4 && (f->stride & 3u)) {
                s->stride = (esz + 3u) & ~3u;
                s->bytes = n * s->stride;
            }
            /* GOWA (27/10, reports 35/91: video on half the screen, stretched buttons on
             * RX 580 / RX 6700 XT): the view ended at the last ELEMENT ((n-1)*stride +
             * esz). Older Radeons bound a vertex fetch by whole vertices (size / stride),
             * so the last vertex fell outside and read as zero -- a corner of a quad
             * pulled to the origin. The view now covers n whole strides; the bytes past
             * the last element are zero (ld_vslots_upload copies only the element).
             * GOWA_VS_TAIL=0 goes back to the short view (A/B). */
            static int tail = -1;
            if (tail < 0) { const char* e = getenv("GOWA_VS_TAIL"); tail = !(e && *e == '0'); }
            if (tail && s->stride == s->sstride) s->bytes = n * s->stride;
        } else {
            s->kind = LD_VS_CONV; s->fmt = (u16)DXGI_FORMAT_R32G32B32A32_FLOAT; s->stride = 16; s->bytes = n * 16u;
        }
    }
    g_ld_vs_n = layout->count;
    return 1;
}

static u64 ld_vslots_bytes(void)
{
    u64 t = 0;
    for (u32 s = 0; s < g_ld_vs_n; s++) t += ((u64)g_ld_vs[s].bytes + 15u) & ~15ull;
    return t;
}

/* guest bytes into the ring, each `swap`-byte unit reversed (bytes % swap == 0) */
static void ld_copy_swap(u8* dst, const u8* src, u32 bytes, u32 swap)
{
    if (swap == 1) { memcpy(dst, src, bytes); return; }
#if defined(__AVX2__)
    const __m256i m = swap == 4
        ? _mm256_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12, 3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12)
        : _mm256_setr_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14, 1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);
    u32 i = 0;
    for (; i + 32 <= bytes; i += 32)
        _mm256_storeu_si256((__m256i*)(dst + i), _mm256_shuffle_epi8(_mm256_loadu_si256((const __m256i*)(src + i)), m));
    if (i < bytes) {
        u8 t[32];
        memcpy(t, src + i, bytes - i);
        _mm256_storeu_si256((__m256i*)t, _mm256_shuffle_epi8(_mm256_loadu_si256((const __m256i*)t), m));
        memcpy(dst + i, t, bytes - i);
    }
#else
    for (u32 i = 0; i < bytes; i += swap)
        for (u32 k = 0; k < swap; k++) dst[i + k] = src[i + swap - 1 - k];
#endif
}

/* the draw's slots into the vertex ring at g.vb_used; the views for IASetVertexBuffers */
static int ld_vslots_upload(u32 n)
{
    const D3D12_GPU_VIRTUAL_ADDRESS va = g.vb->lpVtbl->GetGPUVirtualAddress(g.vb) + g.vb_used;
    u8* const dst0 = g.vb_mapped + g.vb_used;
    u64 off = 0;
    for (u32 si = 0; si < g_ld_vs_n; si++) {
        const ld_vslot* s = &g_ld_vs[si];
        u8* dst = dst0 + off;
        if (s->kind == LD_VS_DEF)
            memcpy(dst, dc.fetch_plan.attr[s->attr].default_value, 16);
        else if (s->kind == LD_VS_RAW && s->stride != s->sstride) {
            /* repacked to an aligned stride (ld_vslots_plan): element by element */
            if (s->esz != s->stride) memset(dst, 0, s->bytes);
            for (u32 v = 0, nv = s->bytes / s->stride; v < nv; v++)
                ld_copy_swap(dst + (u64)v * s->stride, s->src + (u64)v * s->sstride, s->esz, s->swap);
        }
        else if (s->kind == LD_VS_RAW) {
            /* the guest bytes end at the last element; a view of n whole strides
             * (GOWA_VS_TAIL) is padded with zeros past it */
            const u32 n_el = s->stride ? (s->bytes - s->esz) / s->stride + 1u : 1u;
            const u32 copy = s->stride ? (n_el - 1u) * s->stride + s->esz : s->bytes;
            ld_copy_swap(dst, s->src, copy, s->swap);
            if (copy < s->bytes) memset(dst + copy, 0, s->bytes - copy);
        }
        else if (!rsx_vertex_fetch_attr_all(&dc.fetch_plan, s->attr, dc.refs, n, dst, 16u))
            return 0;
        g_ld_vs_view[si].BufferLocation = va + off;
        g_ld_vs_view[si].StrideInBytes = s->stride;
        g_ld_vs_view[si].SizeInBytes = s->bytes;
        off += ((u64)s->bytes + 15u) & ~15ull;
    }
    return 1;
}

static void dc_reset(void)
{
    dc.n_arr = dc.n_idx = dc.n_verts = dc.n_refs = 0;
    dc.n_source_refs = 0;
    dc.refs_remapped = 0;
    dc.slot_mode = 0;
    dc.idx_fast = 0;
    dc.n_packets = 0;
    dc.n_cuts = 0;
    dc.fetch_ok = 1;
    dc.inl = NULL; dc.inl_bytes = dc.inl_stride = 0;
}
static void push_vert(const vtx_t* v)
{
    if (dc.n_verts >= dc.cap_verts) {
        u32 nc = dc.cap_verts ? dc.cap_verts * 2 : 4096;
        vtx_t* nv = (vtx_t*)realloc(dc.verts, (size_t)nc * sizeof(vtx_t));
        if (!nv) { dc.fetch_ok = 0; return; }
        dc.verts = nv; dc.cap_verts = nc;
    }
    dc.verts[dc.n_verts++] = *v;
}

static void push_compact_ref(u32 vertex_id, u32 base_index)
{
    if (dc.n_refs >= dc.cap_refs) {
        const u32 next_capacity = dc.cap_refs ? dc.cap_refs * 2u : 4096u;
        rsx_vertex_ref* refs = (rsx_vertex_ref*)realloc(
            dc.refs, (size_t)next_capacity * sizeof(*refs));
        if (!refs) {
            dc.fetch_ok = 0;
            return;
        }
        dc.refs = refs;
        dc.cap_refs = next_capacity;
    }
    dc.refs[dc.n_refs].vertex_id = vertex_id;
    dc.refs[dc.n_refs].base_index = base_index;
    dc.n_refs++;
}

static int reserve_compact_vertices(u64 bytes)
{
    if (bytes <= dc.compact_capacity)
        return 1;
    if (bytes > SIZE_MAX)
        return 0;
    u64 next_capacity = dc.compact_capacity ? dc.compact_capacity : 1u << 20;
    while (next_capacity < bytes) {
        if (next_capacity > SIZE_MAX / 2u)
            return 0;
        next_capacity *= 2u;
    }
    u8* vertices = (u8*)realloc(
        dc.compact_verts, (size_t)next_capacity);
    if (!vertices)
        return 0;
    dc.compact_verts = vertices;
    dc.compact_capacity = next_capacity;
    return 1;
}

static int reserve_legacy_vertices(u32 count)
{
    if (count <= dc.cap_verts)
        return 1;
    u32 next_capacity = dc.cap_verts ? dc.cap_verts : 4096u;
    while (next_capacity < count) {
        if (next_capacity > UINT32_MAX / 2u)
            return 0;
        next_capacity *= 2u;
    }
    vtx_t* vertices = (vtx_t*)realloc(
        dc.verts, (size_t)next_capacity * sizeof(*vertices));
    if (!vertices)
        return 0;
    dc.verts = vertices;
    dc.cap_verts = next_capacity;
    return 1;
}

static float ld_be_f32(const u8* p)
{
    const u32 v = ((u32)p[0] << 24) | ((u32)p[1] << 16) |
                  ((u32)p[2] << 8) | (u32)p[3];
    float f;
    memcpy(&f, &v, 4);
    return f;
}

static float ld_be_f16(const u8* p)
{
    const u16 h = (u16)((p[0] << 8) | p[1]);
    const u32 sign = (u32)(h >> 15) << 31;
    const u32 exp = (h >> 10) & 0x1F;
    const u32 man = h & 0x3FF;
    u32 out;
    if (exp == 0)
        out = sign;
    else if (exp == 31)
        out = sign | 0x7F800000u | (man << 13);
    else
        out = sign | ((exp + 112) << 23) | (man << 13);
    float f;
    memcpy(&f, &out, 4);
    return f;
}

static int fetch_attr(u32 i, u32 base, u32 vertex_id, u32 base_index,
                      float out[4])
{
    rsx_dsp_vertex_attr a;
    rsx_dsp_get_vertex_attr(&g.rsx, i, &a);
    if (!a.type || !a.size) return 0;
    out[0] = out[1] = out[2] = 0.0f;
    out[3] = 1.0f;
    const u32 elem_size = rsx_vertex_attrib_size(a.type, a.size);
    const u32 stride = a.stride ? a.stride : elem_size;
    const u32 divider_mask = rsx_dsp_reg(&g.rsx, 0x1FC0);
    if (a.frequency >= 2)
        g_ld_divider_fetches++;
    const u32 source_element = rsx_vertex_element_index(
        vertex_id, base_index, a.frequency,
        (divider_mask >> i) & 1u);
    const u8* p;
    if (dc.inl) {
        /* Inline vertices ignore the array offset and the data base: the
         * stream IS the array, packed at the declared stride. */
        const u64 at = (u64)source_element * dc.inl_stride + dc.inl_off[i];
        if (at + elem_size > dc.inl_bytes) return 0;
        p = dc.inl + at;
    } else {
        p = guest_ptr(
            a.location, base + a.offset + source_element * stride, elem_size);
    }
    if (!p) return 0;
    for (u32 c = 0; c < a.size && c < 4; c++) {
        switch (a.type) {
        case RSX_VTX_TYPE_FLOAT:
            out[c] = ld_be_f32(p + c * 4);
            break;
        case RSX_VTX_TYPE_HALF:
            out[c] = ld_be_f16(p + c * 2);
            break;
        case RSX_VTX_TYPE_UNORM8:
            out[c] = p[c] / 255.0f;
            break;
        case RSX_VTX_TYPE_UINT8:
            out[c] = (float)p[c];
            break;
        case RSX_VTX_TYPE_SNORM16: {
            const s16 v = (s16)((p[c * 2] << 8) | p[c * 2 + 1]);
            out[c] = v / 32767.0f;
            break;
        }
        case RSX_VTX_TYPE_SINT16: {
            const s16 v = (s16)((p[c * 2] << 8) | p[c * 2 + 1]);
            out[c] = (float)v;
            break;
        }
        case RSX_VTX_TYPE_CMP32: {
            const u32 w = ((u32)p[0] << 24) | ((u32)p[1] << 16) |
                          ((u32)p[2] << 8) | (u32)p[3];
            s32 x = (s32)(w & 0x7FF);          if (x & 0x400) x -= 0x800;
            s32 y = (s32)((w >> 11) & 0x7FF); if (y & 0x400) y -= 0x800;
            s32 z = (s32)((w >> 22) & 0x3FF); if (z & 0x200) z -= 0x400;
            out[0] = (float)x / 1023.0f;
            out[1] = (float)y / 1023.0f;
            out[2] = (float)z / 511.0f;
            out[3] = 1.0f;
            return 1;
        }
        default:
            return 0;
        }
    }
    return 1;
}

static void fetch_one(u32 base, u32 vertex_id, u32 base_index)
{
    vtx_t v;
    for (u32 i = 0; i < 16; i++) {
        rsx_dsp_vertex_attr a;
        rsx_dsp_get_vertex_attr(&g.rsx, i, &a);
        if (a.type && a.size &&
            fetch_attr(i, base, vertex_id, base_index, v.a[i])) continue;
        if (i == 0) { dc.fetch_ok = 0; return; }
        rsx_dsp_vertex_default(&g.rsx, i, v.a[i]);
        if (i == 3 && v.a[3][0] == 0 && v.a[3][1] == 0 && v.a[3][2] == 0 && v.a[3][3] == 1)
            v.a[3][0] = v.a[3][1] = v.a[3][2] = 1.0f;
    }
    push_vert(&v);
}

static void fetch_batches(void)
{
    const u32 base = rsx_dsp_vertex_data_base_offset(&g.rsx);
    for (u32 r = 0; r < dc.n_arr && dc.fetch_ok; r++)
        for (u32 i = 0; i < dc.arr[r].count && dc.fetch_ok; i++)
            fetch_one(base, dc.arr[r].first + i, 0);
    if (!dc.n_idx) return;
    const u32 base_index = rsx_dsp_vertex_data_base_index(&g.rsx);
    rsx_dsp_index_array ia; rsx_dsp_get_index_array(&g.rsx, &ia);
    /* Restart sentinel handling, same rule as the replay harness (RPCS3
     * RSXThread.cpp:398 "if (value == restart) continue" + rsx_methods.h
     * restart_index_enabled()/restart_index()): record a cut, never fetch
     * the phantom vertex. Kill-switch RSX_NO_RESTART shared with the
     * harness for byte-exact A/B. */
    static int s_no_restart = -1;
    if (s_no_restart < 0) s_no_restart = getenv("RSX_NO_RESTART") ? 1 : 0;
    const int restart_en = !s_no_restart && rsx_dsp_restart_index_enabled(&g.rsx, ia.is_u32);
    const u32 restart_val = rsx_dsp_restart_index(&g.rsx);
    for (u32 r = 0; r < dc.n_idx && dc.fetch_ok; r++)
        for (u32 i = 0; i < dc.idx[r].count && dc.fetch_ok; i++) {
            const u32 esz = ia.is_u32 ? 4 : 2;
            const u8* ip = guest_ptr(ia.location, ia.offset + (dc.idx[r].first + i) * esz, esz);
            if (!ip) { dc.fetch_ok = 0; return; }
            const u32 index = ia.is_u32
                ? (((u32)ip[0] << 24) | ((u32)ip[1] << 16) | ((u32)ip[2] << 8) | ip[3])
                : (u32)((ip[0] << 8) | ip[1]);
            if (restart_en && index == restart_val) {
                if (!rsx_restart_cut_push(&dc.cuts, &dc.n_cuts, &dc.cap_cuts,
                                          dc.n_verts)) {
                    dc.fetch_ok = 0;
                    return;
                }
                continue;
            }
            fetch_one(base, index, base_index);
        }
}

/* GOWA (24/10): one u16 index batch of a triangle list whose indices span a range
 * smaller than their count (an Edge segment): min/max straight from guest memory, the
 * unique references are lo..hi, and write_topology_indices writes idx - lo from guest
 * memory again -- no per-index reference, remap or occurrence table (~2.5 million
 * indices a frame in chapter 16, each written and read 3-4 times on the generic path).
 * Same result as the generic decode + dense remap. GOWA_IDX_FAST=0: the generic path.
 * Returns 0, having changed nothing, when the batch does not fit. */
static int ld_idx_fast_on(void)
{
    static int on = -1;
    if (on < 0) { const char* e = getenv("GOWA_IDX_FAST"); on = !(e && *e == '0') && ld_vtx_slots_on(); }
    return on;
}
static int ld_idx_fast_decode(void)
{
    if (!ld_idx_fast_on()) return 0;
    rsx_dsp_index_array ia;
    rsx_dsp_get_index_array(&g.rsx, &ia);
    if (ia.is_u32) return 0;
    const u32 first = dc.idx[0].first, count = dc.idx[0].count;
    if (count < 3) return 0;
    const u64 start64 = (u64)ia.offset + (u64)first * 2u, bytes64 = (u64)count * 2u;
    if (start64 + bytes64 > 0x100000000ull) return 0;
    const u8* src = guest_ptr(ia.location, (u32)start64, (u32)bytes64);
    if (!src) return 0;
    static int no_restart = -1;
    if (no_restart < 0) no_restart = getenv("RSX_NO_RESTART") ? 1 : 0;
    const int chk = !no_restart && rsx_dsp_restart_index_enabled(&g.rsx, 0) && rsx_dsp_restart_index(&g.rsx) <= 0xFFFFu;
    const u32 rst = rsx_dsp_restart_index(&g.rsx) & 0xFFFFu;
    u32 lo = 0xFFFFu, hi = 0, i = 0;
#if defined(__AVX2__)
    {
        const __m256i bsw = _mm256_setr_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14,
                                             1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);
        const __m256i vr = _mm256_set1_epi16((short)rst);
        __m256i vmin = _mm256_set1_epi16(-1), vmax = _mm256_setzero_si256(), bad = _mm256_setzero_si256();
        for (; i + 16 <= count; i += 16) {
            const __m256i w = _mm256_shuffle_epi8(_mm256_loadu_si256((const __m256i*)(src + (size_t)i * 2)), bsw);
            vmin = _mm256_min_epu16(vmin, w);
            vmax = _mm256_max_epu16(vmax, w);
            bad = _mm256_or_si256(bad, _mm256_cmpeq_epi16(w, vr));
        }
        if (chk && !_mm256_testz_si256(bad, bad)) return 0;
        u16 t[16];
        _mm256_storeu_si256((__m256i*)t, vmin);
        for (int k = 0; k < 16; k++) if (i && t[k] < lo) lo = t[k];
        _mm256_storeu_si256((__m256i*)t, vmax);
        for (int k = 0; k < 16; k++) if (i && t[k] > hi) hi = t[k];
    }
#endif
    for (; i < count; i++) {
        const u32 v = (u32)src[(size_t)i * 2] << 8 | src[(size_t)i * 2 + 1];
        if (chk && v == rst) return 0;
        if (v < lo) lo = v;
        if (v > hi) hi = v;
    }
    const u32 range = hi - lo + 1u;
    if (hi < lo || range >= count) return 0;   /* the generic remap hashes those */
    if (range > dc.cap_refs) {
        u32 cap = dc.cap_refs ? dc.cap_refs : 4096u;
        while (cap < range) cap *= 2u;
        rsx_vertex_ref* refs = (rsx_vertex_ref*)realloc(dc.refs, (size_t)cap * sizeof(*refs));
        if (!refs) return 0;
        dc.refs = refs;
        dc.cap_refs = cap;
    }
    const u32 base_index = rsx_dsp_vertex_data_base_index(&g.rsx);
    for (u32 k = 0; k < range; k++) { dc.refs[k].vertex_id = lo + k; dc.refs[k].base_index = base_index; }
    dc.n_refs = range;
    dc.n_source_refs = count;
    dc.refs_remapped = 1;
    g_rsx_remap_dense = 1;
    dc.idx_fast = 1; dc.idx_src = src; dc.idx_lo = lo;
    return 1;
}

static void fetch_batches_hoisted(
    const rsx_vertex_layout_plan* layout, int packed_payload)
{
    if (packed_payload && !dc.n_arr && dc.n_idx == 1 && dc.fetch_ok &&
        g.rsx.current_primitive == 5u /* TRIANGLES */ && ld_idx_fast_decode())
        goto refs_ready;
    for (u32 batch = 0; batch < dc.n_arr && dc.fetch_ok; batch++)
        for (u32 i = 0; i < dc.arr[batch].count && dc.fetch_ok; i++)
            push_compact_ref(dc.arr[batch].first + i, 0);

    if (dc.n_idx && dc.fetch_ok) {
        const u32 base_index = rsx_dsp_vertex_data_base_index(&g.rsx);
        rsx_dsp_index_array index_array;
        rsx_dsp_get_index_array(&g.rsx, &index_array);
        const u32 element_size = index_array.is_u32 ? 4u : 2u;
        static int no_restart = -1;
        if (no_restart < 0)
            no_restart = getenv("RSX_NO_RESTART") ? 1 : 0;
        const int restart_enabled =
            !no_restart &&
            rsx_dsp_restart_index_enabled(&g.rsx, index_array.is_u32);
        const u32 restart_value = rsx_dsp_restart_index(&g.rsx);

        for (u32 batch = 0; batch < dc.n_idx && dc.fetch_ok; batch++) {
            const u32 first = dc.idx[batch].first;
            const u32 count = dc.idx[batch].count;
            const u64 start64 =
                (u64)index_array.offset + (u64)first * element_size;
            const u64 bytes64 = (u64)count * element_size;
            const u8* contiguous = NULL;
            if (count && start64 <= UINT32_MAX &&
                bytes64 <= UINT32_MAX &&
                start64 + bytes64 <= 0x100000000ull) {
                contiguous = guest_ptr(
                    index_array.location, (u32)start64, (u32)bytes64);
            }
            /* GOWA: contiguous indices -- reserve once and decode in a tight
             * loop (the generic one below re-checks capacity, the source
             * pointer and the width for every index; ~8,000 Edge draws per
             * frame in God of War Ascension). Same refs and cuts. */
            if (contiguous && count) {
                if (dc.n_refs + (u64)count > dc.cap_refs) {
                    u32 cap = dc.cap_refs ? dc.cap_refs : 4096u;
                    while ((u64)cap < dc.n_refs + (u64)count) cap *= 2u;
                    rsx_vertex_ref* refs = (rsx_vertex_ref*)realloc(dc.refs, (size_t)cap * sizeof(*refs));
                    if (!refs) { dc.fetch_ok = 0; break; }
                    dc.refs = refs;
                    dc.cap_refs = cap;
                }
                u32 i = 0;
#if defined(__AVX2__)
                /* GOWA (21/10): 16 u16 indices at a time -- byte swap, look for the
                 * restart value, and if the block has none widen to (vertex_id,
                 * base_index) pairs; a block with a restart goes to the scalar loop. */
                if (!index_array.is_u32) {
                    const __m256i bsw = _mm256_setr_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14,
                                                         1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);
                    const int chk = restart_enabled && restart_value <= 0xFFFFu;
                    const __m256i rst = _mm256_set1_epi16((short)(restart_value & 0xFFFFu));
                    const __m256i bvec = _mm256_set1_epi32((int)base_index);
                    for (; i + 16 <= count; i += 16) {
                        const __m256i w = _mm256_shuffle_epi8(
                            _mm256_loadu_si256((const __m256i*)(contiguous + (size_t)i * 2)), bsw);
                        if (chk && _mm256_movemask_epi8(_mm256_cmpeq_epi16(w, rst)))
                            break;
                        rsx_vertex_ref* out = &dc.refs[dc.n_refs];
                        for (int hseg = 0; hseg < 2; hseg++) {
                            const __m256i v = _mm256_cvtepu16_epi32(hseg ? _mm256_extracti128_si256(w, 1)
                                                                          : _mm256_castsi256_si128(w));
                            /* per 128-bit lane: lo = (v0,b,v1,b | v4,b,v5,b), hi = (v2,b,v3,b | v6,b,v7,b) */
                            const __m256i lo = _mm256_unpacklo_epi32(v, bvec);
                            const __m256i hi = _mm256_unpackhi_epi32(v, bvec);
                            _mm256_storeu_si256((__m256i*)(out + hseg * 8), _mm256_permute2x128_si256(lo, hi, 0x20));
                            _mm256_storeu_si256((__m256i*)(out + hseg * 8 + 4), _mm256_permute2x128_si256(lo, hi, 0x31));
                        }
                        dc.n_refs += 16;
                    }
                }
#endif
                for (; i < count; i++) {
                    const u8* source = contiguous + (size_t)i * element_size;
                    const u32 index = index_array.is_u32
                        ? (((u32)source[0] << 24) | ((u32)source[1] << 16) |
                           ((u32)source[2] << 8) | source[3])
                        : (u32)((source[0] << 8) | source[1]);
                    if (restart_enabled && index == restart_value) {
                        if (!rsx_restart_cut_push(&dc.cuts, &dc.n_cuts, &dc.cap_cuts, dc.n_refs)) {
                            dc.fetch_ok = 0;
                            break;
                        }
                        continue;
                    }
                    dc.refs[dc.n_refs].vertex_id = index;
                    dc.refs[dc.n_refs].base_index = base_index;
                    dc.n_refs++;
                }
                continue;
            }
            for (u32 i = 0; i < count && dc.fetch_ok; i++) {
                const u32 source_offset =
                    index_array.offset + (first + i) * element_size;
                const u8* source = contiguous
                    ? contiguous + (size_t)i * element_size
                    : guest_ptr(
                        index_array.location, source_offset, element_size);
                if (!source) {
                    dc.fetch_ok = 0;
                    break;
                }
                const u32 index = index_array.is_u32
                    ? (((u32)source[0] << 24) |
                       ((u32)source[1] << 16) |
                       ((u32)source[2] << 8) | source[3])
                    : (u32)((source[0] << 8) | source[1]);
                if (restart_enabled && index == restart_value) {
                    if (!rsx_restart_cut_push(
                            &dc.cuts, &dc.n_cuts, &dc.cap_cuts,
                            dc.n_refs)) {
                        dc.fetch_ok = 0;
                        break;
                    }
                    continue;
                }
                push_compact_ref(index, base_index);
            }
        }
    }

    if (!dc.fetch_ok)
        return;
    dc.n_source_refs = dc.n_refs;
    if (packed_payload && dc.n_refs > 1) {
        u32 unique_refs = dc.n_refs;
        if (rsx_vertex_remap_build(
                &dc.ref_remap, dc.refs, dc.n_refs, &unique_refs)) {
            dc.refs_remapped = unique_refs < dc.n_refs;
            dc.n_refs = unique_refs;
        }
    }
refs_ready:
    dc.layout = *layout;
    rsx_vertex_fetch_plan_init(
        &dc.fetch_plan, &g.rsx, layout,
        (rsx_vertex_guest_ptr_fn)g.guest_ptr, g.guest_user);
    rsx_vertex_fetch_plan_set_inline(&dc.fetch_plan, &g.rsx, dc.inl, dc.inl_bytes);
    rsx_vertex_fetch_plan_prepare(&dc.fetch_plan, dc.refs, dc.n_refs);
    for (u32 slot = 0; slot < layout->count; slot++) {
        const u32 attr = layout->attrs[slot];
        if (dc.fetch_plan.attr[attr].desc.frequency >= 2)
            g_ld_divider_fetches += dc.n_refs;
    }
    if (packed_payload) {
        const u64 required = (u64)dc.n_refs * layout->stride;
        if (!reserve_compact_vertices(required)) {
            dc.fetch_ok = 0;
            return;
        }
        static int s_fa = -1;
        if (s_fa < 0) s_fa = getenv("RSX_NO_FETCH_ALL") ? 0 : 1;
        /* GOWA (24/10): slot by slot, written straight into the ring by sink_end_impl */
        if (s_fa && layout->stride && ld_vslots_plan(layout)) {
            dc.n_verts = dc.n_refs;
            dc.slot_mode = 1;
            return;
        }
        if (s_fa && layout->stride &&
            rsx_vertex_fetch_all(&dc.fetch_plan, dc.refs, dc.n_refs,
                                 dc.compact_verts, layout->stride)) {
            dc.n_verts = dc.n_refs;
            /* GOWA_VRAW_STATS=1 (21/10): would this draw fit a "raw" upload -- the guest
             * vertex range byte-swapped and read by the input assembler in its own format
             * instead of converted to float4 per attribute on this thread? Counts draws,
             * unique vertices and the span the raw range would upload; histogram of the
             * (type, size) that do not fit. Every 128 frames. Diagnostic only. */
            { static int vrs = -1; if (vrs < 0) vrs = getenv("GOWA_VRAW_STATS") ? 1 : 0;
              if (vrs) {
                  static u32 frame0 = 0xFFFFFFFFu, nfr; static unsigned long long d_all, d_ok, v_all, v_ok, span_ok, attrs_ok;
                  static unsigned long long miss[8][5];
                  if (frame0 != g_ld_frames) {
                      frame0 = g_ld_frames;
                      if (++nfr % 128 == 0) {
                          fprintf(stderr, "[vraw] 128 quadros: desenhos %llu, cabem %llu (%.0f%%); vertices %llu, dos que cabem %llu, faixa crua %llu (%.2fx), atributos/desenho %.1f\n",
                                  d_all, d_ok, d_all ? 100.0 * d_ok / d_all : 0.0, v_all, v_ok, span_ok, v_ok ? (double)span_ok / v_ok : 0.0,
                                  d_ok ? (double)attrs_ok / d_ok : 0.0);
                          char b[400]; int q = snprintf(b, sizeof b, "[vraw]   nao cabem (tipo/tam: desenhos):");
                          for (int t = 0; t < 8; t++) for (int z = 0; z < 5; z++)
                              if (miss[t][z]) { q += snprintf(b + q, sizeof b - q, " %d/%d:%llu", t, z, miss[t][z]); miss[t][z] = 0; }
                          fprintf(stderr, "%s\n", b);
                          d_all = d_ok = v_all = v_ok = span_ok = attrs_ok = 0;
                      }
                  }
                  int ok = 1; u32 lo = 0xFFFFFFFFu, hi = 0, na = 0;
                  for (u32 sl = 0; sl < layout->count; sl++) {
                      const rsx_vertex_fetch_attr* fa = &dc.fetch_plan.attr[layout->attrs[sl]];
                      const u32 t = fa->desc.type, z = fa->desc.size;
                      if (!t || !z) continue;
                      na++;
                      int fits = 0;
                      switch (t) {
                      case RSX_VTX_TYPE_FLOAT: fits = z >= 1 && z <= 4; break;
                      /* DXGI reads these as float: R16_FLOAT.., R16_SNORM.., R8_UNORM.. (no 3-wide
                       * 8/16-bit formats; UINT8/SINT16 would need *SCALED formats DXGI lacks) */
                      case RSX_VTX_TYPE_HALF: case RSX_VTX_TYPE_SNORM16:
                      case RSX_VTX_TYPE_UNORM8: fits = z == 1 || z == 2 || z == 4; break;
                      default: break;
                      }
                      if (fa->desc.frequency >= 2 || !fa->stable_base) fits = 0;
                      if (!fits) { ok = 0; miss[t & 7][z < 5 ? z : 4]++; }
                      if (fa->stable_first < lo) lo = fa->stable_first;
                      if (fa->stable_last > hi) hi = fa->stable_last;
                  }
                  d_all++; v_all += dc.n_refs;
                  if (ok && na) { d_ok++; v_ok += dc.n_refs; span_ok += (hi >= lo) ? hi - lo + 1 : 0; attrs_ok += na; }
              } }
            return;
        }
        for (u32 i = 0; i < dc.n_refs; i++) {
            u8* destination = layout->stride
                ? dc.compact_verts + (u64)i * layout->stride : NULL;
            if (!rsx_vertex_fetch_one(
                    &dc.fetch_plan, &dc.refs[i], destination)) {
                dc.fetch_ok = 0;
                return;
            }
        }
    } else {
        /*
         * H and M deliberately retain the complete legacy payload.  With an
         * all-attribute layout, slot order is register order, so the helper
         * writes exactly the original attribute*16 byte offsets.
         */
        if (!reserve_legacy_vertices(dc.n_refs)) {
            dc.fetch_ok = 0;
            return;
        }
        /* Legacy treats an absent ATTR0 as a failed group, even though other
         * absent attributes use their RSX current/default value. */
        if (dc.n_refs &&
            (!dc.fetch_plan.attr[0].desc.type ||
             !dc.fetch_plan.attr[0].desc.size)) {
            dc.fetch_ok = 0;
            return;
        }
        for (u32 i = 0; i < dc.n_refs; i++) {
            if (!rsx_vertex_fetch_one(
                    &dc.fetch_plan, &dc.refs[i],
                    (u8*)&dc.verts[i])) {
                dc.fetch_ok = 0;
                return;
            }
        }
    }
    dc.n_verts = dc.n_refs;
}

static const float* dc_vertex_attr(
    u32 vertex, u32 attr, float fallback[4])
{
    if (!ld_vertex_compact_payload())
        return dc.verts[vertex].a[attr];
    const s8 slot = dc.layout.slot_by_attr[attr];
    if (slot >= 0 && vertex < dc.n_verts)
        return (const float*)(
            dc.compact_verts + (u64)vertex * dc.layout.stride +
            (u32)slot * 16u);
    memcpy(fallback, dc.fetch_plan.attr[attr].default_value, 16);
    return fallback;
}

/* Diagnostic-only counterpart to dc_vertex_attr().  Compact rendering omits
 * attributes the active vertex program does not read, but the legacy replay
 * fingerprint contains all sixteen RSX attributes.  Re-fetch an omitted,
 * enabled attribute from the original guest reference so a CSV hash compares
 * the producer bytes rather than the two renderers' different upload layouts. */
static const float* dc_vertex_attr_canonical(
    u32 vertex, u32 attr, float fallback[4])
{
    if (!ld_vertex_compact_payload())
        return dc.verts[vertex].a[attr];
    const s8 slot = dc.layout.slot_by_attr[attr];
    if (slot >= 0 && vertex < dc.n_verts)
        return (const float*)(
            dc.compact_verts + (u64)vertex * dc.layout.stride +
            (u32)slot * 16u);

    const rsx_vertex_fetch_attr* fetch = &dc.fetch_plan.attr[attr];
    if (vertex < dc.n_verts && fetch->desc.type && fetch->desc.size &&
        fetch->elem_size) {
        const rsx_vertex_ref* ref = &dc.refs[vertex];
        const u32 element = rsx_vertex_element_index(
            ref->vertex_id, ref->base_index, fetch->desc.frequency,
            (dc.fetch_plan.divider_mask >> attr) & 1u);
        const u64 offset =
            (u64)dc.fetch_plan.base_offset + fetch->desc.offset +
            (u64)element * fetch->stride;
        if (offset <= UINT32_MAX) {
            const u8* source = guest_ptr(
                fetch->desc.location, (u32)offset, fetch->elem_size);
            if (source && rsx_vertex_decode_element(
                    fetch->desc.type, fetch->desc.size,
                    source, fallback))
                return fallback;
        }
    }
    memcpy(fallback, fetch->default_value, 16);
    return fallback;
}

/* primitive ids = raw NV4097 VERTEX_BEGIN_END arg (rsx_dispatch stores it raw;
 * matches the replay harness). These were off by one (4/5/6/7), which dropped
 * EVERY quad/triangle draw through the switch's default: return -> black. */
#define PRIM_TRIANGLES       5
#define PRIM_TRIANGLE_STRIP  6
#define PRIM_TRIANGLE_FAN    7
#define PRIM_QUADS           8

/* Live-draw activity counters (verification: is real geometry flowing, or only
 * clears?). Reported per presented frame in rsx_live_draw_present. */
/* DRAW_ARRAYS/DRAW_INDEX_ARRAY writes are packets. Multiple packets between a
 * single BEGIN/END are deliberately coalesced into one D3D12 DrawInstanced,
 * so comparing packet count directly with executed D3D draws was invalid.
 * Keep two independently balanced ledgers instead:
 *
 *   packets_seen = packets_queued + packets_movie + packets_queue_full
 *   groups_seen  = groups_executed + every group_drop_* outcome
 *
 * An END with no accepted packet is counted separately as groups_empty and is
 * not a render attempt. This makes every early return explicit. */
typedef struct ld_stats {
    unsigned long long packets_seen;
    unsigned long long packets_queued;
    unsigned long long packets_movie;
    unsigned long long packets_queue_full;
    unsigned long long groups_seen;
    unsigned long long groups_empty;
    unsigned long long groups_executed;
    unsigned long long group_drop_fetch;
    unsigned long long group_drop_degenerate;
    unsigned long long group_drop_primitive;
    unsigned long long group_drop_alloc;
    unsigned long long group_drop_pso;
    unsigned long long group_drop_ring;
    unsigned long long group_drop_surface;
    unsigned long long clears;
    unsigned long long clear_drop_surface;
    unsigned long long implicit_depth_clears;
} ld_stats;

static ld_stats g_ld_stats;

void rsx_live_draw_a010_probe_begin(void)
{
    if ((!getenv("RSX_A010_PROBE") &&
         !getenv("RSX_A010_SURFACE_DUMP")) || !g.ready)
        return;
    /*
     * Targeted surface readbacks submit and fence the open D3D12 list. When
     * surface capture alone is requested, defer that cost until AUTH confirms
     * that both the character palette and verified camera are synchronized.
     * Explicit RSX_A010_PROBE retains its scene-open diagnostic behavior.
     */
    if (!getenv("RSX_A010_PROBE") &&
        InterlockedCompareExchange(
            &g_yz_a010_reference_camera_active, 0, 0) == 0)
        return;
    if (g_yz_runtime_config.a010_start_reference) {
        /* Deterministic visual-verification camera derived from the shipped
         * cam_Came_000_002.cmt frame 612. Install it on the RSX thread as soon
         * as the a010 probe opens so a later blocked PPU scene update cannot
         * prevent the reference matrix from reaching the first world draws. */
        static const float reference_matrix_612[16] = {
             3.18890834f,   -0.000270011135f,  0.08452246f,   5.81511511f,
            -0.0102274104f,  5.65674844f,      0.403935942f, -18.5085361f,
             0.0264371686f,  0.0712562195f,   -0.997207931f,  9.99292802f,
             0.0264345249f,  0.0712490938f,   -0.99710821f,  10.0919287f,
        };
        static const float reference_matrix_874[16] = {
             6.80647601f,   -0.00327668415f,  0.611324803f, -36.9135075f,
             0.00523179535f, 12.1491076f,     0.00686819648f, -38.8146474f,
             0.0894642733f,  0.000524588748f,-0.996090306f, -11.7467763f,
             0.0894553268f,  0.000524536289f,-0.995990697f, -11.6456016f,
        };
        const float* reference_matrix =
            g_yz_runtime_config.a010_reference_874
                ? reference_matrix_874 : reference_matrix_612;
        rsx_live_draw_set_a010_camera_matrix(reference_matrix);
    }
    CreateDirectoryA("scratch\\a010_probe", NULL);
    g_ld_a010_probe_start_frame = g_ld_frames;
    g_ld_a010_probe_sample = 0;
    g_ld_a010_probe_touched = 0;
    InterlockedExchange(&g_ld_a010_world_ready, 0);
    InterlockedExchange(&g_ld_a010_probe_active, 1);
    fprintf(stderr,
            "[a010-probe] BEGIN live_frame=%u surfaces=%u packets=%llu groups=%llu\n",
            g_ld_frames, g.n_surfaces, g_ld_stats.packets_seen,
            g_ld_stats.groups_seen);
    fflush(stderr);
}

int rsx_live_draw_a010_probe_active(void)
{
    return __atomic_load_n(&g_ld_a010_probe_active, __ATOMIC_ACQUIRE) != 0;   /* GOWA: no locked read */
}

int rsx_live_draw_a010_world_ready(void)
{
    return __atomic_load_n(&g_ld_a010_world_ready, __ATOMIC_ACQUIRE) != 0;   /* GOWA: no locked read */
}

static unsigned long long ld_groups_accounted(void)
{
    return g_ld_stats.groups_executed + g_ld_stats.group_drop_fetch +
           g_ld_stats.group_drop_degenerate + g_ld_stats.group_drop_primitive +
           g_ld_stats.group_drop_alloc + g_ld_stats.group_drop_pso +
           g_ld_stats.group_drop_ring + g_ld_stats.group_drop_surface;
}

#if defined(YZ_PERF_PROFILE)
static double ld_profile_ticks_ms(u64 ticks)
{
    return g_ld_profile.qpc_frequency
        ? (double)ticks * 1000.0 / (double)g_ld_profile.qpc_frequency
        : 0.0;
}

static void ld_profile_present(u32 frame)
{
    const LONGLONG now = ld_profile_qpc();
    const double frame_ms = ld_profile_ticks_ms(
        (u64)(now - g_ld_profile.previous_present_qpc));
    const ld_profile_counts* total = &g_ld_profile.total;
    const ld_profile_counts* previous = &g_ld_profile.previous;
#define LD_PROFILE_DELTA(field) (total->field - previous->field)
    const u64 packets =
        g_ld_stats.packets_seen - g_ld_profile.previous_packets;
    const u64 groups =
        g_ld_stats.groups_seen - g_ld_profile.previous_groups;
    const u64 executed =
        g_ld_stats.groups_executed - g_ld_profile.previous_executed;
    const u64 evictions =
        g_ld_texture_cache_evictions - g_ld_profile.previous_evictions;
    const u64 refreshes =
        g_ld_vtex_refreshes - g_ld_profile.previous_refreshes;
    const u64 used_attribute_draws =
        LD_PROFILE_DELTA(used_attribute_draws);
    const double used_attribute_average = used_attribute_draws
        ? (double)LD_PROFILE_DELTA(used_attribute_sum) /
              (double)used_attribute_draws
        : 0.0;
    const int compile_free =
        LD_PROFILE_DELTA(vs_compile_calls) == 0 &&
        LD_PROFILE_DELTA(ps_compile_calls) == 0;
    g_ld_profile.measured_frames++;
    g_ld_profile.total_frame_ms += frame_ms;
    if (compile_free) {
        g_ld_profile.compile_free_frames++;
        g_ld_profile.compile_free_frame_ms += frame_ms;
    }
    /*
     * The two matched visual-validation windows are deliberately small.
     * Emit one aggregate line per presented frame there so an improvement
     * below the generic 250 ms slow-frame threshold remains measurable.
     * Scene identity is still established by the run driver's asset marker
     * and screenshots; these values are only run-local capture locators.
     */
    const int matched_window =
        (frame >= 2240u && frame <= 2300u) ||
        (frame >= 4800u && frame <= 4900u);
    const int emit =
        frame <= 16u || matched_window || (frame & 63u) == 0u ||
        frame_ms >= 250.0 ||
        LD_PROFILE_DELTA(pso_misses) != 0 ||
        LD_PROFILE_DELTA(pso_full) != 0 ||
        LD_PROFILE_DELTA(flush_reason[LD_FLUSH_VERTEX_RING]) != 0 ||
        LD_PROFILE_DELTA(flush_reason[LD_FLUSH_RETIRE_QUEUE]) != 0 ||
        LD_PROFILE_DELTA(
            flush_reason[LD_FLUSH_PIXEL_CONSTANT_RING]) != 0 ||
        LD_PROFILE_DELTA(ps_constant_capacity_failures) != 0;

    if (emit) {
        DXGI_QUERY_VIDEO_MEMORY_INFO memory = {0};
        if (g_ld_profile.adapter)
            g_ld_profile.adapter->lpVtbl->QueryVideoMemoryInfo(
                g_ld_profile.adapter, 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL,
                &memory);
        fprintf(stderr,
                "[rsx-perf] frame=%u dt_ms=%.3f "
                "pso{look=%llu hit=%llu miss=%llu miss_total=%llu "
                "probes=%llu full=%llu cached=%u capacity=%u mem_kb=%.1f} "
                "reject{requests=%llu unique=%u "
                "world=%llu/%u character=%llu/%u "
                "ui=%llu/%u other=%llu/%u} "
                "hlsl{vs_lookup=%llu vs_hit=%llu vs_compile=%llu "
                "vs_new=%llu vs_unique=%u ps_lookup=%llu ps_hit=%llu "
                "ps_compile=%llu ps_new=%llu ps_unique=%u} "
                "time_ms{decompile=%.3f vs_compile=%.3f ps_compile=%.3f "
                "create_pso=%.3f flush=%.3f fence=%.3f draw_cpu=%.3f} "
                "vertex{path=%s used_avg=%.3f used_draws=%llu used_max=%u "
                "legacy_mb=%.3f actual_mb=%.3f fetch_pack_ms=%.3f} "
                "vb_sync{flushes=%llu flush_ms=%.3f wait_ms=%.3f} "
                "frame{compile_free=%u} "
                "draw{packets=%llu groups=%llu exec=%llu "
                "input_v=%llu expanded_v=%llu vb_mb=%.3f} "
                "flush{present=%llu ref=%llu vb=%llu retire=%llu "
                "movie=%llu movie_present=%llu readback=%llu} "
                "tex{cached=%u evict=%llu refresh=%llu upload_mb=%.3f "
                "decode=%llu decode_ms=%.3f upload_hi_mb=%.3f "
                "retired_hi=%u} "
                "rings{vb_hi_mb=%.3f upload_total_hi_mb=%.3f "
                "vb_total_hi_mb=%.3f retired_total_hi=%u} "
                "vram{usage_mb=%.1f budget_mb=%.1f reservation_mb=%.1f}\n",
                frame, frame_ms,
                (unsigned long long)LD_PROFILE_DELTA(pso_lookups),
                (unsigned long long)LD_PROFILE_DELTA(pso_hits),
                (unsigned long long)LD_PROFILE_DELTA(pso_misses),
                (unsigned long long)total->pso_misses,
                (unsigned long long)LD_PROFILE_DELTA(pso_probes),
                (unsigned long long)LD_PROFILE_DELTA(pso_full),
                g.n_psos,
                MAX_PSOS,
                (double)sizeof(g.psos) / 1024.0,
                (unsigned long long)total->pso_full,
                g_ld_profile.n_rejected_pso_keys,
                (unsigned long long)
                    g_ld_profile.rejected_pso_requests[LD_REJECT_WORLD],
                g_ld_profile.rejected_pso_unique[LD_REJECT_WORLD],
                (unsigned long long)
                    g_ld_profile.rejected_pso_requests[LD_REJECT_CHARACTER],
                g_ld_profile.rejected_pso_unique[LD_REJECT_CHARACTER],
                (unsigned long long)
                    g_ld_profile.rejected_pso_requests[LD_REJECT_UI],
                g_ld_profile.rejected_pso_unique[LD_REJECT_UI],
                (unsigned long long)
                    g_ld_profile.rejected_pso_requests[LD_REJECT_OTHER],
                g_ld_profile.rejected_pso_unique[LD_REJECT_OTHER],
                (unsigned long long)LD_PROFILE_DELTA(vs_blob_lookups),
                (unsigned long long)LD_PROFILE_DELTA(vs_blob_hits),
                (unsigned long long)LD_PROFILE_DELTA(vs_compile_calls),
                (unsigned long long)LD_PROFILE_DELTA(vs_unique),
                g_ld_profile.n_vs_hashes,
                (unsigned long long)LD_PROFILE_DELTA(ps_blob_lookups),
                (unsigned long long)LD_PROFILE_DELTA(ps_blob_hits),
                (unsigned long long)LD_PROFILE_DELTA(ps_compile_calls),
                (unsigned long long)LD_PROFILE_DELTA(ps_unique),
                g_ld_profile.n_ps_hashes,
                ld_profile_ticks_ms(LD_PROFILE_DELTA(decompile_qpc)),
                ld_profile_ticks_ms(LD_PROFILE_DELTA(vs_compile_qpc)),
                ld_profile_ticks_ms(LD_PROFILE_DELTA(ps_compile_qpc)),
                ld_profile_ticks_ms(LD_PROFILE_DELTA(create_pso_qpc)),
                ld_profile_ticks_ms(LD_PROFILE_DELTA(flush_qpc)),
                ld_profile_ticks_ms(LD_PROFILE_DELTA(fence_wait_qpc)),
                ld_profile_ticks_ms(LD_PROFILE_DELTA(sink_end_qpc)),
                ld_vertex_mode_name(),
                used_attribute_average,
                (unsigned long long)used_attribute_draws,
                g_ld_profile.frame_used_attribute_max,
                (double)LD_PROFILE_DELTA(legacy_vertex_upload_bytes) /
                    (1024.0 * 1024.0),
                (double)LD_PROFILE_DELTA(vertex_upload_bytes) /
                    (1024.0 * 1024.0),
                ld_profile_ticks_ms(
                    LD_PROFILE_DELTA(vertex_fetch_pack_qpc)),
                (unsigned long long)
                    LD_PROFILE_DELTA(
                        flush_reason[LD_FLUSH_VERTEX_RING]),
                ld_profile_ticks_ms(
                    LD_PROFILE_DELTA(
                        flush_reason_qpc[LD_FLUSH_VERTEX_RING])),
                ld_profile_ticks_ms(
                    LD_PROFILE_DELTA(
                        fence_reason_qpc[LD_FLUSH_VERTEX_RING])),
                compile_free ? 1u : 0u,
                (unsigned long long)packets,
                (unsigned long long)groups,
                (unsigned long long)executed,
                (unsigned long long)LD_PROFILE_DELTA(input_vertices),
                (unsigned long long)LD_PROFILE_DELTA(expanded_vertices),
                (double)LD_PROFILE_DELTA(vertex_upload_bytes) /
                    (1024.0 * 1024.0),
                (unsigned long long)
                    LD_PROFILE_DELTA(flush_reason[LD_FLUSH_PRESENT]),
                (unsigned long long)
                    LD_PROFILE_DELTA(flush_reason[LD_FLUSH_GUEST_REFERENCE]),
                (unsigned long long)
                    LD_PROFILE_DELTA(flush_reason[LD_FLUSH_VERTEX_RING]),
                (unsigned long long)
                    LD_PROFILE_DELTA(flush_reason[LD_FLUSH_RETIRE_QUEUE]),
                (unsigned long long)
                    LD_PROFILE_DELTA(flush_reason[LD_FLUSH_MOVIE]),
                (unsigned long long)
                    LD_PROFILE_DELTA(flush_reason[LD_FLUSH_MOVIE_PRESENT]),
                (unsigned long long)
                    LD_PROFILE_DELTA(flush_reason[LD_FLUSH_READBACK]),
                g.n_textures,
                (unsigned long long)evictions,
                (unsigned long long)refreshes,
                (double)LD_PROFILE_DELTA(texture_upload_bytes) /
                    (1024.0 * 1024.0),
                (unsigned long long)
                    LD_PROFILE_DELTA(texture_decode_calls),
                ld_profile_ticks_ms(
                    LD_PROFILE_DELTA(texture_decode_qpc)),
                (double)g_ld_profile.frame_upload_high /
                    (1024.0 * 1024.0),
                g_ld_profile.frame_retired_high,
                (double)g_ld_profile.frame_vb_high /
                    (1024.0 * 1024.0),
                (double)g_ld_profile.total_upload_high /
                    (1024.0 * 1024.0),
                (double)g_ld_profile.total_vb_high /
                    (1024.0 * 1024.0),
                g_ld_profile.total_retired_high,
                (double)memory.CurrentUsage / (1024.0 * 1024.0),
                (double)memory.Budget / (1024.0 * 1024.0),
                (double)memory.CurrentReservation / (1024.0 * 1024.0));
        fprintf(stderr,
                "[shader-cache-perf] frame=%u "
                "vs{count=%u capacity=%u lookups=%llu hits=%llu "
                "misses=%llu inserts=%llu full_rejects=%llu "
                "compile_calls=%llu lookup_ms=%.3f compile_ms=%.3f "
                "post2048_distinct=%llu post2048_repeats=%llu "
                "source_bytes=%llu blob_bytes=%llu} "
                "ps{count=%u capacity=%u lookups=%llu hits=%llu "
                "misses=%llu inserts=%llu full_rejects=%llu "
                "compile_calls=%llu lookup_ms=%.3f compile_ms=%.3f "
                "post2048_distinct=%llu post2048_repeats=%llu "
                "source_bytes=%llu blob_bytes=%llu}\n",
                frame,
                g.vs_blobs.count, MAX_SHADER_BLOBS,
                (unsigned long long)total->vs_blob_lookups,
                (unsigned long long)total->vs_blob_hits,
                (unsigned long long)total->vs_blob_misses,
                (unsigned long long)total->vs_blob_inserts,
                (unsigned long long)total->vs_blob_full_rejects,
                (unsigned long long)total->vs_compile_calls,
                ld_profile_ticks_ms(total->vs_blob_lookup_qpc),
                ld_profile_ticks_ms(total->vs_compile_qpc),
                (unsigned long long)total->vs_post_boundary_distinct,
                (unsigned long long)total->vs_post_boundary_repeats,
                (unsigned long long)g.vs_blobs.retained_source_bytes,
                (unsigned long long)g.vs_blobs.retained_blob_bytes,
                g.ps_blobs.count, MAX_SHADER_BLOBS,
                (unsigned long long)total->ps_blob_lookups,
                (unsigned long long)total->ps_blob_hits,
                (unsigned long long)total->ps_blob_misses,
                (unsigned long long)total->ps_blob_inserts,
                (unsigned long long)total->ps_blob_full_rejects,
                (unsigned long long)total->ps_compile_calls,
                ld_profile_ticks_ms(total->ps_blob_lookup_qpc),
                ld_profile_ticks_ms(total->ps_compile_qpc),
                (unsigned long long)total->ps_post_boundary_distinct,
                (unsigned long long)total->ps_post_boundary_repeats,
                 (unsigned long long)g.ps_blobs.retained_source_bytes,
                 (unsigned long long)g.ps_blobs.retained_blob_bytes);
        const u32 collapsed_by_constants =
            g_ld_profile.n_ps_constant_specialized_hashes >=
                    g_ld_profile.n_ps_canonical_hashes
                ? g_ld_profile.n_ps_constant_specialized_hashes -
                      g_ld_profile.n_ps_canonical_hashes
                : 0;
        const u32 collapsed_by_alpha =
            g_ld_profile.n_ps_exact_source_hashes >=
                    g_ld_profile.n_ps_constant_specialized_hashes
                ? g_ld_profile.n_ps_exact_source_hashes -
                      g_ld_profile.n_ps_constant_specialized_hashes
                : 0;
        fprintf(
            stderr,
            "[fp-constant-perf] frame=%u mode=%s "
            "variants{exact_source=%u constant_specialized=%u "
            "canonical=%u collapsed_constants=%u collapsed_alpha=%u "
            "tracking_overflow=%llu} "
            "cb{allocations=%llu upload_bytes=%llu upload_ms=%.3f "
            "ring_used=%u ring_capacity=%u capacity_failures=%llu "
            "flushes=%llu flush_ms=%.3f wait_ms=%.3f}\n",
            frame, ld_fp_constant_mode_name(),
            g_ld_profile.n_ps_exact_source_hashes,
            g_ld_profile.n_ps_constant_specialized_hashes,
            g_ld_profile.n_ps_canonical_hashes,
            collapsed_by_constants, collapsed_by_alpha,
            (unsigned long long)
                g_ld_profile.ps_variant_tracking_overflow,
            (unsigned long long)
                LD_PROFILE_DELTA(ps_constant_allocations),
            (unsigned long long)
                LD_PROFILE_DELTA(ps_constant_upload_bytes),
            ld_profile_ticks_ms(
                LD_PROFILE_DELTA(ps_constant_upload_qpc)),
            g.ps_cb_used, PS_CB_RING_BYTES,
            (unsigned long long)
                LD_PROFILE_DELTA(ps_constant_capacity_failures),
            (unsigned long long)LD_PROFILE_DELTA(
                flush_reason[LD_FLUSH_PIXEL_CONSTANT_RING]),
            ld_profile_ticks_ms(LD_PROFILE_DELTA(
                flush_reason_qpc[LD_FLUSH_PIXEL_CONSTANT_RING])),
            ld_profile_ticks_ms(LD_PROFILE_DELTA(
                fence_reason_qpc[LD_FLUSH_PIXEL_CONSTANT_RING])));
        fflush(stderr);
    }

    g_ld_profile.previous = g_ld_profile.total;
    g_ld_profile.previous_packets = g_ld_stats.packets_seen;
    g_ld_profile.previous_groups = g_ld_stats.groups_seen;
    g_ld_profile.previous_executed = g_ld_stats.groups_executed;
    g_ld_profile.previous_evictions = g_ld_texture_cache_evictions;
    g_ld_profile.previous_refreshes = g_ld_vtex_refreshes;
    g_ld_profile.previous_present_qpc = now;
    g_ld_profile.frame_upload_high = 0;
    g_ld_profile.frame_vb_high = 0;
    g_ld_profile.frame_retired_high = 0;
    g_ld_profile.frame_used_attribute_max = 0;
#undef LD_PROFILE_DELTA
}
#endif

static int ld_target_trace_enabled(void)
{
    static int enabled = -1;
    if (enabled < 0) enabled = getenv("RSX_TARGET_TRACE") ? 1 : 0;
    return enabled;
}

static void ld_trace_target(const char* event, u32 target, u32 mask)
{
    if (!ld_target_trace_enabled()) return;
    rsx_dsp_surface sf;
    rsx_dsp_get_surface(&g.rsx, &sf);
    fprintf(stderr,
            "[rsx-target] frame=%u event=%s target=%u loc=%u off=0x%08X "
            "fmt=0x%X pitch=%u clip=%u,%u+%ux%u mask=0x%02X\n",
            g_ld_frames, event, target, sf.color_location[0], sf.color_offset[0],
            sf.color_format, sf.color_pitch[0], sf.clip_x, sf.clip_y,
            sf.clip_w, sf.clip_h, mask);
}

/* Movie mode: while a host-decoded movie owns the window
 * (rsx_live_draw_present_rgba), do not make the default boot process the
 * guest's otherwise invisible RSX stream.  That experiment materially changes
 * CRI/movie handoff scheduling and regressed the proven title/menu path.
 * RSX_MOVIE_TRACK keeps the state-tracking experiment available for a
 * focused post-movie A/B once the transition itself is deterministic. */
static volatile int g_ld_movie_mode = 0;
static int g_ld_movie_track_rsx = -1;
static int g_ld_movie_composite_ui = -1;

#if defined(YZ_PERF_PROFILE)
static int ld_vp_has_indexed_constants(const u8* ucode, u32 instrs)
{
    for (u32 i = 0; i < instrs; i++) {
        const u8* p = ucode + i * 16u + 12u;
        const u32 d3 = (u32)p[0] | ((u32)p[1] << 8) |
                       ((u32)p[2] << 16) | ((u32)p[3] << 24);
        if ((d3 >> 1) & 1u)
            return 1;
    }
    return 0;
}

static ld_reject_class ld_classify_rejected_pso(u32 target)
{
    const u32 start = g_ld_current_pso.vp_start;
    const u8* vp_uc = start < RSX_DSP_VP_INSTR
        ? (const u8*)(g.rsx.vp + start * 4)
        : NULL;
    if (vp_uc && ld_vp_has_indexed_constants(
            vp_uc, g_ld_current_pso.vp_instrs))
        return LD_REJECT_CHARACTER;

    render_state_t rs;
    decode_render_state(&rs);
    if (target < g.n_surfaces) {
        const surface_t* surface = &g.surfaces[target];
        int scanout = 0;
        for (u32 i = 0; i < 8; i++) {
            const display_buffer_t* display = &g.display_buffers[i];
            if (display->valid &&
                display->location == surface->location &&
                display->offset == surface->offset) {
                scanout = 1;
                break;
            }
        }
        if (scanout && !rs.depth_test && rs.blend_enable)
            return LD_REJECT_UI;
        if (rs.depth_test ||
            surface->offset == 0x00E40000u ||
            surface->offset == 0x01800000u)
            return LD_REJECT_WORLD;
    }
    return LD_REJECT_OTHER;
}

static void ld_profile_note_rejected_pso(void)
{
    if (!g_ld_current_pso.valid || g.n_psos < MAX_PSOS)
        return;

    rsx_dsp_surface rsx_surface;
    rsx_dsp_get_surface(&g.rsx, &rsx_surface);
    u32 target = LD_INVALID_SURFACE;
    for (u32 i = 0; i < g.n_surfaces; i++) {
        if (g.surfaces[i].location == rsx_surface.color_location[0] &&
            g.surfaces[i].offset == rsx_surface.color_offset[0]) {
            target = i;
            break;
        }
    }
    const ld_reject_class draw_class = ld_classify_rejected_pso(target);
    const u8 class_bit = (u8)(1u << (u32)draw_class);
    g_ld_profile.rejected_pso_requests[draw_class]++;

    u32 key_index = g_ld_profile.n_rejected_pso_keys;
    for (u32 i = 0; i < g_ld_profile.n_rejected_pso_keys; i++) {
        if (g_ld_profile.rejected_pso_keys[i] ==
            g_ld_current_pso.key) {
            key_index = i;
            break;
        }
    }
    if (key_index == g_ld_profile.n_rejected_pso_keys) {
        if (key_index >= MAX_REJECTED_PSO_KEYS)
            return;
        g_ld_profile.rejected_pso_keys[key_index] =
            g_ld_current_pso.key;
        g_ld_profile.rejected_pso_class_masks[key_index] = 0;
        g_ld_profile.n_rejected_pso_keys++;
    }
    if (!(g_ld_profile.rejected_pso_class_masks[key_index] & class_bit)) {
        g_ld_profile.rejected_pso_class_masks[key_index] |= class_bit;
        g_ld_profile.rejected_pso_unique[draw_class]++;
    }

    if (!g_ld_profile.first_pso_full_frame) {
        g_ld_profile.first_pso_full_frame = g_ld_frames;
        const u32 surface_offset = target < g.n_surfaces
            ? g.surfaces[target].offset
            : 0;
        fprintf(stderr,
                "[pso-capacity] reached count=%u capacity=%u "
                "movie=%d orphanage_world_ready=%d "
                "first_rejected_key=%016llx class=%u "
                "surface=0x%08X frame_locator=%u\n",
                g.n_psos, MAX_PSOS, g_ld_movie_mode,
                InterlockedCompareExchange(
                    &g_ld_a010_world_ready, 0, 0) != 0,
                (unsigned long long)g_ld_current_pso.key,
                (u32)draw_class, surface_offset, g_ld_frames);
        fflush(stderr);
    }
}
#endif

static int ld_movie_composite_ui_enabled(void)
{
    if (g_ld_movie_composite_ui < 0)
        g_ld_movie_composite_ui = getenv("RSX_MOVIE_COMPOSITE_UI") ? 1 : 0;
    return g_ld_movie_composite_ui;
}

static void ld_movie_reset_rings(void)
{
    g.vb_used = 0;
    g.ib_used = 0;
    g.cb_used = 0;
    g.ps_cb_used = 0;
    g.srv_ring_used = 0;
    g.smp_ring_used = 0;
    g.depth_cleared = 0;
}

static int ld_movie_overlay_ensure(void)
{
    if (g.movie_upload && g.movie_upload_mapped &&
        g.movie_overlay_readback && g.movie_overlay_rgba &&
        g.movie_overlay_mask)
        return 1;

    g.movie_overlay_pitch = (LD_PX(g.width) * 4 + 255) & ~255u;   /* the scaled surface (GOWA_RES_SCALE) */
    const UINT64 rb_size = (UINT64)g.movie_overlay_pitch * LD_PX(g.height);
    D3D12_HEAP_PROPERTIES hp = {0};
    D3D12_RESOURCE_DESC bd = {0};
    hp.Type = D3D12_HEAP_TYPE_READBACK;
    bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bd.Width = rb_size;
    bd.Height = 1;
    bd.DepthOrArraySize = 1;
    bd.MipLevels = 1;
    bd.Format = DXGI_FORMAT_UNKNOWN;
    bd.SampleDesc.Count = 1;
    bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if (FAILED(g.dev->lpVtbl->CreateCommittedResource(
            g.dev, &hp, D3D12_HEAP_FLAG_NONE, &bd,
            D3D12_RESOURCE_STATE_COPY_DEST, NULL,
            &IID_ID3D12Resource, (void**)&g.movie_overlay_readback))) {
        fprintf(stderr, "[movie-ui] readback allocation failed\n");
        return 0;
    }
    g.movie_overlay_rgba = (u8*)malloc((size_t)g.width * g.height * 4);
    g.movie_overlay_mask = (u8*)malloc((size_t)g.width * g.height);
    if (!g.movie_overlay_rgba || !g.movie_overlay_mask) {
        free(g.movie_overlay_rgba);
        free(g.movie_overlay_mask);
        g.movie_overlay_rgba = NULL;
        g.movie_overlay_mask = NULL;
        g.movie_overlay_readback->lpVtbl->Release(g.movie_overlay_readback);
        g.movie_overlay_readback = NULL;
        fprintf(stderr, "[movie-ui] CPU overlay allocation failed\n");
        return 0;
    }

    /* Do not reuse the general guest upload ring for host frames. Guest draw
     * commands recorded between flips can still reference that memory; a
     * dedicated upload buffer lets host presentation append to the same
     * command list without a second fence/wait on every movie frame. */
    hp.Type = D3D12_HEAP_TYPE_UPLOAD;
    if (FAILED(g.dev->lpVtbl->CreateCommittedResource(
            g.dev, &hp, D3D12_HEAP_FLAG_NONE, &bd,
            D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
            &IID_ID3D12Resource, (void**)&g.movie_upload))) {
        free(g.movie_overlay_rgba);
        free(g.movie_overlay_mask);
        g.movie_overlay_rgba = NULL;
        g.movie_overlay_mask = NULL;
        g.movie_overlay_readback->lpVtbl->Release(g.movie_overlay_readback);
        g.movie_overlay_readback = NULL;
        fprintf(stderr, "[movie-ui] host upload allocation failed\n");
        return 0;
    }
    D3D12_RANGE no_read = {0, 0};
    if (FAILED(g.movie_upload->lpVtbl->Map(
            g.movie_upload, 0, &no_read, (void**)&g.movie_upload_mapped))) {
        g.movie_upload->lpVtbl->Release(g.movie_upload);
        g.movie_upload = NULL;
        free(g.movie_overlay_rgba);
        free(g.movie_overlay_mask);
        g.movie_overlay_rgba = NULL;
        g.movie_overlay_mask = NULL;
        g.movie_overlay_readback->lpVtbl->Release(g.movie_overlay_readback);
        g.movie_overlay_readback = NULL;
        fprintf(stderr, "[movie-ui] host upload map failed\n");
        return 0;
    }
    return 1;
}

/* Start movies from a known transparent guest surface. This removes the stale
 * pre-movie frame before the game begins drawing captions, while leaving the
 * swap chain entirely owned by the 30 Hz host presenter. */
static void ld_movie_overlay_begin(void)
{
    g.movie_overlay_valid = 0;
    g.movie_overlay_frames = 0;
    if (!ld_movie_overlay_ensure()) return;

    ld_flush(LD_FLUSH_MOVIE);
    const float transparent[4] = {0, 0, 0, 0};
    for (u32 i = 0; i < g.n_surfaces; i++) {
        g.list->lpVtbl->ClearRenderTargetView(
            g.list, rtv_handle(LD_SWAP_BUFFERS + i), transparent, 0, NULL);
        ld_surface_note_write(i, LD_SURFACE_WRITE_OTHER);
    }
    if (g.n_surfaces) ld_flush(LD_FLUSH_MOVIE);
    ld_movie_reset_rings();
    fprintf(stderr, "[movie-ui] compositor armed (%ux%u, %u guest surfaces)\n",
            g.width, g.height, g.n_surfaces);
    fflush(stderr);
}

/* Capture the guest's latest offscreen result without presenting it. The
 * guest target is not a true transparent overlay: depending on the auth
 * sequence it can contain black, a fade, or a complete rendered scene.
 * Extract only low-saturation bright glyphs from the subtitle-safe lower band
 * and synthesize a small dark outline. The decoded movie therefore remains the
 * background even when the guest rendered an opaque full-screen image. */
static void ld_movie_capture_overlay(void)
{
    if (!ld_movie_overlay_ensure()) return;
    const u32 target = current_surface();
    if (target == LD_INVALID_SURFACE) {
        g.movie_overlay_valid = 0;
        return;
    }
    ID3D12Resource* rt = g.surfaces[target].tex;
    D3D12_RESOURCE_BARRIER b = {0};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = rt;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    g.list->lpVtbl->ResourceBarrier(g.list, 1, &b);

    D3D12_TEXTURE_COPY_LOCATION src = {0}, dst = {0};
    src.pResource = rt;
    src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.pResource = g.movie_overlay_readback;
    dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    dst.PlacedFootprint.Footprint.Width = LD_PX(g.width);
    dst.PlacedFootprint.Footprint.Height = LD_PX(g.height);
    dst.PlacedFootprint.Footprint.Depth = 1;
    dst.PlacedFootprint.Footprint.RowPitch = g.movie_overlay_pitch;
    g.list->lpVtbl->CopyTextureRegion(g.list, &dst, 0, 0, 0, &src, NULL);

    b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    g.list->lpVtbl->ResourceBarrier(g.list, 1, &b);
    ld_flush(LD_FLUSH_MOVIE);

    const SIZE_T rb_size = (SIZE_T)g.movie_overlay_pitch * LD_PX(g.height);
    u8* mapped = NULL;
    D3D12_RANGE rr = {0, rb_size};
    if (FAILED(g.movie_overlay_readback->lpVtbl->Map(
            g.movie_overlay_readback, 0, &rr, (void**)&mapped))) {
        g.movie_overlay_valid = 0;
        return;
    }
    if (g.res_scale == 1)
    for (u32 y = 0; y < g.height; y++)
        memcpy(g.movie_overlay_rgba + (size_t)y * g.width * 4,
               mapped + (size_t)y * g.movie_overlay_pitch,
               (size_t)g.width * 4);
    else   /* scaled surface: one sample per guest pixel */
    for (u32 y = 0; y < g.height; y++)
        for (u32 x = 0; x < g.width; x++)
            memcpy(g.movie_overlay_rgba + ((size_t)y * g.width + x) * 4,
                   mapped + (size_t)LD_PX(y) * g.movie_overlay_pitch + (size_t)LD_PX(x) * 4, 4);
    D3D12_RANGE wr = {0, 0};
    g.movie_overlay_readback->lpVtbl->Unmap(
        g.movie_overlay_readback, 0, &wr);

    const u64 total = (u64)g.width * g.height;
    const u32 band_y0 = g.height * 52 / 100;
    const u32 band_y1 = g.height * 96 / 100;
    const u32 band_x0 = g.width * 5 / 100;
    const u32 band_x1 = g.width * 95 / 100;
    const u64 band_pixels =
        (u64)(band_y1 - band_y0) * (band_x1 - band_x0);
    u64 glyph_pixels = 0;
    memset(g.movie_overlay_mask, 0, (size_t)total);

    for (u32 y = band_y0; y < band_y1; y++) {
        for (u32 x = band_x0; x < band_x1; x++) {
            const u64 i = (u64)y * g.width + x;
            const u8* p = g.movie_overlay_rgba + i * 4;
            const int hi = p[0] > p[1]
                ? (p[0] > p[2] ? p[0] : p[2])
                : (p[1] > p[2] ? p[1] : p[2]);
            const int lo = p[0] < p[1]
                ? (p[0] < p[2] ? p[0] : p[2])
                : (p[1] < p[2] ? p[1] : p[2]);
            if (hi < 145 || hi - lo > 52)
                continue;
            int coverage = (hi - 96) * 255 / 159;
            if (coverage < 64) coverage = 64;
            if (coverage > 255) coverage = 255;
            g.movie_overlay_mask[i] = (u8)coverage;
            glyph_pixels++;
        }
    }

    /* A value of 1 denotes the synthetic outline; 64..255 are glyph
     * coverage. Work from a copy condition (>=64) so dilation does not grow
     * recursively. */
    if (glyph_pixels >= 8 && glyph_pixels < band_pixels / 6) {
        for (u32 y = band_y0; y < band_y1; y++) {
            for (u32 x = band_x0; x < band_x1; x++) {
                const u64 i = (u64)y * g.width + x;
                if (g.movie_overlay_mask[i] < 64)
                    continue;
                const u32 ya = y > 1 ? y - 2 : 0;
                const u32 yb = y + 2 < g.height ? y + 2 : g.height - 1;
                const u32 xa = x > 1 ? x - 2 : 0;
                const u32 xb = x + 2 < g.width ? x + 2 : g.width - 1;
                for (u32 oy = ya; oy <= yb; oy++)
                    for (u32 ox = xa; ox <= xb; ox++) {
                        u8* m = &g.movie_overlay_mask[(u64)oy * g.width + ox];
                        if (!*m) *m = 1;
                    }
            }
        }
    }
    g.movie_overlay_valid =
        glyph_pixels >= 8 && glyph_pixels < band_pixels / 6;
    g.movie_overlay_frames++;
    if (g.movie_overlay_frames <= 16 ||
        (g.movie_overlay_frames & 63) == 0 ||
        (!g.movie_overlay_valid && glyph_pixels)) {
        fprintf(stderr,
                "[movie-ui] overlay=%llu glyphs=%llu/%llu %s\n",
                (unsigned long long)g.movie_overlay_frames,
                (unsigned long long)glyph_pixels,
                (unsigned long long)band_pixels,
                g.movie_overlay_valid ? "accepted" : "rejected");
        fflush(stderr);
    }

    /* A guest flip still marks a new texture-generation boundary even though
     * the guest surface is not sent to the swap chain. */
    g_ld_frames++;
    ld_movie_reset_rings();
}

static void sink_begin(void* u, const rsx_dispatch* r, u32 prim) { (void)u; (void)r; (void)prim; dc_reset(); }
static void sink_draw_arrays(void* u, const rsx_dispatch* r, u32 first, u32 count)
{
    (void)u; (void)r; g_ld_stats.packets_seen++;
    if (g_ld_movie_mode && !ld_movie_composite_ui_enabled()) {
        g_ld_stats.packets_movie++;
        return;
    }
    dc.n_packets++;
    if (dc.n_arr >= 256) { g_ld_stats.packets_queue_full++; return; }
    dc.arr[dc.n_arr].first = first; dc.arr[dc.n_arr].count = count; dc.n_arr++;
    g_ld_stats.packets_queued++;
}
/* INLINE_ARRAY arrives as one finished stream instead of a (first, count)
 * range, so plan the layout here -- offsets and stride come from the VTXFMT
 * declaration -- and queue it as an ordinary consecutive batch. Everything
 * downstream (topology, textures, PSO, upload) is then identical to a draw
 * out of a vertex array, because by then it IS one. */
static void sink_inline_array(void* u, const rsx_dispatch* r,
                              const u8* data, u32 bytes)
{
    (void)u; (void)r;
    g_ld_stats.packets_seen++;
    if (g_ld_movie_mode && !ld_movie_composite_ui_enabled()) {
        g_ld_stats.packets_movie++;
        return;
    }

    const u32 stride = rsx_vertex_inline_layout(&g.rsx, dc.inl_off);
    if (!stride || bytes < stride) { g_ld_stats.packets_queue_full++; return; }

    dc.inl = data; dc.inl_bytes = bytes; dc.inl_stride = stride;
    dc.n_packets++;
    if (dc.n_arr >= 256) { g_ld_stats.packets_queue_full++; return; }
    dc.arr[dc.n_arr].first = 0; dc.arr[dc.n_arr].count = bytes / stride;
    dc.n_arr++;
    g_ld_stats.packets_queued++;
}

static void sink_draw_index(void* u, const rsx_dispatch* r, u32 first, u32 count)
{
    (void)u; (void)r; g_ld_stats.packets_seen++;
    if (g_ld_movie_mode && !ld_movie_composite_ui_enabled()) {
        g_ld_stats.packets_movie++;
        return;
    }
    dc.n_packets++;
    /* GOWA (24/10): DRAW_INDEX_ARRAY carries at most 256 indices, so an Edge segment of
     * ~1,000 arrives as 4+ batches that follow one another -- one batch for them, the
     * same indices (they are all read at fetch time from the one index array), and the
     * fused path (ld_idx_fast_decode) takes the draw */
    if (dc.n_idx && ld_idx_fast_on() && dc.idx[dc.n_idx - 1].first + dc.idx[dc.n_idx - 1].count == first) {
        dc.idx[dc.n_idx - 1].count += count;
        g_ld_stats.packets_queued++;
        return;
    }
    if (dc.n_idx >= 256) { g_ld_stats.packets_queue_full++; return; }
    dc.idx[dc.n_idx].first = first; dc.idx[dc.n_idx].count = count; dc.n_idx++;
    g_ld_stats.packets_queued++;
}

void rsx_live_draw_set_a010_camera_matrix(const float* matrix16)
{
    if (!matrix16) {
        InterlockedExchange(&g_ld_a010_camera_ready, 0);
        return;
    }
    memcpy(g_ld_a010_camera_bits, matrix16, sizeof(g_ld_a010_camera_bits));
    MemoryBarrier();
    InterlockedExchange(&g_ld_a010_camera_ready, 1);
}

/* Hash the same pre-cubemap/pre-VTF PSO inputs used by the reference replay's
 * RSX_DRAW_CSV.  Keeping this legacy key lets a live draw be matched directly
 * to the known-good a010 capture even though the current live PSO key contains
 * additional feature masks. */
static u64 live_legacy_pso_key(void)
{
    const u32 start = rsx_dsp_vp_start(&g.rsx);
    if (start >= RSX_DSP_VP_INSTR)
        return 0;
    const u8* vp_uc = (const u8*)(g.rsx.vp + start * 4);
    const u32 vp_instrs = rsx_vp_program_size_instrs(
        vp_uc, (RSX_DSP_VP_INSTR - start) * 16);
    if (!vp_instrs)
        return 0;

    u32 fp_loc = 0;
    const u32 fp_off = rsx_dsp_fragment_program(&g.rsx, &fp_loc);
    const u8* fp_uc = guest_ptr(fp_loc, fp_off, 16);
    if (!fp_uc)
        return 0;
    const u32 fp_size = rsx_fp_program_size(fp_uc, 0x10000);
    if (!fp_size)
        return 0;
    fp_uc = guest_ptr(fp_loc, fp_off, fp_size);
    if (!fp_uc)
        return 0;

    u64 key = fnv1a(vp_uc, vp_instrs * 16, 1469598103934665603ull);
    key = fnv1a(fp_uc, fp_size, key);
    const u32 fp_ctrl_key = rsx_dsp_shader_control(&g.rsx) & 0x4Eu;
    key = fnv1a(&fp_ctrl_key, sizeof(fp_ctrl_key), key);
    render_state_t rs;
    decode_render_state(&rs);
    return fnv1a(&rs, sizeof(rs), key);
}

/* Hash the canonical sixteen-attribute vertex stream in source occurrence
 * order.  Compact mode may deduplicate the uploaded payload, so hashing its
 * packed bytes directly is not comparable with the legacy replay renderer.
 * Re-expanding through the remap and applying the RSX defaults produces the
 * same vtx_t byte stream on both paths and places the producer/consumer
 * boundary immediately after guest-memory vertex decoding. */
static u64 live_decoded_vertex_hash(u64 attr_hash[16])
{
    u64 hash = 1469598103934665603ull;
    for (u32 attr = 0; attr < 16; attr++)
        attr_hash[attr] = 1469598103934665603ull;
    const u32 count = dc.n_source_refs ? dc.n_source_refs : dc.n_verts;
    for (u32 occurrence = 0; occurrence < count; occurrence++) {
        const u32 vertex = dc.refs_remapped
            ? rsx_vertex_remap_index(&dc.ref_remap, occurrence)
            : occurrence;
        for (u32 attr = 0; attr < 16; attr++) {
            float fallback[4];
            const float* value =
                dc_vertex_attr_canonical(vertex, attr, fallback);
            hash = fnv1a(value, 16u, hash);
            attr_hash[attr] = fnv1a(value, 16u, attr_hash[attr]);
        }
    }
    return hash;
}

/* RSX_DRAW_CSV=path: uncapped per-draw fingerprints for direct comparison
 * with the working RPCS3 .rxs replay.  Default-off and renderer-neutral. */
/* The open RSX_DRAW_CSV and its gate, for marker rows from outside a draw
 * (rsx_live_draw_blit notes each 2D copy where it falls in the draw stream). */
static FILE* s_csv_file;
static int   s_csv_gate_open = 1;

static void live_draw_csv_emit(u32 prim, u32 n_tri, const char* outcome)
{
    /* GOWA_VPC_DBG=<frame>: from that frame, the first 40 draws into colour
     * target 0xE10000 print their depth write flag, verts and the transform
     * constants 264..267 (God of War Ascension's position matrix). */
    { static int vf = -2; static int vn = 0;
      /* GOWA_VPC_TARGET=<hex>: another colour target; "+n" frame values count
       * from GOWA_ARM_TEX. */
      static int vt = -2; static u32 vt_off = 0xE10000u;
      if (vt == -2) { const char* e = getenv("GOWA_VPC_TARGET"); vt = 0; if (e) vt_off = (u32)strtoul(e, 0, 16); }
      if (vf == -2) { const char* e = getenv("GOWA_VPC_DBG"); vf = e ? 0 : -1; }
      const int vfrom = vf >= 0 ? ld_from_env("GOWA_VPC_DBG") : 0;
      if (vf >= 0 && (int)g_ld_frames >= vfrom && vn < 60) {
          rsx_dsp_surface sfv; rsx_dsp_get_surface(&g.rsx, &sfv);
          static int nw0 = 0; const int dw0 = !(rsx_dsp_reg(&g.rsx, 0x0A70) & 1u);
          if (sfv.color_offset[0] == vt_off && (!dw0 || nw0++ < 20)) {
              vn++;
              fprintf(stderr, "[vpc] frame %u dw=%u n_tri=%u", g_ld_frames,
                      rsx_dsp_reg(&g.rsx, 0x0A70) & 1u, n_tri);
              for (u32 cr = 264; cr < 268; cr++) {
                  const u32* c = rsx_dsp_constant(&g.rsx, cr);
                  fprintf(stderr, " c%u=(%.6g %.6g %.6g %.6g)", cr, ld_c2f(c[0]), ld_c2f(c[1]), ld_c2f(c[2]), ld_c2f(c[3]));
              }
              fprintf(stderr, "\n");
          }
      } }

#if defined(YZ_PERF_CLEAN) && !defined(YZ_PERF_PROFILE)
    (void)prim;
    (void)n_tri;
    (void)outcome;
#else
    static int inited = 0;
    static FILE* file = NULL;
    static u64 draw = 0;
    /* RSX_DRAW_CSV_FROM=<frame>: skip rows before that frame. Logging every
     * draw from boot slows a 10-minute run to an hour. */
    { static long from = -1;
      if (from < 0) { const char* e = getenv("RSX_DRAW_CSV_FROM"); from = e ? atol(e) : 0; }
      if ((long)g_ld_frames < from) return;
      /* GOWA RSX_DRAW_CSV_TO=<frame>: and none after it */
      static long to = -2;
      if (to == -2) { const char* e = getenv("RSX_DRAW_CSV_TO"); to = e ? atol(e) : -1; }
      if (to >= 0 && (long)g_ld_frames > to) return; }
    /* RSX_DRAW_CSV_GATE=<file>: rows only while <file> exists (polled every
     * 512 draws), so a capture can be switched on when the scene of interest
     * is on screen instead of guessing its frame number. */
    { static const char* gate = (const char*)-1; static int open_gate = 0; static unsigned poll;
      if (gate == (const char*)-1) gate = getenv("RSX_DRAW_CSV_GATE");
      if (gate) {
          if ((poll++ & 511u) == 0) { struct stat st; open_gate = stat(gate, &st) == 0; }
          s_csv_gate_open = open_gate;
          if (!open_gate) return;
      } }
    static int a010_only = -1;
    if (a010_only < 0) a010_only = getenv("RSX_DRAW_CSV_A010_ONLY") != NULL;
    if (a010_only) {
        if (InterlockedCompareExchange(
                &g_yz_a010_root_active, 0, 0) == 0)
            return;
    }
    if (!inited) {
        inited = 1;
        const char* path = getenv("RSX_DRAW_CSV");
        if (path && path[0]) {
            file = fopen(path, "w");
            s_csv_file = file;
            if (file) {
                fprintf(file,
                    "draw,frame,outcome,surf,prim,verts,source_verts,decoded_hash,"
                    "pso_key,regs_hash,vp_hash,"
                    "const_hash,blend,dtest,dwrite,dfunc,cull,cullface,"
                    "frontface,cmask,vpx,vpy,vpw,vph,sclx,scly,sclz,"
                    "trnx,trny,trnz,clipw,cliph,zeta_off,zeta_pitch,zeta_loc,"
                    "seen_vtex,seen_vtxfmt,seen_freqdiv,"
                    "attr0_hash,attr1_hash,attr2_hash,attr3_hash,"
                    "attr4_hash,attr5_hash,attr6_hash,attr7_hash,"
                    "attr8_hash,attr9_hash,attr10_hash,attr11_hash,"
                    "attr12_hash,attr13_hash,attr14_hash,attr15_hash,"
                    "active_attr_mask,used_attr_mask,tex\n");
                fprintf(stderr, "[live-diff] RSX_DRAW_CSV armed: %s\n", path);
            } else {
                fprintf(stderr, "[live-diff] cannot open RSX_DRAW_CSV: %s\n",
                        path);
            }
            fflush(stderr);
        }
    }
    if (!file)
        return;

    const u32 target = current_surface();
    const u32 surf = target < g.n_surfaces ? g.surfaces[target].offset : 0;
    /* LD_DRAW_DUMP=<dir> [LD_DRAW_DUMP_EVERY=k, default 10]: in the first
     * gated frame, read back each k-th draw's target BEFORE it is recorded,
     * named by frame-relative index -- bisects which draw changes a surface. */
    { static const char* dir = (const char*)-1; static u32 every, frame = ~0u, idx, dumped_frame = ~0u;
      if (dir == (const char*)-1) { dir = getenv("LD_DRAW_DUMP");
          const char* e = getenv("LD_DRAW_DUMP_EVERY"); every = e ? (u32)atoi(e) : 10; if (!every) every = 10; }
      if (frame != g_ld_frames) { frame = g_ld_frames; idx = 0; }
      if (dir && target < g.n_surfaces && (dumped_frame == ~0u || dumped_frame == frame) &&
          (idx % every) == 0) {
          dumped_frame = frame;
          char path[MAX_PATH * 2];
          snprintf(path, sizeof(path), "%s\\draw_f%06u_i%04u_%08X.ppm", dir, frame, idx, surf);
          ld_dump_surface_ppm(path, &g.surfaces[target]);
      }
      idx++; }
    const u64 regs_hash = fnv1a(
        g.rsx.regs, RSX_DSP_NUM_REGS * sizeof(g.rsx.regs[0]),
        1469598103934665603ull);
    const u64 vp_hash = fnv1a(
        g.rsx.vp, sizeof(g.rsx.vp), 1469598103934665603ull);
    const u64 const_hash = fnv1a(
        g.rsx.constants, sizeof(g.rsx.constants), 1469598103934665603ull);
    render_state_t rs;
    decode_render_state(&rs);
    rsx_dsp_viewport vp;
    rsx_dsp_get_viewport(&g.rsx, &vp);
    rsx_dsp_surface sf;
    rsx_dsp_get_surface(&g.rsx, &sf);

    u32 seen_vtex = 0, seen_vtxfmt = 0;
    for (u32 i = 0; i < 32; i++)
        seen_vtex += g.rsx.seen[(0x0900u >> 2) + i];
    for (u32 i = 0; i < 16; i++)
        seen_vtxfmt += g.rsx.seen[(0x1740u >> 2) + i];
    const u32 seen_freqdiv = g.rsx.seen[0x1FC0u >> 2];
    u64 attr_hash[16];
    const u64 decoded_hash = live_decoded_vertex_hash(attr_hash);
    u32 active_attr_mask = 0;
    for (u32 attr = 0; attr < 16; attr++)
        if (dc.fetch_plan.attr[attr].desc.type &&
            dc.fetch_plan.attr[attr].desc.size)
            active_attr_mask |= 1u << attr;

    fprintf(file,
        "%llu,%u,%s,0x%X,%u,%u,%u,%016llx,"
        "%016llx,%016llx,%016llx,%016llx,"
        "%u,%u,%u,0x%X,%u,0x%X,0x%X,0x%08X,"
        "%u,%u,%u,%u,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,"
        "%u,%u,0x%X,%u,%u,%u,%u,%u",
        (unsigned long long)draw++, g_ld_frames, outcome, surf, prim, n_tri,
        dc.n_source_refs ? dc.n_source_refs : dc.n_verts,
        (unsigned long long)decoded_hash,
        (unsigned long long)live_legacy_pso_key(),
        (unsigned long long)regs_hash, (unsigned long long)vp_hash,
        (unsigned long long)const_hash,
        rs.blend_enable, rs.depth_test, rs.depth_write, rs.depth_func,
        rs.cull_enable, rs.cull_face, rs.front_face, rs.color_mask,
        vp.x, vp.y, vp.w, vp.h,
        vp.scale[0], vp.scale[1], vp.scale[2],
        vp.translate[0], vp.translate[1], vp.translate[2],
        sf.clip_w, sf.clip_h, sf.zeta_offset, sf.zeta_pitch, sf.zeta_location,
        seen_vtex, seen_vtxfmt, seen_freqdiv);
    for (u32 attr = 0; attr < 16; attr++)
        fprintf(file, ",%016llx", (unsigned long long)attr_hash[attr]);
    fprintf(file, ",0x%04X,0x%04X,",
            active_attr_mask, dc.layout.mask & 0xFFFFu);
    /* Enabled fragment texture units, so a pass that samples a surface (or an
     * empty copy of one) is visible in the row: unit:loc:offset:fmt:WxH. */
    for (u32 u = 0; u < 16; u++) {
        rsx_dsp_texture t; rsx_dsp_get_texture(&g.rsx, u, &t);
        if (t.enabled)
            fprintf(file, "%u:%u:%X:%02X:%ux%u ", u, t.location, t.offset,
                    t.format & 0xFF, t.width, t.height);
    }
    if (rs.blend_enable)
        fprintf(file, "bf=%X/%X/%X/%X eq=%X/%X ", rs.sf_rgb, rs.df_rgb, rs.sf_a, rs.df_a,
                rs.eq_rgb, rs.eq_a);
    fprintf(file, "at=%u/0x%X/0x%X ", rs.alpha_test_enable, rs.alpha_func, rs.alpha_ref_raw);
    fprintf(file, "sc=%08X/%08X ", rsx_dsp_reg(&g.rsx, M_SCISSOR_HORIZONTAL), rsx_dsp_reg(&g.rsx, M_SCISSOR_VERTICAL));
    if (rs.stencil_enable)
        fprintf(file, "st=%X/%X/%X/%X/%X/%X ref=%X ", rs.s_func, rs.s_func_mask, rs.s_write_mask, rs.s_fail, rs.s_zfail, rs.s_zpass, rsx_dsp_reg(&g.rsx, M_STENCIL_FUNC + 4));
    /* Inline draws (HUD sprites): the largest vertex-colour alpha byte, attr 3
     * as UB4 -- an element faded to 0 is drawn but invisible. */
    if (dc.inl && dc.inl_stride) {
        rsx_dsp_vertex_attr va; rsx_dsp_get_vertex_attr(&g.rsx, 3, &va);
        if (va.type == RSX_VTX_TYPE_UNORM8 && va.size == 4) {
            u32 amax = 0;
            for (u32 o = dc.inl_off[3] + 3; o < dc.inl_bytes; o += dc.inl_stride)
                if (dc.inl[o] > amax) amax = dc.inl[o];
            fprintf(file, "va=%u ", amax);
        }
        /* ...and the xy bounding box of attr 0 when it is float (screen-space HUD). */
        rsx_dsp_vertex_attr p0; rsx_dsp_get_vertex_attr(&g.rsx, 0, &p0);
        if (p0.type == RSX_VTX_TYPE_FLOAT && p0.size >= 2) {
            float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
            for (u32 o = dc.inl_off[0]; o + 8 <= dc.inl_bytes; o += dc.inl_stride) {
                u32 xb = (dc.inl[o] << 24) | (dc.inl[o+1] << 16) | (dc.inl[o+2] << 8) | dc.inl[o+3];
                u32 yb = (dc.inl[o+4] << 24) | (dc.inl[o+5] << 16) | (dc.inl[o+6] << 8) | dc.inl[o+7];
                float x, y; memcpy(&x, &xb, 4); memcpy(&y, &yb, 4);
                if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y;
            }
            fprintf(file, "bb=%.0f,%.0f-%.0f,%.0f ", x0, y0, x1, y1);
        }
    }
    fprintf(file, "A=%u:%X/%u fmt=%u rt=%u ", sf.color_location[0], sf.color_offset[0], sf.color_pitch[0], sf.color_format, sf.raster_type);
    fprintf(file, "ct=0x%X B=%u:%X C=%u:%X D=%u:%X ", sf.color_target,
            sf.color_location[1], sf.color_offset[1], sf.color_location[2], sf.color_offset[2],
            sf.color_location[3], sf.color_offset[3]);
    fputc('\n', file);
    fflush(file);
#endif
}

/* RSX_A010_GEOM: trace one stable orphanage mesh.  This is draw 520 from
 * the healthy a010 reference: a 270-vertex triangle strip on the main world
 * surface with PSO 7d9f....  The model-initialization repair now reproduces
 * this exact tuple in the live stream.  Keeping the tuple exact makes the gate
 * quiet outside a010 while allowing capture/playback only after known-good
 * world geometry has actually reached the backend. */
static void live_a010_geom_trace(u32 prim, u32 n_tri)
{
    static int enabled = -1;
    static u32 logged = 0;
    static u32 reference_logged = 0;
    if (enabled < 0)
        enabled = getenv("RSX_A010_GEOM") ? 1 : 0;
    if (prim != PRIM_TRIANGLE_STRIP || n_tri != 270)
        return;

    const u32 target = current_surface();
    const u32 surf =
        target < g.n_surfaces ? g.surfaces[target].offset : 0;
    const u64 key = live_legacy_pso_key();
    if (surf != 0x01800000u || key != 0x7d9f528329f20c52ull)
        return;
    /*
     * This topology/PSO/surface tuple is the stable Kiryu mesh measured in
     * the healthy a010 capture. Reaching this point means vertex fetch has
     * completed and the scene has produced real world geometry, rather than
     * only clears and fullscreen post-processing draws.
     */
    if (InterlockedCompareExchange(&g_ld_a010_world_ready, 1, 0) == 0) {
        fprintf(stderr,
                "[a010-world-ready] frame=%u surface=0x%X "
                "pso=%016llx vertices=%u\n",
                g_ld_frames, surf, (unsigned long long)key, n_tri);
        fflush(stderr);
    }
    if (!enabled)
        return;
    if (InterlockedCompareExchange(
            &g_yz_a010_reference_camera_active, 0, 0) != 0) {
        if (reference_logged >= 8)
            return;
        reference_logged++;
    } else {
        if (logged >= 8)
            return;
        logged++;
    }

    float mn[4] = {1e30f, 1e30f, 1e30f, 1e30f};
    float mx[4] = {-1e30f, -1e30f, -1e30f, -1e30f};
    u32 nan_count = 0;
    for (u32 vi = 0; vi < dc.n_verts; vi++) {
        float fallback[4];
        const float* p = dc_vertex_attr(vi, 0, fallback);
        for (u32 k = 0; k < 4; k++) {
            if (p[k] != p[k]) {
                nan_count++;
                continue;
            }
            if (p[k] < mn[k]) mn[k] = p[k];
            if (p[k] > mx[k]) mx[k] = p[k];
        }
    }

    const u32 base = rsx_dsp_vertex_data_base_offset(&g.rsx);
    const u32 base_index = rsx_dsp_vertex_data_base_index(&g.rsx);
    rsx_dsp_index_array ia;
    rsx_dsp_get_index_array(&g.rsx, &ia);
    rsx_dsp_vertex_attr a0, a7;
    rsx_dsp_get_vertex_attr(&g.rsx, 0, &a0);
    rsx_dsp_get_vertex_attr(&g.rsx, 7, &a7);

    fprintf(stderr,
        "[a010-geom] match=%u frame=%u fetched=%u expanded=%u cuts=%u "
        "pos=[%.6g %.6g %.6g %.6g]-[%.6g %.6g %.6g %.6g] nan=%u "
        "decoded_hash=%016llx\n",
        logged, g_ld_frames, dc.n_verts, n_tri, dc.n_cuts,
        mn[0], mn[1], mn[2], mn[3], mx[0], mx[1], mx[2], mx[3],
        nan_count,
        (unsigned long long)fnv1a(
            ld_vertex_compact_payload()
                ? (const void*)dc.compact_verts
                : (const void*)dc.verts,
            ld_vertex_compact_payload()
                ? (u32)((u64)dc.n_verts * dc.layout.stride)
                : (u32)((u64)dc.n_verts * sizeof(vtx_t)),
            1469598103934665603ull));
    fprintf(stderr,
        "[a010-geom] base=0x%X base_index=%u "
        "idx(loc=%u off=0x%X u32=%u first=%u count=%u) "
        "a0(type=%u size=%u stride=%u loc=%u off=0x%X freq=%u) "
        "a7(type=%u size=%u stride=%u loc=%u off=0x%X freq=%u)\n",
        base, base_index,
        ia.location, ia.offset, ia.is_u32,
        dc.n_idx ? dc.idx[0].first : 0,
        dc.n_idx ? dc.idx[0].count : 0,
        a0.type, a0.size, a0.stride, a0.location, a0.offset, a0.frequency,
        a7.type, a7.size, a7.stride, a7.location, a7.offset, a7.frequency);

    for (u32 slot = 108; slot <= 115; slot++) {
        const u32* c = rsx_dsp_constant(&g.rsx, slot);
        union { u32 u; float f; } v[4];
        for (u32 k = 0; k < 4; k++) v[k].u = c[k];
        fprintf(stderr,
            "[a010-geom] c%u=(%.6g %.6g %.6g %.6g) "
            "raw=%08X/%08X/%08X/%08X\n",
            slot, v[0].f, v[1].f, v[2].f, v[3].f,
            v[0].u, v[1].u, v[2].u, v[3].u);
    }
    {
        const u32* c = rsx_dsp_constant(&g.rsx, 467);
        union { u32 u; float f; } v[4];
        for (u32 k = 0; k < 4; k++) v[k].u = c[k];
        fprintf(stderr,
            "[a010-geom] c467=(%.6g %.6g %.6g %.6g)\n",
            v[0].f, v[1].f, v[2].f, v[3].f);
    }
    {
        float camera[4][4];
        float c467[4];
        for (u32 row = 0; row < 4; row++)
            memcpy(camera[row], rsx_dsp_constant(&g.rsx, 108u + row),
                   sizeof(camera[row]));
        memcpy(c467, rsx_dsp_constant(&g.rsx, 467), sizeof(c467));
        rsx_dsp_viewport vp;
        rsx_dsp_get_viewport(&g.rsx, &vp);
        float sx_min = 1e30f, sy_min = 1e30f;
        float sx_max = -1e30f, sy_max = -1e30f;
        float w_min = 1e30f, w_max = -1e30f;
        u32 finite = 0, in_front = 0, in_view = 0;
        for (u32 vi = 0; vi < dc.n_verts; vi++) {
            float p_fallback[4], weight_fallback[4], bone_fallback[4];
            const float* p = dc_vertex_attr(vi, 0, p_fallback);
            const float* weight =
                dc_vertex_attr(vi, 1, weight_fallback);
            const float* bone =
                dc_vertex_attr(vi, 7, bone_fallback);
            const float pw = p[3] != 0.0f ? p[3] : 1.0f;
            /*
             * Exact transform used by healthy PSO 7d9f...: ATTR7 selects
             * four 3-row bone matrices at c112+, ATTR1.yzw supplies weights
             * 1..3, and weight 0 is implicit.  The old probe incorrectly
             * treated c112..c115 as one fixed 4x4 matrix.
             */
            const float influence[4] = {
                c467[1] - weight[1] - weight[2] - weight[3],
                weight[1], weight[2], weight[3]
            };
            float skin[3][4] = {{0}};
            for (u32 inf = 0; inf < 4; inf++) {
                const u32 base_slot =
                    (u32)(bone[inf] * c467[0]);
                for (u32 row = 0; row < 3; row++) {
                    const float* palette = (const float*)
                        rsx_dsp_constant(
                            &g.rsx, (112u + base_slot + row) & 511u);
                    for (u32 lane = 0; lane < 4; lane++)
                        skin[row][lane] +=
                            influence[inf] * palette[lane];
                }
            }
            const float r7x =
                p[0]*skin[0][0] + p[1]*skin[0][1] +
                p[2]*skin[0][2] + pw*skin[0][3];
            const float r7y =
                p[0]*skin[1][0] + p[1]*skin[1][1] +
                p[2]*skin[1][2] + pw*skin[1][3];
            const float r7z =
                p[0]*skin[2][0] + p[1]*skin[2][1] +
                p[2]*skin[2][2] + pw*skin[2][3];
            const float r4w =
                c467[2] * (p[0] + p[1] + p[2]) + pw;
            const float cx =
                r7x*camera[0][0] + r7y*camera[0][1] +
                r7z*camera[0][2] + r4w*camera[0][3];
            const float cy =
                r7x*camera[1][0] + r7y*camera[1][1] +
                r7z*camera[1][2] + r4w*camera[1][3];
            const float cw =
                r7x*camera[3][0] + r7y*camera[3][1] +
                r7z*camera[3][2] + r4w*camera[3][3];
            if (cx != cx || cy != cy || cw != cw || cw == 0.0f)
                continue;
            finite++;
            if (cw > 0.0f) in_front++;
            if (cw < w_min) w_min = cw;
            if (cw > w_max) w_max = cw;
            const float sx = (cx / cw) * vp.scale[0] + vp.translate[0];
            const float sy = (cy / cw) * vp.scale[1] + vp.translate[1];
            if (sx < sx_min) sx_min = sx;
            if (sx > sx_max) sx_max = sx;
            if (sy < sy_min) sy_min = sy;
            if (sy > sy_max) sy_max = sy;
            if (cw > 0.0f && sx >= 0.0f && sx < (float)vp.w &&
                sy >= 0.0f && sy < (float)vp.h)
                in_view++;
        }
        fprintf(stderr,
                "[a010-clip] frame=%u finite=%u/%u front=%u view=%u "
                "screen=[%.3f %.3f]-[%.3f %.3f] w=[%.6g %.6g] "
                "viewport=%ux%u\n",
                g_ld_frames, finite, dc.n_verts, in_front, in_view,
                sx_min, sy_min, sx_max, sy_max, w_min, w_max,
                vp.w, vp.h);
        if (InterlockedCompareExchange(
                &g_yz_a010_reference_camera_active, 0, 0) != 0) {
            static LONG acceptance_written = 0;
            if (InterlockedCompareExchange(
                    &acceptance_written, 1, 0) == 0) {
                FILE* acceptance =
                    fopen("scratch\\a010_acceptance.txt", "a");
                if (acceptance) {
                    fprintf(acceptance,
                            "reference-mesh frame=%u finite=%u/%u "
                            "front=%u view=%u screen=%.3f,%.3f,"
                            "%.3f,%.3f viewport=%u,%u\n",
                            g_ld_frames, finite, dc.n_verts,
                            in_front, in_view, sx_min, sy_min,
                            sx_max, sy_max, vp.w, vp.h);
                    fclose(acceptance);
                }
            }
        }
    }
    fflush(stderr);
}

typedef enum {
    LD_COMPACT_EXPAND_OK = 1,
    LD_COMPACT_EXPAND_DEGENERATE = 0,
    LD_COMPACT_EXPAND_ALLOC = -1,
    LD_COMPACT_EXPAND_PRIMITIVE = -2
} ld_compact_expand_result;

static void compact_vertex_copy(
    u8* destination, u32 destination_vertex, u32 source_vertex)
{
    if (!dc.layout.stride)
        return;
    memcpy(
        destination + (u64)destination_vertex * dc.layout.stride,
        dc.compact_verts + (u64)source_vertex * dc.layout.stride,
        dc.layout.stride);
}

static ld_compact_expand_result expand_compact_topology(
    u32 prim, u8** expanded, u32* expanded_vertices, int* owned)
{
    *expanded = dc.compact_verts;
    *expanded_vertices = 0;
    *owned = 0;
    const u32 segment_count = dc.n_cuts + 1;
    switch (prim) {
    case PRIM_TRIANGLES:
        *expanded_vertices = dc.n_verts - dc.n_verts % 3;
        break;
    case PRIM_TRIANGLE_STRIP: {
        if (dc.n_verts < 3)
            return LD_COMPACT_EXPAND_DEGENERATE;
        u32 total = 0;
        for (u32 segment = 0; segment < segment_count; segment++) {
            u32 begin, count;
            rsx_restart_segment_bounds(
                dc.cuts, dc.n_cuts, dc.n_verts, segment, &begin, &count);
            if (count >= 3) total += (count - 2) * 3;
        }
        if (!total)
            return LD_COMPACT_EXPAND_DEGENERATE;
        *expanded_vertices = total;
        if (!dc.layout.stride)
            break;
        *expanded = (u8*)malloc((size_t)total * dc.layout.stride);
        if (!*expanded)
            return LD_COMPACT_EXPAND_ALLOC;
        *owned = 1;
        u32 write = 0;
        for (u32 segment = 0; segment < segment_count; segment++) {
            u32 begin, count;
            rsx_restart_segment_bounds(
                dc.cuts, dc.n_cuts, dc.n_verts, segment, &begin, &count);
            if (count < 3) continue;
            for (u32 i = 0; i + 2 < count; i++) {
                if (i & 1) {
                    compact_vertex_copy(*expanded, write++, begin + i + 1);
                    compact_vertex_copy(*expanded, write++, begin + i);
                    compact_vertex_copy(*expanded, write++, begin + i + 2);
                } else {
                    compact_vertex_copy(*expanded, write++, begin + i);
                    compact_vertex_copy(*expanded, write++, begin + i + 1);
                    compact_vertex_copy(*expanded, write++, begin + i + 2);
                }
            }
        }
        break;
    }
    case PRIM_TRIANGLE_FAN: {
        if (dc.n_verts < 3)
            return LD_COMPACT_EXPAND_DEGENERATE;
        u32 total = 0;
        for (u32 segment = 0; segment < segment_count; segment++) {
            u32 begin, count;
            rsx_restart_segment_bounds(
                dc.cuts, dc.n_cuts, dc.n_verts, segment, &begin, &count);
            if (count >= 3) total += (count - 2) * 3;
        }
        if (!total)
            return LD_COMPACT_EXPAND_DEGENERATE;
        *expanded_vertices = total;
        if (!dc.layout.stride)
            break;
        *expanded = (u8*)malloc((size_t)total * dc.layout.stride);
        if (!*expanded)
            return LD_COMPACT_EXPAND_ALLOC;
        *owned = 1;
        u32 write = 0;
        for (u32 segment = 0; segment < segment_count; segment++) {
            u32 begin, count;
            rsx_restart_segment_bounds(
                dc.cuts, dc.n_cuts, dc.n_verts, segment, &begin, &count);
            if (count < 3) continue;
            for (u32 i = 1; i + 1 < count; i++) {
                compact_vertex_copy(*expanded, write++, begin);
                compact_vertex_copy(*expanded, write++, begin + i);
                compact_vertex_copy(*expanded, write++, begin + i + 1);
            }
        }
        break;
    }
    case PRIM_QUADS: {
        const u32 quads = dc.n_verts / 4;
        if (!quads)
            return LD_COMPACT_EXPAND_DEGENERATE;
        *expanded_vertices = quads * 6;
        if (!dc.layout.stride)
            break;
        *expanded = (u8*)malloc(
            (size_t)*expanded_vertices * dc.layout.stride);
        if (!*expanded)
            return LD_COMPACT_EXPAND_ALLOC;
        *owned = 1;
        u32 write = 0;
        for (u32 quad = 0; quad < quads; quad++) {
            const u32 base = quad * 4;
            compact_vertex_copy(*expanded, write++, base);
            compact_vertex_copy(*expanded, write++, base + 1);
            compact_vertex_copy(*expanded, write++, base + 2);
            compact_vertex_copy(*expanded, write++, base + 2);
            compact_vertex_copy(*expanded, write++, base + 3);
            compact_vertex_copy(*expanded, write++, base);
        }
        break;
    }
    default:
        return LD_COMPACT_EXPAND_PRIMITIVE;
    }
    return *expanded_vertices
        ? LD_COMPACT_EXPAND_OK : LD_COMPACT_EXPAND_DEGENERATE;
}

static u32 topology_index_count(u32 prim)
{
    const u32 segment_count = dc.n_cuts + 1;
    switch (prim) {
    case PRIM_TRIANGLES:
        return dc.n_source_refs - dc.n_source_refs % 3u;
    case PRIM_TRIANGLE_STRIP:
    case PRIM_TRIANGLE_FAN: {
        u32 total = 0;
        for (u32 segment = 0; segment < segment_count; segment++) {
            u32 begin, count;
            rsx_restart_segment_bounds(
                dc.cuts, dc.n_cuts, dc.n_source_refs,
                segment, &begin, &count);
            (void)begin;
            if (count >= 3) total += (count - 2) * 3;
        }
        return total;
    }
    case PRIM_QUADS:
        return (dc.n_source_refs / 4) * 6;
    default:
        return 0;
    }
}

static u32 topology_vertex_index(u32 occurrence)
{
    return dc.refs_remapped
        ? rsx_vertex_remap_index(&dc.ref_remap, occurrence)
        : occurrence;
}

static void write_topology_indices(u32 prim, u32* indices)
{
    u32 write = 0;
    const u32 segment_count = dc.n_cuts + 1;
    switch (prim) {
    case PRIM_TRIANGLES: {
        const u32 count =
            dc.n_source_refs - dc.n_source_refs % 3u;
        if (dc.idx_fast) {   /* GOWA (24/10): guest u16 - lo (ld_idx_fast_decode) */
            const u8* s = dc.idx_src;
            const u32 lo = dc.idx_lo;
            u32 o = 0;
#if defined(__AVX2__)
            const __m256i bsw = _mm256_setr_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14,
                                                 1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);
            const __m256i vlo = _mm256_set1_epi16((short)lo);
            for (; o + 16 <= count; o += 16) {
                const __m256i w = _mm256_sub_epi16(
                    _mm256_shuffle_epi8(_mm256_loadu_si256((const __m256i*)(s + (size_t)o * 2)), bsw), vlo);
                _mm256_storeu_si256((__m256i*)(indices + o), _mm256_cvtepu16_epi32(_mm256_castsi256_si128(w)));
                _mm256_storeu_si256((__m256i*)(indices + o + 8), _mm256_cvtepu16_epi32(_mm256_extracti128_si256(w, 1)));
            }
#endif
            for (; o < count; o++) indices[o] = (((u32)s[(size_t)o * 2] << 8) | s[(size_t)o * 2 + 1]) - lo;
            break;
        }
        /* GOWA (21/10): the occurrence -> unique map IS the index list of a triangle list;
         * one call per index through rsx_vertex_remap_index was ~7% of the RSX thread */
        if (dc.refs_remapped && count <= dc.ref_remap.occurrence_capacity)
            memcpy(indices, dc.ref_remap.occurrence_to_unique, (size_t)count * sizeof(u32));
        else if (!dc.refs_remapped)
            for (u32 occurrence = 0; occurrence < count; occurrence++)
                indices[occurrence] = occurrence;
        else
            for (u32 occurrence = 0; occurrence < count; occurrence++)
                indices[write++] = topology_vertex_index(occurrence);
        break;
    }
    case PRIM_TRIANGLE_STRIP:
        for (u32 segment = 0; segment < segment_count; segment++) {
            u32 begin, count;
            rsx_restart_segment_bounds(
                dc.cuts, dc.n_cuts, dc.n_source_refs,
                segment, &begin, &count);
            if (count < 3) continue;
            for (u32 i = 0; i + 2 < count; i++) {
                indices[write++] = topology_vertex_index(
                    begin + i + (i & 1u));
                indices[write++] = topology_vertex_index(
                    begin + i + 1u - (i & 1u));
                indices[write++] = topology_vertex_index(begin + i + 2u);
            }
        }
        break;
    case PRIM_TRIANGLE_FAN:
        for (u32 segment = 0; segment < segment_count; segment++) {
            u32 begin, count;
            rsx_restart_segment_bounds(
                dc.cuts, dc.n_cuts, dc.n_source_refs,
                segment, &begin, &count);
            if (count < 3) continue;
            for (u32 i = 1; i + 1 < count; i++) {
                indices[write++] = topology_vertex_index(begin);
                indices[write++] = topology_vertex_index(begin + i);
                indices[write++] = topology_vertex_index(begin + i + 1u);
            }
        }
        break;
    case PRIM_QUADS:
        for (u32 quad = 0; quad < dc.n_source_refs / 4; quad++) {
            const u32 base = quad * 4;
            indices[write++] = topology_vertex_index(base);
            indices[write++] = topology_vertex_index(base + 1u);
            indices[write++] = topology_vertex_index(base + 2u);
            indices[write++] = topology_vertex_index(base + 2u);
            indices[write++] = topology_vertex_index(base + 3u);
            indices[write++] = topology_vertex_index(base);
        }
        break;
    }
}

/* GOWA (30/10, tela preta 397/399/409/425/431): quando o [tela] vê o jogo todo preto (o mesmo gatilho do
 * [tela-alvos]), o registro [tgt] de cada desenho (shader, blend, máscara de cor, texturas, scissor) liga sozinho
 * por 2 quadros, em qualquer alvo, até 1200 linhas: mostra o que o jogo desenha quando a imagem sai preta. */
static u32 g_ld_tgt_auto_left, g_ld_tgt_auto_fim;
static void sink_end_impl(void* user, const rsx_dispatch* r)
{
    (void)user; (void)r;
    if (g_ld_movie_mode && !ld_movie_composite_ui_enabled()) return;
    if (!dc.n_packets) { g_ld_stats.groups_empty++; return; }
    g_ld_stats.groups_seen++;
    /*
     * a010's native per-mesh constant builder can overwrite the otherwise
     * valid reconstructed camera with NaNs after the ordinary camera upload.
     * Repair at the last measured boundary, immediately before the draw
     * consumes the constants. Once the synchronized a010 reference is armed,
     * replace finite-but-wrong native uploads too: the measured failure
     * matrix is finite, yet projects the model thousands of pixels offscreen.
     */
    /* GOWA (21/10): a plain acquire load -- the locked cmpxchg this was (a read written as
     * InterlockedCompareExchange(x, 0, 0)) was a third of sink_end_impl's samples */
    if (__atomic_load_n(&g_ld_a010_camera_ready, __ATOMIC_ACQUIRE) != 0) {
        int camera_has_nan = 0;
        for (u32 slot = 108; slot <= 111 && !camera_has_nan; slot++)
            for (u32 lane = 0; lane < 4; lane++) {
                const u32 bits = g.rsx.constants[slot][lane];
                if ((bits & 0x7F800000u) == 0x7F800000u &&
                    (bits & 0x007FFFFFu) != 0) {
                    camera_has_nan = 1;
                    break;
                }
            }
        if (camera_has_nan || g_yz_runtime_config.a010_start_reference ||
            InterlockedCompareExchange(
                &g_yz_a010_reference_camera_active, 0, 0) != 0) {
            for (u32 i = 0; i < 16; i++)
                g.rsx.constants[108 + i / 4][i % 4] =
                    g_ld_a010_camera_bits[i];
            static u64 repairs = 0;
            repairs++;
            if (repairs <= 16 || (repairs & (repairs - 1)) == 0) {
                fprintf(stderr,
                        "[a010-rsx-camera-repair] n=%llu frame=%u "
                        "c108=%08X/%08X/%08X/%08X\n",
                        (unsigned long long)repairs, g_ld_frames,
                        g.rsx.constants[108][0], g.rsx.constants[108][1],
                        g.rsx.constants[108][2], g.rsx.constants[108][3]);
                fflush(stderr);
            }
        }
    }
    const u32 prim = g.rsx.current_primitive;
    rsx_vertex_layout_plan used_layout;
    ld_current_vertex_layout(&used_layout);
    /* GOWA_LUZ_GPU (rsx_edge_luz.h): segmento do EDGE com a luz pulada na SPU? A marca está no
     * atributo 7 (float3) do elemento 0. Se o VP lê a luz (7/8/14/15) mas não a posição ou a
     * tangente/bitangente (0/4/5), elas entram no layout para o prólogo do VS. */
    g_ld_edge_luz = 0; g_ld_edge_luz_set = NULL;
    if (g_gowa_luz_gpu && (used_layout.mask & 0xC180u)) {
        rsx_dsp_vertex_attr d7; rsx_dsp_get_vertex_attr(&g.rsx, 7, &d7);
        const u8* p7 = (d7.type == RSX_VTX_TYPE_FLOAT && d7.size == 3)
            ? ((rsx_vertex_guest_ptr_fn)g.guest_ptr)(g.guest_user, d7.location,
                  rsx_dsp_vertex_data_base_offset(&g.rsx) + d7.offset, 12) : NULL;
        if (p7 && ((u32)p7[0] << 24 | (u32)p7[1] << 16 | (u32)p7[2] << 8 | p7[3]) == GOWA_LUZ_MAGICO) {
            g_ld_luz_n[0]++;
            u32 wi = (u32)p7[4] << 24 | (u32)p7[5] << 16 | (u32)p7[6] << 8 | p7[7];
            u32 wc = (u32)p7[8] << 24 | (u32)p7[9] << 16 | (u32)p7[10] << 8 | p7[11];
            float fid, fck; memcpy(&fid, &wi, 4); memcpy(&fck, &wc, 4);
            const u32 id = (fid >= 0.0f && fid < (float)GOWA_LUZ_ANEL) ? (u32)fid : 0xFFFFFFFFu;
            if (id == 0xFFFFFFFFu) g_ld_luz_n[4]++;
            else if ((g_gowa_luz_anel[id].gen & 0xFFFFu) != (u32)fck) g_ld_luz_n[2]++;
            else {
                g_ld_edge_luz = 1; g_ld_edge_luz_set = &g_gowa_luz_anel[id].s; g_ld_luz_n[1]++;
                g_gowa_luz_anel[id].visto = g_gowa_luz_anel[id].gen;
                /* o prólogo do VS refaz 7/8/14/15 (o que a SPU deixou neles é lixo): não buscar nem
                 * converter na CPU; 0/4/5 entram se o VP não os lê (GOWA_LUZ_GPU_FETCH=1: busca tudo) */
                { static int fe = -1; if (fe < 0) { const char* e = getenv("GOWA_LUZ_GPU_FETCH"); fe = e && *e == '1'; }
                  if ((used_layout.mask & 0x31u) != 0x31u) g_ld_luz_n[3]++;
                  const u32 m2 = (fe || used_layout.mask == 0xFFFFu) ? (used_layout.mask | 0x31u) : ((used_layout.mask | 0x31u) & ~0xC180u);
                  if (m2 != used_layout.mask) ld_layout_plan_get(m2, &used_layout); }
            }
        }
        { static u32 f0 = 0xFFFFFFFFu, nf;
          if (f0 != g_ld_frames) { f0 = g_ld_frames;
              if (++nf % 256 == 0) {
                  fprintf(stderr, "[luzgpu] 256 quadros: marcados %llu, na GPU %llu, gen velha %llu, layout ampliado (0/4/5) %llu, id ruim %llu\n",
                          (unsigned long long)g_ld_luz_n[0], (unsigned long long)g_ld_luz_n[1], (unsigned long long)g_ld_luz_n[2],
                          (unsigned long long)g_ld_luz_n[3], (unsigned long long)g_ld_luz_n[4]);
                  memset(g_ld_luz_n, 0, sizeof g_ld_luz_n);
              } } }
    }
#if defined(YZ_PERF_PROFILE)
    g_ld_profile.total.used_attribute_draws++;
    g_ld_profile.total.used_attribute_sum += used_layout.count;
    if (used_layout.count > g_ld_profile.frame_used_attribute_max)
        g_ld_profile.frame_used_attribute_max = used_layout.count;
    if (used_layout.count > g_ld_profile.total_used_attribute_max)
        g_ld_profile.total_used_attribute_max = used_layout.count;
    const LONGLONG fetch_pack_begin = ld_profile_qpc();
#endif
    if (ld_vertex_compact_payload()) {
        fetch_batches_hoisted(&used_layout, 1);
    } else if (ld_vertex_hoist_fetch()) {
        rsx_vertex_layout_plan legacy_layout;
        ld_layout_plan_get(0xFFFFu, &legacy_layout);
        fetch_batches_hoisted(&legacy_layout, 0);
    } else {
        fetch_batches();
    }
    if (!dc.n_source_refs)
        dc.n_source_refs = dc.n_verts;
#if defined(YZ_PERF_PROFILE)
    g_ld_profile.total.vertex_fetch_pack_qpc +=
        (u64)(ld_profile_qpc() - fetch_pack_begin);
#endif
    if (!dc.n_verts || !dc.fetch_ok) { g_ld_stats.group_drop_fetch++; return; }

    /* Preserve validated occurrence order while allowing repeated compact
     * vertex references to share one fetched and uploaded payload. */
    int indexed = 0;
    if (!rsx_vertex_topology_plan(prim, dc.refs_remapped, &indexed)) {
        g_ld_stats.group_drop_primitive++;
        return;
    }
    u32 n_tri = indexed
        ? topology_index_count(prim)
        : dc.n_source_refs - dc.n_source_refs % 3;

    if (!n_tri) {
        g_ld_stats.group_drop_degenerate++;
        return;
    }

    live_a010_geom_trace(prim, n_tri);

    const u32 vertex_stride =
        ld_vertex_compact_payload() ? used_layout.stride : VERT_STRIDE;
    const u32 uploaded_vertices = indexed ? dc.n_verts : n_tri;
    const void* vertex_data = ld_vertex_compact_payload()
        ? (const void*)dc.compact_verts : (const void*)dc.verts;
    /* GOWA (24/10): slot by slot (ld_vslots_plan); a draw planned that way that turns
     * out not to be an indexed one gets compact_verts after all */
    const int vslots = dc.slot_mode && indexed && ld_vertex_compact_payload();
    if (dc.slot_mode && !vslots) {
        if (!rsx_vertex_fetch_all(&dc.fetch_plan, dc.refs, dc.n_refs, dc.compact_verts, dc.layout.stride)) {
            g_ld_stats.group_drop_fetch++;
            return;
        }
        dc.slot_mode = 0;
    }
    g_ld_vs_active = vslots;
    /* GOWA_VSLOTS_STATS=1: every 512 frames, the share of draws that went up slot by
     * slot / with fused indices, and the slots by kind. Diagnostic only. */
    { static int vst = -1; if (vst < 0) vst = getenv("GOWA_VSLOTS_STATS") ? 1 : 0;
      if (vst) {
          static u32 f0 = 0xFFFFFFFFu, nf; static unsigned long long n_d, n_vs, n_if, n_k[3];
          if (f0 != g_ld_frames) {
              f0 = g_ld_frames;
              if (++nf % 512 == 0 && n_d) {
                  const unsigned long long ns = n_k[0] + n_k[1] + n_k[2];
                  fprintf(stderr, "[vslots] 512 quadros: desenhos %llu, por slot %.0f%%, indices fundidos %.0f%%; slots cru %.0f%% convertido %.0f%% padrao %.0f%%\n",
                          n_d, 100.0 * n_vs / n_d, 100.0 * n_if / n_d,
                          ns ? 100.0 * n_k[LD_VS_RAW] / ns : 0.0, ns ? 100.0 * n_k[LD_VS_CONV] / ns : 0.0, ns ? 100.0 * n_k[LD_VS_DEF] / ns : 0.0);
                  n_d = n_vs = n_if = n_k[0] = n_k[1] = n_k[2] = 0;
              }
          }
          n_d++; n_vs += vslots != 0; n_if += dc.idx_fast != 0;
          if (vslots) for (u32 s = 0; s < g_ld_vs_n; s++) n_k[g_ld_vs[s].kind]++;
      } }
    const u64 draw_vb_bytes = vslots ? ld_vslots_bytes() : (u64)uploaded_vertices * vertex_stride;
    const u64 draw_ib_bytes = indexed ? (u64)n_tri * sizeof(u32) : 0;
    const u64 vb_capacity = VERT_BUFFER_SIZE;
    if (draw_vb_bytes > vb_capacity || draw_ib_bytes > INDEX_BUFFER_SIZE) {
        live_draw_csv_emit(prim, n_tri, "drop_vbring_oversize");
        g_ld_stats.group_drop_ring++;
        return;
    }
    if ((u64)g.vb_used + draw_vb_bytes > vb_capacity ||
        (u64)g.ib_used + draw_ib_bytes > INDEX_BUFFER_SIZE) {
        /*
         * The replay-proven renderer recycles this upload ring mid-frame.
         * Live used to drop every remaining draw instead, which discarded
         * 181/184 sampled a010 groups after the orphanage workload filled the
         * 256K-vertex ring.  We are still before this draw's memcpy, so it is
         * safe to submit/wait and reuse every transient draw ring here.
         */
        ld_flush(LD_FLUSH_VERTEX_RING);
        g.vb_used = 0;
        g.ib_used = 0;
        g.cb_used = 0;
        g.ps_cb_used = 0;
        g.srv_ring_used = 0;
        g.smp_ring_used = 0;
    }
    if (vslots) {
        if (!ld_vslots_upload(dc.n_verts)) {
            g_ld_stats.group_drop_fetch++;
            return;
        }
    } else if (draw_vb_bytes)
        memcpy(
            g.vb_mapped + g.vb_used, vertex_data,
            (size_t)draw_vb_bytes);
    if (indexed)
        write_topology_indices(
            prim, (u32*)((u8*)g.ib_mapped + g.ib_used));
    /* GOWA_VCACHE_STATS=1 (21/10): how many draws upload exactly the vertices and
     * indices of some draw of the previous frame (static geometry a GPU-side cache
     * could keep) -- a 64-bit hash of the converted payload, compared against the
     * previous frame's set. Report every 64 frames. Diagnostic only. */
    { static int vcs = -1; if (vcs < 0) vcs = getenv("GOWA_VCACHE_STATS") ? 1 : 0;
      if (vcs) {
          enum { VC_N = 16384 };
          static u64 set_a[VC_N], set_b[VC_N]; static u64* cur = set_a; static u64* prev = set_b;
          static u32 frame = 0xFFFFFFFFu, draws, hits, verts, hit_verts, frames;
          static unsigned long long bytes_all, bytes_hit;
          if (frame != g_ld_frames) {
              if (frame != 0xFFFFFFFFu && ++frames % 64 == 0) {
                  fprintf(stderr, "[vcache] 64 quadros: %u desenhos, %u repetidos do quadro anterior (%.0f%%), vertices %u / %u (%.0f%%), bytes %.1f / %.1f MB\n",
                          draws, hits, draws ? 100.0 * hits / draws : 0.0, hit_verts, verts, verts ? 100.0 * hit_verts / verts : 0.0,
                          bytes_hit / 1048576.0, bytes_all / 1048576.0);
                  draws = hits = verts = hit_verts = 0; bytes_all = bytes_hit = 0;
              }
              u64* t = prev; prev = cur; cur = t; memset(cur, 0, sizeof set_a);
              frame = g_ld_frames;
          }
          u64 h = 1469598103934665603ull ^ draw_vb_bytes ^ ((u64)draw_ib_bytes << 32) ^ prim;
          const u8* vp = (const u8*)vertex_data;
          for (u64 q = 0; q + 8 <= draw_vb_bytes; q += 8) { u64 v; memcpy(&v, vp + q, 8); h = (h ^ v) * 1099511628211ull; }
          if (indexed) { const u8* ip = (const u8*)g.ib_mapped + g.ib_used;
              for (u64 q = 0; q + 8 <= draw_ib_bytes; q += 8) { u64 v; memcpy(&v, ip + q, 8); h = (h ^ v) * 1099511628211ull; } }
          if (!h) h = 1;
          draws++; verts += uploaded_vertices; bytes_all += draw_vb_bytes;
          u32 k = (u32)(h ^ (h >> 29)) & (VC_N - 1);
          for (u32 p2 = 0; p2 < 32; p2++, k = (k + 1) & (VC_N - 1)) {
              if (!prev[k]) break;
              if (prev[k] == h) { hits++; hit_verts += uploaded_vertices; bytes_hit += draw_vb_bytes; break; }
          }
          k = (u32)(h ^ (h >> 29)) & (VC_N - 1);
          for (u32 p2 = 0; p2 < 32; p2++, k = (k + 1) & (VC_N - 1))
              if (!cur[k] || cur[k] == h) { cur[k] = h; break; }
      } }

    ID3D12PipelineState* pso = get_pso(
        ld_vertex_mask_signature() ? &used_layout : NULL,
        ld_vertex_compact_payload());
    if (!pso) {
#if defined(YZ_PERF_PROFILE)
        ld_profile_note_rejected_pso();
#endif
        live_draw_csv_emit(prim, n_tri, "drop_pso");
        g_ld_stats.group_drop_pso++;
        return;   /* no fallback in live path */
    }
    /* GOWA_ATTR_DUMP_KEY=<pso key hex>: the first 3 draws of that structural
     * key print every enabled vertex attribute -- format, stride, offset and
     * its first fetched values -- to see which input feeds a shader's texture
     * coordinates, and decoded as what. */
    { static int ak = -2; static u64 akey; static int an;
      if (ak == -2) { const char* t = getenv("GOWA_ATTR_DUMP_KEY"); ak = t ? 1 : 0; akey = t ? _strtoui64(t, 0, 16) : 0; }
      /* GOWA_ATTR_DUMP_SKIP=<n>: skip that PSO's first n draws */
      static long askip = -2; if (askip == -2) { const char* e = getenv("GOWA_ATTR_DUMP_SKIP"); askip = e ? atol(e) : 0; }
      if (ak && an < 12 && g_ld_current_pso.key == akey && askip > 0) { askip--; }
      else if (ak && an < 12 && g_ld_current_pso.key == akey) {
          an++;
          fprintf(stderr, "[attr] draw %d frame %u\n", an, g_ld_frames);
          const u32 vbase = rsx_dsp_vertex_data_base_offset(&g.rsx);
          for (u32 ai = 0; ai < 16; ai++) {
              rsx_dsp_vertex_attr a; rsx_dsp_get_vertex_attr(&g.rsx, ai, &a);
              if (!a.size) continue;
              fprintf(stderr, "[attr] draw %d a%u type=%u size=%u stride=%u freq=%u fmt=0x%08X off=0x%08X:", an, ai,
                      a.type, a.size, a.stride, a.frequency, rsx_dsp_reg(&g.rsx, 0x1740 + ai * 4),
                      rsx_dsp_reg(&g.rsx, 0x1680 + ai * 4));
              for (u32 vi = 0; vi < 4; vi++) {
                  float v[4] = {0, 0, 0, 1};
                  const u32 vid = vi < dc.n_refs ? dc.refs[vi].vertex_id : vi;
                  const int ok = fetch_attr(ai, vbase, vid, 0, v);
                  fprintf(stderr, " v%u%s(%g %g %g %g)", vid, ok ? "" : "!", v[0], v[1], v[2], v[3]);
                  /* what the upload holds for that vertex (= GPU input) */
                  if (vi < dc.n_verts) { float fb[4]; const float* u = dc_vertex_attr(vi, ai, fb);
                      fprintf(stderr, "=gpu(%g %g %g %g)", u[0], u[1], u[2], u[3]); }
              }
              fprintf(stderr, "\n");
          }
          /* every vertex's attribute 2 (the texture coordinates of God of War
           * Ascension's body) as uploaded, to plot over the texture */
          { char fn[260]; snprintf(fn, sizeof fn, "out/uv_draw%d.txt", an);
            FILE* f = fopen(fn, "w");
            if (f) { for (u32 vi = 0; vi < dc.n_verts; vi++) { float fb[4]; const float* u = dc_vertex_attr(vi, 2, fb);
                                                               fprintf(f, "%g %g\n", u[0], u[1]); }
                     fclose(f); } }
          for (u32 tu = 0; tu < 16; tu++) {
              rsx_dsp_texture t; rsx_dsp_get_texture(&g.rsx, tu, &t);
              if (!t.enabled) continue;
              /* is that address a live render target (render-to-texture)? */
              int rt = -1;
              for (u32 si = 0; si < g.n_surfaces; si++)
                  if (g.surfaces[si].location == t.location && g.surfaces[si].offset == t.offset) { rt = (int)si; break; }
              fprintf(stderr, "[attr] draw %d tex%u loc=%u off=0x%08X %ux%u fmt=0x%02X mips=%u remap=0x%08X wrap=0x%08X%s\n", an, tu,
                      t.location, t.offset, t.width, t.height, t.format & 0xFF, t.mipmaps, t.remap, t.wrap,
                      rt >= 0 ? " (superficie viva)" : "");
          }
      } }
    u32 vertex_cb_offset = 0;
    int vertex_cb_plan = rsx_vertex_constant_ring_plan(
        g.cb_used, CB_RING_BYTES, CB_BLOCK_ALIGNED, &vertex_cb_offset);
    if (vertex_cb_plan == 0) {
#if !defined(YZ_PERF_CLEAN)
        g_ld_vertex_constant_ring_recycles++;
        g_ld_frame_vertex_constant_ring_recycles++;
        yz_frontier_trace_emit(
            YZ_FT_PARITY_CB_RECYCLE, g_ld_frames, g_ld_last_present_target,
            g.cb_used, CB_RING_BYTES, CB_BLOCK_ALIGNED,
            (u32)g_ld_stats.groups_executed,
            g_ld_fifo_get, g_ld_fifo_put);
#endif
        ld_flush(LD_FLUSH_VERTEX_CONSTANT_RING);
        if (!g.ready)
            return;
        g.cb_used = 0;
        vertex_cb_plan = rsx_vertex_constant_ring_plan(
            g.cb_used, CB_RING_BYTES, CB_BLOCK_ALIGNED,
            &vertex_cb_offset);
    }
    if (vertex_cb_plan != 1) {
        live_draw_csv_emit(prim, n_tri, "drop_cbring_invalid");
        g_ld_stats.group_drop_ring++;
        return;
    }
    g.cb_used = vertex_cb_offset;
    u32 ps_cb_offset = 0;
    if (!ld_upload_pixel_constants(&ps_cb_offset)) {
        live_draw_csv_emit(prim, n_tri, "drop_ps_cbring");
        g_ld_stats.group_drop_ring++;
        return;
    }

    const u32 target = current_surface();
    if (target == LD_INVALID_SURFACE) {
        live_draw_csv_emit(prim, n_tri, "drop_surface");
        g_ld_stats.group_drop_surface++;
        return;
    }
    /* LD_SKIP_SAMPLING=<hex offset>: drop draws that sample a texture at that
     * offset. Debug only -- removing a post-process pass shows whether the
     * scene under it has any colour at all. */
    { static int64_t skip = -2;
      if (skip == -2) { const char* e = getenv("LD_SKIP_SAMPLING"); skip = e ? (int64_t)strtoul(e, 0, 16) : -1; }
      if (skip >= 0)
          for (u32 u = 0; u < 16; u++) {
              rsx_dsp_texture st; rsx_dsp_get_texture(&g.rsx, u, &st);
              if (st.enabled && st.offset == (u32)skip) return;
          } }
    /* LD_TRACE_PSO=<hex key>: raw vertex attributes + unit-0 texels of the
     * first three draws with that CSV pso_key. */
    { static u64 tk = 1; static int tn;
      if (tk == 1) { const char* e = getenv("LD_TRACE_PSO"); tk = e ? _strtoui64(e, 0, 16) : 0; }
      if (tk && live_legacy_pso_key() == tk && tn >= 3) {
          static u32 every;
          if (dc.inl && (++every % 60) == 0)
              fprintf(stderr, "[pso-trace] frame=%u col=%02X%02X%02X%02X\n", g_ld_frames,
                      dc.inl[16], dc.inl[17], dc.inl[18], dc.inl[19]);
      }
      /* LD_TRACE_PSO_FROM=<frame>: the first three draws at or after that
       * frame -- a PSO's first uses can predate the data it later draws. */
      static long tfrom = -1;
      if (tfrom < 0) { const char* e = getenv("LD_TRACE_PSO_FROM"); tfrom = e ? atol(e) : 0; }
      if (tk && tn < 3 && live_legacy_pso_key() == tk && (long)g_ld_frames >= tfrom) {
          tn++;
          if (dc.inl) {
              fprintf(stderr, "[pso-trace] inline stride=%u bytes=%u:", dc.inl_stride, dc.inl_bytes);
              for (u32 k = 0; k < dc.inl_bytes && k < 6 * dc.inl_stride; k++)
                  fprintf(stderr, "%s%02X", (k % dc.inl_stride) ? "" : " |", dc.inl[k]);
              fprintf(stderr, "\n");
          }
          for (u32 ai = 0; ai < 16; ai++) {
              rsx_dsp_vertex_attr va; rsx_dsp_get_vertex_attr(&g.rsx, ai, &va);
              if (!va.type) continue;
              fprintf(stderr, "[pso-trace] attr%u type=%u size=%u stride=%u loc=%u off=0x%X:",
                      ai, va.type, va.size, va.stride, va.location, va.offset);
              for (u32 vi = 0; vi < 6; vi++) {
                  const u8* p = guest_ptr(va.location, va.offset + vi * va.stride, 16);
                  if (!p) break;
                  fprintf(stderr, " |");
                  for (u32 k = 0; k < (va.type == RSX_VTX_TYPE_FLOAT ? 4u * va.size : 4u) && k < 16; k++)
                      fprintf(stderr, "%02X", p[k]);
              }
              fprintf(stderr, "\n");
          }
          /* The raw array registers and base (the attrN lines above print the
           * decoded offset only), and the constant colour a disabled attr3
           * feeds -- the two things a draw that vanishes is usually missing. */
          fprintf(stderr, "[pso-trace] vdata base_offset=0x%X base_index=%u raw attr0 off=0x%08X fmt=0x%08X\n",
                  rsx_dsp_vertex_data_base_offset(&g.rsx), rsx_dsp_vertex_data_base_index(&g.rsx),
                  rsx_dsp_reg(&g.rsx, 0x1680), rsx_dsp_reg(&g.rsx, 0x1740));
          { float d3[4]; rsx_dsp_vertex_default(&g.rsx, 3, d3);
            fprintf(stderr, "[pso-trace] attr3 const=%g %g %g %g\n", d3[0], d3[1], d3[2], d3[3]); }
          /* Every non-zero transform constant (raw words), so the VS can be
           * evaluated offline against exactly what this draw saw. */
          for (u32 q = 0; q < 512; q++) {
              const u32* w = g.rsx.constants[q];
              if (!(w[0] | w[1] | w[2] | w[3])) continue;
              fprintf(stderr, "[pso-const] %u %08X %08X %08X %08X\n", q, w[0], w[1], w[2], w[3]);
          }
          for (u32 ai = 0; ai < 16; ai++) {   /* the constant value each attribute falls back to */
              float d[4]; rsx_dsp_vertex_default(&g.rsx, ai, d);
              fprintf(stderr, "[pso-vdef] %u %g %g %g %g\n", ai, d[0], d[1], d[2], d[3]);
          }
          { u32 fl = 0; const u32 fo = rsx_dsp_fragment_program(&g.rsx, &fl);
            const u8* fp = guest_ptr(fl, fo, 16 * 16);
            if (fp) { fprintf(stderr, "[pso-trace] fp %u:0x%X:", fl, fo);
                      for (u32 k = 0; k < 16 * 16; k++) fprintf(stderr, "%s%02X", (k & 15) ? "" : " ", fp[k]);
                      fprintf(stderr, "\n"); } }
          rsx_dsp_texture t0; rsx_dsp_get_texture(&g.rsx, 0, &t0);
          const u8* tp = guest_ptr(t0.location, t0.offset, 32);
          if (tp) { { rsx_dsp_texture t1; rsx_dsp_get_texture(&g.rsx, 1, &t1); fprintf(stderr, "[pso-trace] outmask=0x%X inmask=0x%X tex0 fmt=0x%X remap=0x%X wrap=0x%X border=0x%08X filter=0x%X tex1 remap=0x%X wrap=0x%X:", rsx_dsp_reg(&g.rsx, 0x1FF4), rsx_dsp_reg(&g.rsx, 0x1FF0), t0.format, t0.remap, t0.wrap, rsx_dsp_reg(&g.rsx, 0x1A1C), t0.filter, t1.remap, t1.wrap); }
                    for (u32 k = 0; k < 32; k++) fprintf(stderr, "%02X", tp[k]);
                    fprintf(stderr, "\n"); }
      } }
    /* LD_FORCE_VA=1: debug -- inline draws whose UB4 vertex colour (attr 3)
     * has alpha 0 everywhere get alpha 255, to see what a faded-out element is. */
    { static int fva = -1; if (fva < 0) fva = getenv("LD_FORCE_VA") ? 1 : 0;
      if (fva && dc.inl && dc.inl_stride) {
          rsx_dsp_vertex_attr va; rsx_dsp_get_vertex_attr(&g.rsx, 3, &va);
          if (va.type == RSX_VTX_TYPE_UNORM8 && va.size == 4) {
              u32 amax = 0;
              for (u32 o = dc.inl_off[3] + 3; o < dc.inl_bytes; o += dc.inl_stride)
                  if (dc.inl[o] > amax) amax = dc.inl[o];
              if (!amax)
                  for (u32 o = dc.inl_off[3] + 3; o < dc.inl_bytes; o += dc.inl_stride)
                      ((u8*)dc.inl)[o] = 255;
          } } }
    /* LD_SKIP_PSO=<hex key>[,<hex key>...] (up to 16): drop draws with those
     * CSV pso_keys. Debug only. */
    { static u64 skipk[16]; static int nsk = -1;
      if (nsk < 0) { nsk = 0; const char* e = getenv("LD_SKIP_PSO");
          while (e && *e && nsk < 16) { char* q; skipk[nsk++] = _strtoui64(e, &q, 16); e = (*q == ',') ? q + 1 : q; if (q == e && *q != ',') break; } }
      if (nsk) { const u64 k = live_legacy_pso_key(); for (int i = 0; i < nsk; i++) if (k == skipk[i]) return; } }
    /* MRT: colour targets B..D, resolved (and created) here, before any of
     * this draw's commands are recorded. They share A's format and clip. */
    u32 mrt[4] = { target, 0, 0, 0 };
    u32 n_mrt = 1;
    {
        rsx_dsp_surface msf;
        rsx_dsp_get_surface(&g.rsx, &msf);
        const u32 want = ld_rt_count(msf.color_target);
        for (u32 r = 1; r < want; r++) {
            const u32 s = surface_get(msf.color_location[r], msf.color_offset[r],
                                      msf.clip_w, msf.clip_h,
                                      ld_color_dxgi(msf.color_format));
            if (s == LD_INVALID_SURFACE || s == target) break;
            mrt[n_mrt++] = s;
        }
    }
    /* GOWA_SKIP_ZW_TRI=<n>: drop depth-only (colour mask 0) depth-writing draws
     * of exactly n vertices -- diagnostic for a pre-pass occluder. */
    /* GOWA_SKIP_ZW_ANYCM=1: whatever the colour mask. */
    { static int sz = -2, sa = 0; if (sz == -2) { const char* e = getenv("GOWA_SKIP_ZW_TRI"); sz = e ? atoi(e) : -1; sa = getenv("GOWA_SKIP_ZW_ANYCM") != NULL; }
      if (sz >= 0 && (int)n_tri == sz && (rsx_dsp_reg(&g.rsx, 0x0A70) & 1u) && (sa || rsx_dsp_reg(&g.rsx, M_COLOR_MASK) == 0)) {
          live_draw_csv_emit(prim, n_tri, "skip_zw_diag");
          return; } }
    live_draw_csv_emit(prim, n_tri, "execute");
    if (rsx_live_draw_a010_probe_active() && target < 64)
        g_ld_a010_probe_touched |= 1ull << target;
    { static u32 last_target = LD_INVALID_SURFACE;
      if (target != last_target) { ld_trace_target("draw", target, 0); last_target = target; } }
    rsx_dsp_surface sf;
    rsx_dsp_viewport vp;
    rsx_dsp_get_surface(&g.rsx, &sf);
    rsx_dsp_get_viewport(&g.rsx, &vp);
    const u32 current_zslot = g.depth
        ? zdepth_get(sf.zeta_location, sf.zeta_offset, sf.clip_w, sf.clip_h)
        : 0;
    u32 slots[SRV_TABLE_SIZE], smp_slots[SMP_TABLE_SIZE];
    u32 surf_used[SRV_TABLE_SIZE], n_surf_used = 0;
    u32 zdepth_used[SRV_TABLE_SIZE], n_zdepth_used = 0;
    u32 texture_mask = 0;
    for (u32 u = 0; u < SRV_TABLE_SIZE; u++) slots[u] = SRV_WHITE;
    for (u32 u = 0; u < SMP_TABLE_SIZE; u++) smp_slots[u] = SMP_DEFAULT;
    /* A draw that samples its own zeta while not writing depth (deferred
     * lighting reading the scene depth it also depth-tests against, soft
     * particles) reads the snapshot: the zeta does not change during such a
     * draw, so that is what RSX returns. Before, it fell back to guest VRAM,
     * which the host-side depth never reaches (depth 0). A draw that writes
     * the zeta it reads keeps that fallback. */
    const int self_zeta_read = current_zslot &&
        !((rsx_dsp_reg(&g.rsx, M_DEPTH_TEST_ENABLE) & 1u) &&
          (rsx_dsp_reg(&g.rsx, M_DEPTH_WRITE) & 1u));
    /* LD_UNITS=1: every enabled texture unit of every draw that samples PS1
     * VRAM, once per distinct combination.
     *
     * ps1_netemu's display composite is a Cg program picked from seven
     * (CG_fp_gradient / orientation / mofix / smart / upscale / upscale_smart /
     * sharpen) and its uniforms are texture0, texture1 and hwidth -- TWO
     * textures. Every probe here so far has looked at unit 0 only, so "the
     * renderer reads the right bytes" was never actually tested for the second
     * one. This lists them. */
    { static int lu = -1; static u32 seen[8]; static int ns;
      if (lu < 0) lu = getenv("LD_UNITS") ? 1 : 0;
      if (lu) {
          u32 key = 0; int ps1 = 0;
          for (u32 q = 0; q < SRV_TABLE_SIZE; q++) {
              rsx_dsp_texture tq; rsx_dsp_get_texture(&g.rsx, q, &tq);
              if (!tq.enabled) continue;
              key = key * 31u + (tq.offset ^ (tq.format << 8) ^ (q << 24));
              if (tq.location == 1u && tq.offset >= 0x400000u &&
                  tq.offset < 0x480000u) ps1 = 1;
          }
          if (ps1) {
              int fresh = 1;
              for (int z = 0; z < ns; z++) if (seen[z] == key) fresh = 0;
              if (fresh && ns < 8) {
                  seen[ns++] = key;
                  fprintf(stderr, "[units] draw sampling PS1 VRAM:");
                  for (u32 q = 0; q < SRV_TABLE_SIZE; q++) {
                      rsx_dsp_texture tq; rsx_dsp_get_texture(&g.rsx, q, &tq);
                      if (!tq.enabled) continue;
                      fprintf(stderr, " u%u[%u:0x%08X fmt=0x%02X %ux%u p=%u]",
                              q, tq.location, tq.offset, tq.format,
                              tq.width, tq.height, tq.pitch);
                  }
                  fprintf(stderr, "\n");
              }
          }
      } }
    for (u32 u = 0; u < SRV_TABLE_SIZE; u++) {
        rsx_dsp_texture t;   /* GOWA (24/10): as get_pso decoded it for this draw */
        if (u < RSX_DSP_NUM_TEXTURES) t = g_ld_tx[u]; else rsx_dsp_get_texture(&g.rsx, u, &t);
        /* LD_PS1_BUF0=1 -- DIAGNOSTIC, not a fix. Force a 24-bit PS1 VRAM
         * bind at buffer 1 (offset 0x4003C0) to sample buffer 0 (0x400000).
         *
         * After the intro FMV both the driven and undriven paths end on a blank
         * screen, and during it every bind is 0x4003C0 while the content that
         * exists -- the decoded FMV frame -- sits in buffer 0. That is the shape
         * of a display flip parked on the buffer the game is not drawing into.
         * If forcing buffer 0 puts a picture back on screen the flip is the bug;
         * if the screen stays blank it is not. Nothing else distinguishes those
         * two, and the override is one line. */
        { static int fb0 = -1;
          if (fb0 < 0) fb0 = getenv("LD_PS1_BUF0") ? 1 : 0;
          if (fb0 && t.location == 1u && t.offset == 0x4003C0u)
              t.offset = 0x400000u; }
        if (!t.enabled) continue;
        texture_mask |= 1u << u;
        smp_slots[u] = sampler_slot(&t, sampler_key(&t));
        /* GOWA: a volume unit is Texture3D in the shader: only a decoded
         * volume, or the white one, may sit there */
        if (tex_depth3d(&t)) {
            const u32 s3 = texture_srv_slot(&t);
            slots[u] = s3 == SRV_WHITE ? SRV_WHITE3D : s3;
            if (s3 == SRV_WHITE) g_ld_bind_white++; else g_ld_bind_real++;
            continue;
        }
        /* Pick WHICH surface at this offset to sample.
         *
         * Matching on (location, offset) alone was fine when only one surface
         * could exist per offset. Now that surfaces are keyed on size too --
         * they have to be, or the two passes destroy each other's contents --
         * several can, and taking the first match by offset takes an arbitrary
         * one. ps1_netemu composites the PS1 framebuffer into a 720x512 surface
         * at offset 0x0 and then upscales into a 1280x720 output; the upscale
         * pass samples offset 0x0 and was getting the EMPTY 1280x720 surface
         * that happens to sit earlier in the table.
         *
         * Prefer the surface whose dimensions match what the texture
         * descriptor actually declares; failing that, the most recently drawn
         * one, which is the only other defensible answer. */
        int sampled = -1;
        /* An address can be both a colour target and a zeta (a shadow map
         * pass that binds the same memory as both, colour writes masked).
         * A depth-format read of an address with a written host zeta is a
         * depth read: try the zeta first. Before, the colour surface at the
         * same address was taken (zeros). */
        int zeta_pref = 0;
        if ((t.format & TEX_FMT_BASE_MASK & ~(u32)TEX_FMT_UNNORM) == TEX_FMT_DEPTH24_D8)
            for (u32 i = 0; i < g.n_zdepths; i++)
                if (g.zdepths[i].location == t.location &&
                    g.zdepths[i].offset == t.offset &&
                    (g.zdepths[i].had_write || g.zdepths[i].snapshot_valid)) {
                    zeta_pref = 1;
                    break;
                }
        /* GOWA_ZETA_COLOR_LOG=1: a texture over a live zeta's address read in
         * a non-depth format (a title decoding depth from the bytes of an
         * ARGB8 view), once per (offset, format, size). */
        { static int zl = -1; if (zl < 0) zl = getenv("GOWA_ZETA_COLOR_LOG") ? 1 : 0;
          if (zl)
              for (u32 i = 0; i < g.n_zdepths; i++)
                  if (g.zdepths[i].location == t.location && g.zdepths[i].offset == t.offset) {
                      static u32 seen[64][3]; static int ns;
                      int f = 0;
                      for (int q = 0; q < ns; q++)
                          if (seen[q][0] == t.offset && seen[q][1] == t.format) { f = 1; break; }
                      if (!f && ns < 64) {
                          seen[ns][0] = t.offset; seen[ns][1] = t.format; seen[ns][2] = t.width << 16 | t.height; ns++;
                          int has_color = 0;
                          for (u32 k = 0; k < g.n_surfaces; k++)
                              if (g.surfaces[k].location == t.location && g.surfaces[k].offset == t.offset) has_color = 1;
                          fprintf(stderr, "[zeta-color] u%u tex %u:0x%08X fmt=0x%02X %ux%u pitch=%u remap=0x%X sobre zeta #%u "
                                  "(%ux%u escrita=%d snap=%d) superficie_cor=%d leitura_prof=%d frame=%u\n",
                                  u, t.location, t.offset, t.format, t.width, t.height, t.pitch, t.remap, i,
                                  g.zdepths[i].w, g.zdepths[i].h, g.zdepths[i].had_write, g.zdepths[i].snapshot_valid,
                                  has_color, zeta_pref, g_ld_frames);
                      }
                      break;
                  } }
        if (!zeta_pref) { u32 best_gen = 0;
          for (u32 i = 0; i < g.n_surfaces; i++) {
              const surface_t* sf_i = &g.surfaces[i];
              if (sf_i->location != t.location || sf_i->offset != t.offset ||
                  i == target)
                  continue;
              /* GOWA: a main-memory target is written back to guest memory
               * (rsx_live_draw_sync_mainmem) and the CPU/SPU may rework it
               * there -- God of War Ascension's SPU MLAA rewrites 1:0xB20000
               * in place. Sample guest memory, not the stale host copy.
               * LD_MAINMEM_WRITEBACK=0 restores the alias. */
              if (sf_i->location == 1 && rsx_live_draw_mainmem_writeback_on() &&
                  !ld_mm_bypass(sf_i))
                  continue;
              if (sf_i->w == t.width && sf_i->h == t.height) { sampled = (int)i; break; }
              if (sampled < 0 || sf_i->last_draw_generation > best_gen) {
                  sampled = (int)i;
                  best_gen = sf_i->last_draw_generation;
              }
          } }
        /* GOWA_MLAA_OUT=<hex> (default off): read the live MLAA input surface
         * for a texture at that offset. It was on for 0x2114000 by default,
         * taken for the SPU MLAA output -- but 0x2114000 is the second target
         * (HDR/8) of the fog/tone-map composite, which the exposure and bloom
         * chains read: aliased to the tone-mapped scene, they measured an 8x
         * brighter image (washed-out, flickering exposure, giant bloom). */
        { static int mo = -2; static u32 mo_off;
          if (mo == -2) { const char* e = getenv("GOWA_MLAA_OUT");
                          mo_off = e ? (u32)strtoul(e, 0, 16) : 0u; mo = mo_off != 0; }
          if (mo && t.location == 0 && t.offset == mo_off)
              for (u32 i = 0; i < g.n_surfaces; i++)
                  if (i != target && g.surfaces[i].location == 1 && ld_mm_bypass(&g.surfaces[i]) &&
                      g.surfaces[i].w == t.width && g.surfaces[i].h == t.height) { sampled = (int)i; break; } }
        /* GOWA: an ARGB8 read of a colour surface that never received colour,
         * over a written zeta: the depth bytes (ld_zcolor_convert, below). */
        if (sampled >= 0 && !g_ld_color_written[sampled] &&
            (t.format & TEX_FMT_BASE_MASK & ~(u32)TEX_FMT_UNNORM) == TEX_FMT_A8R8G8B8) {
            static int zc2 = -1; if (zc2 < 0) { const char* e = getenv("GOWA_ZCOLOR"); zc2 = !(e && *e == '0'); }
            if (zc2)
                for (u32 i = 0; i < g.n_zdepths; i++)
                    if (g.zdepths[i].location == t.location && g.zdepths[i].offset == t.offset &&
                        (g.zdepths[i].had_write || g.zdepths[i].snapshot_valid)) {
                        sampled = -1;
                        break;
                    }
        }
        /* LD_UVDBG=1: the texture coordinates of the draw that samples the PS1
         * framebuffer. Placed here rather than in fetch_batches because that
         * function is not on this draw's path -- probing it printed nothing at
         * all, which is how that was established.
         *
         * This is the last unmeasured link: the PS3 side binds all 1024x512 of
         * PS1 VRAM as ONE texture, so which part reaches the screen is decided
         * entirely by these UVs, and every pixel the PS1 draws lives at
         * x >= 640 while columns 0..639 are black. */
        { static int uvd = -1;
          if (uvd < 0) uvd = getenv("LD_UVDBG") ? 1 : 0;
          if (uvd && t.location == 1u && t.offset == 0x400000u) {
              static unsigned long n = 0;
              if (n++ < 4) {
                  const u32 vbase = rsx_dsp_vertex_data_base_offset(&g.rsx);
                  fprintf(stderr, "[uvdbg] unit=%u ps1fb %ux%u pitch=%u fmt=0x%02X"
                                  " base=0x%X target=%u\n",
                          u, t.width, t.height, t.pitch, t.format,
                          vbase, target);
                  /* The GPU state that can silently discard a draw. Clears are
                   * visible on these surfaces and the draws are not, so one of
                   * these is throwing the fragments away. */
                  { const u32 sch = rsx_dsp_reg(&g.rsx, M_SCISSOR_HORIZONTAL);
                    const u32 scv = rsx_dsp_reg(&g.rsx, M_SCISSOR_VERTICAL);
                    fprintf(stderr, "[uvdbg]   vp=(%u,%u %ux%u) scale=(%.2f %.2f)"
                                    " xlate=(%.2f %.2f) scissor=(%u+%u, %u+%u)"
                                    " depth_test=%u zwrite=%u zfunc=0x%X"
                                    " colmask=0x%08X blend=%u cull=%u/%u"
                                    " surf=%ux%u\n",
                            vp.x, vp.y, vp.w, vp.h,
                            vp.scale[0], vp.scale[1],
                            vp.translate[0], vp.translate[1],
                            sch & 0xFFFFu, sch >> 16, scv & 0xFFFFu, scv >> 16,
                            rsx_dsp_reg(&g.rsx, M_DEPTH_TEST_ENABLE) & 1u,
                            rsx_dsp_reg(&g.rsx, 0x0A78) & 1u,
                            rsx_dsp_reg(&g.rsx, 0x0A7C),
                            rsx_dsp_reg(&g.rsx, M_COLOR_MASK),
                            rsx_dsp_reg(&g.rsx, M_BLEND_ENABLE) & 1u,
                            rsx_dsp_reg(&g.rsx, 0x1918) & 1u,
                            rsx_dsp_reg(&g.rsx, 0x090C),
                            sf.clip_w, sf.clip_h);
                    /* The PS1 framebuffer's fragment program unpacks the
                     * sampled value arithmetically (NV40 has no integer ops):
                     *
                     *   r0 = sample(tex0, tc0 * (1/1024, 1/512))
                     *   r0 = r0 * fp_constants[0].x + fp_constants[0].y
                     *   r0 = floor(r0) / 8.0
                     *   r0 = sign-preserving truncate
                     *   r0 = r0 * fp_constants[1].x + fp_constants[1].y
                     *
                     * So the output depends entirely on those two constants.
                     * Wrong or absent, it produces exactly the repeating stripe
                     * pattern the composite currently shows. */
                    fprintf(stderr, "[uvdbg]   fp_const count=%u mode=%c"
                                    " c0=(%.6f %.6f %.6f %.6f)"
                                    " c1=(%.6f %.6f %.6f %.6f)\n",
                            g.fp_constants.count, g.fp_constant_mode,
                            ld_c2f(g.fp_constants.count > 0u ? g.fp_constants.values[0][0] : 0u),
                            ld_c2f(g.fp_constants.count > 0u ? g.fp_constants.values[0][1] : 0u),
                            ld_c2f(g.fp_constants.count > 0u ? g.fp_constants.values[0][2] : 0u),
                            ld_c2f(g.fp_constants.count > 0u ? g.fp_constants.values[0][3] : 0u),
                            ld_c2f(g.fp_constants.count > 1u ? g.fp_constants.values[1][0] : 0u),
                            ld_c2f(g.fp_constants.count > 1u ? g.fp_constants.values[1][1] : 0u),
                            ld_c2f(g.fp_constants.count > 1u ? g.fp_constants.values[1][2] : 0u),
                            ld_c2f(g.fp_constants.count > 1u ? g.fp_constants.values[1][3] : 0u));
                  }
                  for (u32 vi = 0; vi < 4; vi++) {
                      float pos[4] = {0,0,0,1}, tc[4] = {0,0,0,1};
                      const int okp = fetch_attr(0, vbase, vi, 0, pos);
                      const int okt = fetch_attr(8, vbase, vi, 0, tc);
                      fprintf(stderr, "[uvdbg]   v%u pos%s(%.1f %.1f) tc%s(%.4f %.4f)\n",
                              vi, okp ? "" : "!", pos[0], pos[1],
                              okt ? "" : "!", tc[0], tc[1]);
                  }
              }
          } }
        if (sampled < 0 && LD_DIAG_ENABLED("LD_ALIAS_DBG")) {
            static u32 n_dbg = 0;
            if (n_dbg++ < 24) {
                fprintf(stderr, "[alias-miss] tex %u:0x%08X fmt=0x%02X %ux%u target=%u surf:",
                        t.location, t.offset, t.format, t.width, t.height, target);
                for (u32 i = 0; i < g.n_surfaces; i++)
                    fprintf(stderr, " [%u]%u:0x%08X %ux%u", i, g.surfaces[i].location,
                            g.surfaces[i].offset, g.surfaces[i].w, g.surfaces[i].h);
                fprintf(stderr, "\n");
            }
        }
        if (sampled < 0 && !zeta_pref && g.surfaces[target].location == t.location &&
            g.surfaces[target].offset == t.offset) {
            /* Reads its own render target: sample a copy of it. */
            slots[u] = surface_feedback_srv(target);
            g_ld_bind_surf++;
        } else if (sampled >= 0) {
            /* LD_ALIAS_DBG also reports the HIT. A texture that aliases a render
             * surface when it should not is exactly as interesting as one that
             * fails to alias when it should, and only the miss was visible --
             * so a title whose every bind resolves to a surface (binds[real=0
             * surf=N] in the frame stats) had no way to show WHY. Guitar Hero
             * III does that: every bind lands on a surface and not one real
             * texture is ever created, so the screen stays black. */
            static int alias_dbg = -1;
            if (alias_dbg < 0) alias_dbg = getenv("LD_ALIAS_DBG") != NULL;
            if (alias_dbg) {
                static u32 n_hit = 0;
                if (n_hit++ < 24)
                    fprintf(stderr, "[alias-hit] tex %u:0x%08X fmt=0x%02X %ux%u "
                            "-> surface[%d] %u:0x%08X %ux%u" "\n",
                            t.location, t.offset, t.format, t.width, t.height,
                            sampled, g.surfaces[sampled].location,
                            g.surfaces[sampled].offset,
                            g.surfaces[sampled].w, g.surfaces[sampled].h);
            }
            slots[u] = SRV_SURFACE_BASE + sampled;
            /* GOWA: RGBA8 surface read as Y16X16_FLOAT: the halves in its bytes */
            { static int cy = -1; if (cy < 0) { const char* e = getenv("GOWA_CAST_Y16"); cy = !(e && *e == '0'); }
              if (cy && (t.format & TEX_FMT_BASE_MASK & ~(u32)TEX_FMT_UNNORM) == RSX_TEX_FMT_Y16X16_FLOAT &&
                  g.surfaces[sampled].fmt == DXGI_FORMAT_R8G8B8A8_UNORM) {
                  const u32 cs = ld_cast_y16x16((u32)sampled, t.width, t.height, t.remap);
                  if (cs != SRV_WHITE) slots[u] = cs;
              } }
            g_ld_bind_surf++;
            int seen = 0;
            for (u32 k = 0; k < n_surf_used; k++) if (surf_used[k] == (u32)sampled) seen = 1;
            if (!seen && n_surf_used < SRV_TABLE_SIZE) surf_used[n_surf_used++] = (u32)sampled;
        } else {
            int sampled_depth = -1;
            const u32 texture_base_fmt =
                t.format & TEX_FMT_BASE_MASK & ~(u32)TEX_FMT_UNNORM;
            /* Mirror the replay-proven depth-RT contract: address and format
             * must match, and the pass must have executed a depth-writing draw.
             * Clear-only zetas intentionally fall through to guest VRAM. */
            if (texture_base_fmt == TEX_FMT_DEPTH24_D8)
                for (u32 i = 0; i < g.n_zdepths; i++)
                    if (g.zdepths[i].location == t.location &&
                        g.zdepths[i].offset == t.offset &&
                        (current_zslot != 1 + i || self_zeta_read)) {
                        if (zdepth_snapshot(1 + i))
                            sampled_depth = (int)i;
                        else {
                            g_ld_zdepth_srv_reject_no_write++;
                            if (!g.zdepths[i].reject_logged &&
                                rsx_live_draw_a010_probe_active()) {
                                g.zdepths[i].reject_logged = 1;
                                fprintf(stderr,
                                    "[zetatrack] a010 reject #%u %u:0x%X "
                                    "draws=%llu dtest=%llu dwrite=%llu "
                                    "both=%llu snapshot=%d\n",
                                    i, g.zdepths[i].location,
                                    g.zdepths[i].offset,
                                    (unsigned long long)g.zdepths[i].draws,
                                    (unsigned long long)
                                        g.zdepths[i].depth_test_draws,
                                    (unsigned long long)
                                        g.zdepths[i].depth_write_draws,
                                    (unsigned long long)
                                        g.zdepths[i].depth_both_draws,
                                    g.zdepths[i].snapshot_valid);
                            }
                        }
                        break;
                    }
            /* GOWA: an ARGB8 texture over a written zeta reads its depth bytes
             * (ld_zcolor_convert) */
            u32 zcolor_slot = SRV_WHITE;
            { static int zc = -1; if (zc < 0) { const char* e = getenv("GOWA_ZCOLOR"); zc = !(e && *e == '0'); }
              if (zc && texture_base_fmt == TEX_FMT_A8R8G8B8 && sampled_depth < 0)
                  for (u32 i = 0; i < g.n_zdepths; i++)
                      if (g.zdepths[i].location == t.location && g.zdepths[i].offset == t.offset &&
                          (g.zdepths[i].had_write || g.zdepths[i].snapshot_valid) &&
                          (current_zslot != 1 + i || self_zeta_read)) {
                          if (zdepth_snapshot(1 + i)) zcolor_slot = ld_zcolor_convert(i, t.remap);
                          break;
                      } }
            if (zcolor_slot != SRV_WHITE) {
                slots[u] = zcolor_slot;
                g_ld_zdepth_srv_binds++;
            } else if (sampled_depth >= 0) {
                slots[u] = SRV_ZDEPTH_BASE + sampled_depth;
                g_ld_zdepth_srv_binds++;
                int seen = 0;
                for (u32 k = 0; k < n_zdepth_used; k++)
                    if (zdepth_used[k] == (u32)sampled_depth) seen = 1;
                if (!seen && n_zdepth_used < SRV_TABLE_SIZE)
                    zdepth_used[n_zdepth_used++] = (u32)sampled_depth;
            } else {
                slots[u] = texture_srv_slot(&t);
                if (slots[u] == SRV_WHITE) g_ld_bind_white++; else g_ld_bind_real++;
            }
        }
    }

    const u32 vtex_mask = vertex_texture_mask();
    u32 vtex_slots[SRV_TABLE_SIZE];
    for (u32 u = 0; u < SRV_TABLE_SIZE; u++)
        vtex_slots[u] = SRV_WHITE;
    /* Resolve and upload every vertex texture before recording this draw's
     * graphics state.  Cache refresh can retire a texture and flush the open
     * command list; doing that after SetPipelineState/root bindings silently
     * loses those bindings on the replacement list. */
    for (u32 u = 0; u < RSX_DSP_NUM_VERTEX_TEXTURES; u++) {
        if (!((vtex_mask >> u) & 1u)) continue;
        rsx_dsp_vertex_texture vt;
        rsx_dsp_get_vertex_texture(&g.rsx, u, &vt);
        vtex_slots[u] = vertex_texture_srv_slot(&vt);
        if (vtex_slots[u] != SRV_WHITE)
            g_ld_vtex_binds++;
        else
            g_ld_vtex_unsupported++;
    }
    const u32 required_srv_tables = 1u + (vtex_mask != 0);
    if (g.srv_ring_used + required_srv_tables > SRV_RING_TABLES ||
        sampler_table_needs_flush(smp_slots)) {
        /* Every descriptor referenced by the old command list is consumed
         * before recycling its shader-visible heap entries. Texture upload
         * commands recorded while resolving slots are also completed here;
         * no render-state command for this draw has been recorded yet. */
        const u32 set_before = s_cur_set;
        ld_flush(LD_FLUSH_DESCRIPTOR_RING);
        if (!g.ready) return;
        g.srv_ring_used = 0;
        g.smp_ring_used = 0;
        /* GOWA: with GOWA_LD_PIPE the flush went on to the other ring set and
         * reset every cursor, but this draw's vertices, indices and pixel
         * constants were already written into the old set: the draw then read
         * the new set's offset 0 -- another draw's leftovers. In chapter 16
         * (~1 descriptor flush a frame) that hit the auto-exposure luminance
         * pass: its full-screen triangle went off, the pass wrote nothing, the
         * exposure chain read the scene bytes as halves (NaN) and the frame
         * flashed lavender. Write them again into the new set. */
        if (s_cur_set != set_before) {
            if (draw_vb_bytes)
                memcpy(g.vb_mapped + g.vb_used, vertex_data, (size_t)draw_vb_bytes);
            if (indexed)
                write_topology_indices(prim, (u32*)((u8*)g.ib_mapped + g.ib_used));
            if (!ld_upload_pixel_constants(&ps_cb_offset)) {
                live_draw_csv_emit(prim, n_tri, "drop_ps_cbring");
                g_ld_stats.group_drop_ring++;
                return;
            }
        }
    }

    D3D12_RESOURCE_BARRIER bar = {0};
    bar.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    bar.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    for (u32 k = 0; k < n_surf_used; k++) {
        bar.Transition.pResource = g.surfaces[surf_used[k]].tex;
        bar.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        bar.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        g.list->lpVtbl->ResourceBarrier(g.list, 1, &bar);
    }
    /* GOWA_ZWLOG=<frame>: in that frame, every draw with a depth test or
     * write prints its colour target, zeta slot, depth func/write and the
     * first vertex position fetched (where does a constant depth come from?). */
    { static int zw = -2; if (zw == -2) { const char* e = getenv("GOWA_ZWLOG"); zw = e ? atoi(e) : -1; }
      if (zw >= 0 && ((int)g_ld_frames == zw || (int)g_ld_frames == zw + 1)) {
          const u32 dt = rsx_dsp_reg(&g.rsx, M_DEPTH_TEST_ENABLE) & 1u, dwr = rsx_dsp_reg(&g.rsx, 0x0A70) & 1u;
          if (dt || dwr)
              fprintf(stderr, "[zwlog] frame %u target %u (0x%X) zslot %u zeta 0x%X dt=%u dw=%u df=0x%X cmask=0x%08X clip=%ux%u\n",
                      g_ld_frames, target, g.surfaces[target].offset, current_zslot, sf.zeta_offset, dt, dwr,
                      rsx_dsp_reg(&g.rsx, M_DEPTH_FUNC), rsx_dsp_reg(&g.rsx, M_COLOR_MASK), sf.clip_w, sf.clip_h);
      } }
    /* GOWA_VTXDBG=<frame>: in that frame, depth-writing draws print attribute
     * 0's layout and its first six fetched positions (a full-screen quad at a
     * constant depth in God of War's Z pre-pass). */
    /* GOWA_VTXDBG_QUADS=1: instead, from that frame on, only draws of at most
     * six references (quads), 24 lines in all. */
    { static int vx = -2, vq = 0, vqn = 0; if (vx == -2) { const char* e = getenv("GOWA_VTXDBG"); vx = e ? atoi(e) : -1; vq = getenv("GOWA_VTXDBG_QUADS") != NULL; }
      if (vx >= 0 && (vq ? ((int)g_ld_frames >= vx && dc.n_refs <= 6 && vqn < 24) : (int)g_ld_frames == vx) &&
          (rsx_dsp_reg(&g.rsx, 0x0A70) & 1u)) {
          vqn++;
          rsx_dsp_vertex_attr a0; rsx_dsp_get_vertex_attr(&g.rsx, 0, &a0);
          const u32 vbase = rsx_dsp_vertex_data_base_offset(&g.rsx);
          fprintf(stderr, "[vtxdbg] frame %u target 0x%X attr0 type=%u size=%u stride=%u freq=%u vbase=0x%X fmt_reg=0x%08X off_reg=0x%08X:",
                  g_ld_frames, g.surfaces[target].offset, a0.type, a0.size, a0.stride, a0.frequency, vbase,
                  rsx_dsp_reg(&g.rsx, 0x1740), rsx_dsp_reg(&g.rsx, 0x1680));
          for (u32 vi = 0; vi < 6; vi++) {
              float pos[4] = {0, 0, 0, 1};
              const int ok = fetch_attr(0, vbase, vi, 0, pos);
              fprintf(stderr, " v%u%s(%.3f %.3f %.3f %.3f)", vi, ok ? "" : "!", pos[0], pos[1], pos[2], pos[3]);
          }
          fprintf(stderr, " prim=%u refs=%u src=%u order:", prim, dc.n_refs, dc.n_source_refs);
          for (u32 ri = 0; ri < dc.n_source_refs && ri < 12; ri++)
              fprintf(stderr, " %u", dc.refs[topology_vertex_index(ri)].vertex_id);
          fprintf(stderr, " cull=%u/0x%X ff=0x%X", rsx_dsp_reg(&g.rsx, M_CULL_FACE_ENABLE) & 1u,
                  rsx_dsp_reg(&g.rsx, M_CULL_FACE), rsx_dsp_reg(&g.rsx, M_FRONT_FACE));
          fprintf(stderr, "\n");
      } }
    /* GOWA_ZDUMP=<frame>: from that frame on, the first draw of a frame with
     * depth func EQUAL reads its zeta back and prints min/max/mean of the
     * depth plane (is the Z pre-pass there when the colour pass tests it?). */
    { static int zf = -2; static u32 last_frame = ~0u; static int nz = 0;
      if (zf == -2) { const char* e = getenv("GOWA_ZDUMP"); zf = e ? atoi(e) : -1; }
      if (zf >= 0 && (int)g_ld_frames >= zf && last_frame != g_ld_frames && nz < 6 && g.depth && current_zslot &&
          (rsx_dsp_reg(&g.rsx, M_DEPTH_FUNC) == 0x0202u)) {
          last_frame = g_ld_frames; nz++;
          zdepth_t* z = &g.zdepths[current_zslot - 1];
          D3D12_RESOURCE_DESC rdesc; z->tex->lpVtbl->GetDesc(z->tex, &rdesc);
          D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp; UINT nrows = 0; UINT64 rowsz = 0, total = 0;
          g.dev->lpVtbl->GetCopyableFootprints(g.dev, &rdesc, 0, 1, 0, &fp, &nrows, &rowsz, &total);
          D3D12_HEAP_PROPERTIES hp = {0}; hp.Type = D3D12_HEAP_TYPE_READBACK;
          D3D12_RESOURCE_DESC bd = {0};
          bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = total; bd.Height = 1;
          bd.DepthOrArraySize = 1; bd.MipLevels = 1; bd.Format = DXGI_FORMAT_UNKNOWN;
          bd.SampleDesc.Count = 1; bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
          ID3D12Resource* rb = NULL;
          if (SUCCEEDED(g.dev->lpVtbl->CreateCommittedResource(g.dev, &hp, D3D12_HEAP_FLAG_NONE, &bd,
                  D3D12_RESOURCE_STATE_COPY_DEST, NULL, &IID_ID3D12Resource, (void**)&rb))) {
              D3D12_RESOURCE_BARRIER zb = {0};
              zb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
              zb.Transition.pResource = z->tex;
              zb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
              zb.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
              zb.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
              g.list->lpVtbl->ResourceBarrier(g.list, 1, &zb);
              D3D12_TEXTURE_COPY_LOCATION src = {0}, dl = {0};
              src.pResource = z->tex; src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; src.SubresourceIndex = 0;
              dl.pResource = rb; dl.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; dl.PlacedFootprint = fp;
              g.list->lpVtbl->CopyTextureRegion(g.list, &dl, 0, 0, 0, &src, NULL);
              zb.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
              zb.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
              g.list->lpVtbl->ResourceBarrier(g.list, 1, &zb);
              ld_flush(LD_FLUSH_READBACK);
              u8* px = NULL; D3D12_RANGE rr = {0, (SIZE_T)total};
              if (SUCCEEDED(rb->lpVtbl->Map(rb, 0, &rr, (void**)&px))) {
                  double sum = 0; float mn = 1e30f, mx = -1e30f; u64 n1 = 0, n0 = 0, n = 0;
                  for (UINT y = 0; y < nrows && y < 720; y++) {
                      const float* row = (const float*)(px + fp.Offset + (SIZE_T)y * fp.Footprint.RowPitch);
                      for (UINT x = 0; x < fp.Footprint.Width && x < 1280; x++) {
                          const float d = row[x]; n++; sum += d;
                          if (d < mn) mn = d; if (d > mx) mx = d;
                          if (d >= 0.99999f) n1++; if (d <= 0.00001f) n0++;
                      }
                  }
                  fprintf(stderr, "[zdump] frame %u zeta %u:0x%X slot %u fmt=%d: min=%g max=%g mean=%g far=%llu near=%llu of %llu\n",
                          g_ld_frames, z->location, z->offset, current_zslot, (int)fp.Footprint.Format,
                          mn, mx, n ? sum / (double)n : 0.0, (unsigned long long)n1, (unsigned long long)n0, (unsigned long long)n);
                  D3D12_RANGE wr = {0, 0}; rb->lpVtbl->Unmap(rb, 0, &wr);
              }
              rb->lpVtbl->Release(rb);
          }
      } }
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtv_handle(LD_SWAP_BUFFERS + target);
    D3D12_CPU_DESCRIPTOR_HANDLE dsv; int have_dsv = 0;
    if (g.depth) {
        dsv = dsv_handle(current_zslot);
        have_dsv = 1;
        if (current_zslot) {
            zdepth_t* z = &g.zdepths[current_zslot - 1];
            if (!z->cleared) {
                g.list->lpVtbl->ClearDepthStencilView(
                    g.list, dsv,
                    D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
                    1.0f, 0, 0, NULL);
                z->cleared = 1;
                z->had_write = 0;
                g_ld_stats.implicit_depth_clears++;
            }
        } else if (!g.depth_cleared) {
            g.list->lpVtbl->ClearDepthStencilView(g.list, dsv,
                D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);
            g.depth_cleared = 1;
            g_ld_stats.implicit_depth_clears++;
        }
    }
    {
        D3D12_CPU_DESCRIPTOR_HANDLE rtvs[4];
        rtvs[0] = rtv;
        for (u32 r = 1; r < n_mrt; r++)
            rtvs[r] = rtv_handle(LD_SWAP_BUFFERS + mrt[r]);
        int same = ld_d3d_filter() && g_ldc.rt_ok && g_ldc.n_rt == n_mrt && g_ldc.dsv_on == (u32)(have_dsv != 0) &&
                   (!have_dsv || g_ldc.dsv.ptr == dsv.ptr);
        for (u32 r = 0; same && r < n_mrt; r++) same = g_ldc.rt[r].ptr == rtvs[r].ptr;
        if (!same) {
            g.list->lpVtbl->OMSetRenderTargets(g.list, n_mrt, rtvs, FALSE,
                                               have_dsv ? &dsv : NULL);
            g_ldc.rt_ok = n_mrt <= 4; g_ldc.n_rt = n_mrt; g_ldc.dsv_on = have_dsv != 0;
            if (have_dsv) g_ldc.dsv = dsv;
            for (u32 r = 0; r < n_mrt && r < 4; r++) g_ldc.rt[r] = rtvs[r];
        }
    }
    ID3D12DescriptorHeap* heaps[] = {g.srv_heap, g.smp_heap};
    if (!ld_d3d_filter() || !g_ldc.heaps_ok || g_ldc.heaps[0] != heaps[0] || g_ldc.heaps[1] != heaps[1]) {
        g.list->lpVtbl->SetDescriptorHeaps(g.list, 2, heaps);
        g_ldc.heaps_ok = 1; g_ldc.heaps[0] = heaps[0]; g_ldc.heaps[1] = heaps[1];
    }
    const D3D12_GPU_DESCRIPTOR_HANDLE table = srv_table(slots);
    const D3D12_GPU_DESCRIPTOR_HANDLE stbl = sampler_table(smp_slots);
    D3D12_GPU_DESCRIPTOR_HANDLE vtex_table = {0};
    if (vtex_mask)
        vtex_table = srv_table(vtex_slots);
    u64 descriptor_signature =
        fnv1a(slots, sizeof(slots), 1469598103934665603ull);
    descriptor_signature =
        fnv1a(smp_slots, sizeof(smp_slots), descriptor_signature);
    if (vtex_mask)
        descriptor_signature =
            fnv1a(vtex_slots, sizeof(vtex_slots), descriptor_signature);

    const float W = sf.clip_w ? (float)sf.clip_w : (float)g.width;
    const float H = sf.clip_h ? (float)sf.clip_h : (float)g.height;
    /* GOWA_WINORIGIN_LOG=1: distinct SET_SHADER_WINDOW (0x1D88: height,
     * origin bit 12, pixel centre bit 16) with the viewport and target. */
    { static int lg = -1; if (lg < 0) lg = getenv("GOWA_WINORIGIN_LOG") ? 1 : 0;
      if (lg) { static u32 seen[48][3]; static int ns = 0;
        const u32 sw = rsx_dsp_reg(&g.rsx, 0x1D88);
        const u32 key1 = (u32)target << 16 | (sf.clip_h & 0xFFFF);
        const u32 key2 = (vp.scale[1] < 0.0f) ? 1u : (vp.scale[1] > 0.0f ? 2u : 0u);
        int f = 0; for (int i = 0; i < ns; i++) if (seen[i][0] == sw && seen[i][1] == key1 && seen[i][2] == key2) { f = 1; break; }
        if (!f && ns < 48) { seen[ns][0] = sw; seen[ns][1] = key1; seen[ns][2] = key2; ns++;
            fprintf(stderr, "[winorigin] 0x1D88=%08X (h=%u origem=%s centro=%u) alvo=%d %ux%u vp.scale=(%g %g) vp.off=(%g %g) frame=%u\n",
                    sw, sw & 0xFFF, (sw >> 12) & 0xF ? "baixo" : "cima", (sw >> 16) & 0xF, (int)target,
                    sf.clip_w, sf.clip_h, vp.scale[0], vp.scale[1], vp.translate[0], vp.translate[1], g_ld_frames); } } }
    /* The guest viewport is in pixels; an antialiased surface's clip is in
     * samples (aa_sx x aa_sy per pixel), so map NDC over the pixel extent. */
    const float Wv = W / (float)sf.aa_sx;
    const float Hv = H / (float)sf.aa_sy;
    float xf[8] = {1, 1, 1, 0, 0, 0, 0, 0};
    if (vp.scale[0] != 0.0f || vp.translate[0] != 0.0f) {
        xf[0] = vp.scale[0] / (Wv * 0.5f);
        xf[1] = -(vp.scale[1] / (Hv * 0.5f));
        xf[2] = vp.scale[2];
        xf[4] = (vp.translate[0] - Wv * 0.5f) / (Wv * 0.5f);
        xf[5] = -((vp.translate[1] - Hv * 0.5f) / (Hv * 0.5f));
        xf[6] = vp.translate[2];
    }
    /* GOWA: constant polygon offset (units * r, r = 2^-24 for D24) added to
     * the window depth through the viewport z offset -- per draw, no PSO */
    { static int po = -1;
      if (po < 0) { const char* e = getenv("GOWA_POLY_OFFSET"); po = !(e && *e == '0'); }
      if (po && (rsx_dsp_reg(&g.rsx, 0x0A68) & 1u)) {
          const u32 ub = rsx_dsp_reg(&g.rsx, 0x0A7C);
          float pu; memcpy(&pu, &ub, 4);
          if (pu == pu && pu > -1.0e7f && pu < 1.0e7f) xf[6] += pu * (1.0f / 16777216.0f);
      } }
    u8* cbdst = g.cb_mapped + g.cb_used;
    if (g_ld_vpc_lo <= g_ld_vpc_hi)   /* GOWA (24/10): only the slots the VP reads (get_pso) */
        memcpy(cbdst + g_ld_vpc_lo * 16u, g.rsx.constants[g_ld_vpc_lo], (g_ld_vpc_hi - g_ld_vpc_lo + 1u) * 16u);
    memcpy(cbdst + 512 * 16, xf, sizeof(xf));

    if (!ld_d3d_filter() || !g_ldc.pso_ok || g_ldc.pso != pso) {
        g.list->lpVtbl->SetPipelineState(g.list, pso);
        g_ldc.pso_ok = 1; g_ldc.pso = pso;
    }
    if (!ld_d3d_filter() || !g_ldc.rs_ok || g_ldc.rs != g.rootsig_x) {
        g.list->lpVtbl->SetGraphicsRootSignature(g.list, g.rootsig_x);
        g_ldc.rs_ok = 1; g_ldc.rs = g.rootsig_x;
        g_ldc.tbl_ok[0] = g_ldc.tbl_ok[1] = 0;
    }
    g.list->lpVtbl->SetGraphicsRootConstantBufferView(
        g.list, 0, g.cb->lpVtbl->GetGPUVirtualAddress(g.cb) + g.cb_used);
    if (ld_fp_constants_buffered())
        g.list->lpVtbl->SetGraphicsRootConstantBufferView(
            g.list, 4,
            g.ps_cb->lpVtbl->GetGPUVirtualAddress(g.ps_cb) +
                ps_cb_offset);
    if (!ld_d3d_filter() || !g_ldc.tbl_ok[0] || g_ldc.tbl[0] != table.ptr) {
        g.list->lpVtbl->SetGraphicsRootDescriptorTable(g.list, 1, table);
        g_ldc.tbl_ok[0] = 1; g_ldc.tbl[0] = table.ptr;
    }
    if (!ld_d3d_filter() || !g_ldc.tbl_ok[1] || g_ldc.tbl[1] != stbl.ptr) {
        g.list->lpVtbl->SetGraphicsRootDescriptorTable(g.list, 2, stbl);
        g_ldc.tbl_ok[1] = 1; g_ldc.tbl[1] = stbl.ptr;
    }
    if (g_ld_edge_luz && g_ld_edge_luz_set) {   /* GOWA: luzes do segmento EDGE em b2 */
        memcpy(cbdst + CB_LUZ_OFS, g_ld_edge_luz_set, sizeof(gowa_luz_set));
        g.list->lpVtbl->SetGraphicsRootConstantBufferView(
            g.list, 5, g.cb->lpVtbl->GetGPUVirtualAddress(g.cb) + g.cb_used + CB_LUZ_OFS);
    }
    if (vtex_mask)
        g.list->lpVtbl->SetGraphicsRootDescriptorTable(
            g.list, 3, vtex_table);
    g.cb_used += CB_BLOCK_ALIGNED;

    D3D12_VIEWPORT dvp = {0, 0, W, H, 0.0f, 1.0f};
    D3D12_RECT sc = {
        0, 0,
        (LONG)(g.surfaces[target].w ? g.surfaces[target].w : g.width),
        (LONG)(g.surfaces[target].h ? g.surfaces[target].h : g.height)
    };
    /* Guest scissor, intersected with the surface rect. The nv40 reset value
     * is a full-window 4096x4096; never-written regs read 0 here, so an
     * unwritten scissor means "no scissor". A WRITTEN zero width or height is
     * a real, empty scissor and must clip everything: GH3's gameplay venue
     * sits under a ViewportElement whose clip window is off-screen (scissor
     * x=1039 w=0), and drawing it full-screen painted its never-rendered
     * texture over the whole scene. */
    {
        const u32 sh = rsx_dsp_reg(&g.rsx, M_SCISSOR_HORIZONTAL);
        const u32 sv = rsx_dsp_reg(&g.rsx, M_SCISSOR_VERTICAL);
        /* The scissor is in pixels too: scale it to the sample grid. */
        const LONG ax = (LONG)sf.aa_sx, ay = (LONG)sf.aa_sy;
        const LONG sx = (LONG)(sh & 0xFFFFu) * ax, sw = (LONG)(sh >> 16) * ax;
        const LONG sy = (LONG)(sv & 0xFFFFu) * ay, svh = (LONG)(sv >> 16) * ay;
        const int written = g.rsx.seen[M_SCISSOR_HORIZONTAL >> 2] != 0 ||
                            g.rsx.seen[M_SCISSOR_VERTICAL >> 2] != 0;
        if (written) {
            if (sx > sc.left)            sc.left   = sx;
            if (sy > sc.top)             sc.top    = sy;
            if (sx + sw  < sc.right)     sc.right  = sx + sw;
            if (sy + svh < sc.bottom)    sc.bottom = sy + svh;
            if (sc.right  < sc.left)     sc.right  = sc.left;
            if (sc.bottom < sc.top)      sc.bottom = sc.top;
        }
    }
    if (g.res_scale != 1) {          /* GOWA_RES_SCALE: guest units -> host pixels */
        const float k = (float)g.res_scale;
        dvp.Width *= k; dvp.Height *= k;
        sc.left = (LONG)LD_PX(sc.left); sc.top = (LONG)LD_PX(sc.top);
        sc.right = (LONG)LD_PX(sc.right); sc.bottom = (LONG)LD_PX(sc.bottom);
    }
    if (!ld_d3d_filter() || !g_ldc.vpsc_ok || memcmp(&g_ldc.vp, &dvp, sizeof dvp) || memcmp(&g_ldc.sc, &sc, sizeof sc)) {
        g.list->lpVtbl->RSSetViewports(g.list, 1, &dvp);
        g.list->lpVtbl->RSSetScissorRects(g.list, 1, &sc);
        g_ldc.vpsc_ok = 1; g_ldc.vp = dvp; g_ldc.sc = sc;
    }
    /* Dynamic stencil reference (kept out of the PSO key on purpose). */
    { const UINT st = rsx_dsp_reg(&g.rsx, M_STENCIL_FUNC_REF) & 0xFFu;
      if (!ld_d3d_filter() || !g_ldc.st_ok || g_ldc.st != st) {
          g.list->lpVtbl->OMSetStencilRef(g.list, st);
          g_ldc.st_ok = 1; g_ldc.st = st;
      } }
    /* GOWA: the constant blend colour (NV4097_SET_BLEND_COLOR, 0x031C) for
     * the CONSTANT_COLOR / CONSTANT_ALPHA factors (0x8001-0x8004 ->
     * D3D12_BLEND_BLEND_FACTOR). It was never passed on, so D3D12 blended
     * with its default factor: God of War Ascension adds its bloom with
     * (CONSTANT_ALPHA, ONE) -- the bloom strength -- and it went in at full
     * strength, the chapter-16 sky and every bright opening blown out to white.
     * 8-bit targets: A8R8G8B8; FP16 targets: R,G halves here and B,A in
     * BLEND_COLOR2 (0x1FC0). An RGB factor that names the constant ALPHA
     * takes A in every channel. GOWA_BLEND_COLOR=0 restores the old blend. */
    { static int bcon = -1;
      if (bcon < 0) { const char* e = getenv("GOWA_BLEND_COLOR"); bcon = !(e && *e == '0'); }
      if (bcon && (rsx_dsp_reg(&g.rsx, M_BLEND_ENABLE) & 1u)) {
          const u32 bsf = rsx_dsp_reg(&g.rsx, M_BLEND_SFACTOR), bdf = rsx_dsp_reg(&g.rsx, M_BLEND_DFACTOR);
          const u32 f4[4] = { bsf & 0xFFFFu, bdf & 0xFFFFu, bsf >> 16, bdf >> 16 };   /* rgb src/dst, alpha src/dst */
          int uses = 0, rgb_color = 0;
          for (int q = 0; q < 4; q++) if (f4[q] >= 0x8001u && f4[q] <= 0x8004u) uses = 1;
          for (int q = 0; q < 2; q++) if (f4[q] == 0x8001u || f4[q] == 0x8002u) rgb_color = 1;
          if (uses) {
              const u32 bc = rsx_dsp_reg(&g.rsx, 0x031Cu);
              float c[4];   /* r g b a */
              if ((sf.color_format & 0x1Fu) == RSX_SURFACE_FMT_F_W16Z16Y16X16) {
                  const u32 bc2 = rsx_dsp_reg(&g.rsx, 0x1FC0u);
                  const u16 hv[4] = { (u16)(bc & 0xFFFFu), (u16)(bc >> 16), (u16)(bc2 & 0xFFFFu), (u16)(bc2 >> 16) };
                  for (int q = 0; q < 4; q++) {
                      const u32 e = (hv[q] >> 10) & 0x1Fu, m = hv[q] & 0x3FFu;
                      float v = e ? (float)(1u << e) / 32768.0f * (1.0f + m / 1024.0f) : (float)m / 16777216.0f;
                      c[q] = (hv[q] & 0x8000u) ? -v : v;
                  }
              } else {
                  c[0] = (float)((bc >> 16) & 0xFFu) / 255.0f; c[1] = (float)((bc >> 8) & 0xFFu) / 255.0f;
                  c[2] = (float)(bc & 0xFFu) / 255.0f;         c[3] = (float)(bc >> 24) / 255.0f;
              }
              /* with ALPHA_FACTOR (gcm_blend_factor) the colour slots that
               * name the constant alpha read .a themselves: pass the colour */
              const int full = rgb_color || g_ld_alpha_factor_ok;
              const float bf[4] = { full ? c[0] : c[3], full ? c[1] : c[3], full ? c[2] : c[3], c[3] };
              g.list->lpVtbl->OMSetBlendFactor(g.list, bf);
              static int logs = -1; if (logs < 0) logs = getenv("GOWA_BLEND_COLOR_LOG") ? 0 : 99;
              if (logs < 12) { logs++;
                  fprintf(stderr, "[blend-color] frame %u fp=0x%X sf=0x%08X df=0x%08X cor=0x%08X -> fator %.3f %.3f %.3f %.3f\n",
                          g_ld_frames, g_ld_current_pso.fp_offset, bsf, bdf, bc, bf[0], bf[1], bf[2], bf[3]); }
          }
      } }

    D3D12_VERTEX_BUFFER_VIEW vbv;
    vbv.BufferLocation = g.vb->lpVtbl->GetGPUVirtualAddress(g.vb) + g.vb_used;
    vbv.StrideInBytes = vertex_stride;
    vbv.SizeInBytes = (u32)draw_vb_bytes;
    if (vslots)
        g.list->lpVtbl->IASetVertexBuffers(g.list, 0, g_ld_vs_n, g_ld_vs_view);
    else if (vertex_stride)
        g.list->lpVtbl->IASetVertexBuffers(g.list, 0, 1, &vbv);
    if (!ld_d3d_filter() || !g_ldc.topo_ok) {
        g.list->lpVtbl->IASetPrimitiveTopology(g.list, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        g_ldc.topo_ok = 1;
    }
    {
        const u64 serial = ++g_ld_recent_draw_total;
        ld_recent_draw* draw =
            &g_ld_recent_draws[(serial - 1u) & (LD_RECENT_DRAW_CAP - 1u)];
        memset(draw, 0, sizeof(*draw));
        draw->serial = serial;
        draw->frame = g_ld_frames;
        draw->pso_key = g_ld_current_pso.key;
        draw->descriptor_signature = descriptor_signature;
        draw->vp_start = g_ld_current_pso.vp_start;
        draw->vp_instrs = g_ld_current_pso.vp_instrs;
        draw->fp_location = g_ld_current_pso.fp_location;
        draw->fp_offset = g_ld_current_pso.fp_offset;
        draw->fp_size = g_ld_current_pso.fp_size;
        draw->fp_control = g_ld_current_pso.fp_control;
        draw->cube_mask = g_ld_current_pso.cube_mask;
        /* GOWA_TGT_LOG=<colour offset hex>[,from frame]: one line per draw
         * into that target (PSO key for LD_SKIP_PSO, fp, blend, textures), up
         * to 200 lines -- which passes write the final image, in order. */
        { static int tl = -2; static u32 tl_off; static long tl_from; static int tl_n, tl_max = 200;
          static double tl_sec = -1.0; static ULONGLONG tl_t0;
          if (tl == -2) { const char* e = getenv("GOWA_TGT_LOG"); tl = e ? 1 : 0;
                          if (e) { tl_off = (u32)strtoul(e, 0, 16); const char* c = strchr(e, ',');
                                   tl_from = c ? atol(c + 1) : 0;
                                   /* third field: line budget (default 200) */
                                   const char* c2 = c ? strchr(c + 1, ',') : NULL;
                                   if (c2) tl_max = atoi(c2 + 1);
                                   /* "<n>s": seconds after the first draw instead of a frame */
                                   if (c && strchr(c + 1, 's') && (!c2 || strchr(c + 1, 's') < c2)) { tl_sec = atof(c + 1); tl_from = 0; } }
                          tl_t0 = GetTickCount64(); }
          if (g_ld_tgt_auto_left && g_ld_frames > g_ld_tgt_auto_fim) g_ld_tgt_auto_left = 0;
          const int tl_auto = g_ld_tgt_auto_left != 0;
          if (!tl_auto && tl && tl_sec >= 0.0 && (double)(GetTickCount64() - tl_t0) < tl_sec * 1000.0) { }
          else if (tl_auto || (tl && tl_n < tl_max && (long)g_ld_frames >= tl_from)) {
              rsx_dsp_surface st; rsx_dsp_get_surface(&g.rsx, &st);
              if (tl_auto || st.color_offset[0] == tl_off || tl_off == 0xFFFFFFFFu) {   /* FFFFFFFF = any target */
                  if (tl_auto) g_ld_tgt_auto_left--; else tl_n++;
                  fprintf(stderr, "[tgt] alvo=%u:0x%X ", st.color_location[0], st.color_offset[0]);
                  fprintf(stderr, "[tgt] frame %u lk=%016llx pso=%016llx clip=%ux%u mrt=%u:%X,%X,%X fp=%u:0x%X n=%u blend=%u sf=0x%08X df=0x%08X eq=0x%08X cmask=0x%08X tex:",
                          g_ld_frames, (unsigned long long)live_legacy_pso_key(), (unsigned long long)g_ld_current_pso.key,
                          st.clip_w, st.clip_h, ld_rt_count(st.color_target), st.color_offset[1], st.color_offset[2], st.color_offset[3],
                          g_ld_current_pso.fp_location, g_ld_current_pso.fp_offset, n_tri,
                          rsx_dsp_reg(&g.rsx, M_BLEND_ENABLE) & 1u, rsx_dsp_reg(&g.rsx, M_BLEND_SFACTOR),
                          rsx_dsp_reg(&g.rsx, M_BLEND_DFACTOR), rsx_dsp_reg(&g.rsx, M_BLEND_EQUATION),
                          rsx_dsp_reg(&g.rsx, 0x0324));
                  for (u32 u = 0; u < 16; u++) {
                      if (!((texture_mask >> u) & 1u)) continue;
                      rsx_dsp_texture tt; rsx_dsp_get_texture(&g.rsx, u, &tt);
                      fprintf(stderr, " u%u=%u:%X/%02X/%ux%u", u, tt.location, tt.offset, tt.format, tt.width, tt.height);
                      if ((tt.format & 0x9Fu) == 0x90u)   /* depth: pitch, address word */
                          fprintf(stderr, "[p=%u a=%08X]", tt.pitch, tt.wrap);
                  }
                  fprintf(stderr, " sc=%08X,%08X", rsx_dsp_reg(&g.rsx, 0x08C0), rsx_dsp_reg(&g.rsx, 0x08C4));
                  fprintf(stderr, "\n");
              }
          } }
        /* GOWA_CHAIN_RING: the chain's draws, in order (see ld_chain_dump) */
        if (ld_chain_on()) {
            rsx_dsp_surface st; rsx_dsp_get_surface(&g.rsx, &st);
            ld_chain_ev ev; memset(&ev, 0, sizeof ev);
            int hit = ld_chain_hit(st.color_offset[0]);
            u32 k = 0;
            for (u32 u = 0; u < 16 && k < 3; u++) {
                if (!((texture_mask >> u) & 1u)) continue;
                rsx_dsp_texture tt; rsx_dsp_get_texture(&g.rsx, u, &tt);
                ev.tex[k] = tt.offset; ev.tw[k] = tt.width; ev.th[k] = tt.height; k++;
                if (ld_chain_hit(tt.offset)) hit = 1;
            }
            if (hit) {
                ev.frame = g_ld_frames; ev.seq = ++s_chain_seq; ev.tgt = st.color_offset[0];
                ev.cw = st.clip_w; ev.ch = st.clip_h;
                ev.sc0 = rsx_dsp_reg(&g.rsx, 0x08C0); ev.sc1 = rsx_dsp_reg(&g.rsx, 0x08C4);
                /* cmask field now: depth func (low 16) | depth write mask << 16 */
                ev.cmask = (rsx_dsp_reg(&g.rsx, M_DEPTH_FUNC) & 0xFFFFu) | ((rsx_dsp_reg(&g.rsx, 0x0A70) & 1u) << 16);
                ev.pitch = st.zeta_offset;
                ev.fp = g_ld_current_pso.fp_offset; ev.ntri = n_tri;
                /* fmt: colour format | our surface index << 8 | depth test << 16 | stencil << 17 | zslot << 20 */
                ev.fmt = (st.color_format & 0xFFu) | ((u32)target << 8) |
                         ((rsx_dsp_reg(&g.rsx, M_DEPTH_TEST_ENABLE) & 1u) << 16) |
                         ((rsx_dsp_reg(&g.rsx, M_STENCIL_TEST_ENABLE) & 1u) << 17) | ((u32)(current_zslot & 0xFFF) << 20);
                /* first three positions as uploaded (attribute 0) */
                const u8* vd = (const u8*)vertex_data;
                for (u32 q = 0; vd && q < 3 && q < uploaded_vertices; q++) {
                    memcpy(&ev.v[q * 2], vd + (size_t)q * vertex_stride, 2 * sizeof(float));
                }
                ev.key = g_ld_current_pso.key;
                ev.blend = (rsx_dsp_reg(&g.rsx, M_BLEND_ENABLE) & 1u) << 31 | (rsx_dsp_reg(&g.rsx, M_BLEND_SFACTOR) & 0xFFFu) << 12 |
                           (rsx_dsp_reg(&g.rsx, M_BLEND_DFACTOR) & 0xFFFu);
                ev.cmask2 = rsx_dsp_reg(&g.rsx, M_COLOR_MASK);
                ev.vp0 = rsx_dsp_reg(&g.rsx, 0x0A00); ev.vp1 = rsx_dsp_reg(&g.rsx, 0x0A04);   /* viewport horizontal / vertical */
                s_chain_ev[s_chain_n++ % 48u] = ev;
            }
        }
        draw->vtex_mask = g_ld_current_pso.vtex_mask;
        draw->txl_mask = g_ld_current_pso.txl_mask;
        draw->texture_mask = texture_mask;
        draw->primitive = prim;
        draw->vertices = n_tri;
        draw->target = target;
        draw->zslot = current_zslot;
        draw->clip_x = sf.clip_x;
        draw->clip_y = sf.clip_y;
        draw->clip_w = sf.clip_w;
        draw->clip_h = sf.clip_h;
        draw->srv_ring_used = g.srv_ring_used;
        draw->sampler_ring_used = g.smp_ring_used;
        draw->cb_used = g.cb_used;
        draw->vb_used = g.vb_used;
    }
    /* GOWA_RSX_SLOW_NS=<ns> (experiment): spin that long per draw on the RSX
     * thread -- does a slower RSX thread slow the frame (critical path)? */
    { static long slow_ns = -1; static double qpc_per_ns;
      if (slow_ns < 0) { const char* e = getenv("GOWA_RSX_SLOW_NS"); slow_ns = e ? atol(e) : 0;
                         LARGE_INTEGER f; QueryPerformanceFrequency(&f); qpc_per_ns = (double)f.QuadPart / 1e9; }
      if (slow_ns > 0) { LARGE_INTEGER a, b; QueryPerformanceCounter(&a);
                         const LONGLONG until = a.QuadPart + (LONGLONG)(slow_ns * qpc_per_ns);
                         do { YieldProcessor(); QueryPerformanceCounter(&b); } while (b.QuadPart < until); } }
    /* GOWA_SKIP_KEY=<hex>[,<hex>...] (up to 16): drop the draw call of
     * every draw whose structural PSO key (the [tgt] log's pso=) is listed --
     * a whole program, unlike LD_SKIP_PSO's per-draw legacy key. Debug only. */
    static u64 s_skipk[16]; static int s_nskk = -1;
    if (s_nskk < 0) { s_nskk = 0; const char* e = getenv("GOWA_SKIP_KEY");
        while (e && *e && s_nskk < 16) { char* q; s_skipk[s_nskk++] = _strtoui64(e, &q, 16);
                                         if (q == e) break; e = (*q == ',') ? q + 1 : q; } }
    int skip_key = 0;
    for (int i = 0; i < s_nskk; i++) if (g_ld_current_pso.key == s_skipk[i]) skip_key = 1;
    if (skip_key) {
    } else if (indexed) {
        D3D12_INDEX_BUFFER_VIEW ibv;
        ibv.BufferLocation =
            g.ib->lpVtbl->GetGPUVirtualAddress(g.ib) + g.ib_used;
        ibv.SizeInBytes = (u32)draw_ib_bytes;
        ibv.Format = DXGI_FORMAT_R32_UINT;
        g.list->lpVtbl->IASetIndexBuffer(g.list, &ibv);
        g.list->lpVtbl->DrawIndexedInstanced(
            g.list, n_tri, 1, 0, 0, 0);
    } else {
        g.list->lpVtbl->DrawInstanced(g.list, n_tri, 1, 0, 0);
    }
    ld_surface_note_write(target, LD_SURFACE_WRITE_DRAW);
    for (u32 r = 1; r < n_mrt; r++)
        ld_surface_note_write(mrt[r], LD_SURFACE_WRITE_DRAW);
    { rsx_dsp_surface cw; rsx_dsp_get_surface(&g.rsx, &cw);
      if ((cw.color_target & 0xFu) && rsx_dsp_reg(&g.rsx, M_COLOR_MASK) != 0) {
          if (target < MAX_SURFACES) g_ld_color_written[target] = 1;
          for (u32 r = 1; r < n_mrt; r++) if (mrt[r] < MAX_SURFACES) g_ld_color_written[mrt[r]] = 1;
      } }
    if (current_zslot) {
        render_state_t depth_state;
        decode_render_state(&depth_state);
        zdepth_t* z = &g.zdepths[current_zslot - 1];
        z->draws++;
        if (depth_state.depth_test) z->depth_test_draws++;
        if (depth_state.depth_write) z->depth_write_draws++;
        if (depth_state.depth_test && depth_state.depth_write)
            z->depth_both_draws++;
        /* A write-enable bit alone does not prove that this draw produced a
         * usable depth map.  With depth testing disabled RSX does not execute
         * the depth pass represented by this tracked zeta. */
        if (depth_state.depth_test && depth_state.depth_write)
            z->had_write = 1;
    }
    g_ld_stats.groups_executed++;
#if defined(YZ_PERF_PROFILE)
    g_ld_profile.total.input_vertices += dc.n_source_refs;
    g_ld_profile.total.expanded_vertices += n_tri;
    g_ld_profile.total.legacy_vertex_upload_bytes +=
        (u64)(prim == PRIM_TRIANGLES ? n_tri : dc.n_source_refs) *
        VERT_STRIDE;
    g_ld_profile.total.vertex_upload_bytes += draw_vb_bytes;
#endif

    for (u32 k = 0; k < n_surf_used; k++) {
        bar.Transition.pResource = g.surfaces[surf_used[k]].tex;
        bar.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        bar.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        g.list->lpVtbl->ResourceBarrier(g.list, 1, &bar);
    }
    /* A guest-side failure can occur before the next flip even though useful
     * orphanage draws have already reached internal world targets.  Preserve
     * a few progressively later surface snapshots directly from the draw
     * stream so presentation is not a prerequisite for visual diagnosis. */
    if (LD_DIAG_ENABLED("RSX_A010_EAGER_DUMP") &&
        InterlockedCompareExchange(&g_ld_a010_world_ready, 0, 0) != 0) {
        static u64 eager_origin = 0;
        static u32 eager_sample = 0;
        if (!eager_origin)
            eager_origin = g_ld_stats.groups_executed;
        if (eager_sample < 4u &&
            g_ld_stats.groups_executed >=
                eager_origin + (u64)eager_sample * 32u) {
            CreateDirectoryA("scratch\\a010_probe", NULL);
            for (u32 i = 0; i < g.n_surfaces; i++) {
                const u32 off = g.surfaces[i].offset;
                if (off != 0x00E40000u && off != 0x01800000u &&
                    off != 0x02D10000u && off != 0x02710000u &&
                    off != 0x01440000u)
                    continue;
                char path[256];
                snprintf(path, sizeof(path),
                         "scratch\\a010_probe\\eager_%03u_group_%08llu_"
                         "surface_%02u_off_%08X.ppm",
                         eager_sample,
                         (unsigned long long)g_ld_stats.groups_executed,
                         i, off);
                ld_dump_surface_ppm(path, &g.surfaces[i]);
            }
            fprintf(stderr,
                    "[a010-eager-dump] sample=%u groups=%llu surfaces=%u\n",
                    eager_sample,
                    (unsigned long long)g_ld_stats.groups_executed,
                    g.n_surfaces);
            eager_sample++;
            fflush(stderr);
        }
    }
    g.vb_used += (u32)draw_vb_bytes;
    g.ib_used += (u32)draw_ib_bytes;
    ld_profile_note_ring_highwater();
}

static void sink_end(void* user, const rsx_dispatch* r)
{
#if defined(YZ_PERF_PROFILE)
    const LONGLONG begin = ld_profile_qpc();
#endif
    sink_end_impl(user, r);
#if defined(YZ_PERF_PROFILE)
    g_ld_profile.total.sink_end_qpc += (u64)(ld_profile_qpc() - begin);
#endif
}

static void sink_clear(void* user, const rsx_dispatch* r, u32 mask)
{
    (void)user; (void)r;
    if (g_ld_movie_mode && !ld_movie_composite_ui_enabled()) return;
    g_ld_stats.clears++;
    const u32 target = current_surface();
    if (target == LD_INVALID_SURFACE) { g_ld_stats.clear_drop_surface++; return; }
    if (rsx_live_draw_a010_probe_active() && target < 64)
        g_ld_a010_probe_touched |= 1ull << target;
    if (ld_target_trace_enabled()) {
        static u32 last_clear_target = LD_INVALID_SURFACE;
        const int changed = target != last_clear_target;
        if (changed) {
            ld_trace_target("clear-target", target, mask);
            last_clear_target = target;
        }
        /* A broken scene can issue tens of thousands of clears. Preserve the
         * opening sequence and a periodic heartbeat without turning tracing
         * itself into a scheduler perturbation. Target changes are always
         * emitted above. */
        if (g_ld_stats.clears <= 256 || (g_ld_stats.clears & 1023) == 0) {
            const u32 z = rsx_dsp_clear_zstencil(&g.rsx);
            fprintf(stderr,
                    "[rsx-clear] frame=%u n=%llu target=%u mask=0x%02X "
                    "argb=0x%08X z24=0x%06X stencil=0x%02X\n",
                    g_ld_frames, g_ld_stats.clears, target, mask,
                    rsx_dsp_clear_color(&g.rsx), z >> 8, z & 0xFF);
        }
    }
    if (s_csv_file && s_csv_gate_open) {
        fprintf(s_csv_file, "#clear,%u,0x%X,mask=0x%02X,scissor=0x%08X/0x%08X,cmask=0x%08X,argb=0x%08X\n",
                g_ld_frames, g.surfaces[target].offset, mask,
                rsx_dsp_reg(&g.rsx, M_SCISSOR_HORIZONTAL), rsx_dsp_reg(&g.rsx, M_SCISSOR_VERTICAL),
                rsx_dsp_reg(&g.rsx, M_COLOR_MASK), rsx_dsp_clear_color(&g.rsx));
        fflush(s_csv_file);
    }
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtv_handle(LD_SWAP_BUFFERS + target);
    if (mask & (RSX_CLEAR_COLOR_R | RSX_CLEAR_COLOR_G | RSX_CLEAR_COLOR_B | RSX_CLEAR_COLOR_A)) {
        const u32 c = rsx_dsp_clear_color(&g.rsx);
        float col[4] = { ((c >> 16) & 0xFF) / 255.0f, ((c >> 8) & 0xFF) / 255.0f,
                         (c & 0xFF) / 255.0f, ((c >> 24) & 0xFF) / 255.0f };
        /* LD_CLEAR_TEST=1: clear to magenta instead of the guest's colour.
         *
         * This validates the MEASUREMENT, not the renderer. LD_SURF_DUMP
         * reports nonblack=0 for every surface, and every conclusion drawn from
         * that depends on the readback actually being able to see a non-black
         * pixel. If magenta clears still read back as 0, the readback is broken
         * and those conclusions are void. Establish that X would be visible
         * before concluding X never happens. */
        { static int ct = -1;
          if (ct < 0) ct = getenv("LD_CLEAR_TEST") ? 1 : 0;
          if (ct) { col[0] = 1.0f; col[1] = 0.0f; col[2] = 1.0f; col[3] = 1.0f; } }
        g.list->lpVtbl->ClearRenderTargetView(g.list, rtv, col, 0, NULL);
        ld_surface_note_write(target, LD_SURFACE_WRITE_CLEAR);
    }
    if ((mask & (RSX_CLEAR_DEPTH | RSX_CLEAR_STENCIL)) && g.depth) {
        rsx_dsp_surface sf;
        rsx_dsp_get_surface(&g.rsx, &sf);
        const u32 zslot = zdepth_get(sf.zeta_location, sf.zeta_offset,
                                     sf.clip_w, sf.clip_h);
        D3D12_CPU_DESCRIPTOR_HANDLE dsv = dsv_handle(zslot);
        D3D12_CLEAR_FLAGS clear_flags = (D3D12_CLEAR_FLAGS)0;
        if (mask & RSX_CLEAR_DEPTH)
            clear_flags = (D3D12_CLEAR_FLAGS)(clear_flags | D3D12_CLEAR_FLAG_DEPTH);
        if (mask & RSX_CLEAR_STENCIL)
            clear_flags = (D3D12_CLEAR_FLAGS)(clear_flags | D3D12_CLEAR_FLAG_STENCIL);
        /* ZSTENCIL_CLEAR_VALUE is laid out as the zeta is: depth24 << 8 |
         * stencil8 for Z24S8, a bare depth16 for Z16 (SURFACE_FORMAT depth
         * field 1). Read as Z24S8, a Z16 surface's 0xFFFF -- its far plane --
         * would clear it to 255/16777215, nearly the near plane, and every
         * LESS or LEQUAL draw into it would then fail. The nv40 reset
         * 0xFFFFFF00 seeded by rsx_dispatch_init clears to 1.0 / 0. Same
         * decode as the draw engine's clear. */
        {
            const u32 zs = rsx_dsp_reg(&g.rsx, M_ZSTENCIL_CLEAR);
            const int z16 = sf.depth_format == 1u;
            const float cd = !zs ? 1.0f
                           : z16 ? (float)(zs & 0xFFFFu) / 65535.0f
                                 : (float)(zs >> 8) / 16777215.0f;
            { static int zd = -1; static int zn = 0; if (zd < 0) { const char* e4 = getenv("GOWA_ZCLEAR_DBG"); zd = e4 ? atoi(e4) : 0; }
              if (zd && zn < 60 && (int)g_ld_frames > zd) { zn++;
                  fprintf(stderr, "[zclear] frame %u zeta %u:0x%X slot %u zs=0x%08X z16=%d -> %f mask=0x%X\n",
                          g_ld_frames, sf.zeta_location, sf.zeta_offset, zslot, zs, z16, cd, mask); } }
            g.list->lpVtbl->ClearDepthStencilView(g.list, dsv,
                clear_flags, cd, z16 ? (UINT8)0 : (UINT8)(zs & 0xFFu), 0, NULL);
        }
        if (zslot) {
            g.zdepths[zslot - 1].cleared = 1;
            if (mask & RSX_CLEAR_DEPTH) {
                g.zdepths[zslot - 1].had_write = 0;
                /* Each guest zeta owns a distinct live resource.  A clear
                 * invalidates its older published image; do not resolve it
                 * speculatively.  The first later texture consumer resolves
                 * the newly written pass exactly once. */
                g.zdepths[zslot - 1].snapshot_valid = 0;
            }
        } else {
            if (mask & RSX_CLEAR_DEPTH)
                g.depth_cleared = 1;
        }
    }
}

static void sink_flip(void* user, const rsx_dispatch* r, u32 arg)
{
    (void)user; (void)r;
    const u32 buffer_id = arg & 7u;
    g_ld_flip_requested++;
    g_ld_last_requested_buffer = buffer_id;
    if (g_ld_movie_mode) {
        if (ld_movie_composite_ui_enabled()) ld_movie_capture_overlay();
        g_ld_flip_consumed++;
        g_ld_last_consumed_buffer = buffer_id;
        return;
    }
    rsx_live_draw_present(buffer_id);
    g_ld_flip_consumed++;
    g_ld_last_consumed_buffer = buffer_id;
}

/* ---------------------------------------------------------------------------
 * device / resource setup
 * -----------------------------------------------------------------------*/
static int make_root_signature(void)
{
    D3D12_DESCRIPTOR_RANGE xrange = {0};
    xrange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    xrange.NumDescriptors = SRV_TABLE_SIZE;
    D3D12_DESCRIPTOR_RANGE srange = {0};
    srange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    srange.NumDescriptors = SMP_TABLE_SIZE;
    D3D12_DESCRIPTOR_RANGE vrange = {0};
    vrange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    vrange.NumDescriptors = RSX_DSP_NUM_VERTEX_TEXTURES;
    vrange.BaseShaderRegister = 16;
    D3D12_ROOT_PARAMETER xp[6] = {0};
    xp[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    xp[0].Descriptor.ShaderRegister = 0;
    xp[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    xp[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    xp[1].DescriptorTable.NumDescriptorRanges = 1;
    xp[1].DescriptorTable.pDescriptorRanges = &xrange;
    xp[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    xp[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    xp[2].DescriptorTable.NumDescriptorRanges = 1;
    xp[2].DescriptorTable.pDescriptorRanges = &srange;
    xp[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    xp[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    xp[3].DescriptorTable.NumDescriptorRanges = 1;
    xp[3].DescriptorTable.pDescriptorRanges = &vrange;
    xp[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    xp[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    xp[4].Descriptor.ShaderRegister = 1;
    xp[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    xp[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;   /* GOWA: luzes do EDGE (b2) */
    xp[5].Descriptor.ShaderRegister = 2;
    xp[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    D3D12_STATIC_SAMPLER_DESC vsmp[RSX_DSP_NUM_VERTEX_TEXTURES] = {0};
    for (u32 i = 0; i < RSX_DSP_NUM_VERTEX_TEXTURES; i++) {
        vsmp[i].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        vsmp[i].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        vsmp[i].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        vsmp[i].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        vsmp[i].MaxLOD = D3D12_FLOAT32_MAX;
        vsmp[i].ShaderRegister = i;
        vsmp[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    }
    D3D12_ROOT_SIGNATURE_DESC rsd = {0};
    rsd.NumParameters = 6; rsd.pParameters = xp;
    rsd.NumStaticSamplers = RSX_DSP_NUM_VERTEX_TEXTURES;
    rsd.pStaticSamplers = vsmp;
    rsd.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ID3DBlob* sig = NULL; ID3DBlob* err = NULL;
    if (FAILED(D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err))) {
        if (err) err->lpVtbl->Release(err); return -1;
    }
    HRESULT hr = g.dev->lpVtbl->CreateRootSignature(g.dev, 0, sig->lpVtbl->GetBufferPointer(sig),
                                                    sig->lpVtbl->GetBufferSize(sig),
                                                    &IID_ID3D12RootSignature, (void**)&g.rootsig_x);
    sig->lpVtbl->Release(sig);
    return SUCCEEDED(hr) ? 0 : -1;
}

static int make_depth_snapshot_pipeline(void)
{
    static const char source[] =
        "Texture2D<float> depth_source : register(t0);\n"
        "RWTexture2D<float> depth_destination : register(u0);\n"
        "[numthreads(8, 8, 1)]\n"
        "void main(uint3 id : SV_DispatchThreadID) {\n"
        "    uint width, height;\n"
        "    depth_destination.GetDimensions(width, height);\n"
        "    if (id.x < width && id.y < height)\n"
        "        depth_destination[id.xy] = "
        "depth_source.Load(int3(id.xy, 0));\n"
        "}\n";
    ID3DBlob* shader = NULL;
    ID3DBlob* errors = NULL;
    HRESULT hr = D3DCompile(
        source, sizeof(source) - 1u, "depth_snapshot_compute", NULL, NULL,
        "main", "cs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
        &shader, &errors);
    if (FAILED(hr)) {
        fprintf(stderr,
                "[zetatrack] depth snapshot shader compile failed "
                "hr=0x%08lX: %s\n",
                (unsigned long)hr,
                errors ? (const char*)errors->lpVtbl->GetBufferPointer(errors)
                       : "<no diagnostics>");
        if (errors) errors->lpVtbl->Release(errors);
        return -1;
    }
    if (errors) errors->lpVtbl->Release(errors);

    D3D12_DESCRIPTOR_RANGE ranges[2] = {0};
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[0].NumDescriptors = 1;
    ranges[0].BaseShaderRegister = 0;
    ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[1].NumDescriptors = 1;
    ranges[1].BaseShaderRegister = 0;
    D3D12_ROOT_PARAMETER parameters[2] = {0};
    for (u32 i = 0; i < 2; ++i) {
        parameters[i].ParameterType =
            D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameters[i].DescriptorTable.NumDescriptorRanges = 1;
        parameters[i].DescriptorTable.pDescriptorRanges = &ranges[i];
        parameters[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    }
    D3D12_ROOT_SIGNATURE_DESC description = {0};
    description.NumParameters = 2;
    description.pParameters = parameters;
    ID3DBlob* serialized = NULL;
    ID3DBlob* signature_errors = NULL;
    hr = D3D12SerializeRootSignature(
        &description, D3D_ROOT_SIGNATURE_VERSION_1,
        &serialized, &signature_errors);
    if (FAILED(hr)) {
        fprintf(stderr,
                "[zetatrack] depth snapshot root signature failed "
                "hr=0x%08lX: %s\n",
                (unsigned long)hr,
                signature_errors
                    ? (const char*)signature_errors->lpVtbl->GetBufferPointer(
                          signature_errors)
                    : "<no diagnostics>");
        if (signature_errors)
            signature_errors->lpVtbl->Release(signature_errors);
        shader->lpVtbl->Release(shader);
        return -1;
    }
    if (signature_errors)
        signature_errors->lpVtbl->Release(signature_errors);
    hr = g.dev->lpVtbl->CreateRootSignature(
        g.dev, 0, serialized->lpVtbl->GetBufferPointer(serialized),
        serialized->lpVtbl->GetBufferSize(serialized),
        &IID_ID3D12RootSignature, (void**)&g.depth_snapshot_rootsig);
    serialized->lpVtbl->Release(serialized);
    if (FAILED(hr)) {
        shader->lpVtbl->Release(shader);
        return -1;
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline = {0};
    pipeline.pRootSignature = g.depth_snapshot_rootsig;
    pipeline.CS.pShaderBytecode = shader->lpVtbl->GetBufferPointer(shader);
    pipeline.CS.BytecodeLength = shader->lpVtbl->GetBufferSize(shader);
    hr = g.dev->lpVtbl->CreateComputePipelineState(
        g.dev, &pipeline, &IID_ID3D12PipelineState,
        (void**)&g.depth_snapshot_pso);
    shader->lpVtbl->Release(shader);
    return SUCCEEDED(hr) ? 0 : -1;
}

int rsx_live_draw_init(void* hwnd, u32 width, u32 height,
                       rsx_live_guest_ptr_fn guest_fn, void* guest_user)
{
    if (!rsx_live_draw_enabled()) return 0;
    if (g.ready) return 0;
    ld_present_measure_init();
#if defined(YZ_PERF_PROFILE)
    memset(&g_ld_profile, 0, sizeof(g_ld_profile));
    {
        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        g_ld_profile.qpc_frequency = frequency.QuadPart;
        g_ld_profile.previous_present_qpc = ld_profile_qpc();
    }
#endif
    g.width = width; g.height = height;
    { const char* e = getenv("GOWA_RES_SCALE"); const long v = e ? strtol(e, NULL, 10) : 1;
      g.res_scale = (v >= 1 && v <= 4) ? (u32)v : 1u;
      if (g.res_scale != 1) fprintf(stderr, "[live-draw] resolucao interna %ux (%ux%u)\n",
                                    g.res_scale, width * g.res_scale, height * g.res_scale); }
    g.guest_ptr = guest_fn; g.guest_user = guest_user;
    {
        const char* requested = getenv("RSX_FP_CONSTANT_MODE");
        char mode = 'B';
        if (requested && requested[0]) {
            if ((requested[0] == 'L' || requested[0] == 'l') &&
                (!requested[1] ||
                 _stricmp(requested, "literal") == 0))
                mode = 'L';
            else if ((requested[0] == 'B' || requested[0] == 'b') &&
                     (!requested[1] ||
                      _stricmp(requested, "buffered") == 0))
                mode = 'B';
            else
                fprintf(
                    stderr,
                    "[rsx-fp-constants] unknown "
                    "RSX_FP_CONSTANT_MODE='%s'; using buffered\n",
                    requested);
        }
        g.fp_constant_mode = mode;
        fprintf(
            stderr,
            "[rsx-fp-constants] mode=%s (startup-selected) "
            "d3dcompile_flags1=0x0 d3dcompile_flags2=0x0\n",
            ld_fp_constant_mode_name());
    }
    {
        const char* requested = getenv("RSX_VERTEX_MODE");
        const char* old_path = getenv("RSX_VERTEX_PATH");
        char mode = 'C';
        if (requested && requested[0] && !requested[1]) {
            mode = (char)toupper((unsigned char)requested[0]);
        } else if (!requested && old_path) {
            mode = strcmp(old_path, "compact") == 0 ? 'C' : 'L';
        }
        switch (mode) {
        case 'L':
            g.vertex_features = 0;
            break;
        case 'H':
            g.vertex_features = LD_VERTEX_HOIST_FETCH;
            break;
        case 'M':
            g.vertex_features =
                LD_VERTEX_HOIST_FETCH | LD_VERTEX_MASK_SIGNATURE;
            break;
        case 'C':
            g.vertex_features =
                LD_VERTEX_HOIST_FETCH | LD_VERTEX_MASK_SIGNATURE |
                LD_VERTEX_COMPACT_PAYLOAD;
            break;
        default:
            fprintf(stderr,
                    "[rsx-vertex] unknown RSX_VERTEX_MODE='%s'; "
                    "using C\n",
                    requested ? requested : "");
            mode = 'C';
            g.vertex_features =
                LD_VERTEX_HOIST_FETCH | LD_VERTEX_MASK_SIGNATURE |
                LD_VERTEX_COMPACT_PAYLOAD;
            break;
        }
        g.vertex_mode = mode;
#if !defined(YZ_PERF_CLEAN)
        const char* diag_dir = getenv("RSX_VERTEX_DIAG_DIR");
        if (diag_dir && diag_dir[0]) {
            strncpy(
                g.vertex_diag_dir, diag_dir,
                sizeof(g.vertex_diag_dir) - 1u);
            g.vertex_diag_dir[sizeof(g.vertex_diag_dir) - 1u] = '\0';
        }
#endif
        fprintf(stderr,
                "[rsx-vertex] mode=%s features=0x%X "
                "(startup-selected)\n",
                ld_vertex_mode_name(), g.vertex_features);
    }
    g_ld_flip_requested = 0;
    g_ld_flip_consumed = 0;
    g_ld_last_requested_buffer = UINT32_MAX;
    g_ld_last_consumed_buffer = UINT32_MAX;
    g_ld_last_present_target = UINT32_MAX;
#if !defined(YZ_PERF_CLEAN)
    g_ld_surface_generation = 0;
    g_ld_surface_resource_serial = 0;
    g_ld_guest_blit_generation = 0;
    g_ld_present_copy_generation = 0;
    g_ld_vertex_constant_ring_recycles = 0;
    g_ld_frame_vertex_constant_ring_recycles = 0;
    g_ld_fifo_get = 0;
    g_ld_fifo_put = 0;
    InterlockedExchange(&g_ld_diag_post_movie_pending, 0);
    InterlockedExchange(&g_ld_diag_post_movie_presents, 0);
#endif
    g_ld_last_dump_fingerprint = 0;
    memset(g_ld_layout_cache, 0, sizeof(g_ld_layout_cache));
    g_ld_layout_cache_count = 0;

    ld_enable_debug_layer();
    ld_enable_dred();
    IDXGIFactory4* factory = NULL;
    if (FAILED(CreateDXGIFactory1(&IID_IDXGIFactory4, (void**)&factory))) return -1;
    /* GOWA: the adapter. D3D12CreateDevice(NULL) takes DXGI's adapter 0, the
     * one behind the primary desktop -- on a machine whose primary display is
     * a virtual one, or is wired to the integrated GPU, that is not the fast
     * card. GOWA_GPU=high (default) asks DXGI for the high-performance adapter,
     * =low for the power-saving one, =default for the old adapter 0; the one
     * in use goes to the log. */
    IDXGIAdapter1* gpu = NULL;
    { const char* e = getenv("GOWA_GPU");
      /* GOWA_GPU=warp (30/10): o renderizador de software da Microsoft (WARP), a referência do D3D12, para
       * conferir defeitos que só aparecem com o driver de alguns jogadores (tela preta 397/425/431) */
      if (e && !_stricmp(e, "warp")) {
          IDXGIFactory4* f4 = NULL;
          if (SUCCEEDED(factory->lpVtbl->QueryInterface(factory, &IID_IDXGIFactory4, (void**)&f4))) {
              f4->lpVtbl->EnumWarpAdapter(f4, &IID_IDXGIAdapter1, (void**)&gpu);
              f4->lpVtbl->Release(f4);
          }
      }
      const int pref = gpu ? 0 : !e || !_stricmp(e, "high") ? 2 : !_stricmp(e, "low") ? 1 : 0;
      IDXGIFactory6* f6 = NULL;
      if (pref && SUCCEEDED(factory->lpVtbl->QueryInterface(factory, &IID_IDXGIFactory6, (void**)&f6))) {
          f6->lpVtbl->EnumAdapterByGpuPreference(f6, 0, pref == 2 ? DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE
                                                                   : DXGI_GPU_PREFERENCE_MINIMUM_POWER,
                                                 &IID_IDXGIAdapter1, (void**)&gpu);
          f6->lpVtbl->Release(f6);
      } }
    if (FAILED(D3D12CreateDevice((IUnknown*)gpu, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void**)&g.dev))) {
        if (!gpu || FAILED(D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void**)&g.dev))) {
            if (gpu) gpu->lpVtbl->Release(gpu);
            factory->lpVtbl->Release(factory); return -1;
        }
    }
    if (gpu) gpu->lpVtbl->Release(gpu);
    /* GOWA: D3D12_BLEND_ALPHA_FACTOR (the blend factor's alpha in the colour
     * channels), for the RSX's CONSTANT_ALPHA next to a CONSTANT_COLOR in the
     * same blend (see gcm_blend_factor) */
    { D3D12_FEATURE_DATA_D3D12_OPTIONS13 o13; memset(&o13, 0, sizeof o13);
      if (SUCCEEDED(g.dev->lpVtbl->CheckFeatureSupport(g.dev, D3D12_FEATURE_D3D12_OPTIONS13, &o13, sizeof o13)))
          g_ld_alpha_factor_ok = o13.AlphaBlendFactorSupported ? 1 : 0;
      { const char* e = getenv("GOWA_BLEND_COLOR"); if (e && *e == '0') g_ld_alpha_factor_ok = 0; }
      fprintf(stderr, "[D3D12] fator de blend por alfa: %s\n", g_ld_alpha_factor_ok ? "sim" : "nao"); }
    { LUID luid = {0}; g.dev->lpVtbl->GetAdapterLuid(g.dev, &luid);
      IDXGIAdapter1* used = NULL;
      if (SUCCEEDED(factory->lpVtbl->EnumAdapterByLuid(factory, luid, &IID_IDXGIAdapter1, (void**)&used))) {
          DXGI_ADAPTER_DESC1 ad; used->lpVtbl->GetDesc1(used, &ad);
          char name[128]; WideCharToMultiByte(CP_UTF8, 0, ad.Description, -1, name, sizeof name, NULL, NULL);
          fprintf(stderr, "[D3D12] adaptador: %s (%llu MB de VRAM dedicada)\n", name,
                  (unsigned long long)(ad.DedicatedVideoMemory >> 20));
          { extern char g_ld_gpu_nome[128]; extern unsigned g_ld_gpu_vram_mb;   /* desempenho.json */
            memcpy(g_ld_gpu_nome, name, sizeof name); g_ld_gpu_vram_mb = (unsigned)(ad.DedicatedVideoMemory >> 20); }
          { extern LUID g_ld_osd_luid; g_ld_osd_luid = luid; }   /* GPU%/temperatura do adaptador do jogo */
          { extern IDXGIAdapter3* g_ld_osd_adapter;   /* VRAM em uso para o medidor na tela */
            used->lpVtbl->QueryInterface(used, &IID_IDXGIAdapter3, (void**)&g_ld_osd_adapter); }
          used->lpVtbl->Release(used);
      } }
#if defined(YZ_PERF_PROFILE)
    {
        LUID luid = {0};
        g.dev->lpVtbl->GetAdapterLuid(g.dev, &luid);
        factory->lpVtbl->EnumAdapterByLuid(
            factory, luid, &IID_IDXGIAdapter3,
            (void**)&g_ld_profile.adapter);
    }
#endif
    ld_open_info_queue();
    D3D12_COMMAND_QUEUE_DESC qd = {0};
    g.dev->lpVtbl->CreateCommandQueue(g.dev, &qd, &IID_ID3D12CommandQueue, (void**)&g.queue);
    g.dev->lpVtbl->CreateCommandAllocator(g.dev, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                          &IID_ID3D12CommandAllocator, (void**)&g.alloc);
    g.dev->lpVtbl->CreateCommandList(g.dev, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, g.alloc, NULL,
                                     &IID_ID3D12GraphicsCommandList, (void**)&g.list);
    if (g.queue) g.queue->lpVtbl->SetName(g.queue, L"rsx-live-queue");
    if (g.list) g.list->lpVtbl->SetName(g.list, L"rsx-live-list");
    g.dev->lpVtbl->CreateFence(g.dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void**)&g.fence);
    g.fence_event = CreateEventA(NULL, FALSE, FALSE, NULL);

    /* swap chain bound to the runtime's HWND */
    DXGI_SWAP_CHAIN_DESC1 scd = {0};
    scd.Width = LD_PX(width); scd.Height = LD_PX(height); scd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;   /* GOWA_RES_SCALE */
    scd.SampleDesc.Count = 1; scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    { const char* e = getenv("GOWA_SWAP_BUFFERS");
      if (e) { const int n = atoi(e); if (n >= 2 && n <= LD_SWAP_BUFFERS) g_ld_swap_count = (u32)n; } }
    scd.BufferCount = g_ld_swap_count; scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    /* GOWA (27/10): VRR (G-SYNC / FreeSync), o padrão; GOWA_VRR=0 desliga (opção
     * "VRR" do launcher). Ligado, o quadro vai para a tela sem esperar a atualização
     * dela (tearing permitido quando o sistema deixa): num monitor com VRR a tela
     * acompanha o jogo, e uma queda de fps não vira travadinha; o ritmo é o do
     * GOWA_FLIP_HZ e o da geração de quadros. Também tira do Present a espera que
     * segurava a thread que anda o FIFO. Desligado, cada quadro espera a tela (vsync).
     * GOWA_PRESENT_SYNC=0/1, quando definido, manda (o ajuste manual de antes). */
    {   const char* e = getenv("GOWA_PRESENT_SYNC");
        const char* vrr = getenv("GOWA_VRR");
        g_ld_present_sync = (e && *e) ? *e != '0' : (vrr && *vrr == '0');
        if (!g_ld_present_sync) {
            IDXGIFactory5* f5 = NULL; BOOL allow = FALSE;
            if (SUCCEEDED(factory->lpVtbl->QueryInterface(factory, &IID_IDXGIFactory5, (void**)&f5)) && f5) {
                if (FAILED(f5->lpVtbl->CheckFeatureSupport(f5, DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow, sizeof allow))) allow = FALSE;
                f5->lpVtbl->Release(f5);
            }
            g_ld_present_tearing = allow ? 1 : 0;
            if (allow) scd.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
            fprintf(stderr, "[present] VRR: sem esperar a tela (tearing %s)\n", allow ? "permitido" : "indisponivel");
        } else fprintf(stderr, "[present] vsync: cada quadro espera a tela\n"); }
    IDXGISwapChain1* sc1 = NULL;
    if (FAILED(factory->lpVtbl->CreateSwapChainForHwnd(factory, (IUnknown*)g.queue,
            (HWND)hwnd, &scd, NULL, NULL, &sc1))) {
        factory->lpVtbl->Release(factory); return -1;
    }
    sc1->lpVtbl->QueryInterface(sc1, &IID_IDXGISwapChain3, (void**)&g.swap);
    sc1->lpVtbl->Release(sc1);
    /* GOWA: sem o Alt+Enter do DXGI. Ele trocava para a tela cheia exclusiva, que
     * este port não trata (os buffers continuam do tamanho da janela, e
     * ALLOW_TEARING é proibido nela): os Present falhavam e a imagem congelava com
     * o som seguindo (relato de jogador, 25/10). A tela cheia é a sem bordas da
     * janela (rsx_null_backend.c: Alt+Enter, F11, maximizar). */
    factory->lpVtbl->MakeWindowAssociation(factory, (HWND)hwnd, DXGI_MWA_NO_ALT_ENTER);
    factory->lpVtbl->Release(factory);

    /* RTV heap: [0..1] backbuffers, [2..] surface cache */
    D3D12_DESCRIPTOR_HEAP_DESC hd = {0};
    hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    hd.NumDescriptors = LD_SWAP_BUFFERS + MAX_SURFACES;
    g.dev->lpVtbl->CreateDescriptorHeap(g.dev, &hd, &IID_ID3D12DescriptorHeap, (void**)&g.rtv_heap);
    g.rtv_step = g.dev->lpVtbl->GetDescriptorHandleIncrementSize(g.dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for (u32 i = 0; i < g_ld_swap_count; i++) {
        g.swap->lpVtbl->GetBuffer(g.swap, i, &IID_ID3D12Resource, (void**)&g.backbuf[i]);
        g.dev->lpVtbl->CreateRenderTargetView(g.dev, g.backbuf[i], NULL, rtv_handle(i));
    }

    /* upload arena */
    D3D12_HEAP_PROPERTIES hp = {0}; hp.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC bd = {0};
    bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = UPLOAD_SIZE;
    bd.Height = 1; bd.DepthOrArraySize = 1; bd.MipLevels = 1;
    bd.Format = DXGI_FORMAT_UNKNOWN; bd.SampleDesc.Count = 1;
    bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    g.dev->lpVtbl->CreateCommittedResource(g.dev, &hp, D3D12_HEAP_FLAG_NONE, &bd,
        D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource, (void**)&g.upload);
    D3D12_RANGE rr = {0, 0};
    g.upload->lpVtbl->Map(g.upload, 0, &rr, (void**)&g.upload_mapped);

    bd.Width = VERT_BUFFER_SIZE;
    g.dev->lpVtbl->CreateCommittedResource(g.dev, &hp, D3D12_HEAP_FLAG_NONE, &bd,
        D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource, (void**)&g.vb);
    g.vb->lpVtbl->Map(g.vb, 0, &rr, (void**)&g.vb_mapped);

    bd.Width = INDEX_BUFFER_SIZE;
    g.dev->lpVtbl->CreateCommittedResource(g.dev, &hp, D3D12_HEAP_FLAG_NONE, &bd,
        D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource, (void**)&g.ib);
    g.ib->lpVtbl->Map(g.ib, 0, &rr, (void**)&g.ib_mapped);

    bd.Width = CB_RING_BYTES;
    g.dev->lpVtbl->CreateCommittedResource(g.dev, &hp, D3D12_HEAP_FLAG_NONE, &bd,
        D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource, (void**)&g.cb);
    g.cb->lpVtbl->Map(g.cb, 0, &rr, (void**)&g.cb_mapped);

    bd.Width = PS_CB_RING_BYTES;
    g.dev->lpVtbl->CreateCommittedResource(
        g.dev, &hp, D3D12_HEAP_FLAG_NONE, &bd,
        D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
        &IID_ID3D12Resource, (void**)&g.ps_cb);
    if (g.ps_cb)
        g.ps_cb->lpVtbl->Map(
            g.ps_cb, 0, &rr, (void**)&g.ps_cb_mapped);

    /* SRV heaps */
    hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    hd.NumDescriptors = SRV_CPU_HEAP_SLOTS; hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    g.dev->lpVtbl->CreateDescriptorHeap(g.dev, &hd, &IID_ID3D12DescriptorHeap, (void**)&g.srv_cpu_heap);
    hd.NumDescriptors = SRV_RING_TABLES * SRV_TABLE_SIZE;
    hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    g.dev->lpVtbl->CreateDescriptorHeap(g.dev, &hd, &IID_ID3D12DescriptorHeap, (void**)&g.srv_heap);
    g.srv_step = g.dev->lpVtbl->GetDescriptorHandleIncrementSize(g.dev, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    /* sampler heaps */
    D3D12_DESCRIPTOR_HEAP_DESC shd = {0};
    shd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
    shd.NumDescriptors = SMP_CACHE_SLOTS + 1; shd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    g.dev->lpVtbl->CreateDescriptorHeap(g.dev, &shd, &IID_ID3D12DescriptorHeap, (void**)&g.smp_cpu_heap);
    shd.NumDescriptors = SMP_RING_TABLES * SMP_TABLE_SIZE;
    shd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    g.dev->lpVtbl->CreateDescriptorHeap(g.dev, &shd, &IID_ID3D12DescriptorHeap, (void**)&g.smp_heap);
    g.smp_step = g.dev->lpVtbl->GetDescriptorHandleIncrementSize(g.dev, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    /* Fail init (fall back to null present) rather than crash later if any
     * shader-visible/CPU descriptor heap didn't create -- e.g. an over-limit
     * sampler heap would return NULL here and fault in sampler_table. */
    if (!g.srv_cpu_heap || !g.srv_heap || !g.smp_cpu_heap || !g.smp_heap) {
        fprintf(stderr, "[live-draw] descriptor heap alloc failed "
                "(srv_cpu=%p srv=%p smp_cpu=%p smp=%p)\n",
                (void*)g.srv_cpu_heap, (void*)g.srv_heap,
                (void*)g.smp_cpu_heap, (void*)g.smp_heap);
        return -1;
    }
    /* GOWA_LD_PIPE=1: the second set of per-submission resources (see
     * ld_ring_set); the first is everything created above. */
    {
        s_pipe = 0; s_cur_set = 0;
        s_sets[0].retired = (ID3D12Resource**)calloc(MAX_RETIRED_TEXTURES, sizeof(ID3D12Resource*));
        s_sets[1].retired = (ID3D12Resource**)calloc(MAX_RETIRED_TEXTURES, sizeof(ID3D12Resource*));
        ld_set_save(0);
        const char* pe = getenv("GOWA_LD_PIPE");   /* default on (A/B +4.6%); =0 off */
        if (!(pe && *pe == '0') && s_sets[0].retired && s_sets[1].retired) {
            ld_ring_set* r = &s_sets[1];
            D3D12_HEAP_PROPERTIES uhp = {0}; uhp.Type = D3D12_HEAP_TYPE_UPLOAD;
            D3D12_RESOURCE_DESC ubd = bd;
            int ok = SUCCEEDED(g.dev->lpVtbl->CreateCommandAllocator(g.dev, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                   &IID_ID3D12CommandAllocator, (void**)&r->alloc));
            const UINT64 sizes[5] = { UPLOAD_SIZE, VERT_BUFFER_SIZE, INDEX_BUFFER_SIZE, CB_RING_BYTES, PS_CB_RING_BYTES };
            ID3D12Resource** res[5] = { &r->upload, &r->vb, &r->ib, &r->cb, &r->ps_cb };
            void** maps[5] = { (void**)&r->upload_mapped, (void**)&r->vb_mapped, (void**)&r->ib_mapped,
                               (void**)&r->cb_mapped, (void**)&r->ps_cb_mapped };
            for (int k = 0; k < 5 && ok; k++) {
                ubd.Width = sizes[k];
                ok = SUCCEEDED(g.dev->lpVtbl->CreateCommittedResource(g.dev, &uhp, D3D12_HEAP_FLAG_NONE, &ubd,
                        D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource, (void**)res[k])) &&
                     SUCCEEDED((*res[k])->lpVtbl->Map(*res[k], 0, &rr, maps[k]));
            }
            D3D12_DESCRIPTOR_HEAP_DESC h2 = {0};
            h2.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            h2.NumDescriptors = SRV_RING_TABLES * SRV_TABLE_SIZE;
            h2.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            if (ok) ok = SUCCEEDED(g.dev->lpVtbl->CreateDescriptorHeap(g.dev, &h2, &IID_ID3D12DescriptorHeap, (void**)&r->srv_heap));
            h2.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
            h2.NumDescriptors = SMP_RING_TABLES * SMP_TABLE_SIZE;
            if (ok) ok = SUCCEEDED(g.dev->lpVtbl->CreateDescriptorHeap(g.dev, &h2, &IID_ID3D12DescriptorHeap, (void**)&r->smp_heap));
            s_pipe = ok;
            fprintf(stderr, "[live-draw] GOWA_LD_PIPE: %s\n", ok ? "two resource sets, CPU and GPU overlap"
                                                                  : "second set failed; synchronous flushes");
        }
    }
    {
        D3D12_SAMPLER_DESC def = {0};
        def.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        def.AddressU = def.AddressV = def.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        def.MaxLOD = D3D12_FLOAT32_MAX; def.MaxAnisotropy = 1;
        g.dev->lpVtbl->CreateSampler(g.dev, &def, smp_cpu(SMP_DEFAULT));
    }

    /* depth */
    D3D12_DESCRIPTOR_HEAP_DESC dhd = {0};
    dhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dhd.NumDescriptors = 1 + MAX_SURFACES;
    g.dev->lpVtbl->CreateDescriptorHeap(g.dev, &dhd, &IID_ID3D12DescriptorHeap, (void**)&g.dsv_heap);
    g.dsv_step = g.dev->lpVtbl->GetDescriptorHandleIncrementSize(
        g.dev, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    {
        D3D12_HEAP_PROPERTIES dhp = {0}; dhp.Type = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC drd = {0};
        drd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        drd.Width = width; drd.Height = height; drd.DepthOrArraySize = 1;
        drd.MipLevels = 1; drd.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT; drd.SampleDesc.Count = 1;
        drd.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        D3D12_CLEAR_VALUE dcv = {0}; dcv.Format = drd.Format; dcv.DepthStencil.Depth = 1.0f;
        if (SUCCEEDED(g.dev->lpVtbl->CreateCommittedResource(g.dev, &dhp, D3D12_HEAP_FLAG_NONE, &drd,
                D3D12_RESOURCE_STATE_DEPTH_WRITE, &dcv, &IID_ID3D12Resource, (void**)&g.depth))) {
            D3D12_CPU_DESCRIPTOR_HANDLE dh;
            g.dsv_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(g.dsv_heap, &dh);
            g.dev->lpVtbl->CreateDepthStencilView(g.dev, g.depth, NULL, dh);
        } else {
            g.depth = NULL;
        }
    }

    if (make_root_signature() != 0 ||
        make_depth_snapshot_pipeline() != 0)
        return -1;

    { extern int g_rsx_vp_tc89;   /* rsx_vp_decompiler.c */
      const char* e = getenv("GOWA_TC89"); g_rsx_vp_tc89 = !(e && *e == '0'); }
    static const u8 white[4] = {255, 255, 255, 255};
    g.white_tex = create_texture_rgba(white, 1, 1);
    if (g.white_tex) srv_write(SRV_WHITE, g.white_tex);
    { const tex_level_t wl = { 1, 1, white, 4, 1 }; const u32 wd = 1;
      g.white_tex3d = create_texture_3d(DXGI_FORMAT_R8G8B8A8_UNORM, &wl, &wd, 1);
      if (g.white_tex3d) srv_write(SRV_WHITE3D, g.white_tex3d); }

    /* dispatcher + sink */
    rsx_dispatch_sink sink = {0};
    sink.user = &g;
    sink.clear = sink_clear;
    sink.begin = sink_begin;
    sink.end = sink_end;
    sink.draw_arrays = sink_draw_arrays;
    sink.inline_array = sink_inline_array;
    sink.draw_index_array = sink_draw_index;
    sink.flip = sink_flip;
    rsx_dispatch_init(&g.rsx, &sink);

    g.ready = 1;
    g.enabled = 1;
    return 0;
}

void rsx_live_draw_seed_registers(const u32* regs, u32 count)
{
    if (g.ready) rsx_dispatch_seed_registers(&g.rsx, regs, count);
}
void rsx_live_draw_seed_transform_program(const u32* words, u32 count)
{
    if (g.ready) rsx_dispatch_seed_transform_program(&g.rsx, words, count);
}
void rsx_live_draw_seed_transform_constants(const u32* words, u32 count)
{
    if (g.ready) rsx_dispatch_seed_transform_constants(&g.rsx, words, count);
}

void rsx_live_draw_set_display_buffer(
    u32 buffer_id, u32 location, u32 offset, u32 pitch, u32 width, u32 height)
{
    if (buffer_id >= 8) return;
    display_buffer_t* display = &g.display_buffers[buffer_id];
    display->location = location;
    display->offset = offset;
    display->pitch = pitch;
    display->width = width;
    display->height = height;
    display->valid = width && height;
    display->resolve_src = 0;
    fprintf(stderr,
            "[live-draw] display buffer %u = loc%u:0x%08X pitch=%u %ux%u\n",
            buffer_id, location, offset, pitch, width, height);
}

/* GOWA_MTRACE=<frame>: every method of that frame and the next (minus the
 * transform program / constant uploads and inline vertex data), plus the
 * presents, in FIFO order -- what happens between two draws. */
/* GOWA_MTRACE_ARM=<hex>: instead, start 60 frames after texture unit 0 is
 * first pointed at that offset (frame numbers drift between runs). */
static int g_ld_mtrace_frame = -2;
static u32 g_ld_mtrace_arm = 0;
static int ld_mtrace_on(void)
{
    static u32 lines = 0;
    int f = g_ld_mtrace_frame;
    if (f < 0 || (int)g_ld_frames < f || (int)g_ld_frames > f + 1) return 0;
    return lines++ < 20000;
}

/* Marks from the FIFO walker (the flip command) inside the [mtrace] window. */
void rsx_live_draw_mtrace_note(const char* what, u32 v)
{
    if (ld_mtrace_on()) fprintf(stderr, "[mtrace] f=%u %s 0x%X\n", g_ld_frames, what, v);
}

void rsx_live_draw_method(u32 method, u32 arg)
{
    /* GOWA (21/10): the per-method diagnostics below cost ~a dozen tests on every FIFO
     * method word (millions a second); none of their switches is set in a normal run. */
    static int s_diag = -1;
    if (s_diag < 0) s_diag = (getenv("GOWA_MTRACE") || getenv("GOWA_MTRACE_ARM") || getenv("GOWA_ARM_TEX") ||
                              getenv("RSX_VERTEX_STATE_TRACE")) ? 1 : 0;
    if (s_diag) {
    if (g_ld_mtrace_frame == -2) {
        const char* e = getenv("GOWA_MTRACE"); const char* a = getenv("GOWA_MTRACE_ARM");
        g_ld_mtrace_frame = e ? atoi(e) : -1;
        if (a) { g_ld_mtrace_arm = (u32)strtoul(a, 0, 16); g_ld_mtrace_frame = -1; }
    }
    if (g_ld_mtrace_arm && g_ld_mtrace_frame < 0 && (method & 0x1FFCu) == 0x1A00u && arg == g_ld_mtrace_arm)
        g_ld_mtrace_frame = (int)g_ld_frames + 60;
    { static int at = -2; static u32 at_off;
      if (at == -2) { const char* e = getenv("GOWA_ARM_TEX"); at = e ? 1 : 0; at_off = e ? (u32)strtoul(e, 0, 16) : 0; }
      if (at && g_ld_arm_frame < 0 && (method & 0x1FFCu) == 0x1A00u && arg == at_off) {
          g_ld_arm_frame = (int)g_ld_frames;
          fprintf(stderr, "[arm] frame %u: texture 0 -> 0x%X\n", g_ld_frames, at_off);
      } }
    { const u32 c = method & 0x1FFCu;
      const int keep = ((method >> 13) & 7u) != 0 || (c >= 0x0180u && c < 0x02C0u) || (c >= 0x0300u && c < 0x03A0u) ||
                       (c >= 0x0A60u && c < 0x0A80u) || c == 0x1A00u || c == 0x1808u || c == 0x1800u ||
                       (c >= 0x1D6Cu && c < 0x1DB8u) || (c >= 0x1830u && c < 0x1840u) || c < 0x0100u;
      if (keep && ld_mtrace_on())
          fprintf(stderr, "[mtrace] f=%u m=0x%04X a=0x%08X\n", g_ld_frames, method & 0xFFFFu, arg); }
    /* Bounded ingress trace for the a010 live-vs-capture comparison.  Keep the
     * raw method (including its FIFO subchannel bits) visible: if an SPU-built
     * command list binds NV4097 on a different subchannel, feeding the raw
     * 0x2xxx-shifted method into the canonical dispatcher silently stores the
     * state in the wrong register bank. */
    static int state_trace = -1;
    static u32 state_trace_lines = 0;
    if (state_trace < 0)
        state_trace = getenv("RSX_VERTEX_STATE_TRACE") ? 1 : 0;
    if (state_trace && state_trace_lines < 512) {
        const u32 canonical = method & 0x1FFCu;
        const int vertex_texture =
            canonical >= 0x0900u && canonical < 0x0980u;
        const int vertex_format =
            canonical >= 0x1740u && canonical < 0x1780u;
        const int frequency_divider = canonical == 0x1FC0u;
        if (vertex_texture || vertex_format || frequency_divider) {
            fprintf(stderr,
                    "[rsx-vstate] frame=%u raw=0x%04X sub=%u canonical=0x%04X "
                    "arg=0x%08X\n",
                    g_ld_frames, method & 0xFFFFu, (method >> 13) & 7u,
                    canonical, arg);
            fflush(stderr);
            state_trace_lines++;
        }
    }

    }
    const int composite = ld_movie_composite_ui_enabled();
    const int serialized = composite || g_ld_debug_layer_enabled;
    if (serialized) {
        for (;;) {
            if (g_ld_movie_mode || g_ld_host_waiting) {
                while (g_ld_host_waiting)
                    SwitchToThread();
                AcquireSRWLockExclusive(&g_ld_access_lock);
                if (!g.ready) {
                    ReleaseSRWLockExclusive(&g_ld_access_lock);
                    return;
                }
                if (g_ld_movie_mode && !composite) {
                    if (g_ld_movie_track_rsx < 0)
                        g_ld_movie_track_rsx =
                            getenv("RSX_MOVIE_TRACK") ? 1 : 0;
                    if (!g_ld_movie_track_rsx) {
                        ReleaseSRWLockExclusive(&g_ld_access_lock);
                        return;
                    }
                }
                rsx_dispatch_method(&g.rsx, method, arg);
                ReleaseSRWLockExclusive(&g_ld_access_lock);
                return;
            }
            InterlockedIncrement(&g_ld_guest_active);
            MemoryBarrier();
            if (!g_ld_movie_mode && !g_ld_host_waiting) {
                if (g.ready)
                    rsx_dispatch_method(&g.rsx, method, arg);
                InterlockedDecrement(&g_ld_guest_active);
                return;
            }
            InterlockedDecrement(&g_ld_guest_active);
        }
    }

    if (!g.ready) return;
    if (g_ld_movie_mode) {
        if (g_ld_movie_track_rsx < 0)
            g_ld_movie_track_rsx = getenv("RSX_MOVIE_TRACK") ? 1 : 0;
        if (!g_ld_movie_track_rsx) return;
    }
    rsx_dispatch_method(&g.rsx, method, arg);
}

/* GOWA (24/10): see rsx_live_draw.h. The same per-method diagnostics switch as
 * rsx_live_draw_method; any of them on, or the serialized/movie paths, and the
 * walker feeds the packet word by word instead. */
int rsx_live_draw_method_run(u32 method, const u8* words, u32 count, int ni)
{
    static int s_diag = -1;
    if (s_diag < 0) s_diag = (getenv("GOWA_MTRACE") || getenv("GOWA_MTRACE_ARM") || getenv("GOWA_ARM_TEX") ||
                              getenv("RSX_VERTEX_STATE_TRACE")) ? 1 : 0;
    if (s_diag || !g.ready || g_ld_movie_mode || g_ld_debug_layer_enabled || ld_movie_composite_ui_enabled())
        return 0;
    for (u32 i = 0; i < count; i++) {
        u32 w;
        memcpy(&w, words + 4u * i, 4);
        rsx_dispatch_method(&g.rsx, ni ? method : method + 4u * i, __builtin_bswap32(w));
    }
    return 1;
}

void rsx_live_draw_set_movie_mode(int on)
{
    static unsigned long long suppressed_at_start = 0;
    const int composite = ld_movie_composite_ui_enabled();
    const int serialized = composite || g_ld_debug_layer_enabled;
    if (on && rsx_live_draw_a010_probe_active()) {
        fprintf(stderr,
                "[a010-probe] END movie-mode live_frame=%u elapsed_frames=%u "
                "surfaces=%u samples=%u touched=0x%016llX\n",
                g_ld_frames, g_ld_frames - g_ld_a010_probe_start_frame,
                g.n_surfaces, g_ld_a010_probe_sample,
                (unsigned long long)g_ld_a010_probe_touched);
        fflush(stderr);
        InterlockedExchange(&g_ld_a010_probe_active, 0);
    }
    if (serialized) {
        InterlockedExchange(&g_ld_host_waiting, 1);
        if (on) InterlockedExchange((volatile LONG*)&g_ld_movie_mode, 1);
        while (InterlockedCompareExchange(&g_ld_guest_active, 0, 0) != 0)
            SwitchToThread();
        AcquireSRWLockExclusive(&g_ld_access_lock);
    }
    if (on) {
        suppressed_at_start = g_ld_stats.packets_movie;
        if (composite) ld_movie_overlay_begin();
        else g_ld_movie_mode = 1;
    } else {
        if (composite) {
            g.movie_overlay_valid = 0;
            /* Do not expose a partially rendered auth/fade surface between
             * the last host movie frame and the next clean guest scene. */
            ld_flush(LD_FLUSH_MOVIE);
            const float black[4] = {0, 0, 0, 1};
            for (u32 i = 0; i < g.n_surfaces; i++) {
                g.list->lpVtbl->ClearRenderTargetView(
                    g.list, rtv_handle(LD_SWAP_BUFFERS + i), black, 0, NULL);
                ld_surface_note_write(i, LD_SURFACE_WRITE_OTHER);
            }
            if (g.n_surfaces) ld_flush(LD_FLUSH_MOVIE);
            ld_movie_reset_rings();
            InterlockedExchange((volatile LONG*)&g_ld_movie_mode, 0);
            fprintf(stderr, "[movie-ui] compositor disarmed after %llu guest overlays\n",
                    (unsigned long long)g.movie_overlay_frames);
            fflush(stderr);
        } else {
            g_ld_movie_mode = 0;
        }
        if (!composite && g_ld_movie_track_rsx > 0) {
            fprintf(stderr,
                    "[live-draw] movie handoff: tracked RSX state, suppressed %llu guest draw packets\n",
                    g_ld_stats.packets_movie - suppressed_at_start);
            fflush(stderr);
        }
#if !defined(YZ_PERF_CLEAN)
        if (g.vertex_diag_dir[0]) {
            InterlockedExchange(&g_ld_diag_post_movie_presents, 0);
            InterlockedExchange(&g_ld_diag_post_movie_pending, 1);
        }
#endif
    }
    ld_vertex_diag_emit(on ? "movie-begin" : "movie-end", 0);
    if (serialized) {
        ReleaseSRWLockExclusive(&g_ld_access_lock);
        InterlockedExchange(&g_ld_host_waiting, 0);
    }
}

u32 rsx_live_draw_get_frames(void) { return g_ld_frames; }

void rsx_live_draw_set_fifo_position(u32 get, u32 put)
{
#if defined(YZ_PERF_CLEAN)
    (void)get;
    (void)put;
#else
    g_ld_fifo_get = get;
    g_ld_fifo_put = put;
#endif
}

void rsx_live_draw_note_inline_transfer(u32 dma, u32 offset, u32 value)
{
#if defined(YZ_PERF_CLEAN)
    (void)dma;
    (void)offset;
    (void)value;
#else
    if (!g.ready || !yz_frontier_trace_is_armed())
        return;
    const u32 location = dma == 0xFEED0000u ? 0u :
                         dma == 0xFEED0001u ? 1u : UINT32_MAX;
    if (location == UINT32_MAX)
        return;
    for (u32 i = 0; i < g.n_surfaces; ++i) {
        surface_t* surface = &g.surfaces[i];
        const u64 begin = surface->offset;
        const u64 end = begin + (u64)surface->w * surface->h * 4u;
        if (surface->location != location || offset < begin || offset >= end)
            continue;
        surface->last_guest_blit_generation = ++g_ld_guest_blit_generation;
        yz_frontier_trace_emit(
            YZ_FT_PARITY_GUEST_BLIT, i, location,
            surface->last_guest_blit_generation, offset, value,
            g_ld_fifo_get, g_ld_fifo_put, surface->last_write_generation);
    }
#endif
}

void* rsx_live_draw_get_present_thread_handle(void)
{
#if defined(YZ_PPU_SAMPLE)
    return (void*)InterlockedCompareExchangePointer(
        (PVOID volatile*)&g_ld_present_thread_handle, NULL, NULL);
#else
    return NULL;
#endif
}

static u32 g_ld_last_frame_draws = 0;
/* Draws in the last COMPLETED frame (title-bar telemetry: distinguishes
 * "presenting fresh content" from "flipping a static image" -- the dead
 * journal-consumer limp state renders ~0 draws/frame while flips tick). */
u32 rsx_live_draw_get_last_draws(void) { return g_ld_last_frame_draws; }

double rsx_live_draw_get_present_fps(void)
{
    const u64 total = g_ld_present_total;
    if (total < 2u || g_ld_qpc_frequency <= 0)
        return 0.0;

    const u64 available =
        total < LD_PRESENT_RING_CAP ? total : LD_PRESENT_RING_CAP;
    const ld_present_sample* newest =
        &g_ld_present_ring[(total - 1u) & (LD_PRESENT_RING_CAP - 1u)];
    if (newest->present_id != total)
        return 0.0;

    const ld_present_sample* oldest = newest;
    u64 intervals = 0;
    for (u64 back = 1u; back < available; ++back) {
        const u64 present_id = total - back;
        const ld_present_sample* candidate =
            &g_ld_present_ring[(present_id - 1u) &
                               (LD_PRESENT_RING_CAP - 1u)];
        if (candidate->present_id != present_id)
            break;
        oldest = candidate;
        intervals = back;
        const double elapsed =
            (double)(newest->qpc - oldest->qpc) /
            (double)g_ld_qpc_frequency;
        /*
         * Keep at least a 15-second view and about 30 intervals when possible;
         * cap at 30 seconds so sub-1-FPS operation still updates usefully.
         */
        if (elapsed >= 30.0 || (elapsed >= 15.0 && intervals >= 30u))
            break;
    }

    const LONGLONG ticks = newest->qpc - oldest->qpc;
    return intervals != 0u && ticks > 0
        ? (double)intervals * (double)g_ld_qpc_frequency / (double)ticks
        : 0.0;
}

void rsx_live_draw_dump_present_samples(void)
{
    ld_present_measure_dump();
}

/* GOWA: scaled blit into the current backbuffer (GOWA_RES_SCALE). A
 * fullscreen triangle samples one texture into the whole backbuffer at any
 * size: the guest-size movie frames into a scaled swap chain, and a
 * presented surface whose size is not the backbuffer's. Own root signature:
 * one SRV table (t0) and a static bilinear clamp sampler (s0). */
static ID3D12RootSignature* s_blit_rs;
static ID3D12PipelineState* s_blit_pso;
static int ld_blit_pipeline(void)
{
    static int failed;
    if (s_blit_pso) return 1;
    if (failed) return 0;
    failed = 1;
    D3D12_DESCRIPTOR_RANGE r = {0};
    r.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    r.NumDescriptors = 1;
    D3D12_ROOT_PARAMETER rp = {0};
    rp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rp.DescriptorTable.NumDescriptorRanges = 1;
    rp.DescriptorTable.pDescriptorRanges = &r;
    rp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC ss = {0};
    ss.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    ss.AddressU = ss.AddressV = ss.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    ss.MaxLOD = D3D12_FLOAT32_MAX;
    ss.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC rsd = {0};
    rsd.NumParameters = 1; rsd.pParameters = &rp;
    rsd.NumStaticSamplers = 1; rsd.pStaticSamplers = &ss;
    ID3DBlob* sig = NULL; ID3DBlob* err = NULL;
    if (FAILED(D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err))) {
        if (err) err->lpVtbl->Release(err);
        return 0;
    }
    HRESULT hr = g.dev->lpVtbl->CreateRootSignature(g.dev, 0, sig->lpVtbl->GetBufferPointer(sig),
                                                    sig->lpVtbl->GetBufferSize(sig),
                                                    &IID_ID3D12RootSignature, (void**)&s_blit_rs);
    sig->lpVtbl->Release(sig);
    if (FAILED(hr)) return 0;
    static const char src[] =
        "struct V { float4 p : SV_Position; float2 uv : TEXCOORD0; };\n"
        "V vs(uint id : SV_VertexID) {\n"
        "    V o; o.uv = float2((id << 1) & 2, id & 2);\n"
        "    o.p = float4(o.uv * float2(2, -2) + float2(-1, 1), 0, 1); return o; }\n"
        "Texture2D t0 : register(t0); SamplerState s0 : register(s0);\n"
        "float4 ps(V i) : SV_Target { return t0.SampleLevel(s0, i.uv, 0); }\n";
    ID3DBlob *vsb = NULL, *psb = NULL;
    if (FAILED(D3DCompile(src, sizeof src - 1, "ld_blit", NULL, NULL, "vs", "vs_5_0", 0, 0, &vsb, &err))) {
        if (err) { fprintf(stderr, "[blit] vs: %s\n", (const char*)err->lpVtbl->GetBufferPointer(err)); err->lpVtbl->Release(err); }
        return 0;
    }
    if (FAILED(D3DCompile(src, sizeof src - 1, "ld_blit", NULL, NULL, "ps", "ps_5_0", 0, 0, &psb, &err))) {
        if (err) { fprintf(stderr, "[blit] ps: %s\n", (const char*)err->lpVtbl->GetBufferPointer(err)); err->lpVtbl->Release(err); }
        vsb->lpVtbl->Release(vsb);
        return 0;
    }
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pd = {0};
    pd.pRootSignature = s_blit_rs;
    pd.VS.pShaderBytecode = vsb->lpVtbl->GetBufferPointer(vsb);
    pd.VS.BytecodeLength = vsb->lpVtbl->GetBufferSize(vsb);
    pd.PS.pShaderBytecode = psb->lpVtbl->GetBufferPointer(psb);
    pd.PS.BytecodeLength = psb->lpVtbl->GetBufferSize(psb);
    pd.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pd.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pd.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pd.RasterizerState.DepthClipEnable = TRUE;
    pd.SampleMask = 0xFFFFFFFFu;
    pd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pd.NumRenderTargets = 1;
    pd.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pd.SampleDesc.Count = 1;
    hr = g.dev->lpVtbl->CreateGraphicsPipelineState(g.dev, &pd, &IID_ID3D12PipelineState, (void**)&s_blit_pso);
    vsb->lpVtbl->Release(vsb); psb->lpVtbl->Release(psb);
    if (FAILED(hr)) { fprintf(stderr, "[blit] PSO hr=0x%08lX\n", (unsigned long)hr); return 0; }
    failed = 0;
    return 1;
}
/* Draws `srv_slot` (already a pixel-shader resource) over the whole
 * backbuffer `bbi`, which must be in the RENDER_TARGET state. */
/* GOWA (28/10): tela preta com som (reports 90, 93, 103, 165, 176; não reproduz aqui). A cada 2 s nos 3 primeiros
 * minutos e depois a cada 30 s, a surface que o jogo mandou mostrar e o backbuffer já pronto para o Present são
 * copiados para a CPU (sem esperar a GPU), e "[tela]" registra o brilho médio (0-255) e a parte não preta de cada
 * um: jogo escuro = o desenho do jogo; jogo claro e final escuro = a cópia para a janela; os dois claros = depois
 * do Present (driver, HDR, monitor). Uma vez no começo, "[tela-saida]" diz o formato do swap chain, o monitor e o
 * HDR. GOWA_TELA_DIAG=0 desliga. */
int rsx_live_draw_framegen_on(void);
/* GOWA (30/10, reports 455-460): o relatório anexa só o fim do jogo.log (~8 KB); depois dos 3 primeiros minutos o
 * "[tela]" sai a cada 30 s e o "[tgt]" automático enche o fim, então nenhum aparecia. As linhas do diagnóstico
 * ficam guardadas (a saída, os 2 primeiros e os 6 últimos "[tela]", os "[tela-alvos]" e os "[modulos]") e
 * rsx_live_draw_resumo_tela() reimprime tudo ao fechar o jogo, como "[resumo-tela]". */
#define LD_RES_LIN 176
static char s_res_saida[LD_RES_LIN], s_res_ini[2][LD_RES_LIN], s_res_ult[6][LD_RES_LIN], s_res_alvos[16][LD_RES_LIN], s_res_mods[12][LD_RES_LIN];
static unsigned s_res_n_tela, s_res_n_alvos, s_res_n_mods;
static void ld_res_copia(char* d, const char* s)
{
    size_t n = strlen(s);
    while (n && (s[n - 1] == '\n' || s[n - 1] == '\r')) n--;
    if (n >= LD_RES_LIN) n = LD_RES_LIN - 1;
    memcpy(d, s, n); d[n] = 0;
}
/* cat: 0 saída, 1 tela, 2 alvos (novo=1 recomeça a lista), 3 módulos (novo=1 recomeça) */
static void ld_res_linha(int cat, int novo, const char* fmt, ...)
{
    char b[512];
    va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    fputs(b, stderr);
    switch (cat) {
    case 0: ld_res_copia(s_res_saida, b); break;
    case 1:
        if (s_res_n_tela < 2) ld_res_copia(s_res_ini[s_res_n_tela], b);
        else ld_res_copia(s_res_ult[(s_res_n_tela - 2) % 6], b);
        s_res_n_tela++;
        break;
    case 2: if (novo) s_res_n_alvos = 0; if (s_res_n_alvos < 16) ld_res_copia(s_res_alvos[s_res_n_alvos++], b); break;
    default: if (novo) s_res_n_mods = 0; if (s_res_n_mods < 12) ld_res_copia(s_res_mods[s_res_n_mods++], b); break;
    }
}
void rsx_live_draw_resumo_tela(void)
{
    fprintf(stderr, "[resumo-tela] diagnóstico da tela (%u medidas):\n", s_res_n_tela);
    if (s_res_saida[0]) fprintf(stderr, "[resumo-tela] %s\n", s_res_saida);
    for (unsigned i = 0; i < 2 && i < s_res_n_tela; i++) fprintf(stderr, "[resumo-tela] %s\n", s_res_ini[i]);
    if (s_res_n_tela > 8) fprintf(stderr, "[resumo-tela] ... (%u medidas no meio)\n", s_res_n_tela - 8);
    if (s_res_n_tela > 2) {
        const unsigned u = s_res_n_tela - 2 < 6 ? s_res_n_tela - 2 : 6;
        for (unsigned k = s_res_n_tela - 2 - u; k < s_res_n_tela - 2; k++) fprintf(stderr, "[resumo-tela] %s\n", s_res_ult[k % 6]);
    }
    for (unsigned i = 0; i < s_res_n_alvos; i++) fprintf(stderr, "[resumo-tela] %s\n", s_res_alvos[i]);
    for (unsigned i = 0; i < s_res_n_mods; i++) fprintf(stderr, "[resumo-tela] %s\n", s_res_mods[i]);
}
typedef struct { ID3D12Resource* rb; UINT64 size, offset; u64 fence; u32 frame, w, h, pitch; DXGI_FORMAT fmt; int busy; } ld_tela_rb;
static ld_tela_rb s_tela[2];   /* 0 = a surface do jogo, 1 = o backbuffer */
static void ld_tela_copia(ld_tela_rb* t, ID3D12Resource* res, D3D12_RESOURCE_STATES state)
{
    t->busy = 0;
    if (!res) return;
    D3D12_RESOURCE_DESC d; res->lpVtbl->GetDesc(res, &d);
    t->w = (u32)d.Width; t->h = d.Height; t->fmt = d.Format; t->frame = g_ld_frames;
    if (d.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || d.SampleDesc.Count != 1) { t->pitch = 0; return; }
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp; UINT rows; UINT64 rowsize, total = 0;
    g.dev->lpVtbl->GetCopyableFootprints(g.dev, &d, 0, 1, 0, &fp, &rows, &rowsize, &total);
    if (!total) return;
    if (t->rb && t->size < total) { t->rb->lpVtbl->Release(t->rb); t->rb = NULL; }
    if (!t->rb) {
        D3D12_HEAP_PROPERTIES hp = {0}; hp.Type = D3D12_HEAP_TYPE_READBACK;
        D3D12_RESOURCE_DESC bd = {0};
        bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = total;
        bd.Height = 1; bd.DepthOrArraySize = 1; bd.MipLevels = 1; bd.SampleDesc.Count = 1;
        bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        if (FAILED(g.dev->lpVtbl->CreateCommittedResource(g.dev, &hp, D3D12_HEAP_FLAG_NONE, &bd,
                D3D12_RESOURCE_STATE_COPY_DEST, NULL, &IID_ID3D12Resource, (void**)&t->rb))) { t->rb = NULL; return; }
        t->size = total;
    }
    D3D12_RESOURCE_BARRIER b = {0};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; b.Transition.pResource = res;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = state; b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    if (state != D3D12_RESOURCE_STATE_COPY_SOURCE) g.list->lpVtbl->ResourceBarrier(g.list, 1, &b);
    D3D12_TEXTURE_COPY_LOCATION src = {0}, dst = {0};
    src.pResource = res; src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.pResource = t->rb; dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; dst.PlacedFootprint = fp;
    g.list->lpVtbl->CopyTextureRegion(g.list, &dst, 0, 0, 0, &src, NULL);
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE; b.Transition.StateAfter = state;
    if (state != D3D12_RESOURCE_STATE_COPY_SOURCE) g.list->lpVtbl->ResourceBarrier(g.list, 1, &b);
    t->offset = fp.Offset; t->pitch = fp.Footprint.RowPitch;
    t->fence = g.fence_value + 1;   /* the next signal covers this list */
    t->busy = 1;
}
static void ld_tela_medir(ld_tela_rb* t, char* out, size_t n)
{
    const int bgra = t->fmt == DXGI_FORMAT_B8G8R8A8_UNORM || t->fmt == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    const int rgba = t->fmt == DXGI_FORMAT_R8G8B8A8_UNORM || t->fmt == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    const int half = t->fmt == DXGI_FORMAT_R16G16B16A16_FLOAT;
    if (!t->busy || !t->pitch || !(bgra || rgba || half)) {
        snprintf(out, n, "%ux%u fmt %d (não medido)", t->w, t->h, (int)t->fmt); t->busy = 0; return;
    }
    u8* m = NULL; D3D12_RANGE r0 = {0, (SIZE_T)t->size};
    if (FAILED(t->rb->lpVtbl->Map(t->rb, 0, &r0, (void**)&m)) || !m) { snprintf(out, n, "map falhou"); t->busy = 0; return; }
    u64 sum = 0, claros = 0, total = 0;
    const u32 passo = t->w >= 1920 ? 8 : 4;
    for (u32 y = 0; y < t->h; y += passo)
        for (u32 x = 0; x < t->w; x += passo) {
            u32 l;
            if (half) {   /* 30/10: superfícies em float16 no diagnóstico [tela-alvos] */
                const u16* q = (const u16*)(m + t->offset + (size_t)y * t->pitch + (size_t)x * 8u);
                float c[3];
                for (int k = 0; k < 3; k++) {
                    const u32 hv = q[k], e = (hv >> 10) & 31u, f = hv & 1023u;
                    float v = e == 0 ? (float)f / 16777216.0f : e == 31 ? 65504.0f : ldexpf(1.0f + (float)f / 1024.0f, (int)e - 15);
                    if (hv & 0x8000u) v = 0.0f;
                    c[k] = v > 1.0f ? 1.0f : v;
                }
                l = (u32)((c[0] * 0.30f + c[1] * 0.59f + c[2] * 0.11f) * 255.0f);
            } else {
                const u8* p = m + t->offset + (size_t)y * t->pitch + (size_t)x * 4u;
                const u32 r = bgra ? p[2] : p[0], gg = p[1], bl = bgra ? p[0] : p[2];
                l = (r * 30 + gg * 59 + bl * 11) / 100;
            }
            sum += l; claros += l > 8; total++;
        }
    D3D12_RANGE w0 = {0, 0}; t->rb->lpVtbl->Unmap(t->rb, 0, &w0);
    snprintf(out, n, "%ux%u brilho %.1f, %.0f%% não preto", t->w, t->h,
             total ? (double)sum / (double)total : 0.0, total ? 100.0 * (double)claros / (double)total : 0.0);
    t->busy = 0;
}
static void ld_tela_saida(void)
{
    DXGI_SWAP_CHAIN_DESC1 sd = {0}; g.swap->lpVtbl->GetDesc1(g.swap, &sd);
    char mon[200] = "monitor ?";
    IDXGIOutput* o = NULL;
    if (SUCCEEDED(g.swap->lpVtbl->GetContainingOutput(g.swap, &o)) && o) {
        IDXGIOutput6* o6 = NULL;
        if (SUCCEEDED(o->lpVtbl->QueryInterface(o, &IID_IDXGIOutput6, (void**)&o6)) && o6) {
            DXGI_OUTPUT_DESC1 od; o6->lpVtbl->GetDesc1(o6, &od);
            snprintf(mon, sizeof mon, "monitor %ldx%ld, %u bits, espaço de cor %d (%s), luminância %.0f nits",
                     od.DesktopCoordinates.right - od.DesktopCoordinates.left,
                     od.DesktopCoordinates.bottom - od.DesktopCoordinates.top, od.BitsPerColor, (int)od.ColorSpace,
                     od.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 ? "HDR ligado" : "SDR",
                     od.MaxLuminance);
            o6->lpVtbl->Release(o6);
        }
        o->lpVtbl->Release(o);
    }
    ld_res_linha(0, 0, "[tela-saida] swap chain %ux%u fmt %d, %u buffers, flags 0x%X, vsync %d, tearing %d; %s\n",
            sd.Width, sd.Height, (int)sd.Format, sd.BufferCount, sd.Flags, g_ld_present_sync, g_ld_present_tearing, mon);
}
/* GOWA (30/10, reports 397, 399, 409): a logo aparece, mas do menu em diante a surface mostrada fica preta (brilho
 * 0,0) com o jogo desenhando normalmente (30-48 fps, milhões de pacotes), em GPUs da NVIDIA e da AMD; aqui não
 * reproduz nem com o cache frio e os shaders do pacote. Depois de 3 medidas seguidas de "[tela]" com o jogo todo
 * preto (passados 30 s), copia uma vez cada surface viva e escreve "[tela-alvos]" com o endereço, tamanho, formato
 * e brilho de cada uma: mostra em qual passo do desenho a imagem se perde. No máximo 2 vezes por execução. */
static ld_tela_rb s_alvos[MAX_SURFACES];
static u32 s_alvos_loc[MAX_SURFACES], s_alvos_off[MAX_SURFACES];
static int s_alvos_pend = 0, s_alvos_vezes = 0;
static void ld_tela_alvos_copiar(ID3D12Resource* mostrada)
{
    UINT64 total = 0;
    for (u32 i = 0; i < MAX_SURFACES; i++) {
        surface_t* s = &g.surfaces[i];
        s_alvos[i].busy = 0;
        if (!s->tex || s->tex == mostrada) continue;   /* a mostrada pode estar em outro estado; já sai no [tela] */
        const UINT64 bytes = (UINT64)s->w * s->h * 8u;
        if (total + bytes > 384ull * 1024 * 1024) continue;   /* teto da memória de leitura */
        total += bytes;
        s_alvos_loc[i] = s->location; s_alvos_off[i] = s->offset;
        ld_tela_copia(&s_alvos[i], s->tex, D3D12_RESOURCE_STATE_RENDER_TARGET);
    }
    s_alvos_pend = 1;
}
static void ld_tela_alvos_medir(void)
{
    const u64 done = g.fence->lpVtbl->GetCompletedValue(g.fence);
    for (u32 i = 0; i < MAX_SURFACES; i++)
        if (s_alvos[i].busy && s_alvos[i].fence > done) return;
    ld_res_linha(2, 1, "[tela-alvos] quadro %u: surfaces vivas (a mostrada é a de \"[tela]\")\n", g_ld_frames);
    for (u32 i = 0; i < MAX_SURFACES; i++) {
        if (!s_alvos[i].busy) continue;
        char a[96];
        ld_tela_medir(&s_alvos[i], a, sizeof a);
        ld_res_linha(2, 0, "[tela-alvos]   %2u loc %u off 0x%08X fmt %d: %s\n", i, s_alvos_loc[i], s_alvos_off[i],
                (int)s_alvos[i].fmt, a);
        if (s_alvos[i].rb) { s_alvos[i].rb->lpVtbl->Release(s_alvos[i].rb); s_alvos[i].rb = NULL; s_alvos[i].size = 0; }
    }
    fflush(stderr);
    s_alvos_pend = 0;
}
/* GOWA (30/10): as DLLs carregadas no processo que não são do Windows (System32/SysWOW64/WinSxS): é onde aparecem
 * as sobreposições e filtros que se injetam no D3D12 (RivaTuner, Discord, Steam, ReShade, filtros da NVIDIA/AMD) */
static void ld_log_modulos(const char* quando)
{
    HMODULE mods[1024]; DWORD nb = 0;
    if (!K32EnumProcessModules(GetCurrentProcess(), mods, sizeof mods, &nb)) return;
    char win[MAX_PATH] = {0}; GetWindowsDirectoryA(win, sizeof win);
    const size_t lw = strlen(win);
    int n = 0;
    for (DWORD k = 0; k < nb / sizeof(HMODULE); k++) {
        char cam[MAX_PATH];
        if (!K32GetModuleFileNameExA(GetCurrentProcess(), mods[k], cam, sizeof cam)) continue;
        if (lw && !_strnicmp(cam, win, lw)) continue;   /* do Windows */
        ld_res_linha(3, n == 0, "[modulos] %s: %s\n", quando, cam);
        n++;
    }
    ld_res_linha(3, n == 0, "[modulos] %s: %d DLLs fora do Windows\n", quando, n);
}
static void ld_tela_diag(ID3D12Resource* src, D3D12_RESOURCE_STATES src_state, ID3D12Resource* bb, D3D12_RESOURCE_STATES bb_state)
{
    static int on = -1; static ULONGLONG t0, next; static int pretos = 0;
    if (on < 0) {
        const char* e = getenv("GOWA_TELA_DIAG"); on = !(e && *e == '0');
        t0 = GetTickCount64(); next = t0 + 2000;
        if (on) ld_tela_saida();
    }
    if (!on) return;
    if (s_alvos_pend) { ld_tela_alvos_medir(); return; }
    if (s_tela[0].busy || s_tela[1].busy) {
        const u64 done = g.fence->lpVtbl->GetCompletedValue(g.fence);
        if ((s_tela[0].busy && s_tela[0].fence > done) || (s_tela[1].busy && s_tela[1].fence > done)) return;
        char a[96], b[96];
        const u32 frame = s_tela[0].frame;
        ld_tela_medir(&s_tela[0], a, sizeof a); ld_tela_medir(&s_tela[1], b, sizeof b);
        HWND w = NULL; g.swap->lpVtbl->GetHwnd(g.swap, &w);
        ld_res_linha(1, 0, "[tela] quadro %u, %.0f s: jogo %s | final %s | janela %ux%u%s%s\n", frame,
                (GetTickCount64() - t0) / 1000.0, a, b, g.width, g.height, w && IsIconic(w) ? " (minimizada)" : "",
                rsx_live_draw_framegen_on() ? " (geração de quadros)" : "");
        static int forca = -1;   /* GOWA_TELA_ALVOS=1: registra as surfaces mesmo sem tela preta (teste) */
        if (forca < 0) { const char* e = getenv("GOWA_TELA_ALVOS"); forca = e && *e == '1'; }
        pretos = (strstr(a, "brilho 0.0, 0% não preto") || forca) ? pretos + 1 : 0;
        if (pretos >= 3 && GetTickCount64() - t0 > 30000 && s_alvos_vezes < 2) {
            s_alvos_vezes++; pretos = -60;   /* a 2ª vez, se ainda preto, uns 2 min depois */
            ld_tela_alvos_copiar(src);
            g_ld_tgt_auto_left = 1200; g_ld_tgt_auto_fim = g_ld_frames + 2u;
            fprintf(stderr, "[tgt] registro automático dos desenhos por 2 quadros (tela preta)\n");
            ld_log_modulos("tela preta");
        }
        { static int mods_ok; if (!mods_ok && GetTickCount64() - t0 > 10000) { mods_ok = 1; ld_log_modulos("10 s"); } }
        return;
    }
    const ULONGLONG now = GetTickCount64();
    if (now < next) return;
    next = now + (now - t0 < 180000 ? 2000 : 30000);
    ld_tela_copia(&s_tela[0], src, src_state);
    ld_tela_copia(&s_tela[1], bb, bb_state);
}

static int ld_present_blit(u32 srv_slot, u32 bbi)
{
    if (!ld_blit_pipeline()) return 0;
    u32 slots[SRV_TABLE_SIZE];
    for (u32 i = 0; i < SRV_TABLE_SIZE; i++) slots[i] = SRV_WHITE;
    slots[0] = srv_slot;
    const D3D12_GPU_DESCRIPTOR_HANDLE table = srv_table(slots);
    if (!table.ptr) return 0;
    D3D12_RESOURCE_DESC bd;
    g.backbuf[bbi]->lpVtbl->GetDesc(g.backbuf[bbi], &bd);
    const D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtv_handle(bbi);
    g.list->lpVtbl->OMSetRenderTargets(g.list, 1, &rtv, FALSE, NULL);
    D3D12_VIEWPORT vp = {0, 0, (float)bd.Width, (float)bd.Height, 0.0f, 1.0f};
    D3D12_RECT sc = {0, 0, (LONG)bd.Width, (LONG)bd.Height};
    g.list->lpVtbl->RSSetViewports(g.list, 1, &vp);
    g.list->lpVtbl->RSSetScissorRects(g.list, 1, &sc);
    ID3D12DescriptorHeap* heaps[] = {g.srv_heap, g.smp_heap};
    g.list->lpVtbl->SetDescriptorHeaps(g.list, 2, heaps); ld_state_dirty();
    g.list->lpVtbl->SetPipelineState(g.list, s_blit_pso);
    g.list->lpVtbl->SetGraphicsRootSignature(g.list, s_blit_rs);
    g.list->lpVtbl->SetGraphicsRootDescriptorTable(g.list, 0, table);
    g.list->lpVtbl->IASetPrimitiveTopology(g.list, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g.list->lpVtbl->DrawInstanced(g.list, 3, 1, 0, 0);
    return 1;
}
/* Guest-size texture the movie frames go through when the swap chain is
 * scaled (GOWA_RES_SCALE > 1). */
static ID3D12Resource* s_movie_tex;
static u32 s_movie_tex_w, s_movie_tex_h;

/* Present a host-decoded RGBA8 frame to the window: copy it straight into the
 * swap-chain backbuffer (both R8G8B8A8_UNORM at the swap size) and Present.
 * The frame is clamped to the backbuffer size. Call from a single thread with
 * movie mode on (so guest draws don't touch g.list). */
void rsx_live_draw_present_rgba(const uint8_t* rgba, u32 w, u32 h)
{
    const int composite = ld_movie_composite_ui_enabled();
    const int serialized = composite || g_ld_debug_layer_enabled;
    if (serialized) {
        InterlockedExchange(&g_ld_host_waiting, 1);
        AcquireSRWLockExclusive(&g_ld_access_lock);
    }
    if (!g.ready || !rgba) {
        if (serialized) {
            ReleaseSRWLockExclusive(&g_ld_access_lock);
            InterlockedExchange(&g_ld_host_waiting, 0);
        }
        return;
    }
    if (composite && !ld_movie_overlay_ensure()) {
        ReleaseSRWLockExclusive(&g_ld_access_lock);
        InterlockedExchange(&g_ld_host_waiting, 0);
        return;
    }
    if (w > g.width)  w = g.width;
    if (h > g.height) h = g.height;
    const u32 pitch = (w * 4 + 255) & ~255u;          /* D3D12 copy pitch align */
    if ((UINT64)pitch * h > UPLOAD_SIZE) {
        if (serialized) {
            ReleaseSRWLockExclusive(&g_ld_access_lock);
            InterlockedExchange(&g_ld_host_waiting, 0);
        }
        return;
    }

    u8* host_upload = composite ? g.movie_upload_mapped : g.upload_mapped;
    for (u32 y = 0; y < h; y++) {
        u8* dstrow = host_upload + (size_t)y * pitch;
        memcpy(dstrow, rgba + (size_t)y * w * 4, (size_t)w * 4);
        if (composite && g.movie_overlay_valid && g.movie_overlay_rgba &&
            g.movie_overlay_mask) {
            const u8* ov = g.movie_overlay_rgba + (size_t)y * g.width * 4;
            const u8* mask =
                g.movie_overlay_mask + (size_t)y * g.width;
            for (u32 x = 0; x < w; x++, ov += 4) {
                const int coverage = mask[x];
                if (!coverage) continue;
                u8* out = dstrow + (size_t)x * 4;
                if (coverage == 1) {
                    out[0] = (u8)((out[0] * 64 + 127) / 255);
                    out[1] = (u8)((out[1] * 64 + 127) / 255);
                    out[2] = (u8)((out[2] * 64 + 127) / 255);
                    continue;
                }
                const int white = ov[0] > ov[1]
                    ? (ov[0] > ov[2] ? ov[0] : ov[2])
                    : (ov[1] > ov[2] ? ov[1] : ov[2]);
                for (int c = 0; c < 3; c++) {
                    const int v =
                        (white * coverage +
                         out[c] * (255 - coverage) + 127) / 255;
                    out[c] = (u8)v;
                }
                out[3] = 255;
            }
        }
    }

    const u32 bbi = g.swap->lpVtbl->GetCurrentBackBufferIndex(g.swap);
    ID3D12Resource* bb = g.backbuf[bbi];

    D3D12_TEXTURE_COPY_LOCATION dst = {0}, src = {0};
    src.pResource = composite ? g.movie_upload : g.upload;
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint.Offset = 0;
    src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    src.PlacedFootprint.Footprint.Width = w;
    src.PlacedFootprint.Footprint.Height = h;
    src.PlacedFootprint.Footprint.Depth = 1;
    src.PlacedFootprint.Footprint.RowPitch = pitch;
    D3D12_RESOURCE_BARRIER b = {0};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    int blitted = 0;
    if (g.res_scale != 1) {
        /* scaled swap chain: through a guest-size texture and the blit */
        if (s_movie_tex && (s_movie_tex_w != w || s_movie_tex_h != h)) {
            retire_texture(s_movie_tex); s_movie_tex = NULL;
        }
        if (!s_movie_tex) {
            D3D12_HEAP_PROPERTIES hp = {0}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
            D3D12_RESOURCE_DESC rd = {0};
            rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            rd.Width = w; rd.Height = h; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
            rd.Format = DXGI_FORMAT_R8G8B8A8_UNORM; rd.SampleDesc.Count = 1;
            if (SUCCEEDED(g.dev->lpVtbl->CreateCommittedResource(g.dev, &hp, D3D12_HEAP_FLAG_NONE, &rd,
                    D3D12_RESOURCE_STATE_COPY_DEST, NULL, &IID_ID3D12Resource, (void**)&s_movie_tex))) {
                s_movie_tex_w = w; s_movie_tex_h = h;
                srv_write(SRV_BLIT_SRC, s_movie_tex);
            } else s_movie_tex = NULL;
        } else {
            b.Transition.pResource = s_movie_tex;
            b.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            g.list->lpVtbl->ResourceBarrier(g.list, 1, &b);
        }
        if (s_movie_tex) {
            dst.pResource = s_movie_tex; dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            g.list->lpVtbl->CopyTextureRegion(g.list, &dst, 0, 0, 0, &src, NULL);
            D3D12_RESOURCE_BARRIER b2[2] = {0};
            for (int q = 0; q < 2; q++) {
                b2[q].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                b2[q].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            }
            b2[0].Transition.pResource = s_movie_tex;
            b2[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            b2[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            b2[1].Transition.pResource = bb;
            b2[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            b2[1].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            g.list->lpVtbl->ResourceBarrier(g.list, 2, b2);
            blitted = ld_present_blit(SRV_BLIT_SRC, bbi);
            b.Transition.pResource = bb;
            b.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            b.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            g.list->lpVtbl->ResourceBarrier(g.list, 1, &b);
        }
    }
    if (!blitted && !(g.res_scale != 1 && s_movie_tex)) {
    b.Transition.pResource = bb;
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    g.list->lpVtbl->ResourceBarrier(g.list, 1, &b);

    dst.pResource = bb; dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; dst.SubresourceIndex = 0;
    g.list->lpVtbl->CopyTextureRegion(g.list, &dst, 0, 0, 0, &src, NULL);

    b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    g.list->lpVtbl->ResourceBarrier(g.list, 1, &b);
    }

    ld_flush(LD_FLUSH_MOVIE_PRESENT);
    {
        const HRESULT present_hr = ld_swap_present();
        if (SUCCEEDED(present_hr))
            ld_present_measure_record(g_ld_frames);
        else {
            fprintf(stderr,
                    "[d3d-fail] movie Present hr=0x%08lX frame=%u\n",
                    (unsigned long)present_hr, g_ld_frames);
            ld_dump_dred("MoviePresent", present_hr);
            g.ready = 0;
        }
    }
    if (serialized) {
        ReleaseSRWLockExclusive(&g_ld_access_lock);
        InterlockedExchange(&g_ld_host_waiting, 0);
    }
}

/* Env-gated (RSX_DUMP) framebuffer dump: read the current color surface back
 * and write a binary PPM. Self-contained -- creates + releases its own readback
 * buffer, so no init/struct changes. Uses g.list which ld_flush leaves open. */
/* Readback texel -> 8-bit RGB.  This title renders HDR into FP16 targets, so a
 * dump has to tone it down rather than reinterpret the bytes.  Reinhard keeps
 * the whole range visible without pretending to match the guest tonemapper. */
static void ld_dump_texel_rgb(const u8* p, int fp16, u8* out)
{
    if (!fp16) { out[0] = p[0]; out[1] = p[1]; out[2] = p[2]; return; }
    for (int c = 0; c < 3; c++) {
        const u16 h = (u16)(p[c * 2] | (p[c * 2 + 1] << 8));
        const int ex = (h >> 10) & 0x1F, ma = h & 0x3FF;
        float f;
        if (ex == 0)       f = (float)ma * (1.0f / 16384.0f) * (1.0f / 64.0f);
        else if (ex == 31) f = 65504.0f;
        else { f = 1.0f + (float)ma / 1024.0f;
               int k = ex - 15;
               while (k > 0) { f *= 2.0f; k--; }
               while (k < 0) { f *= 0.5f; k++; } }
        if (h & 0x8000) f = 0.0f;
        f = f / (1.0f + f);                          /* Reinhard */
        out[c] = (u8)(f * 255.0f + 0.5f);
    }
}

/* --record: when set, ld_dump_surface_ppm appends to this ffmpeg pipe instead
 * of opening `path`, and skips its per-dump diagnostics (every frame goes
 * through here while recording). */
static FILE* g_ld_record_to;

static u64 ld_dump_surface_ppm(const char* path, const surface_t* surface)
{
    ID3D12Resource* rt = surface ? surface->tex : NULL;
    if (!rt) return UINT64_MAX;
    const u32 width = LD_PX(surface->w);    /* the resource's own size (GOWA_RES_SCALE) */
    const u32 height = LD_PX(surface->h);
    if (!width || !height) return UINT64_MAX;
    const int fp16 = surface->fmt == DXGI_FORMAT_R16G16B16A16_FLOAT;
    const u32 texel = fp16 ? 8u : 4u;
    const u32 pitch = (width * texel + 255) & ~255u;             /* 256-align */
    const UINT64 rb_size = (UINT64)pitch * height;

    D3D12_HEAP_PROPERTIES hp = {0}; hp.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC bd = {0};
    bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = rb_size;
    bd.Height = 1; bd.DepthOrArraySize = 1; bd.MipLevels = 1;
    bd.Format = DXGI_FORMAT_UNKNOWN; bd.SampleDesc.Count = 1;
    bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ID3D12Resource* rb = NULL;
    if (FAILED(g.dev->lpVtbl->CreateCommittedResource(g.dev, &hp, D3D12_HEAP_FLAG_NONE, &bd,
            D3D12_RESOURCE_STATE_COPY_DEST, NULL, &IID_ID3D12Resource, (void**)&rb)))
        return UINT64_MAX;

    D3D12_RESOURCE_BARRIER b = {0};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = rt;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    g.list->lpVtbl->ResourceBarrier(g.list, 1, &b);

    D3D12_TEXTURE_COPY_LOCATION src = {0}, dst = {0};
    src.pResource = rt; src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; src.SubresourceIndex = 0;
    dst.pResource = rb; dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst.PlacedFootprint.Footprint.Format = surface->fmt;
    dst.PlacedFootprint.Footprint.Width = width;
    dst.PlacedFootprint.Footprint.Height = height;
    dst.PlacedFootprint.Footprint.Depth = 1;
    dst.PlacedFootprint.Footprint.RowPitch = pitch;
    g.list->lpVtbl->CopyTextureRegion(g.list, &dst, 0, 0, 0, &src, NULL);

    b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    g.list->lpVtbl->ResourceBarrier(g.list, 1, &b);

    ld_flush(LD_FLUSH_READBACK);                   /* copy completes on the GPU */

    u64 nonblack = UINT64_MAX;
    u64 fingerprint = 1469598103934665603ull;
    u8* px = NULL; D3D12_RANGE rr = {0, (SIZE_T)rb_size};
    if (SUCCEEDED(rb->lpVtbl->Map(rb, 0, &rr, (void**)&px))) {
        nonblack = 0;
        if (!g_ld_record_to) {   /* raw texels, to tell an FP16 target from 8-bit data read as one */
            const u8* r0 = px + (SIZE_T)(height / 4) * pitch + (width / 4) * texel;
            fprintf(stderr, "[dump-raw] %s fmt=%d texel=%u bytes:", path, (int)surface->fmt, texel);
            for (u32 bi = 0; bi < 16; bi++) fprintf(stderr, " %02X", r0[bi]);
            fprintf(stderr, "\n");
        }
        /* GOWA: an R32_FLOAT target as grey, min..max of the image */
        if (surface->fmt == DXGI_FORMAT_R32_FLOAT && !g_ld_record_to) {
            float mn = 3.4e38f, mx = -3.4e38f;
            for (u32 y = 0; y < height; y++)
                for (u32 x = 0; x < width; x++) {
                    float v; memcpy(&v, px + (SIZE_T)y * pitch + x * 4u, 4);
                    if (v == v) { if (v < mn) mn = v; if (v > mx) mx = v; }
                }
            fprintf(stderr, "[dump-r32] %s min=%g max=%g\n", path, mn, mx);
            FILE* fr = fopen(path, "wb");
            if (fr) {
                fprintf(fr, "P6\n%u %u\n255\n", width, height);
                const float k = mx > mn ? 255.0f / (mx - mn) : 0.0f;
                for (u32 y = 0; y < height; y++)
                    for (u32 x = 0; x < width; x++) {
                        float v; memcpy(&v, px + (SIZE_T)y * pitch + x * 4u, 4);
                        const u8 gv = (u8)((v == v ? v - mn : 0.0f) * k);
                        const u8 rgb[3] = { gv, gv, gv };
                        fwrite(rgb, 1, 3, fr);
                    }
                fclose(fr);
            }
            D3D12_RANGE wr0 = {0, 0};
            rb->lpVtbl->Unmap(rb, 0, &wr0);
            retire_texture(rb);
            return 1;
        }
        FILE* f = g_ld_record_to ? g_ld_record_to : fopen(path, "wb");
        if (f) {
            fprintf(f, "P6\n%u %u\n255\n", width, height);
            for (u32 y = 0; y < height; y++) {
                const u8* row = px + (SIZE_T)y * pitch;
                for (u32 x = 0; x < width; x++) {
                    const u8* pixel = row + x * texel;
                    u8 rgb[3]; ld_dump_texel_rgb(pixel, fp16, rgb);
                    if (rgb[0] || rgb[1] || rgb[2])
                        nonblack++;
                    fingerprint = fnv1a(rgb, 3, fingerprint);
                    fwrite(rgb, 1, 3, f);  /* RGBA->RGB */
                }
            }
            if (f != g_ld_record_to) {
                fclose(f);
                fprintf(stderr, "[live-draw] wrote %s\n", path);
            }
            /* GOWA LD_DUMP_ALPHA=1: the alpha channel too, as <path>.alpha.pgm
             * (8-bit targets only) -- passes that keep a blend factor there. */
            { static int da = -1; if (da < 0) da = getenv("LD_DUMP_ALPHA") ? 1 : 0;
              if (da && !fp16 && f != g_ld_record_to) {
                  char ap[MAX_PATH * 2 + 16]; snprintf(ap, sizeof ap, "%s.alpha.pgm", path);
                  FILE* fa = fopen(ap, "wb");
                  if (fa) {
                      fprintf(fa, "P5\n%u %u\n255\n", width, height);
                      for (u32 y = 0; y < height; y++) {
                          const u8* row = px + (SIZE_T)y * pitch;
                          for (u32 x = 0; x < width; x++) fputc(row[x * texel + 3], fa);
                      }
                      fclose(fa);
                  }
              } }
        } else {
            for (u32 y = 0; y < height; y++) {
                const u8* row = px + (SIZE_T)y * pitch;
                for (u32 x = 0; x < width; x++) {
                    const u8* pixel = row + x * texel;
                    u8 rgb[3]; ld_dump_texel_rgb(pixel, fp16, rgb);
                    if (rgb[0] || rgb[1] || rgb[2])
                        nonblack++;
                    fingerprint = fnv1a(rgb, 3, fingerprint);
                }
            }
        }
        D3D12_RANGE wr = {0, 0};
        rb->lpVtbl->Unmap(rb, 0, &wr);
        g_ld_last_dump_fingerprint = fingerprint;
    }
    rb->lpVtbl->Release(rb);
    return nonblack;
}

/* ---- GOWA: write-back of main-memory render targets ---------------------
 * RPCS3 calls this "write color buffers". A title that renders into MAIN
 * memory (location 1) does so because the CPU or the SPUs read the result:
 * God of War Ascension blits each frame to 1:0xB20000 for its SPU MLAA and
 * renders its average luminance into 1:0xB10000 (1x1) for exposure. The live
 * engine kept those pixels on the host GPU only, so the SPUs and the CPU read
 * whatever guest memory held -- and the 3D menu came out black.
 *
 * Every GPU write to a location-1 surface marks it dirty; the FIFO walker
 * calls rsx_live_draw_sync_mainmem before a GET_REPORT / semaphore release
 * (the points where the CPU or an SPU job may look), which reads the dirty
 * targets back and stores them in the guest layout (A8R8G8B8 big-endian, or
 * four big-endian halves for FP16 targets). LD_MAINMEM_WRITEBACK=0 disables. */
/* GOWA_MLAA_BYPASS=1: a LARGE main-memory colour target (>= 640x360) stays
 * on the GPU -- no write-back, and a texture at its address samples the GPU
 * surface. That is the frame God of War Ascension copies to 1:0xB20000 for
 * its SPU MLAA: the round trip cost a full GPU sync, a 3.6 MB readback and a
 * re-upload per frame, and pins the final image at 720p. The image comes out
 * without the MLAA (the SPUs still process the stale guest copy, unseen).
 * Small targets keep the write-back -- the 1x1 average luminance at
 * 1:0xB10000 feeds the CPU's exposure. */
static int ld_mm_bypass(const surface_t* sf)
{
    static int on = -1;
    if (on < 0) { const char* e = getenv("GOWA_MLAA_BYPASS"); on = e ? (*e != '0') : 0; }
    return on && sf->location == 1 && sf->w >= 640 && sf->h >= 360;
}
int rsx_live_draw_mainmem_writeback_on(void)
{
    static int on = -1;
    if (on < 0) { const char* e = getenv("LD_MAINMEM_WRITEBACK"); on = e ? (*e != '0') : 1; }
    return on;
}
static ID3D12Resource* g_ld_mm_rb;
static UINT64 g_ld_mm_rb_size;
static unsigned long long g_ld_mm_n, g_ld_mm_bytes;
/* The read-back pixels (host layout, LD_PX size, rows rpitch apart) into the
 * guest layout at dst: A8R8G8B8 bytes or big-endian halves, box-averaged
 * back to guest size under GOWA_RES_SCALE. */
static void ld_mm_store(const surface_t* sf, const u8* px, u32 rpitch, u8* dst)
{
    const int fp16 = sf->fmt == DXGI_FORMAT_R16G16B16A16_FLOAT;
    const u32 texel = fp16 ? 8u : 4u;
    const u32 k = g.res_scale;
    if (k != 1) {   /* back to guest size: box average (argb8), one sample (fp16) */
        for (u32 y = 0; y < sf->h; y++) {
            u8* out = dst + (SIZE_T)y * sf->w * texel;
            for (u32 x = 0; x < sf->w; x++) {
                if (!fp16) {
                    u32 acc[4] = {0, 0, 0, 0};
                    for (u32 j = 0; j < k; j++) {
                        const u8* q = px + (SIZE_T)(y * k + j) * rpitch + (SIZE_T)x * k * 4u;
                        for (u32 i = 0; i < k; i++, q += 4) { acc[0] += q[0]; acc[1] += q[1]; acc[2] += q[2]; acc[3] += q[3]; }
                    }
                    const u32 n = k * k; u8* o = out + x * 4u;   /* RGBA -> A,R,G,B */
                    o[0] = (u8)((acc[3] + n / 2) / n); o[1] = (u8)((acc[0] + n / 2) / n);
                    o[2] = (u8)((acc[1] + n / 2) / n); o[3] = (u8)((acc[2] + n / 2) / n);
                } else {
                    const u8* q = px + (SIZE_T)(y * k) * rpitch + (SIZE_T)x * k * 8u;
                    u8* o = out + x * 8u;
                    for (u32 c = 0; c < 4; c++) { o[c * 2u] = q[c * 2u + 1u]; o[c * 2u + 1u] = q[c * 2u]; }
                }
            }
        }
    } else
    for (u32 y = 0; y < sf->h; y++) {
        const u8* row = px + (SIZE_T)y * rpitch;
        u8* out = dst + (SIZE_T)y * sf->w * texel;
        if (!fp16) {
            for (u32 x = 0; x < sf->w; x++) {          /* RGBA -> A,R,G,B */
                const u8* q = row + x * 4u; u8* o = out + x * 4u;
                o[0] = q[3]; o[1] = q[0]; o[2] = q[1]; o[3] = q[2];
            }
        } else {
            for (u32 x = 0; x < sf->w * 4u; x++) {     /* LE halves -> BE */
                out[x * 2u] = row[x * 2u + 1u]; out[x * 2u + 1u] = row[x * 2u];
            }
        }
    }
}
/* GOWA_MM_ASYNC (default on): a tiny main-memory target -- the 1x1 average
 * luminance the CPU reads for exposure -- comes back without a GPU sync. Its
 * copy goes into the current command list, and the guest gets the pixels of
 * the newest copy that has completed at the next write-back of that surface,
 * about a frame later. The synchronous path waited for the GPU every frame,
 * which defeated GOWA_LD_PIPE. GOWA_MM_ASYNC=0 restores it. */
/* GOWA_MM_LOG1=<frame>: from that presented frame on (1 = from the start), every
 * write-back of a 1x1 surface (the auto-exposure luminance) with the bytes the
 * guest receives, sync or async, until 600 lines. */
static void ld_mm_log1(const surface_t* sf, const u8* dst, const char* how)
{
    static int l1 = -2; static int n1;
    if (l1 == -2) { const char* e = getenv("GOWA_MM_LOG1"); l1 = e ? atoi(e) : -1; }
    if (l1 >= 0 && sf->w == 1 && sf->h == 1 && (int)g_ld_frames >= l1 && n1++ < 600)
        fprintf(stderr, "[mm-1x1] frame=%u 1:0x%X %s %02X%02X%02X%02X %02X%02X%02X%02X\n", g_ld_frames, sf->offset, how,
                dst[0], dst[1], dst[2], dst[3], dst[4], dst[5], dst[6], dst[7]);
}
#define LD_MMA_BUFS 3
typedef struct {
    ID3D12Resource* rb[LD_MMA_BUFS];
    UINT64 size[LD_MMA_BUFS];
    u64 fence[LD_MMA_BUFS], seq[LD_MMA_BUFS];
    u32 rpitch[LD_MMA_BUFS], w[LD_MMA_BUFS], h[LD_MMA_BUFS];
    DXGI_FORMAT fmt[LD_MMA_BUFS];
    u8 pend[LD_MMA_BUFS];
} ld_mm_async_t;
static ld_mm_async_t s_mma[MAX_SURFACES];
static u64 s_mma_seq;
static int ld_mm_async(u32 slot, const surface_t* sf, ID3D12Resource* rt, u32 rpitch, UINT64 need, u8* dst)
{
    static int on = -1;
    if (on < 0) { const char* e = getenv("GOWA_MM_ASYNC"); on = e ? (*e != '0') : 1; }
    if (!on || sf->w * sf->h > 256u || slot >= MAX_SURFACES) return 0;
    ld_mm_async_t* a = &s_mma[slot];
    const u64 done = g.fence->lpVtbl->GetCompletedValue(g.fence);
    int best = -1;
    for (int b = 0; b < LD_MMA_BUFS; b++)
        if (a->pend[b] && a->fence[b] <= done && (best < 0 || a->seq[b] > a->seq[best])) best = b;
    if (best >= 0) {
        if (a->w[best] == sf->w && a->h[best] == sf->h && a->fmt[best] == sf->fmt) {
            u8* px = NULL; D3D12_RANGE rr = {0, (SIZE_T)a->size[best]};
            if (SUCCEEDED(a->rb[best]->lpVtbl->Map(a->rb[best], 0, &rr, (void**)&px))) {
                ld_mm_store(sf, px, a->rpitch[best], dst);
                ld_mm_log1(sf, dst, "async");
                D3D12_RANGE wr = {0, 0};
                a->rb[best]->lpVtbl->Unmap(a->rb[best], 0, &wr);
            }
        }
        const u64 bs = a->seq[best];
        for (int b = 0; b < LD_MMA_BUFS; b++)
            if (a->pend[b] && a->fence[b] <= done && a->seq[b] <= bs) a->pend[b] = 0;
    }
    int f = -1;
    for (int b = 0; b < LD_MMA_BUFS; b++) if (!a->pend[b]) { f = b; break; }
    if (f < 0) return 1;            /* every buffer in flight: the guest keeps the last value */
    if (!a->rb[f] || a->size[f] < need) {
        if (a->rb[f]) { retire_texture(a->rb[f]); a->rb[f] = NULL; }
        D3D12_HEAP_PROPERTIES hp = {0}; hp.Type = D3D12_HEAP_TYPE_READBACK;
        D3D12_RESOURCE_DESC bd = {0};
        bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = need;
        bd.Height = 1; bd.DepthOrArraySize = 1; bd.MipLevels = 1;
        bd.Format = DXGI_FORMAT_UNKNOWN; bd.SampleDesc.Count = 1;
        bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        if (FAILED(g.dev->lpVtbl->CreateCommittedResource(g.dev, &hp, D3D12_HEAP_FLAG_NONE, &bd,
                D3D12_RESOURCE_STATE_COPY_DEST, NULL, &IID_ID3D12Resource, (void**)&a->rb[f])))
            { a->rb[f] = NULL; return 0; }
        a->size[f] = need;
    }
    D3D12_RESOURCE_BARRIER b = {0};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = rt;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    g.list->lpVtbl->ResourceBarrier(g.list, 1, &b);
    D3D12_TEXTURE_COPY_LOCATION src = {0}, dl = {0};
    src.pResource = rt; src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; src.SubresourceIndex = 0;
    dl.pResource = a->rb[f]; dl.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dl.PlacedFootprint.Footprint.Format = sf->fmt;
    dl.PlacedFootprint.Footprint.Width = LD_PX(sf->w);
    dl.PlacedFootprint.Footprint.Height = LD_PX(sf->h);
    dl.PlacedFootprint.Footprint.Depth = 1;
    dl.PlacedFootprint.Footprint.RowPitch = rpitch;
    g.list->lpVtbl->CopyTextureRegion(g.list, &dl, 0, 0, 0, &src, NULL);
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    g.list->lpVtbl->ResourceBarrier(g.list, 1, &b);
    a->fence[f] = g.fence_value + 1;     /* the value the next ld_flush signals */
    a->seq[f] = ++s_mma_seq;
    a->pend[f] = 1;
    a->rpitch[f] = rpitch; a->w[f] = sf->w; a->h[f] = sf->h; a->fmt[f] = sf->fmt;
    g_ld_mm_n++;
    return 1;
}
static void ld_mm_writeback_one(u32 slot)
{
    surface_t* sf = &g.surfaces[slot];
    ID3D12Resource* rt = sf->tex;
    if (!rt || !sf->w || !sf->h) return;
    const int fp16 = sf->fmt == DXGI_FORMAT_R16G16B16A16_FLOAT;
    const u32 texel = fp16 ? 8u : 4u;
    const u32 k = g.res_scale;   /* GOWA_RES_SCALE: read the k x larger resource back */
    const u32 rpitch = (LD_PX(sf->w) * texel + 255) & ~255u;
    const UINT64 need = (UINT64)rpitch * LD_PX(sf->h);
    u8* dst = (u8*)guest_ptr(1, sf->offset, sf->w * texel * sf->h);
    if (!dst) return;
    if (ld_mm_async(slot, sf, rt, rpitch, need, dst)) return;
    if (!g_ld_mm_rb || g_ld_mm_rb_size < need) {
        if (g_ld_mm_rb) { retire_texture(g_ld_mm_rb); g_ld_mm_rb = NULL; }
        D3D12_HEAP_PROPERTIES hp = {0}; hp.Type = D3D12_HEAP_TYPE_READBACK;
        D3D12_RESOURCE_DESC bd = {0};
        bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = need;
        bd.Height = 1; bd.DepthOrArraySize = 1; bd.MipLevels = 1;
        bd.Format = DXGI_FORMAT_UNKNOWN; bd.SampleDesc.Count = 1;
        bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        if (FAILED(g.dev->lpVtbl->CreateCommittedResource(g.dev, &hp, D3D12_HEAP_FLAG_NONE, &bd,
                D3D12_RESOURCE_STATE_COPY_DEST, NULL, &IID_ID3D12Resource, (void**)&g_ld_mm_rb)))
            { g_ld_mm_rb = NULL; return; }
        g_ld_mm_rb_size = need;
    }
    D3D12_RESOURCE_BARRIER b = {0};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = rt;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    g.list->lpVtbl->ResourceBarrier(g.list, 1, &b);
    D3D12_TEXTURE_COPY_LOCATION src = {0}, dl = {0};
    src.pResource = rt; src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; src.SubresourceIndex = 0;
    dl.pResource = g_ld_mm_rb; dl.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dl.PlacedFootprint.Footprint.Format = sf->fmt;
    dl.PlacedFootprint.Footprint.Width = LD_PX(sf->w);
    dl.PlacedFootprint.Footprint.Height = LD_PX(sf->h);
    dl.PlacedFootprint.Footprint.Depth = 1;
    dl.PlacedFootprint.Footprint.RowPitch = rpitch;
    g.list->lpVtbl->CopyTextureRegion(g.list, &dl, 0, 0, 0, &src, NULL);
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    g.list->lpVtbl->ResourceBarrier(g.list, 1, &b);
    ld_flush(LD_FLUSH_READBACK);
    u8* px = NULL; D3D12_RANGE rr = {0, (SIZE_T)need};
    if (FAILED(g_ld_mm_rb->lpVtbl->Map(g_ld_mm_rb, 0, &rr, (void**)&px))) return;
    ld_mm_store(sf, px, rpitch, dst);
    D3D12_RANGE wr = {0, 0};
    g_ld_mm_rb->lpVtbl->Unmap(g_ld_mm_rb, 0, &wr);
    g_ld_mm_n++; g_ld_mm_bytes += (unsigned long long)sf->w * texel * sf->h;
    static u8 first[MAX_SURFACES];
    if (!first[slot] || (g_ld_mm_n & (g_ld_mm_n - 1)) == 0)
        fprintf(stderr, "[mm-writeback] #%llu 1:0x%X %ux%u %s (%llu MB no total) primeiro texel %02X%02X%02X%02X\n",
                g_ld_mm_n, sf->offset, sf->w, sf->h, fp16 ? "fp16" : "argb8",
                g_ld_mm_bytes >> 20, dst[0], dst[1], dst[2], dst[3]);
    ld_mm_log1(sf, dst, "sync");
    first[slot] = 1;
}
void rsx_live_draw_sync_mainmem(void)
{
    if (!g.ready || g_ld_movie_mode || !rsx_live_draw_mainmem_writeback_on()) return;
    for (u32 i = 0; i < g.n_surfaces && i < MAX_SURFACES; i++) {
        if (!g_ld_mm_dirty[i]) continue;
        g_ld_mm_dirty[i] = 0;
        if (g.surfaces[i].location == 1 && !ld_mm_bypass(&g.surfaces[i])) ld_mm_writeback_one(i);
    }
}

#if !defined(YZ_PERF_CLEAN)
static void ld_parity_capture_surface(
    const char* directory, const char* phase, u32 serial,
    u32 buffer_id, u32 target)
{
    if (!directory || !directory[0] || target >= g.n_surfaces)
        return;
    surface_t* surface = &g.surfaces[target];
    char path[MAX_PATH * 2];
    snprintf(
        path, sizeof(path),
        "%s\\%s_%03u_flip_%llu_buf_%u_surface_%u_write_%u_kind_%u.ppm",
        directory, phase, serial,
        (unsigned long long)g_ld_flip_requested,
        buffer_id, target, surface->last_write_generation,
        surface->last_write_kind);
    const u64 nonblack = ld_dump_surface_ppm(path, surface);
    fprintf(
        stderr,
        "[parity-capture] phase=%s serial=%u flip=%llu buffer=%u "
        "surface=%u resource=%u write=%u kind=%u draw=%u clear=%u "
        "copy=%u blit=%u resolve=%u other=%u guest_blit=%u "
        "present_copy=%u get=%08X put=%08X cb=%u vb=%u srv=%u "
        "groups=%llu drop_ring=%llu cb_recycles=%llu frame_cb_recycles=%u "
        "hash=%016llX nonblack=%s%llu path=%s\n",
        phase, serial, (unsigned long long)g_ld_flip_requested,
        buffer_id, target, surface->resource_serial,
        surface->last_write_generation, surface->last_write_kind,
        surface->last_draw_generation, surface->last_clear_generation,
        surface->last_copy_generation, surface->last_blit_generation,
        surface->last_resolve_generation, surface->last_other_generation,
        surface->last_guest_blit_generation,
        surface->last_present_copy_generation,
        g_ld_fifo_get, g_ld_fifo_put, g.cb_used, g.vb_used,
        g.srv_ring_used,
        (unsigned long long)g_ld_stats.groups_executed,
        (unsigned long long)g_ld_stats.group_drop_ring,
        (unsigned long long)g_ld_vertex_constant_ring_recycles,
        g_ld_frame_vertex_constant_ring_recycles,
        (unsigned long long)g_ld_last_dump_fingerprint,
        nonblack == UINT64_MAX ? "unknown:" : "",
        (unsigned long long)(nonblack == UINT64_MAX ? 0 : nonblack),
        path);
    fflush(stderr);
}
#endif

int rsx_live_draw_debug_dump_surface(
    u32 location, u32 offset, const char* path)
{
    if (!g.ready || !path || !path[0]) return -1;
    for (u32 i = 0; i < g.n_surfaces; i++) {
        const surface_t* surface = &g.surfaces[i];
        if (surface->location != location || surface->offset != offset)
            continue;
        const u64 nonblack = ld_dump_surface_ppm(path, surface);
        if (nonblack == UINT64_MAX) return -1;
        fprintf(stderr,
                "[live-replay-boundary] surface=%u:%08X index=%u "
                "size=%ux%u fmt=%d nonblack=%llu path=%s\n",
                location, offset, i, surface->w, surface->h, (int)surface->fmt,
                (unsigned long long)nonblack, path);
        fflush(stderr);
        return 0;
    }
    fprintf(stderr,
            "[live-replay-boundary] missing surface=%u:%08X path=%s\n",
            location, offset, path);
    return 1;
}

static void ld_vertex_diag_emit(const char* reason, int dump_surface)
{
    if (!g.ready || !g.vertex_diag_dir[0])
        return;
    rsx_dsp_surface state_surface;
    rsx_dsp_get_surface(&g.rsx, &state_surface);
    u32 current = LD_INVALID_SURFACE;
    for (u32 i = 0; i < g.n_surfaces; i++) {
        if (g.surfaces[i].location == state_surface.color_location[0] &&
            g.surfaces[i].offset == state_surface.color_offset[0]) {
            current = i;
            break;
        }
    }
    const u32 presented = g_ld_last_present_target;
    const surface_t* presented_surface =
        presented < g.n_surfaces ? &g.surfaces[presented] : NULL;
    char attrs[80];
    size_t used = 0;
    attrs[0] = '\0';
    for (u32 attr = 0; attr < 16; attr++) {
        if (!(g_ld_current_pso.input_mask & (1u << attr)))
            continue;
        const int written = snprintf(
            attrs + used, sizeof(attrs) - used,
            "%s%u", used ? "," : "", attr);
        if (written < 0 || (size_t)written >= sizeof(attrs) - used)
            break;
        used += (size_t)written;
    }
    const UINT64 d3d_messages = g_ld_info_queue
        ? g_ld_info_queue->lpVtbl->GetNumStoredMessages(g_ld_info_queue)
        : 0;
#if defined(YZ_PERF_PROFILE)
    const u64 pso_lookups = g_ld_profile.total.pso_lookups;
    const u64 pso_hits = g_ld_profile.total.pso_hits;
    const u64 pso_misses = g_ld_profile.total.pso_misses;
    const u64 pso_full = g_ld_profile.total.pso_full;
#else
    const u64 pso_lookups = 0, pso_hits = 0, pso_misses = 0, pso_full = 0;
#endif
    fprintf(stderr,
            "[rsx-vertex-state] reason=%s mode=%s frame=%u "
            "flip{requested=%llu last_req=%u consumed=%llu last_cons=%u} "
            "surface{present=%u off=0x%08X size=%ux%u current=%u} "
            "draw{packets=%llu groups=%llu exec=%llu "
            "drop_fetch=%llu drop_pso=%llu drop_ring=%llu} "
            "pso{look=%llu hit=%llu miss=%llu full=%llu "
            "cached=%u capacity=%u} "
            "vp{hash=%016llX start=%u instrs=%u mask=0x%04X "
            "attrs=[%s] stride=%u packed=%u} "
            "d3d{debug=%d queued_messages=%llu}\n",
            reason ? reason : "unknown", ld_vertex_mode_name(), g_ld_frames,
            (unsigned long long)g_ld_flip_requested,
            g_ld_last_requested_buffer,
            (unsigned long long)g_ld_flip_consumed,
            g_ld_last_consumed_buffer,
            presented,
            presented_surface ? presented_surface->offset : 0,
            presented_surface ? presented_surface->w : 0,
            presented_surface ? presented_surface->h : 0,
            current,
            (unsigned long long)g_ld_stats.packets_seen,
            (unsigned long long)g_ld_stats.groups_seen,
            (unsigned long long)g_ld_stats.groups_executed,
            (unsigned long long)g_ld_stats.group_drop_fetch,
            (unsigned long long)g_ld_stats.group_drop_pso,
            (unsigned long long)g_ld_stats.group_drop_ring,
            (unsigned long long)pso_lookups,
            (unsigned long long)pso_hits,
            (unsigned long long)pso_misses,
            (unsigned long long)pso_full,
            g.n_psos, MAX_PSOS,
            (unsigned long long)g_ld_current_pso.vp_hash,
            g_ld_current_pso.vp_start,
            g_ld_current_pso.vp_instrs,
            g_ld_current_pso.input_mask,
            attrs,
            g_ld_current_pso.input_stride,
            g_ld_current_pso.packed_offsets,
            g_ld_debug_layer_enabled,
            (unsigned long long)d3d_messages);
    if (dump_surface && presented_surface && g.vertex_diag_dir[0]) {
        char path[MAX_PATH * 2];
        snprintf(
            path, sizeof(path), "%s\\last_present_surface.ppm",
            g.vertex_diag_dir);
        const u64 nonblack =
            ld_dump_surface_ppm(path, presented_surface);
        fprintf(stderr,
                "[rsx-vertex-surface] mode=%s target=%u "
                "pixels=%llu nonblack=%s%llu path=%s\n",
                ld_vertex_mode_name(), presented,
                (unsigned long long)presented_surface->w *
                    presented_surface->h,
                nonblack == UINT64_MAX ? "unknown:" : "",
                (unsigned long long)(
                    nonblack == UINT64_MAX ? 0 : nonblack),
                path);
    }
    fflush(stderr);
}

/* =====================================================================
 * GOWA_FRAMEGEN=1: frame generation. The game runs at ~40 FPS, limited by
 * the SPUs; each guest frame is followed on screen by an interpolated one,
 * so a high-refresh display shows ~2x the frames.
 *
 *   1. the guest frame is blitted into fg[k] (backbuffer size, RGBA8) and its
 *      luminance into lum[k] (320x180);
 *   2. motion: one vector per 4x4 block of lum (80x45), full search of
 *      +-GOWA_FRAMEGEN_R pixels (default 12) on an 8x8 window, SAD with a
 *      small bias towards zero; cur(x) ~= prev(x + d);
 *   3. the middle frame samples prev at x + d/2 and cur at x - d/2 and
 *      averages them, straight into the backbuffer, and is presented now;
 *   4. the real frame is presented half a guest interval later
 *      (rsx_live_draw_framegen_tick, from the present thread's loop), or
 *      right away when the next guest frame comes first.
 * HUD and static parts get d = 0. A guest interval over 60 ms (a stall, a
 * loading screen) skips the interpolation. Latency: + half a frame.
 * ===================================================================== */
#define FG_LW 320u
#define FG_LH 180u
#define FG_B  4u
#define FG_MW (FG_LW / FG_B)
#define FG_MH (FG_LH / FG_B)
#define FG_READ (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
static struct {
    int on;
    int failed;
    u32 w, h;
    ID3D12Resource *fg[2], *lum[2], *mv;
    ID3D12DescriptorHeap *heap, *rtv;
    u32 step, rtv_step;
    ID3D12RootSignature* rs;
    ID3D12PipelineState *luma_pso, *me_pso, *interp_pso;
    int have_prev, cur, pending;
    LONGLONG due, last_guest, freq, last_log;
    long n_interp, n_real;
    int radius;
    /* the delayed real frame goes through its own small command list, so it
     * does not cost an ld_flush (which, with GOWA_LD_PIPE, waits for the
     * submission before last) */
    ID3D12CommandAllocator* alloc[3];
    u64 alloc_fence[3];
    ID3D12GraphicsCommandList* list;
    u32 ai;
} s_fg = { -1 };

int rsx_live_draw_framegen_on(void)
{
    if (s_fg.on < 0) {
        const char* e = getenv("GOWA_FRAMEGEN");
        s_fg.on = e && *e == '1';
        e = getenv("GOWA_FRAMEGEN_R");
        s_fg.radius = e ? atoi(e) : 12;
        if (s_fg.radius < 1) s_fg.radius = 1;
        if (s_fg.radius > 32) s_fg.radius = 32;
        LARGE_INTEGER f; QueryPerformanceFrequency(&f); s_fg.freq = f.QuadPart;
        if (s_fg.on) fprintf(stderr, "[framegen] ligado: movimento por blocos %ux%u em %ux%u, raio %d\n",
                             FG_B, FG_B, FG_LW, FG_LH, s_fg.radius);
    }
    return s_fg.on;
}

static ID3DBlob* ld_fg_compile(const char* src, const char* entry, const char* target)
{
    ID3DBlob *b = NULL, *err = NULL;
    if (FAILED(D3DCompile(src, strlen(src), "ld_framegen", NULL, NULL, entry, target,
                          D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &b, &err))) {
        if (err) { fprintf(stderr, "[framegen] %s: %s\n", entry, (const char*)err->lpVtbl->GetBufferPointer(err)); err->lpVtbl->Release(err); }
        return NULL;
    }
    return b;
}

static int ld_fg_pipeline(void)
{
    if (s_fg.interp_pso) return 1;
    if (s_fg.failed) return 0;
    s_fg.failed = 1;
    D3D12_DESCRIPTOR_RANGE r[4] = {0};
    for (u32 i = 0; i < 3; i++) { r[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; r[i].NumDescriptors = 1; r[i].BaseShaderRegister = i; }
    r[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; r[3].NumDescriptors = 1;
    D3D12_ROOT_PARAMETER rp[5] = {0};
    for (u32 i = 0; i < 4; i++) {
        rp[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rp[i].DescriptorTable.NumDescriptorRanges = 1;
        rp[i].DescriptorTable.pDescriptorRanges = &r[i];
        rp[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    }
    rp[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rp[4].Constants.Num32BitValues = 4;
    rp[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    D3D12_STATIC_SAMPLER_DESC ss = {0};
    ss.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    ss.AddressU = ss.AddressV = ss.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    ss.MaxLOD = D3D12_FLOAT32_MAX;
    ss.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    D3D12_ROOT_SIGNATURE_DESC rsd = {0};
    rsd.NumParameters = 5; rsd.pParameters = rp;
    rsd.NumStaticSamplers = 1; rsd.pStaticSamplers = &ss;
    ID3DBlob *sig = NULL, *err = NULL;
    if (FAILED(D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err))) {
        if (err) err->lpVtbl->Release(err);
        return 0;
    }
    HRESULT hr = g.dev->lpVtbl->CreateRootSignature(g.dev, 0, sig->lpVtbl->GetBufferPointer(sig),
                                                    sig->lpVtbl->GetBufferSize(sig),
                                                    &IID_ID3D12RootSignature, (void**)&s_fg.rs);
    sig->lpVtbl->Release(sig);
    if (FAILED(hr)) return 0;
    static const char luma_src[] =
        "Texture2D<float4> src : register(t0);\n"
        "RWTexture2D<float> dst : register(u0);\n"
        "SamplerState s0 : register(s0);\n"
        "[numthreads(8, 8, 1)]\n"
        "void main(uint3 id : SV_DispatchThreadID) {\n"
        "    uint w, h; dst.GetDimensions(w, h);\n"
        "    if (id.x >= w || id.y >= h) return;\n"
        "    float2 inv = 1.0 / float2(w, h);\n"
        "    float2 uv = (float2(id.xy) + 0.5) * inv;\n"
        "    float3 c = src.SampleLevel(s0, uv + float2(-0.25, -0.25) * inv, 0).rgb\n"
        "             + src.SampleLevel(s0, uv + float2( 0.25, -0.25) * inv, 0).rgb\n"
        "             + src.SampleLevel(s0, uv + float2(-0.25,  0.25) * inv, 0).rgb\n"
        "             + src.SampleLevel(s0, uv + float2( 0.25,  0.25) * inv, 0).rgb;\n"
        "    dst[id.xy] = dot(c * 0.25, float3(0.299, 0.587, 0.114));\n"
        "}\n";
    static const char me_src[] =
        "Texture2D<float> lc : register(t0);\n"
        "Texture2D<float> lp : register(t1);\n"
        "RWTexture2D<float2> mv : register(u0);\n"
        "cbuffer C : register(b0) { int R; int B; float lam; float pad; };\n"
        "[numthreads(8, 8, 1)]\n"
        "void main(uint3 id : SV_DispatchThreadID) {\n"
        "    uint mw, mh; mv.GetDimensions(mw, mh);\n"
        "    if (id.x >= mw || id.y >= mh) return;\n"
        "    uint lw, lh; lc.GetDimensions(lw, lh);\n"
        "    int2 hi = int2(lw - 1, lh - 1);\n"
        "    int2 o = int2(id.xy) * B - 2;\n"
        "    float cw[64];\n"
        "    [unroll] for (int y = 0; y < 8; y++) [unroll] for (int x = 0; x < 8; x++)\n"
        "        cw[y * 8 + x] = lc.Load(int3(clamp(o + int2(x, y), int2(0, 0), hi), 0));\n"
        "    float best = 1e30; int2 bd = int2(0, 0);\n"
        "    for (int dy = -R; dy <= R; dy++) {\n"
        "        for (int dx = -R; dx <= R; dx++) {\n"
        "            float s = lam * (abs(dx) + abs(dy));\n"
        "            [unroll] for (int y = 0; y < 8; y++) [unroll] for (int x = 0; x < 8; x++)\n"
        "                s += abs(cw[y * 8 + x] - lp.Load(int3(clamp(o + int2(x + dx, y + dy), int2(0, 0), hi), 0)));\n"
        "            if (s < best) { best = s; bd = int2(dx, dy); }\n"
        "        }\n"
        "    }\n"
        "    mv[id.xy] = float2(bd) / float2(lw, lh);\n"
        "}\n";
    static const char interp_src[] =
        "Texture2D<float4> tprev : register(t0);\n"
        "Texture2D<float4> tcur : register(t1);\n"
        "Texture2D<float2> tmv : register(t2);\n"
        "SamplerState s0 : register(s0);\n"
        "struct V { float4 p : SV_Position; float2 uv : TEXCOORD0; };\n"
        "V vs(uint id : SV_VertexID) {\n"
        "    V o; o.uv = float2((id << 1) & 2, id & 2);\n"
        "    o.p = float4(o.uv * float2(2, -2) + float2(-1, 1), 0, 1); return o; }\n"
        "float4 ps(V i) : SV_Target {\n"
        "    float2 d = tmv.SampleLevel(s0, i.uv, 0);\n"
        "    float3 a = tprev.SampleLevel(s0, i.uv + d * 0.5, 0).rgb;\n"
        "    float3 b = tcur.SampleLevel(s0, i.uv - d * 0.5, 0).rgb;\n"
        "    return float4(lerp(a, b, 0.5), 1.0);\n"
        "}\n";
    ID3DBlob* lb = ld_fg_compile(luma_src, "main", "cs_5_0");
    ID3DBlob* mb = ld_fg_compile(me_src, "main", "cs_5_0");
    ID3DBlob* vb = ld_fg_compile(interp_src, "vs", "vs_5_0");
    ID3DBlob* pb = ld_fg_compile(interp_src, "ps", "ps_5_0");
    int ok = lb && mb && vb && pb;
    if (ok) {
        D3D12_COMPUTE_PIPELINE_STATE_DESC cd = {0};
        cd.pRootSignature = s_fg.rs;
        cd.CS.pShaderBytecode = lb->lpVtbl->GetBufferPointer(lb);
        cd.CS.BytecodeLength = lb->lpVtbl->GetBufferSize(lb);
        ok = SUCCEEDED(g.dev->lpVtbl->CreateComputePipelineState(g.dev, &cd, &IID_ID3D12PipelineState, (void**)&s_fg.luma_pso));
        cd.CS.pShaderBytecode = mb->lpVtbl->GetBufferPointer(mb);
        cd.CS.BytecodeLength = mb->lpVtbl->GetBufferSize(mb);
        ok = ok && SUCCEEDED(g.dev->lpVtbl->CreateComputePipelineState(g.dev, &cd, &IID_ID3D12PipelineState, (void**)&s_fg.me_pso));
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pd = {0};
        pd.pRootSignature = s_fg.rs;
        pd.VS.pShaderBytecode = vb->lpVtbl->GetBufferPointer(vb);
        pd.VS.BytecodeLength = vb->lpVtbl->GetBufferSize(vb);
        pd.PS.pShaderBytecode = pb->lpVtbl->GetBufferPointer(pb);
        pd.PS.BytecodeLength = pb->lpVtbl->GetBufferSize(pb);
        pd.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        pd.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        pd.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        pd.RasterizerState.DepthClipEnable = TRUE;
        pd.SampleMask = 0xFFFFFFFFu;
        pd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pd.NumRenderTargets = 1;
        pd.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        pd.SampleDesc.Count = 1;
        ok = ok && SUCCEEDED(g.dev->lpVtbl->CreateGraphicsPipelineState(g.dev, &pd, &IID_ID3D12PipelineState, (void**)&s_fg.interp_pso));
    }
    if (lb) lb->lpVtbl->Release(lb);
    if (mb) mb->lpVtbl->Release(mb);
    if (vb) vb->lpVtbl->Release(vb);
    if (pb) pb->lpVtbl->Release(pb);
    if (!ok) { fprintf(stderr, "[framegen] pipeline falhou\n"); return 0; }
    s_fg.failed = 0;
    return 1;
}

/* descriptor slots in s_fg.heap: SRV fg0 fg1 lum0 lum1 mv, UAV lum0 lum1 mv */
static D3D12_GPU_DESCRIPTOR_HANDLE ld_fg_gpu(u32 i)
{
    D3D12_GPU_DESCRIPTOR_HANDLE h;
    s_fg.heap->lpVtbl->GetGPUDescriptorHandleForHeapStart(s_fg.heap, &h);
    h.ptr += (u64)i * s_fg.step;
    return h;
}
static D3D12_CPU_DESCRIPTOR_HANDLE ld_fg_cpu(ID3D12DescriptorHeap* hp, u32 step, u32 i)
{
    D3D12_CPU_DESCRIPTOR_HANDLE h;
    hp->lpVtbl->GetCPUDescriptorHandleForHeapStart(hp, &h);
    h.ptr += (size_t)i * step;
    return h;
}
static ID3D12Resource* ld_fg_tex(u32 w, u32 h, DXGI_FORMAT f, D3D12_RESOURCE_FLAGS fl, D3D12_RESOURCE_STATES st)
{
    D3D12_HEAP_PROPERTIES hp = {0}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC rd = {0};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rd.Width = w; rd.Height = h; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
    rd.Format = f; rd.SampleDesc.Count = 1; rd.Flags = fl;
    ID3D12Resource* r = NULL;
    if (FAILED(g.dev->lpVtbl->CreateCommittedResource(g.dev, &hp, D3D12_HEAP_FLAG_NONE, &rd, st, NULL,
                                                      &IID_ID3D12Resource, (void**)&r))) return NULL;
    return r;
}
static int ld_fg_resources(u32 w, u32 h)
{
    if (s_fg.fg[0] && s_fg.w == w && s_fg.h == h) return 1;
    for (int i = 0; i < 2; i++) {
        if (s_fg.fg[i]) { retire_texture(s_fg.fg[i]); s_fg.fg[i] = NULL; }
        if (s_fg.lum[i]) { retire_texture(s_fg.lum[i]); s_fg.lum[i] = NULL; }
    }
    if (s_fg.mv) { retire_texture(s_fg.mv); s_fg.mv = NULL; }
    if (!s_fg.heap) {
        D3D12_DESCRIPTOR_HEAP_DESC hd = {0};
        hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; hd.NumDescriptors = 8;
        hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (FAILED(g.dev->lpVtbl->CreateDescriptorHeap(g.dev, &hd, &IID_ID3D12DescriptorHeap, (void**)&s_fg.heap))) return 0;
        hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; hd.NumDescriptors = 2; hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        if (FAILED(g.dev->lpVtbl->CreateDescriptorHeap(g.dev, &hd, &IID_ID3D12DescriptorHeap, (void**)&s_fg.rtv))) return 0;
        s_fg.step = g.dev->lpVtbl->GetDescriptorHandleIncrementSize(g.dev, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        s_fg.rtv_step = g.dev->lpVtbl->GetDescriptorHandleIncrementSize(g.dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }
    for (int i = 0; i < 2; i++) {
        s_fg.fg[i] = ld_fg_tex(w, h, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, FG_READ);
        s_fg.lum[i] = ld_fg_tex(FG_LW, FG_LH, DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, FG_READ);
        if (!s_fg.fg[i] || !s_fg.lum[i]) return 0;
        g.dev->lpVtbl->CreateShaderResourceView(g.dev, s_fg.fg[i], NULL, ld_fg_cpu(s_fg.heap, s_fg.step, (u32)i));
        g.dev->lpVtbl->CreateShaderResourceView(g.dev, s_fg.lum[i], NULL, ld_fg_cpu(s_fg.heap, s_fg.step, 2u + (u32)i));
        g.dev->lpVtbl->CreateUnorderedAccessView(g.dev, s_fg.lum[i], NULL, NULL, ld_fg_cpu(s_fg.heap, s_fg.step, 5u + (u32)i));
        g.dev->lpVtbl->CreateRenderTargetView(g.dev, s_fg.fg[i], NULL, ld_fg_cpu(s_fg.rtv, s_fg.rtv_step, (u32)i));
    }
    s_fg.mv = ld_fg_tex(FG_MW, FG_MH, DXGI_FORMAT_R16G16_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, FG_READ);
    if (!s_fg.mv) return 0;
    g.dev->lpVtbl->CreateShaderResourceView(g.dev, s_fg.mv, NULL, ld_fg_cpu(s_fg.heap, s_fg.step, 4u));
    g.dev->lpVtbl->CreateUnorderedAccessView(g.dev, s_fg.mv, NULL, NULL, ld_fg_cpu(s_fg.heap, s_fg.step, 7u));
    s_fg.w = w; s_fg.h = h; s_fg.have_prev = 0; s_fg.pending = 0;
    fprintf(stderr, "[framegen] recursos %ux%u\n", w, h);
    return 1;
}
static void ld_fg_barrier(ID3D12Resource* r, D3D12_RESOURCE_STATES a, D3D12_RESOURCE_STATES b)
{
    D3D12_RESOURCE_BARRIER x = {0};
    x.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    x.Transition.pResource = r;
    x.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    x.Transition.StateBefore = a; x.Transition.StateAfter = b;
    g.list->lpVtbl->ResourceBarrier(g.list, 1, &x);
}
static void ld_fg_bind_compute(ID3D12PipelineState* pso)
{
    ID3D12DescriptorHeap* heaps[] = {s_fg.heap};
    g.list->lpVtbl->SetDescriptorHeaps(g.list, 1, heaps); ld_state_dirty();
    g.list->lpVtbl->SetPipelineState(g.list, pso);
    g.list->lpVtbl->SetComputeRootSignature(g.list, s_fg.rs);
}
/* lum[k] from fg[k] */
static void ld_fg_luma(int k)
{
    ld_fg_barrier(s_fg.lum[k], FG_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    ld_fg_bind_compute(s_fg.luma_pso);
    g.list->lpVtbl->SetComputeRootDescriptorTable(g.list, 0, ld_fg_gpu((u32)k));
    g.list->lpVtbl->SetComputeRootDescriptorTable(g.list, 1, ld_fg_gpu((u32)k));
    g.list->lpVtbl->SetComputeRootDescriptorTable(g.list, 2, ld_fg_gpu((u32)k));
    g.list->lpVtbl->SetComputeRootDescriptorTable(g.list, 3, ld_fg_gpu(5u + (u32)k));
    g.list->lpVtbl->Dispatch(g.list, (FG_LW + 7u) / 8u, (FG_LH + 7u) / 8u, 1);
    ld_fg_barrier(s_fg.lum[k], D3D12_RESOURCE_STATE_UNORDERED_ACCESS, FG_READ);
}
/* mv from lum[cur] (t0) against lum[prev] (t1) */
static void ld_fg_motion(int cur, int prev)
{
    ld_fg_barrier(s_fg.mv, FG_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    ld_fg_bind_compute(s_fg.me_pso);
    g.list->lpVtbl->SetComputeRootDescriptorTable(g.list, 0, ld_fg_gpu(2u + (u32)cur));
    g.list->lpVtbl->SetComputeRootDescriptorTable(g.list, 1, ld_fg_gpu(2u + (u32)prev));
    g.list->lpVtbl->SetComputeRootDescriptorTable(g.list, 2, ld_fg_gpu(2u + (u32)prev));
    g.list->lpVtbl->SetComputeRootDescriptorTable(g.list, 3, ld_fg_gpu(7u));
    const float lam = 0.02f;
    u32 c[4] = { (u32)s_fg.radius, FG_B, 0, 0 };
    memcpy(&c[2], &lam, 4);
    g.list->lpVtbl->SetComputeRoot32BitConstants(g.list, 4, 4, c, 0);
    g.list->lpVtbl->Dispatch(g.list, (FG_MW + 7u) / 8u, (FG_MH + 7u) / 8u, 1);
    ld_fg_barrier(s_fg.mv, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, FG_READ);
}
/* the middle frame of fg[prev] and fg[cur] into backbuffer bbi (RENDER_TARGET) */
static void ld_fg_interp(int prev, int cur, u32 bbi)
{
    D3D12_RESOURCE_DESC bd;
    g.backbuf[bbi]->lpVtbl->GetDesc(g.backbuf[bbi], &bd);
    const D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtv_handle(bbi);
    g.list->lpVtbl->OMSetRenderTargets(g.list, 1, &rtv, FALSE, NULL);
    D3D12_VIEWPORT vp = {0, 0, (float)bd.Width, (float)bd.Height, 0.0f, 1.0f};
    D3D12_RECT sc = {0, 0, (LONG)bd.Width, (LONG)bd.Height};
    g.list->lpVtbl->RSSetViewports(g.list, 1, &vp);
    g.list->lpVtbl->RSSetScissorRects(g.list, 1, &sc);
    ID3D12DescriptorHeap* heaps[] = {s_fg.heap};
    g.list->lpVtbl->SetDescriptorHeaps(g.list, 1, heaps); ld_state_dirty();
    g.list->lpVtbl->SetPipelineState(g.list, s_fg.interp_pso);
    g.list->lpVtbl->SetGraphicsRootSignature(g.list, s_fg.rs);
    g.list->lpVtbl->SetGraphicsRootDescriptorTable(g.list, 0, ld_fg_gpu((u32)prev));
    g.list->lpVtbl->SetGraphicsRootDescriptorTable(g.list, 1, ld_fg_gpu((u32)cur));
    g.list->lpVtbl->SetGraphicsRootDescriptorTable(g.list, 2, ld_fg_gpu(4u));
    g.list->lpVtbl->SetGraphicsRootDescriptorTable(g.list, 3, ld_fg_gpu(5u));
    g.list->lpVtbl->IASetPrimitiveTopology(g.list, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g.list->lpVtbl->DrawInstanced(g.list, 3, 1, 0, 0);
}
static LONGLONG ld_fg_now(void) { LARGE_INTEGER q; QueryPerformanceCounter(&q); return q.QuadPart; }
static void ld_fg_log(void)
{
    const LONGLONG now = ld_fg_now();
    if (!s_fg.last_log) s_fg.last_log = now;
    if (now - s_fg.last_log >= 5 * s_fg.freq) {
        fprintf(stderr, "[framegen] 5 s: %ld interpolados, %ld reais (%.1f quadros/s na tela)\n",
                s_fg.n_interp, s_fg.n_real, (s_fg.n_interp + s_fg.n_real) * (double)s_fg.freq / (double)(now - s_fg.last_log));
        s_fg.n_interp = s_fg.n_real = 0; s_fg.last_log = now;
    }
}
static void ld_fg_barrier_on(ID3D12GraphicsCommandList* l, ID3D12Resource* r, D3D12_RESOURCE_STATES a, D3D12_RESOURCE_STATES b)
{
    D3D12_RESOURCE_BARRIER x = {0};
    x.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    x.Transition.pResource = r;
    x.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    x.Transition.StateBefore = a; x.Transition.StateAfter = b;
    l->lpVtbl->ResourceBarrier(l, 1, &x);
}
/* the real frame fg[cur] to the screen, through s_fg.list. Everything that
 * produced fg[cur] was submitted before (with the middle frame), and the
 * queue keeps the order. */
static void ld_fg_present_real(void)
{
    if (!s_fg.list) {
        int ok = 1;
        for (int i = 0; i < 3 && ok; i++)
            ok = SUCCEEDED(g.dev->lpVtbl->CreateCommandAllocator(g.dev, D3D12_COMMAND_LIST_TYPE_DIRECT,
                               &IID_ID3D12CommandAllocator, (void**)&s_fg.alloc[i]));
        if (ok) ok = SUCCEEDED(g.dev->lpVtbl->CreateCommandList(g.dev, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, s_fg.alloc[0],
                                   NULL, &IID_ID3D12GraphicsCommandList, (void**)&s_fg.list));
        if (!ok) { s_fg.list = NULL; s_fg.pending = 0; return; }
        s_fg.list->lpVtbl->Close(s_fg.list);
    }
    const u32 ai = s_fg.ai; s_fg.ai = (ai + 1u) % 3u;
    if (s_fg.alloc_fence[ai] && g.fence->lpVtbl->GetCompletedValue(g.fence) < s_fg.alloc_fence[ai] &&
        SUCCEEDED(g.fence->lpVtbl->SetEventOnCompletion(g.fence, s_fg.alloc_fence[ai], g.fence_event)))
        WaitForSingleObject(g.fence_event, INFINITE);
    s_fg.alloc[ai]->lpVtbl->Reset(s_fg.alloc[ai]);
    ID3D12GraphicsCommandList* l = s_fg.list;
    l->lpVtbl->Reset(l, s_fg.alloc[ai], NULL);
    const u32 bbi = g.swap->lpVtbl->GetCurrentBackBufferIndex(g.swap);
    ID3D12Resource* bb = g.backbuf[bbi];
    ld_fg_barrier_on(l, s_fg.fg[s_fg.cur], FG_READ, D3D12_RESOURCE_STATE_COPY_SOURCE);
    ld_fg_barrier_on(l, bb, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
    l->lpVtbl->CopyResource(l, bb, s_fg.fg[s_fg.cur]);
    ld_fg_barrier_on(l, bb, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
    ld_fg_barrier_on(l, s_fg.fg[s_fg.cur], D3D12_RESOURCE_STATE_COPY_SOURCE, FG_READ);
    l->lpVtbl->Close(l);
    ID3D12CommandList* lists[] = {(ID3D12CommandList*)l};
    g.queue->lpVtbl->ExecuteCommandLists(g.queue, 1, lists);
    const u64 v = ++g.fence_value;
    g.queue->lpVtbl->Signal(g.queue, g.fence, v);
    s_fg.alloc_fence[ai] = v;
    if (g.ready) ld_swap_present();
    s_fg.pending = 0;
    s_fg.n_real++;
    ld_fg_log();
}
/* A guest frame (surface `src`, in RENDER_TARGET, SRV slot src_srv of the main
 * heap) through frame generation. 0: not handled (caller presents it). */
static int ld_fg_present_guest(ID3D12Resource* src, u32 src_srv)
{
    if (!g.ready || !ld_fg_pipeline() || !ld_blit_pipeline()) return 0;
    D3D12_RESOURCE_DESC bd;
    g.backbuf[0]->lpVtbl->GetDesc(g.backbuf[0], &bd);
    if (!ld_fg_resources((u32)bd.Width, bd.Height)) return 0;
    if (s_fg.pending) ld_fg_present_real();
    const LONGLONG now = ld_fg_now();
    const int k = s_fg.have_prev ? (s_fg.cur ^ 1) : s_fg.cur;
    /* 1. the guest surface into fg[k] */
    u32 slots[SRV_TABLE_SIZE];
    for (u32 i = 0; i < SRV_TABLE_SIZE; i++) slots[i] = SRV_WHITE;
    slots[0] = src_srv;
    const D3D12_GPU_DESCRIPTOR_HANDLE table = srv_table(slots);
    if (!table.ptr) return 0;
    ld_fg_barrier(src, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    ld_fg_barrier(s_fg.fg[k], FG_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
    {
        const D3D12_CPU_DESCRIPTOR_HANDLE rtv = ld_fg_cpu(s_fg.rtv, s_fg.rtv_step, (u32)k);
        g.list->lpVtbl->OMSetRenderTargets(g.list, 1, &rtv, FALSE, NULL);
        D3D12_VIEWPORT vp = {0, 0, (float)bd.Width, (float)bd.Height, 0.0f, 1.0f};
        D3D12_RECT sc = {0, 0, (LONG)bd.Width, (LONG)bd.Height};
        g.list->lpVtbl->RSSetViewports(g.list, 1, &vp);
        g.list->lpVtbl->RSSetScissorRects(g.list, 1, &sc);
        ID3D12DescriptorHeap* heaps[] = {g.srv_heap, g.smp_heap};
        g.list->lpVtbl->SetDescriptorHeaps(g.list, 2, heaps); ld_state_dirty();
        g.list->lpVtbl->SetPipelineState(g.list, s_blit_pso);
        g.list->lpVtbl->SetGraphicsRootSignature(g.list, s_blit_rs);
        g.list->lpVtbl->SetGraphicsRootDescriptorTable(g.list, 0, table);
        g.list->lpVtbl->IASetPrimitiveTopology(g.list, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        g.list->lpVtbl->DrawInstanced(g.list, 3, 1, 0, 0);
    }
    ld_fg_barrier(s_fg.fg[k], D3D12_RESOURCE_STATE_RENDER_TARGET, FG_READ);
    ld_fg_barrier(src, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    /* 2. its luminance */
    ld_fg_luma(k);
    const LONGLONG dt = now - s_fg.last_guest;
    s_fg.last_guest = now;
    /* GOWA (25/10): só interpola com a média do jogo acima de GOWA_FRAMEGEN_MIN_FPS (padrão
     * 30). Abaixo disso o quadro inventado borra o movimento ("efeito fantasma") e, num PC
     * limitado pela CPU, os apresentados a mais custam quadros reais (5600X: 25,0 sem contra
     * 19,7 com). A média (móvel, 1/10 por quadro) evita alternar perto do limite. */
    static double s_fg_dt_medio;
    static LONGLONG s_fg_dt_max = -1;
    if (s_fg_dt_max < 0) {
        const char* e = getenv("GOWA_FRAMEGEN_MIN_FPS");
        long f = e ? strtol(e, NULL, 10) : 30;
        if (f < 1) f = 1;
        if (f > 240) f = 240;
        s_fg_dt_max = s_fg.freq / f;
    }
    if (dt > 0 && dt < s_fg.freq / 4)
        s_fg_dt_medio = s_fg_dt_medio > 0 ? 0.9 * s_fg_dt_medio + 0.1 * (double)dt : (double)dt;
    const int interp = s_fg.have_prev && dt > 0 && dt < s_fg.freq * 60 / 1000 &&
                       s_fg_dt_medio <= (double)s_fg_dt_max;
    s_fg.cur = k;
    s_fg.have_prev = 1;
    if (!interp) { ld_flush(LD_FLUSH_PRESENT); ld_fg_present_real(); return 1; }   /* fg[k] submitted first */
    /* 3. motion and the middle frame, presented now */
    ld_fg_motion(k, k ^ 1);
    const u32 bbi = g.swap->lpVtbl->GetCurrentBackBufferIndex(g.swap);
    ld_fg_barrier(g.backbuf[bbi], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    ld_fg_interp(k ^ 1, k, bbi);
    ld_fg_barrier(g.backbuf[bbi], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    ld_flush(LD_FLUSH_PRESENT);
    if (g.ready) ld_swap_present();
    s_fg.n_interp++;
    /* 4. the real one half an interval later */
    s_fg.pending = 1;
    s_fg.due = now + dt / 2;
    return 1;
}
/* For the present thread's loop: microseconds until the pending real frame
 * is due (-1: none), and presenting it once due. */
int rsx_live_draw_framegen_wait_us(void)
{
    if (!s_fg.pending) return -1;
    const LONGLONG now = ld_fg_now();
    return now >= s_fg.due ? 0 : (int)((s_fg.due - now) * 1000000 / s_fg.freq);
}
void rsx_live_draw_framegen_tick(void)
{
    if (s_fg.pending && g.ready && ld_fg_now() >= s_fg.due) ld_fg_present_real();
}

/* GOWA (25/10): resumo de desempenho para o launcher. A cada amostra de ~5 s do
 * [fps], se o jogo está jogando (g_gowa_jogando: o estado "go to playing" do log do
 * jogo; carregamentos e menus não contam), guarda o FPS do jogo; a cada 6 amostras
 * (~30 s) grava o JSON em GOWA_PERF_JSON ou <pasta do exe>\desempenho.json, por um
 * .tmp trocado de uma vez -- vale mesmo se o jogo for fechado à força. "p1" é o
 * percentil 1 das amostras de 5 s (com poucas amostras, perto do mínimo). */
#include <intrin.h>
char g_ld_gpu_nome[128];
unsigned g_ld_gpu_vram_mb;
extern volatile int g_gowa_jogando;
extern int g_gowa_pin_on;
static float s_perf_fps[8192];
static unsigned s_perf_n;
static double s_perf_seg;
static unsigned s_perf_ram_max_mb;
static int ld_perf_cmp(const void* a, const void* b)
{
    const float x = *(const float*)a, y = *(const float*)b;
    return x < y ? -1 : x > y ? 1 : 0;
}
static void ld_perf_json_grava(void)
{
    static char caminho[MAX_PATH];
    if (!caminho[0]) {
        const char* e = getenv("GOWA_PERF_JSON");
        if (e && *e) snprintf(caminho, sizeof caminho, "%s", e);
        else {
            char exe[MAX_PATH]; DWORD n = GetModuleFileNameA(NULL, exe, sizeof exe);
            char* barra = n ? strrchr(exe, '\\') : NULL;
            if (barra) { barra[1] = 0; snprintf(caminho, sizeof caminho, "%sdesempenho.json", exe); }
            else snprintf(caminho, sizeof caminho, "desempenho.json");
        }
    }
    /* CPU: nome e AVX2 / AVX-512 (F, BW, VL e DQ, como o gerar.ps1 confere) */
    char cpu[49] = ""; int r[4];
    __cpuid(r, 0x80000000);
    if ((unsigned)r[0] >= 0x80000004u) {
        for (int k = 0; k < 3; k++) { __cpuid(r, 0x80000002 + k); memcpy(cpu + 16 * k, r, 16); }
        cpu[48] = 0;
    }
    char* c0 = cpu; while (*c0 == ' ') c0++;
    __cpuidex(r, 7, 0);
    const int avx2 = (r[1] >> 5) & 1;
    const int avx512 = ((r[1] >> 16) & 1) && ((r[1] >> 30) & 1) && ((r[1] >> 31) & 1) && ((r[1] >> 17) & 1);
    SYSTEM_INFO si; GetSystemInfo(&si);
    MEMORYSTATUSEX ms; ms.dwLength = sizeof ms; GlobalMemoryStatusEx(&ms);
    /* FPS: média ponderada pelo tempo, percentil 1, mínimo e máximo das amostras */
    double soma = 0; float mn = 0, mx = 0, p1 = 0;
    if (s_perf_n) {
        static float ord[8192];
        memcpy(ord, s_perf_fps, s_perf_n * sizeof(float));
        qsort(ord, s_perf_n, sizeof(float), ld_perf_cmp);
        for (unsigned k = 0; k < s_perf_n; k++) soma += s_perf_fps[k];
        mn = ord[0]; mx = ord[s_perf_n - 1]; p1 = ord[s_perf_n / 100];
    }
    extern double cellGcm_flip_hz_atual(void);
    char lim_txt[16]; { const double hz = cellGcm_flip_hz_atual(); if (hz > 0) snprintf(lim_txt, sizeof lim_txt, "%.0f", hz); else snprintf(lim_txt, sizeof lim_txt, "0"); }
    char tmp[MAX_PATH + 8]; snprintf(tmp, sizeof tmp, "%s.tmp", caminho);
    FILE* f = fopen(tmp, "w");
    if (!f) return;
    /* o nome da CPU e da GPU sem aspas nem barras (vão no JSON como estão) */
    for (char* q = c0; *q; q++) if (*q == '"' || *q == '\\') *q = ' ';
    for (char* q = g_ld_gpu_nome; *q; q++) if (*q == '"' || *q == '\\') *q = ' ';
    SYSTEMTIME st; GetSystemTime(&st);
#ifdef SPU_SX_RZ
    const char* build = "portatil";   /* sem AVX-512: contas de float das SPUs em arredondamento para zero */
#else
    const char* build = "avx512";
#endif
    fprintf(f, "{\n  \"versao\": 1,\n  \"jogo\": \"gow-ascension\",\n  \"quando\": \"%04u-%02u-%02uT%02u:%02u:%02uZ\",\n",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    fprintf(f, "  \"build\": \"%s\",\n  \"cpu\": \"%s\",\n  \"cpu_recursos\": [%s%s],\n  \"threads\": %lu,\n  \"ram_mb\": %llu,\n"
               "  \"gpu\": \"%s\",\n  \"vram_mb\": %u,\n",
            build, c0, avx2 ? "\"avx2\"" : "", avx512 ? (avx2 ? ", \"avx512\"" : "\"avx512\"") : "",
            (unsigned long)si.dwNumberOfProcessors, (unsigned long long)(ms.ullTotalPhys >> 20), g_ld_gpu_nome, g_ld_gpu_vram_mb);
    fprintf(f, "  \"fps\": {\"medio\": %.2f, \"p1\": %.2f, \"min\": %.2f, \"max\": %.2f, \"amostras\": %u},\n  \"minutos\": %.1f,\n",
            s_perf_n ? soma / (double)s_perf_n : 0.0, p1, mn, mx, s_perf_n, s_perf_seg / 60.0);
    { char tc[16] = "null", tg[16] = "null";   /* só com o medidor na tela ligado: é ele que lê os sensores */
      if (s_osd_temp_cpu_max > 0) snprintf(tc, sizeof tc, "%ld", (long)s_osd_temp_cpu_max);
      if (s_osd_temp_gpu_max > 0) snprintf(tg, sizeof tg, "%ld", (long)s_osd_temp_gpu_max);
      fprintf(f, "  \"temp_max_c\": {\"cpu\": %s, \"gpu\": %s},\n", tc, tg); }
    fprintf(f, "  \"ram_uso_max_mb\": %u,\n", s_perf_ram_max_mb);
    fprintf(f, "  \"opcoes\": {\"fixacao_threads\": %d, \"resolucao_interna\": %u, \"limitador_hz\": %s}\n}\n",
            g_gowa_pin_on, (unsigned)g.res_scale, lim_txt);
    fclose(f);
    MoveFileExA(tmp, caminho, MOVEFILE_REPLACE_EXISTING);
}
static void ld_perf_amostra(double fps, double seg)
{
    /* as 2 primeiras amostras (~10 s) de cada entrada no jogo ainda pegam o fim do carregamento */
    static int s_jogando_antes, s_pula;
    if (!g_gowa_jogando) { s_jogando_antes = 0; return; }
    if (!s_jogando_antes) { s_jogando_antes = 1; s_pula = 2; }
    if (s_pula > 0) { s_pula--; return; }
    if (seg <= 0 || fps <= 0) return;
    { MEMORYSTATUSEX ms; ms.dwLength = sizeof ms;   /* pico de RAM em uso no sistema (desempenho.json) */
      if (GlobalMemoryStatusEx(&ms)) { const unsigned u = (unsigned)((ms.ullTotalPhys - ms.ullAvailPhys) >> 20); if (u > s_perf_ram_max_mb) s_perf_ram_max_mb = u; } }
    if (s_perf_n < sizeof s_perf_fps / sizeof s_perf_fps[0]) s_perf_fps[s_perf_n++] = (float)fps;
    s_perf_seg += seg;
    if (s_perf_n % 6 == 0) ld_perf_json_grava();
}

void rsx_live_draw_present(u32 buffer_id)
{
    if (ld_mtrace_on()) fprintf(stderr, "[mtrace] f=%u PRESENT buffer %u\n", g_ld_frames, buffer_id);
    if (!g.ready) return;

    /* LD_SURF_DUMP=<dir>: dump EVERY live surface once, with its non-black
     * count, at LD_SURF_DUMP_FRAME (default 1200).
     *
     * LD_FRAME_DUMP only reads back the presented surface, which answers "is
     * the screen black" (it is: nonblack=0 on every frame) but not WHERE the
     * pixels stop. ps1_netemu composites PS1 VRAM into a 720x512 surface and
     * then upscales into the presented 1280x720 one, so the interesting
     * question is which of those is already empty. */
    { static int done = 0; const char* sd = getenv("LD_SURF_DUMP");
      u32 at = 1200; { const char* fe = getenv("LD_SURF_DUMP_FRAME");
                       if (fe) at = (u32)strtoul(fe, 0, 0); }
      /* LD_SURF_DUMP_ON_VRAM=1 waits for PS1 VRAM to hold a drawn frame instead
       * of firing at a frame number. Needs PS1_PC=1, which is what samples it. */
      int gate;
      { static int onv = -1;
        if (onv < 0) onv = getenv("LD_SURF_DUMP_ON_VRAM") ? 1 : 0;
        gate = onv ? (g_ld_ps1_vram_ready != 0) : (g_ld_frames >= at);
        /* GOWA LD_SURF_DUMP_SEC=<s>: fire that many seconds after the first
         * present instead (menus run at 100-230 FPS, so a frame number lands
         * on a different scene from run to run) */
        { static double ss = -2.0; static ULONGLONG t0;
          if (ss == -2.0) { const char* e = getenv("LD_SURF_DUMP_SEC"); ss = e ? atof(e) : -1.0; t0 = GetTickCount64(); }
          if (ss >= 0.0) gate = (double)(GetTickCount64() - t0) >= ss * 1000.0; }
        /* LD_SURF_DUMP_GATE=<file>: fire when <file> appears instead. */
        static const char* gf = (const char*)-1;
        if (gf == (const char*)-1) gf = getenv("LD_SURF_DUMP_GATE");
        /* Re-arms when the file goes away, so each appearance dumps once. */
        if (gf && sd) { FILE* f = fopen(gf, "rb"); gate = f != NULL; if (f) fclose(f);
                        if (!gate) done = 0; } }
      /* GOWA: LD_SURF_DUMP_ON_BAD=1 -- dump at the present after the cast
       * check found the exposure chain corrupt (a flashing frame), and once
       * at a good one, into <dir>\ruim / <dir>\bom */
      { static int ob = -1; if (ob < 0) ob = getenv("LD_SURF_DUMP_ON_BAD") ? 1 : 0;
        if (ob && sd && g_ld_surf_dump_req) {
            static char sub[MAX_PATH]; static int nb, ng;
            const int ruim = g_ld_surf_dump_req == 1;
            g_ld_surf_dump_req = 0;
            if ((ruim && nb < 2) || (!ruim && ng < 1)) {
                snprintf(sub, sizeof sub, "%s\\%s%d", sd, ruim ? "ruim" : "bom", ruim ? nb++ : ng++);
                CreateDirectoryA(sub, NULL);
                for (u32 i = 0; i < g.n_surfaces; i++) {
                    char path[MAX_PATH * 2];
                    snprintf(path, sizeof(path), "%s\\f%06u_surf_%02u_%08X_%ux%u.ppm",
                             sub, g_ld_frames, i, g.surfaces[i].offset, g.surfaces[i].w, g.surfaces[i].h);
                    ld_dump_surface_ppm(path, &g.surfaces[i]);
                }
                fprintf(stderr, "[surf-dump] %s (quadro %u)\n", sub, g_ld_frames);
            }
        } }
      if (sd && sd[0] && !done && gate) {
          done = 1;
          for (u32 i = 0; i < g.n_surfaces; i++) {
              char path[MAX_PATH * 2];
              snprintf(path, sizeof(path), "%s\\f%06u_surf_%02u_%08X_%ux%u.ppm",
                       sd, g_ld_frames, i, g.surfaces[i].offset,
                       g.surfaces[i].w, g.surfaces[i].h);
              const u64 nb = ld_dump_surface_ppm(path, &g.surfaces[i]);
              fprintf(stderr, "[surf-dump] slot=%u %u:0x%08X %ux%u nonblack=%llu"
                              " draw_gen=%u clear_gen=%u\n",
                      i, g.surfaces[i].location, g.surfaces[i].offset,
                      g.surfaces[i].w, g.surfaces[i].h,
                      (unsigned long long)(nb == UINT64_MAX ? 0 : nb),
                      g.surfaces[i].last_draw_generation,
                      g.surfaces[i].last_clear_generation);
          }
      } }
    /* LD_FRAME_DUMP=<dir> [+ LD_FRAME_DUMP_EVERY=<n>, default 300]: write the
     * PRESENTED surface to a .ppm every n flips. The engine could already dump
     * a surface, but only at shutdown or behind the parity harness -- neither
     * reachable when a title is simply left running, which is exactly when you
     * want to see what is on screen. PrintWindow and CopyFromScreen both come
     * back white for a D3D12 swapchain, so a readback here is the only way to
     * answer "what is it actually drawing?". */
    /* The dump itself runs once the presented surface is known (below): a
     * title that scans out through cellResc never matches a registered display
     * buffer and presents the fallback surface, so matching here dumped nothing. */
    static int dump_every = -1; static const char* dump_dir; static unsigned dump_n;
    if (dump_every < 0) { dump_dir = getenv("LD_FRAME_DUMP");
                          const char* e = getenv("LD_FRAME_DUMP_EVERY");
                          dump_every = (dump_dir && dump_dir[0]) ? (e ? atoi(e) : 300) : 0;
                          if (dump_every <= 0 && dump_dir && dump_dir[0]) dump_every = 300; }
    /* LD_FRAME_DUMP_FROM=<frame>: start dumping there (dense captures of a
     * late scene, e.g. frames for a GIF, without thousands of early files). */
    static long dump_from = -1;
    if (dump_from < 0) { const char* e = getenv("LD_FRAME_DUMP_FROM"); dump_from = e ? atol(e) : 0; }
    /* GOWA LD_FRAME_DUMP_FROM_SEC=<s>: start that many seconds after the
     * first present (see LD_SURF_DUMP_SEC) */
    { static double fs = -2.0; static ULONGLONG f0;
      if (fs == -2.0) { const char* e = getenv("LD_FRAME_DUMP_FROM_SEC"); fs = e ? atof(e) : -1.0; f0 = GetTickCount64(); }
      if (fs >= 0.0) dump_from = (double)(GetTickCount64() - f0) >= fs * 1000.0 ? 0 : 0x7FFFFFFF; }
    const int dump_now = dump_every > 0 && (long)g_ld_frames >= dump_from &&
                         (dump_n++ % (unsigned)dump_every) == 0;
    /* A flip names a registered display buffer. The current color target may
     * be an offscreen shadow/postprocess surface at that instant; copying it
     * caused a010 to present black despite executing the scene's draws. */
    u32 target = LD_INVALID_SURFACE;
    if (buffer_id < 8 && g.display_buffers[buffer_id].valid) {
        const display_buffer_t* display = &g.display_buffers[buffer_id];
        for (u32 i = 0; i < g.n_surfaces; i++) {
            if (g.surfaces[i].location == display->location &&
                g.surfaces[i].offset == display->offset) {
                target = i;
                break;
            }
        }
    }
    if (target == LD_INVALID_SURFACE && buffer_id < 8 &&
        g.display_buffers[buffer_id].valid && g.display_buffers[buffer_id].resolve_src > 0 &&
        (u32)g.display_buffers[buffer_id].resolve_src <= g.n_surfaces)
        target = (u32)g.display_buffers[buffer_id].resolve_src - 1;   /* see note_resolve */
    if (target == LD_INVALID_SURFACE) {
        target = current_surface();
        static u32 fallback_logs = 0;
        if (fallback_logs++ < 32)
            fprintf(stderr,
                    "[live-draw] flip %u has no registered/rendered scanout; "
                    "falling back to current surface %u\n",
                    buffer_id, target);
    }
    if (target == LD_INVALID_SURFACE) {
        fprintf(stderr, "[live-draw] frame present skipped: no color surface\n");
        return;
    }
    g_ld_last_present_target = target;
    /* GOWA_PRESENT_LOG=<s>[,<n>]: n presents (default 60) from s seconds on:
     * flip buffer, presented surface, draws executed since the last present */
    { static double ps = -2.0; static int pn, pmax = 60; static ULONGLONG p0; static u64 last_groups;
      if (ps == -2.0) { const char* e = getenv("GOWA_PRESENT_LOG"); ps = e ? atof(e) : -1.0;
                        const char* c = e ? strchr(e, ',') : NULL; if (c) pmax = atoi(c + 1); p0 = GetTickCount64(); }
      if (ps >= 0.0 && pn < pmax && (double)(GetTickCount64() - p0) >= ps * 1000.0) {
          pn++;
          LARGE_INTEGER q; QueryPerformanceCounter(&q);
          fprintf(stderr, "[present-log] t=%lld buf=%u alvo=%u (0x%X) desenhos=%llu frame=%u\n",
                  (long long)(q.QuadPart / 10000), buffer_id, target, g.surfaces[target].offset,
                  (unsigned long long)(g_ld_stats.groups_executed - last_groups), g_ld_frames);
      }
      last_groups = g_ld_stats.groups_executed; }
    /* GOWA_SKIP_DUP_PRESENT=1: a flip of the surface just presented with no
     * draw in between is not shown again (God of War Ascension flips every
     * menu frame twice; A/B for the blinking menu text) */
    { static int sd = -1; static u32 last_t = LD_INVALID_SURFACE; static u64 last_g = ~0ull;
      if (sd < 0) { const char* e = getenv("GOWA_SKIP_DUP_PRESENT"); sd = e && *e == '1'; }
      const int dup = target == last_t && g_ld_stats.groups_executed == last_g;
      last_t = target; last_g = g_ld_stats.groups_executed;
      if (sd && dup) return; }
    /* GOWA_MEMDUMP=loc:addr:len:frame (hex addr/len): guest memory to
     * out\memdump_<addr>.bin once, at that presented frame */
    /* GOWA: the last field may be a list "t1,t2,..." of frames or "<n>s"
     * seconds after the first present; dump k > 0 gets the suffix _k */
    { static int md = -1; static u32 mloc, maddr, mlen; static double mt[8]; static int mts[8], mn, mi;
      static ULONGLONG m0;
      if (md < 0) { const char* e = getenv("GOWA_MEMDUMP"); md = 0; char rest[128] = {0};
          if (e && sscanf(e, "%u:%x:%x:%127s", &mloc, &maddr, &mlen, rest) == 4) {
              md = 1; m0 = GetTickCount64();
              for (char* t = strtok(rest, ","); t && mn < 8; t = strtok(NULL, ",")) {
                  mt[mn] = atof(t); mts[mn] = strchr(t, 's') != NULL; mn++; } } }
      if (md == 1 && mi < mn &&
          (mts[mi] ? (double)(GetTickCount64() - m0) >= mt[mi] * 1000.0 : (double)g_ld_frames >= mt[mi])) {
          const u8* src = guest_ptr(mloc, maddr, mlen);
          char fn[MAX_PATH];
          if (mi) snprintf(fn, sizeof fn, "out\\memdump_%08X_%d.bin", maddr, mi);
          else snprintf(fn, sizeof fn, "out\\memdump_%08X.bin", maddr);
          FILE* f = src ? fopen(fn, "wb") : NULL;
          if (f) { fwrite(src, 1, mlen, f); fclose(f); fprintf(stderr, "[memdump] frame %u %u:0x%X 0x%X bytes -> %s\n", g_ld_frames, mloc, maddr, mlen, fn); }
          mi++;
      } }
    if (dump_now) {
        char path[MAX_PATH * 2];
        snprintf(path, sizeof(path), "%s\\frame_%06u.ppm", dump_dir, dump_n - 1u);
        const u64 nb = ld_dump_surface_ppm(path, &g.surfaces[target]);
        fprintf(stderr, "[frame-dump] %s %ux%u nonblack=%llu\n", path,
                g.surfaces[target].w, g.surfaces[target].h,
                (unsigned long long)(nb == UINT64_MAX ? 0 : nb));
        fflush(stderr);
    }
    /* LD_RECORD=<out.mp4> (boot_main's --record): every presented frame, as
     * PPM, into an ffmpeg pipe -- ffmpeg's image2pipe reads concatenated PPMs
     * as a stream. Fragmented MP4 so a run that is killed rather than exiting
     * still leaves a playable file. Frames are stamped at a fixed 30 fps, so
     * playback speed is only right when the title presents at 30.
     * ponytail: one blocking GPU readback per frame; a ring of readback
     * buffers if recording ever has to run at full speed. */
    { static FILE* rec = (FILE*)-1;
      if (rec == (FILE*)-1) {
          const char* out = getenv("LD_RECORD");
          rec = NULL;
          if (out && out[0]) {
              char cmd[MAX_PATH * 2];
              snprintf(cmd, sizeof(cmd),
                       "ffmpeg -loglevel error -y -f image2pipe -framerate 30 -c:v ppm -i - "
                       "-vf \"scale=trunc(iw/2)*2:trunc(ih/2)*2\" -pix_fmt yuv420p "
                       "-movflags frag_keyframe+empty_moov \"%s\"", out);
              rec = _popen(cmd, "wb");
              fprintf(stderr, "[record] %s -> %s\n", rec ? "ffmpeg" : "FAILED to start ffmpeg", out);
          }
      }
      if (rec) {
          g_ld_record_to = rec;
          ld_dump_surface_ppm(NULL, &g.surfaces[target]);
          g_ld_record_to = NULL;
          fflush(rec);
      } }
    const u32 current = current_surface();
    surface_t* presented_surface = &g.surfaces[target];
    /* LD_PRESENT_DBG=1: is this surface actually finished? A surface whose last
     * clear is NEWER than its last draw has been wiped and not redrawn -- that
     * presents as a black frame. Counting them separates "we present the wrong
     * buffer" from "we present the right buffer too early". */
    { static int dbg = -1;
      if (dbg < 0) dbg = getenv("LD_PRESENT_DBG") ? 1 : 0;
      if (dbg) {
          static unsigned long long n = 0, blank = 0;
          n++;
          if (presented_surface->last_draw_generation <
              presented_surface->last_clear_generation) blank++;
          if ((n % 256) == 0)
              /* Also say WHICH surface. Surfaces are now keyed on size as well
               * as offset, so several exist at once and "we present the wrong
               * one" is a live possibility -- the PS1 composite is 720x512 and
               * the PS3 output 1280x720. */
              fprintf(stderr,
                      "[present] %llu/%llu presents showed a surface cleared "
                      "after its last draw (%.1f%%); target=%u %u:0x%X %ux%u"
                      " draw_gen=%u clear_gen=%u\n",
                      (unsigned long long)blank, (unsigned long long)n,
                      100.0 * (double)blank / (double)n,
                      target, presented_surface->location,
                      presented_surface->offset,
                      presented_surface->w, presented_surface->h,
                      presented_surface->last_draw_generation,
                      presented_surface->last_clear_generation);
      } }
#if !defined(YZ_PERF_CLEAN)
    presented_surface->last_present_copy_generation =
        ++g_ld_present_copy_generation;
    yz_frontier_trace_emit(
        YZ_FT_PARITY_FLIP, buffer_id, target,
        (u32)g_ld_flip_requested,
        presented_surface->last_draw_generation,
        presented_surface->last_clear_generation,
        g_ld_fifo_get, g_ld_fifo_put, current);
    yz_frontier_trace_emit(
        YZ_FT_PARITY_SURFACE, target, presented_surface->last_write_kind,
        presented_surface->last_write_generation,
        presented_surface->last_draw_generation,
        presented_surface->last_clear_generation,
        presented_surface->last_copy_generation,
        presented_surface->last_blit_generation,
        presented_surface->last_resolve_generation);
    yz_frontier_trace_emit(
        YZ_FT_PARITY_SURFACE_AUX, target,
        presented_surface->resource_serial,
        presented_surface->last_create_generation,
        presented_surface->last_other_generation,
        presented_surface->last_guest_blit_generation,
        presented_surface->offset, presented_surface->location,
        presented_surface->last_present_copy_generation);
    yz_frontier_trace_emit(
        YZ_FT_PARITY_RENDER, target, current,
        (u32)g_ld_stats.groups_executed,
        (u32)g_ld_stats.group_drop_ring,
        g.cb_used, g.vb_used, g.srv_ring_used,
        presented_surface->last_present_copy_generation);
#endif
    { static u32 last_present_target = LD_INVALID_SURFACE;
      if (target != last_present_target) {
          ld_trace_target("present", target, buffer_id);
          last_present_target = target;
      } }
    ID3D12Resource* srcimg = presented_surface->tex;
    /* GOWA_UI_CHECK=<s>: from s seconds on, copy the bottom-left 640x100 of
     * every presented surface into a ring of read-back buffers (no wait) and,
     * once its fence passes, log the mean luminance -- what really went to
     * Present, at full speed (the menu text blinked on screen while every
     * dumped frame had it). */
    { static double uc = -2.0; static ULONGLONG u0;
      static ID3D12Resource* rb[16]; static u64 rbf[16]; static u32 rbframe[16]; static int rbuse[16];
      static u32 ui; static int logged;
      if (uc == -2.0) { const char* e = getenv("GOWA_UI_CHECK"); uc = e ? atof(e) : -1.0; u0 = GetTickCount64(); }
      if (uc >= 0.0 && logged < 400) {
          const u64 done = g.fence->lpVtbl->GetCompletedValue(g.fence);
          for (int k = 0; k < 16; k++) {
              if (!rbuse[k] || rbf[k] > done) continue;
              u8* m = NULL; D3D12_RANGE r0 = {0, 2560u * 100u};
              if (SUCCEEDED(rb[k]->lpVtbl->Map(rb[k], 0, &r0, (void**)&m)) && m) {
                  u64 sum = 0;
                  for (u32 y = 0; y < 100; y++)
                      for (u32 x = 0; x < 640; x++) {
                          const u8* px = m + y * 2560u + x * 4u;
                          sum += (u64)px[0] * 30 + (u64)px[1] * 59 + (u64)px[2] * 11;
                      }
                  D3D12_RANGE w0 = {0, 0};
                  rb[k]->lpVtbl->Unmap(rb[k], 0, &w0);
                  fprintf(stderr, "[ui-check] frame %u media=%.1f\n", rbframe[k], (double)sum / (640.0 * 100.0 * 100.0));
                  logged++;
              }
              rbuse[k] = 0;
          }
          const LONG pw = LD_PX(presented_surface->w), ph = LD_PX(presented_surface->h);
          if ((double)(GetTickCount64() - u0) >= uc * 1000.0 && !rbuse[ui] && pw >= 640 && ph >= 100 &&
              presented_surface->fmt == DXGI_FORMAT_R8G8B8A8_UNORM) {
              if (!rb[ui]) {
                  D3D12_HEAP_PROPERTIES hp = {0}; hp.Type = D3D12_HEAP_TYPE_READBACK;
                  D3D12_RESOURCE_DESC bd = {0};
                  bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = 2560u * 100u;
                  bd.Height = 1; bd.DepthOrArraySize = 1; bd.MipLevels = 1; bd.SampleDesc.Count = 1;
                  bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
                  g.dev->lpVtbl->CreateCommittedResource(g.dev, &hp, D3D12_HEAP_FLAG_NONE, &bd,
                      D3D12_RESOURCE_STATE_COPY_DEST, NULL, &IID_ID3D12Resource, (void**)&rb[ui]);
              }
              if (rb[ui]) {
                  D3D12_RESOURCE_BARRIER ub = {0};
                  ub.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; ub.Transition.pResource = srcimg;
                  ub.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                  ub.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
                  ub.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
                  g.list->lpVtbl->ResourceBarrier(g.list, 1, &ub);
                  D3D12_TEXTURE_COPY_LOCATION src = {0}, dst = {0};
                  src.pResource = srcimg; src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                  dst.pResource = rb[ui]; dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                  dst.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                  dst.PlacedFootprint.Footprint.Width = 640; dst.PlacedFootprint.Footprint.Height = 100;
                  dst.PlacedFootprint.Footprint.Depth = 1; dst.PlacedFootprint.Footprint.RowPitch = 2560;
                  D3D12_BOX box = {0, (UINT)(ph - 100), 0, 640, (UINT)ph, 1};
                  g.list->lpVtbl->CopyTextureRegion(g.list, &dst, 0, 0, 0, &src, &box);
                  ub.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
                  ub.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
                  g.list->lpVtbl->ResourceBarrier(g.list, 1, &ub);
                  rbf[ui] = g.fence_value + 1;   /* the next signal covers this list */
                  rbframe[ui] = g_ld_frames;
                  rbuse[ui] = 1;
                  ui = (ui + 1) % 16;
              }
          }
      } }
    if (rsx_live_draw_framegen_on()) {
        /* GOWA: [tela] também com a geração de quadros (report 218 veio sem); só a surface do jogo */
        ld_tela_diag(srcimg, D3D12_RESOURCE_STATE_RENDER_TARGET, NULL, D3D12_RESOURCE_STATE_PRESENT);
        if (ld_fg_present_guest(srcimg, SRV_SURFACE_BASE + target))
            goto ld_present_done;   /* GOWA_FRAMEGEN */
    }
    {
    const u32 bbi = g.swap->lpVtbl->GetCurrentBackBufferIndex(g.swap);
    ID3D12Resource* bb = g.backbuf[bbi];

    /* GOWA: a surface whose size is not the backbuffer's (or a format the
     * copy cannot convert) goes through the scaled blit (ld_present_blit). */
    D3D12_RESOURCE_DESC sdesc, bdesc;
    srcimg->lpVtbl->GetDesc(srcimg, &sdesc);
    bb->lpVtbl->GetDesc(bb, &bdesc);
    const int same = sdesc.Width == bdesc.Width && sdesc.Height == bdesc.Height &&
                     sdesc.Format == bdesc.Format;
    const D3D12_RESOURCE_STATES src_use = same ? D3D12_RESOURCE_STATE_COPY_SOURCE
                                               : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    const D3D12_RESOURCE_STATES bb_use = same ? D3D12_RESOURCE_STATE_COPY_DEST
                                              : D3D12_RESOURCE_STATE_RENDER_TARGET;
    D3D12_RESOURCE_BARRIER bar[2] = {0};
    bar[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    bar[0].Transition.pResource = srcimg;
    bar[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    bar[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    bar[0].Transition.StateAfter = src_use;
    bar[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    bar[1].Transition.pResource = bb;
    bar[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    bar[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    bar[1].Transition.StateAfter = bb_use;
    g.list->lpVtbl->ResourceBarrier(g.list, 2, bar);

    { static long n_same, n_blit; static ULONGLONG t0; const ULONGLONG now = GetTickCount64();
      if (same) n_same++; else n_blit++;
      if (!t0) t0 = now;
      if (now - t0 >= 5000) {
          if (getenv("GOWA_PRESENT_PATH_LOG"))
              fprintf(stderr, "[present-path] 5 s: %ld copia, %ld blit (alvo %u %ux%u fmt %d, bb %llux%u)\n", n_same, n_blit,
                      target, (unsigned)sdesc.Width, sdesc.Height, (int)sdesc.Format, (unsigned long long)bdesc.Width, bdesc.Height);
          n_same = n_blit = 0; t0 = now;
      } }
    if (same)
        g.list->lpVtbl->CopyResource(g.list, bb, srcimg);
    else {
        static u32 s_logs;
        if (s_logs++ < 4)
            fprintf(stderr, "[present] superficie %llux%u fmt=%d -> backbuffer %llux%u: blit escalado\n",
                    (unsigned long long)sdesc.Width, sdesc.Height, (int)sdesc.Format,
                    (unsigned long long)bdesc.Width, bdesc.Height);
        ld_present_blit(SRV_SURFACE_BASE + target, bbi);
    }
    ld_tela_diag(srcimg, src_use, bb, bb_use);   /* GOWA: tela preta com som */

    bar[0].Transition.StateBefore = src_use;
    bar[0].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    bar[1].Transition.StateBefore = bb_use;
    bar[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    g.list->lpVtbl->ResourceBarrier(g.list, 2, bar);

    ld_flush(LD_FLUSH_PRESENT);
    {
        const HRESULT present_hr = ld_swap_present();
        if (SUCCEEDED(present_hr))
            ld_present_measure_record(g_ld_frames + 1u);
        else {
            fprintf(stderr,
                    "[d3d-fail] Present hr=0x%08lX frame=%u "
                    "target=%u size=%ux%u window=%ux%u\n",
                    (unsigned long)present_hr, g_ld_frames,
                    target, g.surfaces[target].w, g.surfaces[target].h,
                    g.width, g.height);
            ld_dump_dred("Present", present_hr);
            g.ready = 0;
        }
    }
    }
ld_present_done:;
#if !defined(YZ_PERF_CLEAN)
    if (InterlockedCompareExchange(
            &g_ld_diag_post_movie_pending, 0, 0) != 0) {
        const LONG post_movie_present =
            InterlockedIncrement(&g_ld_diag_post_movie_presents);
        if (post_movie_present == 32 &&
            InterlockedCompareExchange(
                &g_ld_diag_post_movie_pending, 0, 1) == 1) {
            char path[MAX_PATH * 2];
            snprintf(
                path, sizeof(path),
                "%s\\semantic_checkpoint_surface.ppm",
                g.vertex_diag_dir);
            const u64 nonblack =
                ld_dump_surface_ppm(path, &g.surfaces[target]);
            fprintf(stderr,
                    "[rsx-vertex-surface] reason=post-movie-32 "
                    "mode=%s target=%u pixels=%llu "
                    "nonblack=%s%llu path=%s\n",
                    ld_vertex_mode_name(), target,
                    (unsigned long long)g.surfaces[target].w *
                        g.surfaces[target].h,
                    nonblack == UINT64_MAX ? "unknown:" : "",
                    (unsigned long long)(
                        nonblack == UINT64_MAX ? 0 : nonblack),
                    path);
            fflush(stderr);
        }
    }
#endif

    { static unsigned long long packets_at_last_frame = 0;
      g_ld_last_frame_draws = (u32)(g_ld_stats.packets_seen - packets_at_last_frame);
      packets_at_last_frame = g_ld_stats.packets_seen; }
    g_ld_frames++;
#if defined(YZ_PERF_PROFILE)
    ld_profile_present(g_ld_frames);
#endif
#if !defined(YZ_PERF_CLEAN)
    /* Autonomous parity capture.  Scalar records remain circular and cheap.
     * Readback begins only after two alternating display surfaces stop
     * receiving writes while FIFO progress continues, and again for the first
     * 16 held-movement presentations.  The latter continues counting to 120
     * without readback so validation does not materially alter movement. */
    {
        static int parity_enabled = -1;
        static char parity_dir[MAX_PATH * 2];
        static u32 observed_write[MAX_SURFACES];
        static u8 observed_valid[MAX_SURFACES];
        static u32 prior_target = LD_INVALID_SURFACE;
        static u32 prior_get;
        static u32 prior_put;
        static u32 dialogue_stable;
        static u64 dialogue_targets;
        static int dialogue_armed;
        static u32 dialogue_captures;
        static u32 movement_frames;
        static u32 movement_captures;
        if (parity_enabled < 0) {
            const char* enabled = getenv("RSX_PARITY_DIAG");
            const char* directory = getenv("RSX_VALIDATION_DIR");
            parity_enabled = enabled && *enabled && directory && *directory;
            if (parity_enabled) {
                strncpy(parity_dir, directory, sizeof(parity_dir) - 1u);
                parity_dir[sizeof(parity_dir) - 1u] = '\0';
            }
        }
        if (parity_enabled) {
            const LONG movement_phase = InterlockedCompareExchange(
                &g_yz_movement_proof_phase, 0, 0);
            const u64 elapsed = g_yz_auto_start_tick
                ? GetTickCount64() - g_yz_auto_start_tick : 0;
            surface_t* surface = &g.surfaces[target];
            const int fifo_progress =
                g_ld_fifo_get != prior_get || g_ld_fifo_put != prior_put;
            const int alternating =
                prior_target != LD_INVALID_SURFACE && prior_target != target;
            const int unchanged = observed_valid[target] &&
                observed_write[target] == surface->last_write_generation;

            if (!dialogue_armed && !dialogue_captures &&
                movement_phase == 0 &&
                ((g_ld_frame_vertex_constant_ring_recycles != 0 &&
                  elapsed >= 300000ull) || elapsed >= 420000ull)) {
                if (g_ld_frame_vertex_constant_ring_recycles != 0) {
                    dialogue_armed = 1;
                    fprintf(
                        stderr,
                        "[parity-diag] DIALOGUE HIGH-DRAW TRIGGER "
                        "elapsed_ms=%llu frame_recycles=%u total_recycles=%llu "
                        "drop_ring=%llu frame=%u flip=%llu\n",
                        (unsigned long long)elapsed,
                        g_ld_frame_vertex_constant_ring_recycles,
                        (unsigned long long)
                            g_ld_vertex_constant_ring_recycles,
                        (unsigned long long)g_ld_stats.group_drop_ring,
                        g_ld_frames,
                        (unsigned long long)g_ld_flip_requested);
                    fflush(stderr);
                } else if (fifo_progress && alternating && unchanged) {
                    dialogue_stable++;
                    if (target < 64u)
                        dialogue_targets |= 1ull << target;
                } else if (fifo_progress) {
                    dialogue_stable = 0;
                    dialogue_targets = target < 64u ? 1ull << target : 0;
                }
                if (dialogue_stable >= 12u &&
                    dialogue_targets &&
                    (dialogue_targets & (dialogue_targets - 1u)) != 0) {
                    dialogue_armed = 1;
                    fprintf(
                        stderr,
                        "[parity-diag] DIALOGUE TRIGGER elapsed_ms=%llu "
                        "stable=%u targets=%016llX drop_ring=%llu "
                        "frame=%u flip=%llu\n",
                        (unsigned long long)elapsed, dialogue_stable,
                        (unsigned long long)dialogue_targets,
                        (unsigned long long)g_ld_stats.group_drop_ring,
                        g_ld_frames,
                        (unsigned long long)g_ld_flip_requested);
                    fflush(stderr);
                }
            }
            if (dialogue_armed && dialogue_captures < 16u) {
                const u32 capture = ++dialogue_captures;
                ld_parity_capture_surface(
                    parity_dir, "dialogue", capture, buffer_id, target);
                if (capture == 16u) {
                    dialogue_armed = 0;
                    yz_frontier_trace_dump(5u);
                    fprintf(
                        stderr,
                        "[parity-diag] DIALOGUE COMPLETE captures=16 "
                        "frame=%u flip=%llu\n",
                        g_ld_frames,
                        (unsigned long long)g_ld_flip_requested);
                    fflush(stderr);
                }
            }

            if (movement_phase == 4) {
                movement_frames++;
                if (movement_captures < 16u) {
                    const u32 capture = ++movement_captures;
                    ld_parity_capture_surface(
                        parity_dir, "movement", capture, buffer_id, target);
                    if (capture == 16u)
                        yz_frontier_trace_dump(6u);
                }
                if (movement_frames == 120u) {
                    yz_frontier_trace_dump(7u);
                    fprintf(
                        stderr,
                        "[parity-diag] MOVEMENT 120 COMPLETE "
                        "captures=16 frame=%u flip=%llu groups=%llu "
                        "drop_ring=%llu\n",
                        g_ld_frames,
                        (unsigned long long)g_ld_flip_requested,
                        (unsigned long long)g_ld_stats.groups_executed,
                        (unsigned long long)g_ld_stats.group_drop_ring);
                    fflush(stderr);
                }
            }

            observed_write[target] = surface->last_write_generation;
            observed_valid[target] = 1;
            prior_target = target;
            prior_get = g_ld_fifo_get;
            prior_put = g_ld_fifo_put;
        }
    }
    /* Capture the actual presented surface at acknowledged input boundaries.
     * This is independent of desktop visibility/focus and is inert unless the
     * promotion controller explicitly enables RSX_MOVEMENT_PROOF. */
    {
        static int movement_capture = -1;
        static char movement_dir[MAX_PATH * 2];
        if (movement_capture < 0) {
            const char* enabled = getenv("RSX_MOVEMENT_PROOF");
            const char* directory = getenv("RSX_VALIDATION_DIR");
            movement_capture = enabled && directory && *directory;
            if (movement_capture) {
                strncpy(movement_dir, directory,
                        sizeof(movement_dir) - 1u);
                movement_dir[sizeof(movement_dir) - 1u] = '\0';
            }
        }
        if (movement_capture) {
            const LONG phase = InterlockedCompareExchange(
                &g_yz_movement_proof_phase, 0, 0);
            static u64 next_probe_tick;
            static u32 probe_serial;
            static u64 probe_delay_ms;
            static u64 probe_interval_ms;
            static int probe_configured;

            if (!probe_configured) {
                const char* delay = getenv("RSX_MOVEMENT_PROOF_DELAY_MS");
                const char* interval = getenv("RSX_MOVEMENT_PROBE_INTERVAL_MS");
                probe_delay_ms = delay && *delay
                    ? _strtoui64(delay, NULL, 10) : 780000ull;
                probe_interval_ms = interval && *interval
                    ? _strtoui64(interval, NULL, 10) : 0ull;
                if (probe_interval_ms && probe_interval_ms < 30000ull)
                    probe_interval_ms = 30000ull;
                probe_configured = 1;
            }

            /* A time threshold alone cannot distinguish authored camera work
             * from playable Akiyama.  In visual-arm mode, retain occasional
             * renderer-surface probes while dialogue input continues.  The
             * proof is armed only after one of these images visibly shows the
             * gameplay HUD, avoiding timing-dependent false positives. */
            if (phase == 0 && probe_interval_ms && g_yz_auto_start_tick) {
                const u64 now = GetTickCount64();
                const u64 elapsed = now - g_yz_auto_start_tick;
                if (elapsed >= probe_delay_ms &&
                    (!next_probe_tick || now >= next_probe_tick)) {
                    char path[MAX_PATH * 2];
                    const u32 serial = ++probe_serial;
                    snprintf(path, sizeof(path),
                             "%s\\frontier_probe_%03u.ppm",
                             movement_dir, serial);
                    {
                        const u64 nonblack =
                            ld_dump_surface_ppm(path, &g.surfaces[target]);
                        fprintf(stderr,
                                "[movement-proof] frontier-probe serial=%u "
                                "elapsed_ms=%llu frame=%u nonblack=%s%llu "
                                "path=%s\n",
                                serial, (unsigned long long)elapsed,
                                g_ld_frames,
                                nonblack == UINT64_MAX ? "unknown:" : "",
                                (unsigned long long)(
                                    nonblack == UINT64_MAX ? 0 : nonblack),
                                path);
                        fflush(stderr);
                    }
                    next_probe_tick = now + probe_interval_ms;
                }
            }
            const char* name = NULL;
            LONG next_phase = phase;
            if (phase == 1) {
                name = "movement_before.ppm";
                next_phase = 2;
            } else if (phase == 3) {
                name = "camera_after.ppm";
                next_phase = 4;
            } else if (phase == 5) {
                name = "movement_after_60s.ppm";
                next_phase = 6;
            }
            if (name) {
                char path[MAX_PATH * 2];
                snprintf(path, sizeof(path), "%s\\%s", movement_dir, name);
                ld_movement_camera_snapshot(phase);
                {
                    const u64 nonblack =
                        ld_dump_surface_ppm(path, &g.surfaces[target]);
                    fprintf(stderr,
                            "[movement-proof] capture phase=%ld frame=%u "
                            "flip=%llu nonblack=%s%llu path=%s\n",
                            phase, g_ld_frames,
                            (unsigned long long)g_ld_flip_requested,
                            nonblack == UINT64_MAX ? "unknown:" : "",
                            (unsigned long long)(
                                nonblack == UINT64_MAX ? 0 : nonblack),
                            path);
                    fflush(stderr);
                }
                InterlockedCompareExchange(
                    &g_yz_movement_proof_phase, next_phase, phase);
                if (next_phase == 6) {
                    fprintf(stderr,
                            "[movement-proof] COMPLETE sustained_ms=60000 "
                            "camera_ms=5000 frame=%u\n", g_ld_frames);
                    fflush(stderr);
                }
            }
        }
    }
    /* Narrow validation readback for the four user-visible orphanage shots.
     * This captures the renderer's actual presented surface and therefore is
     * independent of desktop focus, window visibility, and screenshot APIs.
     * It is inert unless the validation controller supplies both variables. */
    {
        static int validation_capture = -1;
        static u32 validation_dumped = 0;
        static u64 validation_origin = UINT64_MAX;
        static char validation_dir[MAX_PATH * 2];
        if (validation_capture < 0) {
            const char* enabled = getenv("RSX_VALIDATION_CAPTURE");
            const char* directory = getenv("RSX_VALIDATION_DIR");
            validation_capture = enabled && directory && *directory;
            if (validation_capture) {
                strncpy(validation_dir, directory,
                        sizeof(validation_dir) - 1u);
                validation_dir[sizeof(validation_dir) - 1u] = '\0';
            }
        }
        /* The a010 file-lifetime gate is the authoritative scene boundary in
         * clean production builds.  The older exact-mesh gate was useful for
         * a geometry experiment but is not guaranteed to occur in every
         * authored camera block, so using it for validation can miss the
         * entire scene even while valid draws are being presented. */
        if (validation_capture && validation_origin == UINT64_MAX &&
            InterlockedCompareExchange(&g_yz_a010_root_active, 0, 0) != 0) {
            validation_origin = g_ld_flip_requested;
            fprintf(stderr,
                    "[validation-capture] a010 origin flip=%llu frame=%u\n",
                    (unsigned long long)validation_origin, g_ld_frames);
        }
        if (validation_capture && validation_origin != UINT64_MAX) {
            /* Scene-open itself is a 43-flip black/fade interval.  Preserve
             * the original four-shot spacing after that measured lead-in so
             * the promotion run lands on the outdoor ground, children,
             * over-shoulder hair, and face close-up rather than the fade,
             * sink, and pre-pickup phone shots. */
            static const u64 offsets[] = {43, 128, 302, 316};
            static const char* labels[] = {
                "background-ground", "children-eyes", "hair", "face-eyes"
            };
            for (u32 i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
                const u64 requested = validation_origin + offsets[i];
                if (!(validation_dumped & (1u << i)) &&
                    g_ld_flip_requested >= requested) {
                    char path[MAX_PATH * 2];
                    snprintf(path, sizeof(path),
                             "%s\\orphanage_gpu_%s_flip_%llu.ppm",
                             validation_dir, labels[i],
                             (unsigned long long)requested);
                    const u64 nonblack =
                        ld_dump_surface_ppm(path, &g.surfaces[target]);
                    fprintf(stderr,
                            "[validation-capture] label=%s requested_flip=%llu "
                            "actual_flip=%llu frame=%u target=%u "
                            "nonblack=%s%llu path=%s\n",
                            labels[i], (unsigned long long)requested,
                            (unsigned long long)g_ld_flip_requested,
                            g_ld_frames, target,
                            nonblack == UINT64_MAX ? "unknown:" : "",
                            (unsigned long long)(
                                nonblack == UINT64_MAX ? 0 : nonblack),
                            path);
                    validation_dumped |= 1u << i;
                }
            }
        }
    }
#endif
    /* First 32 frames verbatim, then every 32nd: keeps the log bounded while
     * making the TRUE frame count measurable from the log. (The old hard cap
     * at 32 made "stalls at frame ~32" unfalsifiable from the .err alone.) */
    { static double lim = -1.0; static LARGE_INTEGER qf, t_ant; static long long h0, p0, w0; static u32 hn0, pn0, tx0;
      static long fl0[LD_FLUSH_REASON_COUNT];
      if (lim < 0.0) { const char* e = getenv("GOWA_ENGASGO"); lim = e ? atof(e) : 0.0; QueryPerformanceFrequency(&qf); }
      if (lim > 0.0) {
          LARGE_INTEGER t; QueryPerformanceCounter(&t);
          const double ms = t_ant.QuadPart ? (t.QuadPart - t_ant.QuadPart) * 1000.0 / qf.QuadPart : 0.0;
          if (ms >= lim) {
              long fl = 0; for (int q = 0; q < LD_FLUSH_REASON_COUNT; q++) fl += g_ld_flush_n[q] - fl0[q];
              fprintf(stderr, "[engasgo] quadro %u: %.0f ms | hlsl %u (%.0f ms) pso %u (%.0f ms) | flush %ld, espera da GPU %.0f ms | texturas %+d (%u no cache)\n",
                      g_ld_frames, ms, g_eng_hlsl_n - hn0, (g_eng_hlsl_qpc - h0) * 1000.0 / qf.QuadPart,
                      g_eng_pso_n - pn0, (g_eng_pso_qpc - p0) * 1000.0 / qf.QuadPart, fl,
                      (g_ld_flush_wait_qpc - w0) * 1000.0 / qf.QuadPart, (int)(g.n_textures - tx0), g.n_textures);
          }
          t_ant = t; h0 = g_eng_hlsl_qpc; p0 = g_eng_pso_qpc; w0 = g_ld_flush_wait_qpc;
          hn0 = g_eng_hlsl_n; pn0 = g_eng_pso_n; tx0 = g.n_textures;
          for (int q = 0; q < LD_FLUSH_REASON_COUNT; q++) fl0[q] = g_ld_flush_n[q];
      } }
    if (g_ld_frames <= 32 || (g_ld_frames & 31) == 0)
        fprintf(stderr,
                "[live-draw] frame %u packets[seen=%llu queued=%llu movie=%llu qfull=%llu] "
                "groups[seen=%llu exec=%llu empty=%llu drop{fetch=%llu degen=%llu prim=%llu "
                "alloc=%llu pso=%llu ring=%llu surface=%llu}] clears[guest=%llu badsurf=%llu implicitZ=%llu] "
                "textures[cached=%u/%u full=%llu decodefail=%llu depthSRV=%llu rejectZ=%llu] "
                "vtex[cached=%u enabled=%llu binds=%llu uploads=%llu refresh=%llu "
                "unsupported=%llu missingTXL=%llu] divider=%llu "
                "balance[p=%s g=%s] binds[white=%llu real=%llu surf=%llu] (cumulative)\n",
                g_ld_frames,
                g_ld_stats.packets_seen, g_ld_stats.packets_queued,
                g_ld_stats.packets_movie, g_ld_stats.packets_queue_full,
                g_ld_stats.groups_seen, g_ld_stats.groups_executed,
                g_ld_stats.groups_empty, g_ld_stats.group_drop_fetch,
                g_ld_stats.group_drop_degenerate, g_ld_stats.group_drop_primitive,
                g_ld_stats.group_drop_alloc, g_ld_stats.group_drop_pso,
                g_ld_stats.group_drop_ring, g_ld_stats.group_drop_surface,
                g_ld_stats.clears, g_ld_stats.clear_drop_surface,
                g_ld_stats.implicit_depth_clears,
                ld_tex_vivas(), ld_tex_cap(),
                (unsigned long long)g_ld_texture_cache_full,
                (unsigned long long)g_ld_texture_decode_fail,
                (unsigned long long)g_ld_zdepth_srv_binds,
                (unsigned long long)g_ld_zdepth_srv_reject_no_write,
                g.n_vtex,
                (unsigned long long)g_ld_vtex_enabled,
                (unsigned long long)g_ld_vtex_binds,
                (unsigned long long)g_ld_vtex_uploads,
                (unsigned long long)g_ld_vtex_refreshes,
                (unsigned long long)g_ld_vtex_unsupported,
                (unsigned long long)g_ld_vtex_missing_for_txl,
                (unsigned long long)g_ld_divider_fetches,
                g_ld_stats.packets_seen == g_ld_stats.packets_queued +
                    g_ld_stats.packets_movie + g_ld_stats.packets_queue_full ? "ok" : "BAD",
                g_ld_stats.groups_seen == ld_groups_accounted() ? "ok" : "BAD",
                g_ld_bind_white, g_ld_bind_real, g_ld_bind_surf);
    /* [fps] heartbeat: direct frame-rate logging, one line per ~5s wall.
     * Exists because pace repeatedly had to be inferred from frame-counter
     * arithmetic and log ordering, and two such inferences were wrong in one
     * night (s42). Volume-bounded per LESSONS #6c. */
    { static ULONGLONG fps_t0 = 0; static u32 fps_f0 = 0;
      ULONGLONG now = GetTickCount64();
      if (fps_t0 == 0) { fps_t0 = now; fps_f0 = g_ld_frames; }
      else if (now - fps_t0 >= 5000) {
          /* PS1_PC=1: the R3000 state block (0x76C080) on the same heartbeat.
           *
           * READ +0x124 (instructions retired), NOT +0x108. The interpreter
           * loads PC from +0x108 on entry (lwz r26, 0x108(r23) at 0x001066CC)
           * and writes it back only in its epilogue (stw r26, 0x108(r23) at
           * 0x00106964) -- and it has not returned, because it is entered once
           * and loops internally. So +0x108 is stale from boot: 280,000 reads
           * across 14 samples gave 0xBFC00000 every time while +0x124 climbed
           * past a billion. Reading that as "stuck at the reset vector" would
           * be wrong twice over -- the epilogue does `ori r26, r26, 0x80`
           * first, so a real exit there could not even write 0xBFC00000.
           *
           * +0x124 is updated from inside the loop and is the honest progress
           * signal. Finding the LIVE pc needs the interpreter's register, not
           * this field. */
          { static int pc_on = -1;
            if (pc_on < 0) pc_on = getenv("PS1_PC") ? 1 : 0;
            if (pc_on) {
                extern uint32_t vm_read32(uint64_t);
                /* One read of +0x108 cannot tell a reset loop from a field that
                 * is simply not live between interpreter entries -- both look
                 * like a constant 0xBFC00000. So sample it hard: 20000 reads,
                 * and report how many DISTINCT values appear plus the lowest
                 * and highest. A live PC sweeps; a dead field does not. */
                uint32_t lo = 0xFFFFFFFFu, hi = 0, prev = 0, chg = 0;
                for (int k = 0; k < 20000; k++) {
                    uint32_t v = vm_read32(0x76C080u + 0x108u);
                    if (v < lo) lo = v;
                    if (v > hi) hi = v;
                    if (k && v != prev) chg++;
                    prev = v;
                }
                /* The PS1 GPU command ring. func_0010F658 pushes one 0x100-byte
                 * packet per GP0 batch (header 3 at +0, payload from +0x10) and
                 * advances a write offset; the four GPU SPUs consume it. If this
                 * offset never moves, the R3000 is not producing draw commands
                 * at all, and no amount of SPU-side work will help.
                 *
                 *   ring_off_ptr = *(TOC-0x794C)   (TOC = 0x1C3D30)
                 *   ring_base    = *(TOC-0x7918)
                 * from func_0010F658's first two instructions. */
                uint32_t roff_p = vm_read32(0x1BC3E4u);
                uint32_t rbase  = vm_read32(0x1BC418u);
                uint32_t roff   = roff_p ? vm_read32(roff_p) : 0xFFFFFFFFu;
                /* Each GPU SPU polls its OWN local store at 0x15010 for that
                 * offset (it reports that address to the PPU through its
                 * outbound mailbox during init), and a raw SPU's local store is
                 * aliased into guest memory at 0xE0000000 + n*0x100000 -- so
                 * these four reads see exactly what the SPU sees. If they match
                 * `off`, the kick landed and the SPUs are simply not reacting;
                 * if they do not, the kick is being lost. Opposite problems. */
                /* +0x15010 is the offset the PPU publishes to this SPU;
                 * +0x15020 is how far the SPU has actually CONSUMED (its work
                 * loop at 0x3158 advances it by 0x100 per packet under the same
                 * 0x007FFFFF mask the PPU uses, and stores it back). The pair
                 * separates "the SPUs are not being told" from "the SPUs are
                 * told and not working" from "both are fine and the problem is
                 * downstream in compositing". */
                uint32_t ls[4], cons[4];
                for (int n = 0; n < 4; n++) {
                    uint32_t w = 0xE0000000u + (uint32_t)n * 0x100000u;
                    ls[n]   = vm_read32(w + 0x15010u);
                    cons[n] = vm_read32(w + 0x15020u);
                }
                /* The scheduler's own state. func_00105FA8 hands the
                 * interpreter a budget at +0x120; when that is permanently zero
                 * the interpreter spins without executing. These are the fields
                 * it works from, so a frozen run says WHICH state it froze in
                 * rather than just that it froze. */
                fprintf(stderr, "[ps1] pc=0x%08X(chg=%u) exited=%u total=%u"
                                " cost=%u budget=%d reason=%d evhead=0x%08X"
                                " ring[off=0x%08X] pub[%06X %06X %06X %06X]"
                                " done[%06X %06X %06X %06X]\n",

                        lo, chg,
                        vm_read32(0x76C080u + 0x110u),
                        vm_read32(0x76C080u + 0x124u),
                        vm_read32(0x76C080u + 0x114u),
                        (int32_t)vm_read32(0x76C080u + 0x120u),
                        (int32_t)vm_read32(0x76C080u + 0x138u),
                        vm_read32(0x76C080u + 0x540u),
                        roff, ls[0], ls[1], ls[2], ls[3],
                        cons[0], cons[1], cons[2], cons[3]);
                /* Does PS1 VRAM actually have pixels in it?
                 *
                 * The texture the PS3 side binds for the PS1 framebuffer is
                 * location 1 (RSX local), offset 0x400000, 1024x512 fmt 0xE2 --
                 * exactly PS1 VRAM. Everything upstream of here is now known
                 * good (the four GPU SPUs consume every packet the R3000
                 * produces), so this is the first place the picture can go
                 * missing: either the SPUs are not writing pixels, or they are
                 * and our compositing drops them. Count non-zero words rather
                 * than guessing from a window capture -- PrintWindow on a D3D12
                 * swapchain cannot tell "black" from "capture failed". */
                { const u8* fb = guest_ptr(1u, 0x400000u, 1024u * 512u * 2u);
                  /* The guest EA of PS1 VRAM, once. Watching 0xC0400000 --
                   * derived from cellGcmSys's localAddress -- caught no writes
                   * at all from either the PPU store path or SPU DMA, which is
                   * either a real finding or a wrong address. This says which,
                   * instead of assuming. */
                  { static int shown = 0;
                    extern uint8_t* vm_base;
                    if (!shown && fb && vm_base) { shown = 1;
                        fprintf(stderr, "[ps1] VRAM guest EA = 0x%08X\n",
                                (u32)(size_t)(fb - vm_base)); } }
                  if (!fb) fprintf(stderr, "[ps1] vram: not mapped\n");
                  else {
                      u32 nz = 0, first = 0xFFFFFFFFu, n = 1024u * 512u / 2u;
                      const u32* w32 = (const u32*)fb;
                      for (u32 i = 0; i < n; i++)
                          if (w32[i]) { nz++; if (first == 0xFFFFFFFFu) first = i * 4u; }
                      fprintf(stderr, "[ps1] vram nonzero=%u/%u first=+0x%X\n",
                              nz, n, first == 0xFFFFFFFFu ? 0u : first);
                      /* PS1_EVENTS=1: the three kernel events the BIOS polls.
                       *
                       * The BIOS parks at 0xBFC53840 calling TestEvent
                       * (B-table 0x0B, trampoline 0xBFC58B60) on three
                       * descriptors loaded from PS1 kernel RAM 0x0000B21C,
                       * 0x0000B224 and 0x0000B228. Reading their event CLASS
                       * says what it is waiting for; the PS1 CD class is
                       * 0xF0000003.
                       *
                       * PS1 RAM base is *(TOC-0x79D4) = 0x00770780, and the
                       * interpreter reads PS1 memory with lwbrx -- PS1 RAM is
                       * stored LITTLE-endian in guest memory, so every read
                       * here has to be byte-swapped. Reading it big-endian
                       * would produce plausible-looking nonsense. */
                      { static int ev = -1; static int ev_done = 0;
                        if (ev < 0) ev = getenv("PS1_EVENTS") ? 1 : 0;
                        static int ev_beats = 0;
                        if (ev && !ev_done && ++ev_beats >= 3) {
                            ev_done = 1;
                            const u32 ram = vm_read32(0x001BC35Cu);
                            #define PS1LE(a) __builtin_bswap32(vm_read32((ram) + (u32)(a)))
                            const u32 tot_evcb = PS1LE(0x0120);
                            const u32 tot_size = PS1LE(0x0124);
                            fprintf(stderr, "[ps1ev] ram=0x%08X  EvCB tbl=0x%08X size=%u\n",
                                    ram, tot_evcb, tot_size);
                            /* All FIVE slots CdInit fills, not just the three
                             * the poll loop reads: -0x4de8/-0x4de4/-0x4de0/
                             * -0x4ddc/-0x4dd8 = 0xB218..0xB228. All zero means
                             * CdInit never ran; a mix means it ran partway. */
                            static const u32 slot[5] = { 0xB218u, 0xB21Cu, 0xB220u,
                                                         0xB224u, 0xB228u };
                            for (int k = 0; k < 5; k++) {
                                const u32 h = PS1LE(slot[k]);
                                fprintf(stderr, "[ps1ev]   slot 0x%04X handle=0x%08X",
                                        slot[k], h);
                                if ((h >> 24) == 0xF1u && tot_evcb) {
                                    const u32 idx = h & 0xFFFFu;
                                    const u32 cb  = (tot_evcb & 0x1FFFFFu) + idx * 0x1Cu;
                                    fprintf(stderr, "  idx=%u class=0x%08X status=0x%08X"
                                                    " spec=0x%08X mode=0x%08X",
                                            idx, PS1LE(cb + 0), PS1LE(cb + 4),
                                            PS1LE(cb + 8), PS1LE(cb + 12));
                                }
                                fprintf(stderr, "\n");
                            }
                            /* Dump the whole EvCB table. The table pointer
                             * itself is valid (0xA000E028, 448 bytes = 16
                             * entries of 0x1C), so the PS1 kernel IS up and
                             * events CAN be opened. Whether any ARE distinguishes
                             * "CdInit specifically never ran" from "nothing ever
                             * opens an event", which are very different faults.
                             * EvCB layout: class +0, status +4, spec +8,
                             * mode +0xC, func +0x10. */
                            { const u32 tb = tot_evcb & 0x1FFFFFu;
                              const u32 n = tot_size / 0x1Cu;
                              u32 live = 0;
                              for (u32 i = 0; i < n && i < 32; i++) {
                                  const u32 cb = tb + i * 0x1Cu;
                                  const u32 cls = PS1LE(cb + 0);
                                  const u32 st  = PS1LE(cb + 4);
                                  if (!cls && !st) continue;
                                  live++;
                                  fprintf(stderr, "[ps1ev]   EvCB[%2u] class=0x%08X"
                                                  " status=0x%08X spec=0x%08X"
                                                  " mode=0x%08X func=0x%08X\n",
                                          i, cls, st, PS1LE(cb + 8),
                                          PS1LE(cb + 12), PS1LE(cb + 16));
                              }
                              fprintf(stderr, "[ps1ev]   %u of %u EvCB entries in use\n",
                                      live, n); }
                            #undef PS1LE
                        } }
                      /* PS1_GP0HIST=1: which GP0 PRIMITIVES the ring actually
                       * carries, as a histogram over the last 64 packets.
                       *
                       * "The menu draws but nothing 3D does" is a statement
                       * about primitive TYPES, and no counter here reported
                       * those -- packets, groups and VRAM pixels all count the
                       * same whether the PS1 is drawing a textured rectangle or
                       * a gouraud triangle. GP0 opcodes: 0x20-0x3F polygons,
                       * 0x40-0x5F lines, 0x60-0x7F rectangles, 0x80+ transfers,
                       * 0x00-0x1F misc/state. If the 3D is missing because the
                       * polygons never reach the ring, that is upstream in the
                       * R3000; if they reach it, it is the rasteriser or the
                       * composite. Opposite halves of the pipeline. */
                      { static int gh = -1;
                        if (gh < 0) gh = getenv("PS1_GP0HIST") ? 1 : 0;
                        if (gh && rbase && roff >= 0x4000u) {
                            unsigned cls[8] = {0}; unsigned pk_types[8] = {0};
                            for (u32 k = 1; k <= 64u; k++) {
                                const u32 pk = rbase + ((roff - k * 0x100u) & 0x007FFFFFu);
                                pk_types[vm_read32(pk) & 7u]++;
                                for (u32 q = 0; q < 60u; q++) {
                                    const u32 w = vm_read32(pk + 0x10u + q * 4u);
                                    if (!w) continue;
                                    cls[(w >> 24) >> 5]++;   /* opcode/0x20 */
                                }
                            }
                            /* PS1 VRAM comes out solid 0x83E0 -- R=0, G=31,
                             * B=0, mask bit set -- across the full 640-pixel
                             * display width, fully written (207 DMA writes per
                             * 64-byte block). Something paints the screen
                             * green. GP0 0x02 is Fill Rectangle and its colour
                             * word is right here in the ring, so print the
                             * fills: if the guest asks for green, the bug is
                             * upstream in the R3000; if it asks for something
                             * else, our SPU rasteriser is packing the colour
                             * wrong. Opposite halves of the pipeline. */
                            for (u32 k = 1; k <= 64u; k++) {
                                const u32 pk = rbase + ((roff - k * 0x100u) & 0x007FFFFFu);
                                for (u32 q = 0; q < 58u; q++) {
                                    const u32 w = vm_read32(pk + 0x10u + q * 4u);
                                    if ((w >> 24) != 0x02u) continue;
                                    fprintf(stderr, "[gp0fill] pkt+0x%X"
                                                    " cmd=%08X xy=%08X wh=%08X\n",
                                            0x10u + q * 4u, w,
                                            vm_read32(pk + 0x14u + q * 4u),
                                            vm_read32(pk + 0x18u + q * 4u));
                                    break;
                                }
                            }
                            fprintf(stderr, "[gp0hist] last 64 pkts types[");
                            for (int q = 0; q < 8; q++) fprintf(stderr, "%u ", pk_types[q]);
                            fprintf(stderr, "] words: misc=%u POLY=%u line=%u"
                                            " rect=%u xfer=%u %u %u %u\n",
                                    cls[0], cls[1], cls[2], cls[3],
                                    cls[4], cls[5], cls[6], cls[7]);
                        } }
                      /* PS1_RINGDUMP=1: the last few GP0 command packets.
                       *
                       * func_0010F658 writes one 0x100-byte packet per batch:
                       * a type word at +0 (it writes 3 in one variant and 2 in
                       * another) and the payload from +0x10. The four GPU SPUs
                       * consume every packet and never DMA a pixel to VRAM, so
                       * the question is what these packets actually contain --
                       * PS1 GP0 drawing commands, or only state and sync. */
                      { static int rd2 = -1; static int shown = 0;
                        if (rd2 < 0) rd2 = getenv("PS1_RINGDUMP") ? 1 : 0;
                        if (rd2 && shown < 3 && rbase && roff >= 0x300u) {
                            shown++;
                            for (int k = 3; k >= 1; k--) {
                                const u32 pk = rbase + ((roff - (u32)k * 0x100u) & 0x007FFFFFu);
                                fprintf(stderr, "[ring] pkt@0x%08X type=%u:", pk,
                                        vm_read32(pk));
                                for (u32 q = 0; q < 12; q++)
                                    fprintf(stderr, " %08X", vm_read32(pk + 0x10u + q * 4u));
                                fprintf(stderr, "\n");
                            }
                        } }
                      { static u32 rdy = 0;
                        if (!rdy) { const char* re = getenv("PS1_VRAM_READY");
                                    rdy = re ? (u32)strtoul(re, 0, 0) : 20000u; }
                        if (nz >= rdy) g_ld_ps1_vram_ready = 1; }
                      /* PS1_FBDUMP=<path>: write PS1 VRAM out as a PPM.
                       *
                       * This exists because "the window is black" was an
                       * ASSUMPTION for a long stretch of this port: PrintWindow
                       * on a D3D12 swapchain returns black whether the page is
                       * black or the capture simply failed, so every screenshot
                       * was unfalsifiable. This reads the pixels the PS1 itself
                       * produced, straight out of guest memory, with no D3D and
                       * no window involved. 1024x512, 16-bit 1-5-5-5, pitch
                       * 2048 -- the format the [tex-refresh] line reports. */
                      /* ONE dump, taken once the framebuffer has at least
                       * PS1_FBDUMP_MIN non-zero words (default 20000).
                       *
                       * It used to write a 1.5 MB PPM on every heartbeat, and
                       * that instrumentation was itself most of the
                       * "nondeterminism" this port was chasing: runs carrying
                       * the periodic dump reached 0-7,127 non-zero words, while
                       * clean runs reach 75,527 every time. Measuring the thing
                       * was changing it. One dump, gated on content. */
                      { const char* dp = getenv("PS1_FBDUMP");
                        static int dumped = 0;
                        u32 want = 20000;
                        { const char* mn = getenv("PS1_FBDUMP_MIN");
                          if (mn) want = (u32)strtoul(mn, 0, 0); }
                        if (dp && !dumped && nz >= want) {
                            dumped = 1;
                            char path[512];
                            snprintf(path, sizeof path, "%s", dp);
                            FILE* f = fopen(path, "wb");
                            if (f) {
                                fprintf(f, "P6\n1024 512\n255\n");
                                for (u32 y = 0; y < 512; y++) {
                                    const u8* row = fb + (size_t)y * 2048u;
                                    for (u32 x = 0; x < 1024; x++) {
                                        u16 p = (u16)(row[x * 2] | (row[x * 2 + 1] << 8));
                                        u8 rgb[3];
                                        rgb[0] = (u8)(((p >> 10) & 0x1F) * 255 / 31);
                                        rgb[1] = (u8)(((p >> 5)  & 0x1F) * 255 / 31);
                                        rgb[2] = (u8)(( p        & 0x1F) * 255 / 31);
                                        fwrite(rgb, 1, 3, f);
                                    }
                                }
                                fclose(f);
                            }
                        } }
                  } }
                (void)hi; (void)rbase;
            } }
          { static int fs = -1; if (fs < 0) fs = getenv("GOWA_FLUSH_STATS") ? 1 : 0;
            if (fs) { LARGE_INTEGER qf; QueryPerformanceFrequency(&qf);
                fprintf(stderr, "[flush] present=%ld ref=%ld vtx=%ld vcb=%ld retire=%ld readback=%ld pcb=%ld desc=%ld movie=%ld espera_gpu=%.0f ms Present()=%.0f ms\n",
                        g_ld_flush_n[LD_FLUSH_PRESENT], g_ld_flush_n[LD_FLUSH_GUEST_REFERENCE], g_ld_flush_n[LD_FLUSH_VERTEX_RING],
                        g_ld_flush_n[LD_FLUSH_VERTEX_CONSTANT_RING], g_ld_flush_n[LD_FLUSH_RETIRE_QUEUE], g_ld_flush_n[LD_FLUSH_READBACK],
                        g_ld_flush_n[LD_FLUSH_PIXEL_CONSTANT_RING], g_ld_flush_n[LD_FLUSH_DESCRIPTOR_RING],
                        g_ld_flush_n[LD_FLUSH_MOVIE] + g_ld_flush_n[LD_FLUSH_MOVIE_PRESENT],
                        g_ld_flush_wait_qpc * 1000.0 / (double)qf.QuadPart, g_ld_present_qpc * 1000.0 / (double)qf.QuadPart);
                for (int q = 0; q < LD_FLUSH_REASON_COUNT; q++) g_ld_flush_n[q] = 0;
                g_ld_flush_wait_qpc = 0; g_ld_present_qpc = 0; } }
          /* GOWA: sink=N -- quantas dessas apresentações vieram de um método
           * de flip no fluxo da RSX (sink_flip); o resto veio do flip pendente
           * do cellGcm (present_guest_frame) */
          /* jogo=N: flips que o próprio jogo pediu por segundo (quadros
           * novos), que é o FPS de verdade; o primeiro número conta
           * apresentações. scripts/fps_fase.ps1 usa o jogo= quando existe. */
          { static u32 s_sink0, s_flip0;
            extern u32 cellGcm_flip_request_count(void);
            const u32 sink_now = (u32)g_ld_flip_requested, flip_now = cellGcm_flip_request_count();
            /* cpu=N: CPUs the process kept busy in the interval; cpuq=N: process CPU ms per
             * game frame (26/10). On a saturated PC (the 4-core notebook) the FPS follows
             * whatever CPU the background leaves; the CPU per frame is the work itself. */
            static ULONGLONG s_cpu0;
            ULONGLONG cpu_now = 0;
            { FILETIME c_, x_, k_, u_;
              if (GetProcessTimes(GetCurrentProcess(), &c_, &x_, &k_, &u_))
                  cpu_now = (((ULONGLONG)k_.dwHighDateTime << 32) | k_.dwLowDateTime) +
                            (((ULONGLONG)u_.dwHighDateTime << 32) | u_.dwLowDateTime); }
            const double cpu_ms = s_cpu0 ? (double)(cpu_now - s_cpu0) / 1e4 : 0.0;
            fprintf(stderr, "[fps] %.1f (frames %u..%u over %.1fs) sink=%u jogo=%.1f cpu=%.2f cpuq=%.1f\n",
                    (g_ld_frames - fps_f0) * 1000.0 / (double)(now - fps_t0),
                    fps_f0, g_ld_frames, (now - fps_t0) / 1000.0, sink_now - s_sink0,
                    (flip_now - s_flip0) * 1000.0 / (double)(now - fps_t0),
                    cpu_ms / (double)(now - fps_t0), flip_now != s_flip0 ? cpu_ms / (double)(flip_now - s_flip0) : 0.0);
            s_cpu0 = cpu_now;
            ld_perf_amostra((flip_now - s_flip0) * 1000.0 / (double)(now - fps_t0), (now - fps_t0) / 1000.0);
            s_sink0 = sink_now; s_flip0 = flip_now; }
          fps_t0 = now; fps_f0 = g_ld_frames;
      } }
    /* RSX_DUMP_EVERY=N: also dump every Nth frame, not just the first 8 --
     * a title whose content starts after the boot clears is invisible otherwise. */
    static int ld_dump_every = -1;
    if (ld_dump_every < 0) { const char* e = getenv("RSX_DUMP_EVERY"); ld_dump_every = e ? atoi(e) : 0; }
    if (LD_DIAG_ENABLED("RSX_DUMP") &&
        (g_ld_frames <= 8 || (ld_dump_every > 0 && (g_ld_frames % (u32)ld_dump_every) == 0))) {
        /* Dump the presented color surface (RENDER_TARGET state -> safe). */
        const u32 cur = current_surface();
        if (cur != LD_INVALID_SURFACE) {
            char path[256];
            snprintf(path, sizeof(path), "scratch\\ld_frame_%02u.ppm", g_ld_frames);
            ld_dump_surface_ppm(path, &g.surfaces[cur]);
        }
    }
#if !defined(YZ_PERF_CLEAN)
    if (rsx_live_draw_a010_probe_active()) {
        const u32 elapsed = g_ld_frames - g_ld_a010_probe_start_frame;
        const int targeted = getenv("RSX_A010_SURFACE_DUMP") != NULL;
        u32 sample_every = 16u;
        const char *sample_every_env = getenv("RSX_A010_SURFACE_EVERY");
        if (sample_every_env) {
            const int parsed = atoi(sample_every_env);
            if (parsed > 0 && parsed <= 64)
                sample_every = (u32)parsed;
        }
        /* Loading consumes roughly 150 fast flips before a010. Sampling every
         * 16 frames spans the load and complete AUTH window while keeping the
         * synchronous readbacks from becoming the scene's clock. */
        const int world_ready =
            InterlockedCompareExchange(&g_ld_a010_world_ready, 0, 0) != 0;
        /* Targeted readbacks are intentionally expensive: each surface dump
         * submits and fences the open D3D12 list.  Do not perturb the producer
         * while it is still assembling the first a010 world command chain;
         * begin targeted capture only after a real scene mesh was observed. */
        const int capture_once =
            targeted && getenv("RSX_A010_CAPTURE_ONCE") != NULL;
        const int explicit_probe_fallback =
            capture_once && getenv("RSX_A010_PROBE") != NULL &&
            elapsed >= 192u;
        /* The exact healthy-mesh tuple is the strongest capture gate, but a
         * broken natural scene may never produce it. An explicitly requested
         * one-shot probe therefore gets one bounded fallback after 192 scene
         * flips, late enough to be past a010's loading/transition frames. */
        if ((!targeted || world_ready || explicit_probe_fallback) &&
            (elapsed % sample_every) == 0 &&
            (!targeted || elapsed <= 208u ||
             (capture_once && g_ld_a010_probe_sample == 0u))) {
            u64 mask = g_ld_a010_probe_touched;
            const u32 cur = current_surface();
            if (targeted) {
                mask = 0;
                for (u32 i = 0; i < g.n_surfaces && i < 64; i++) {
                    const u32 off = g.surfaces[i].offset;
                    /* Working a010 replay: 0xE40000 and 0x1800000 carry
                     * 1,481/1,727 scene draws; 0x2D10000 and 0x2710000 are
                     * the two depth-derived color passes; 0x1440000 is the
                     * final seven-draw composite. */
                    if (off == 0x00E40000u || off == 0x01800000u ||
                        off == 0x02D10000u || off == 0x02710000u ||
                        off == 0x01440000u ||
                        i == target)
                        mask |= 1ull << i;
                }
            } else if (cur < 64) {
                mask |= 1ull << cur;
            }
            fprintf(stderr,
                    "[a010-probe] SAMPLE n=%u live_frame=%u elapsed=%u "
                    "present=%u current=%u buffer=%u touched=0x%016llX "
                    "surfaces=%u targeted=%d\n",
                    g_ld_a010_probe_sample, g_ld_frames, elapsed, target, cur,
                    buffer_id, (unsigned long long)mask, g.n_surfaces,
                    targeted);
            for (u32 i = 0; i < g.n_surfaces && i < 64; i++) {
                if (!(mask & (1ull << i))) continue;
                char path[256];
                snprintf(path, sizeof(path),
                         "scratch\\a010_probe\\sample_%03u_frame_%05u_"
                         "surface_%02u_off_%08X.ppm",
                         g_ld_a010_probe_sample, g_ld_frames, i,
                         g.surfaces[i].offset);
                ld_dump_surface_ppm(path, &g.surfaces[i]);
                fprintf(stderr,
                        "[a010-probe] SURFACE sample=%u index=%u location=%u "
                        "offset=0x%08X size=%ux%u role=%s\n",
                        g_ld_a010_probe_sample, i, g.surfaces[i].location,
                        g.surfaces[i].offset, g.surfaces[i].w,
                        g.surfaces[i].h,
                        i == target ? "present" : "offscreen");
            }
            g_ld_a010_probe_touched = 0;
            g_ld_a010_probe_sample++;
            fflush(stderr);
            if (capture_once) {
                FILE* acceptance =
                    fopen("scratch\\a010_acceptance.txt", "a");
                if (acceptance) {
                    fprintf(acceptance,
                            "surface-capture sample=%u frame=%u mask=%016llX\n",
                            g_ld_a010_probe_sample - 1u, g_ld_frames,
                            (unsigned long long)mask);
                    fclose(acceptance);
                }
                InterlockedExchange(&g_ld_a010_probe_active, 0);
            }
        }
        if (elapsed >= 640 &&
            (!capture_once || g_ld_a010_probe_sample != 0u)) {
            fprintf(stderr,
                    "[a010-probe] END frame-cap live_frame=%u samples=%u\n",
                    g_ld_frames, g_ld_a010_probe_sample);
            fflush(stderr);
            InterlockedExchange(&g_ld_a010_probe_active, 0);
        }
    }
#endif

    /* new frame: reset per-frame ring cursors */
    g.vb_used = 0; g.ib_used = 0; g.cb_used = 0; g.ps_cb_used = 0;
    g.srv_ring_used = 0; g.smp_ring_used = 0;
#if !defined(YZ_PERF_CLEAN)
    g_ld_frame_vertex_constant_ring_recycles = 0;
#endif
    g.depth_cleared = 0;
    /* Per-zeta resources model persistent RSX memory.  Do not mark them
     * uncleared at a host-frame boundary: the implicit-clear branch would
     * erase depth rendered in an earlier frame and demote had_write before a
     * later pass can sample that zeta.  Only an actual guest depth clear may
     * clear/demote a tracked zeta (sink_clear does that above). */
}

void rsx_live_draw_shutdown(void)
{
    if (!g.ready) return;
    ld_vertex_diag_emit("shutdown", 1);
    ld_present_measure_dump();
    /* let the GPU drain, then release. (Best-effort; process teardown also
     * reclaims.) */
    ld_flush(LD_FLUSH_SHUTDOWN);
#if defined(YZ_PERF_PROFILE)
    fprintf(stderr,
            "[rsx-perf-summary] frames=%u "
            "pso{look=%llu hit=%llu miss=%llu probes=%llu full=%llu "
            "cached=%u capacity=%u mem_kb=%.1f} "
            "reject{requests=%llu unique=%u "
            "world=%llu/%u character=%llu/%u "
            "ui=%llu/%u other=%llu/%u} "
            "hlsl{vs_lookup=%llu vs_hit=%llu vs_compile=%llu vs_unique=%u "
            "ps_lookup=%llu ps_hit=%llu ps_compile=%llu ps_unique=%u} "
            "time_ms{decompile=%.3f vs_compile=%.3f ps_compile=%.3f "
            "create_pso=%.3f flush=%.3f fence=%.3f draw_cpu=%.3f} "
            "vertex{path=%s used_avg=%.3f used_max=%u "
            "legacy_mb=%.3f actual_mb=%.3f fetch_pack_ms=%.3f} "
            "vb_sync{flushes=%llu flush_ms=%.3f wait_ms=%.3f} "
            "frame{count=%llu avg_ms=%.3f compile_free=%llu "
            "compile_free_avg_ms=%.3f} "
            "draw{input_v=%llu expanded_v=%llu vb_mb=%.3f} "
            "tex{cached=%u evictions=%llu upload_mb=%.3f "
            "decode=%llu decode_ms=%.3f} "
            "high{upload_mb=%.3f vb_mb=%.3f retired=%u}\n",
            g_ld_frames,
            (unsigned long long)g_ld_profile.total.pso_lookups,
            (unsigned long long)g_ld_profile.total.pso_hits,
            (unsigned long long)g_ld_profile.total.pso_misses,
            (unsigned long long)g_ld_profile.total.pso_probes,
            (unsigned long long)g_ld_profile.total.pso_full,
            g.n_psos,
            MAX_PSOS,
            (double)sizeof(g.psos) / 1024.0,
            (unsigned long long)g_ld_profile.total.pso_full,
            g_ld_profile.n_rejected_pso_keys,
            (unsigned long long)
                g_ld_profile.rejected_pso_requests[LD_REJECT_WORLD],
            g_ld_profile.rejected_pso_unique[LD_REJECT_WORLD],
            (unsigned long long)
                g_ld_profile.rejected_pso_requests[LD_REJECT_CHARACTER],
            g_ld_profile.rejected_pso_unique[LD_REJECT_CHARACTER],
            (unsigned long long)
                g_ld_profile.rejected_pso_requests[LD_REJECT_UI],
            g_ld_profile.rejected_pso_unique[LD_REJECT_UI],
            (unsigned long long)
                g_ld_profile.rejected_pso_requests[LD_REJECT_OTHER],
            g_ld_profile.rejected_pso_unique[LD_REJECT_OTHER],
            (unsigned long long)g_ld_profile.total.vs_blob_lookups,
            (unsigned long long)g_ld_profile.total.vs_blob_hits,
            (unsigned long long)g_ld_profile.total.vs_compile_calls,
            g_ld_profile.n_vs_hashes,
            (unsigned long long)g_ld_profile.total.ps_blob_lookups,
            (unsigned long long)g_ld_profile.total.ps_blob_hits,
            (unsigned long long)g_ld_profile.total.ps_compile_calls,
            g_ld_profile.n_ps_hashes,
            ld_profile_ticks_ms(g_ld_profile.total.decompile_qpc),
            ld_profile_ticks_ms(g_ld_profile.total.vs_compile_qpc),
            ld_profile_ticks_ms(g_ld_profile.total.ps_compile_qpc),
            ld_profile_ticks_ms(g_ld_profile.total.create_pso_qpc),
            ld_profile_ticks_ms(g_ld_profile.total.flush_qpc),
            ld_profile_ticks_ms(g_ld_profile.total.fence_wait_qpc),
            ld_profile_ticks_ms(g_ld_profile.total.sink_end_qpc),
            ld_vertex_mode_name(),
            g_ld_profile.total.used_attribute_draws
                ? (double)g_ld_profile.total.used_attribute_sum /
                      (double)g_ld_profile.total.used_attribute_draws
                : 0.0,
            g_ld_profile.total_used_attribute_max,
            (double)g_ld_profile.total.legacy_vertex_upload_bytes /
                (1024.0 * 1024.0),
            (double)g_ld_profile.total.vertex_upload_bytes /
                (1024.0 * 1024.0),
            ld_profile_ticks_ms(
                g_ld_profile.total.vertex_fetch_pack_qpc),
            (unsigned long long)
                g_ld_profile.total.flush_reason[LD_FLUSH_VERTEX_RING],
            ld_profile_ticks_ms(
                g_ld_profile.total
                    .flush_reason_qpc[LD_FLUSH_VERTEX_RING]),
            ld_profile_ticks_ms(
                g_ld_profile.total
                    .fence_reason_qpc[LD_FLUSH_VERTEX_RING]),
            (unsigned long long)g_ld_profile.measured_frames,
            g_ld_profile.measured_frames
                ? g_ld_profile.total_frame_ms /
                      (double)g_ld_profile.measured_frames
                : 0.0,
            (unsigned long long)g_ld_profile.compile_free_frames,
            g_ld_profile.compile_free_frames
                ? g_ld_profile.compile_free_frame_ms /
                      (double)g_ld_profile.compile_free_frames
                : 0.0,
            (unsigned long long)g_ld_profile.total.input_vertices,
            (unsigned long long)g_ld_profile.total.expanded_vertices,
            (double)g_ld_profile.total.vertex_upload_bytes /
                (1024.0 * 1024.0),
            g.n_textures,
            (unsigned long long)g_ld_texture_cache_evictions,
            (double)g_ld_profile.total.texture_upload_bytes /
                (1024.0 * 1024.0),
            (unsigned long long)
                g_ld_profile.total.texture_decode_calls,
            ld_profile_ticks_ms(
                g_ld_profile.total.texture_decode_qpc),
            (double)g_ld_profile.total_upload_high / (1024.0 * 1024.0),
            (double)g_ld_profile.total_vb_high / (1024.0 * 1024.0),
            g_ld_profile.total_retired_high);
    fprintf(stderr,
            "[shader-cache-summary] "
            "vs{count=%u capacity=%u lookups=%llu hits=%llu "
            "misses=%llu inserts=%llu full_rejects=%llu "
            "compile_calls=%llu lookup_ms=%.3f compile_ms=%.3f "
            "post2048_distinct=%llu post2048_repeats=%llu "
            "source_bytes=%llu blob_bytes=%llu} "
            "ps{count=%u capacity=%u lookups=%llu hits=%llu "
            "misses=%llu inserts=%llu full_rejects=%llu "
            "compile_calls=%llu lookup_ms=%.3f compile_ms=%.3f "
            "post2048_distinct=%llu post2048_repeats=%llu "
            "source_bytes=%llu blob_bytes=%llu}\n",
            g.vs_blobs.count, MAX_SHADER_BLOBS,
            (unsigned long long)g_ld_profile.total.vs_blob_lookups,
            (unsigned long long)g_ld_profile.total.vs_blob_hits,
            (unsigned long long)g_ld_profile.total.vs_blob_misses,
            (unsigned long long)g_ld_profile.total.vs_blob_inserts,
            (unsigned long long)g_ld_profile.total.vs_blob_full_rejects,
            (unsigned long long)g_ld_profile.total.vs_compile_calls,
            ld_profile_ticks_ms(g_ld_profile.total.vs_blob_lookup_qpc),
            ld_profile_ticks_ms(g_ld_profile.total.vs_compile_qpc),
            (unsigned long long)
                g_ld_profile.total.vs_post_boundary_distinct,
            (unsigned long long)
                g_ld_profile.total.vs_post_boundary_repeats,
            (unsigned long long)g.vs_blobs.retained_source_bytes,
            (unsigned long long)g.vs_blobs.retained_blob_bytes,
            g.ps_blobs.count, MAX_SHADER_BLOBS,
            (unsigned long long)g_ld_profile.total.ps_blob_lookups,
            (unsigned long long)g_ld_profile.total.ps_blob_hits,
            (unsigned long long)g_ld_profile.total.ps_blob_misses,
            (unsigned long long)g_ld_profile.total.ps_blob_inserts,
            (unsigned long long)g_ld_profile.total.ps_blob_full_rejects,
            (unsigned long long)g_ld_profile.total.ps_compile_calls,
            ld_profile_ticks_ms(g_ld_profile.total.ps_blob_lookup_qpc),
            ld_profile_ticks_ms(g_ld_profile.total.ps_compile_qpc),
            (unsigned long long)
                g_ld_profile.total.ps_post_boundary_distinct,
            (unsigned long long)
                g_ld_profile.total.ps_post_boundary_repeats,
            (unsigned long long)g.ps_blobs.retained_source_bytes,
            (unsigned long long)g.ps_blobs.retained_blob_bytes);
    fprintf(
        stderr,
        "[fp-constant-summary] mode=%s "
        "variants{exact_source=%u constant_specialized=%u canonical=%u "
        "collapsed_constants=%u collapsed_alpha=%u "
        "tracking_overflow=%llu} "
        "cb{allocations=%llu upload_bytes=%llu upload_ms=%.3f "
        "ring_capacity=%u capacity_failures=%llu "
        "flushes=%llu flush_ms=%.3f wait_ms=%.3f}\n",
        ld_fp_constant_mode_name(),
        g_ld_profile.n_ps_exact_source_hashes,
        g_ld_profile.n_ps_constant_specialized_hashes,
        g_ld_profile.n_ps_canonical_hashes,
        g_ld_profile.n_ps_constant_specialized_hashes >=
                g_ld_profile.n_ps_canonical_hashes
            ? g_ld_profile.n_ps_constant_specialized_hashes -
                  g_ld_profile.n_ps_canonical_hashes
            : 0,
        g_ld_profile.n_ps_exact_source_hashes >=
                g_ld_profile.n_ps_constant_specialized_hashes
            ? g_ld_profile.n_ps_exact_source_hashes -
                  g_ld_profile.n_ps_constant_specialized_hashes
            : 0,
        (unsigned long long)
            g_ld_profile.ps_variant_tracking_overflow,
        (unsigned long long)
            g_ld_profile.total.ps_constant_allocations,
        (unsigned long long)
            g_ld_profile.total.ps_constant_upload_bytes,
        ld_profile_ticks_ms(
            g_ld_profile.total.ps_constant_upload_qpc),
        PS_CB_RING_BYTES,
        (unsigned long long)
            g_ld_profile.total.ps_constant_capacity_failures,
        (unsigned long long)g_ld_profile.total.flush_reason[
            LD_FLUSH_PIXEL_CONSTANT_RING],
        ld_profile_ticks_ms(g_ld_profile.total.flush_reason_qpc[
            LD_FLUSH_PIXEL_CONSTANT_RING]),
        ld_profile_ticks_ms(g_ld_profile.total.fence_reason_qpc[
            LD_FLUSH_PIXEL_CONSTANT_RING]));
    fflush(stderr);
#endif
    if (g_ld_shader_disk_ready > 0) {
        fprintf(stderr,
                "[shader-disk-cache-summary] hits{vs=%llu ps=%llu} "
                "misses{vs=%llu ps=%llu} writes{vs=%llu ps=%llu} "
                "rejects=%llu\n",
                (unsigned long long)g_ld_shader_disk_hits[0],
                (unsigned long long)g_ld_shader_disk_hits[1],
                (unsigned long long)g_ld_shader_disk_misses[0],
                (unsigned long long)g_ld_shader_disk_misses[1],
                (unsigned long long)g_ld_shader_disk_writes[0],
                (unsigned long long)g_ld_shader_disk_writes[1],
                (unsigned long long)g_ld_shader_disk_rejects);
    }
    for (u32 i = 0; i < g.n_psos; i++) if (g.psos[i].pso) g.psos[i].pso->lpVtbl->Release(g.psos[i].pso);
    shader_blob_cache_release(&g.vs_blobs);
    shader_blob_cache_release(&g.ps_blobs);
    for (u32 i = 0; i < g.n_textures; i++) if (g.textures[i].tex) g.textures[i].tex->lpVtbl->Release(g.textures[i].tex);
    for (u32 i = 0; i < g.n_vtex; i++) if (g.vtex[i].tex) g.vtex[i].tex->lpVtbl->Release(g.vtex[i].tex);
    for (u32 i = 0; i < g.n_surfaces; i++) if (g.surfaces[i].tex) g.surfaces[i].tex->lpVtbl->Release(g.surfaces[i].tex);
    for (u32 i = 0; i < g.n_zdepths; i++) {
        if (g.zdepths[i].tex)
            g.zdepths[i].tex->lpVtbl->Release(g.zdepths[i].tex);
        if (g.zdepths[i].snapshot)
            g.zdepths[i].snapshot->lpVtbl->Release(
                g.zdepths[i].snapshot);
    }
    if (g.movie_upload) g.movie_upload->lpVtbl->Release(g.movie_upload);
    if (g.movie_overlay_readback) g.movie_overlay_readback->lpVtbl->Release(g.movie_overlay_readback);
    if (g.movie_overlay_rgba) free(g.movie_overlay_rgba);
    if (g.movie_overlay_mask) free(g.movie_overlay_mask);
    if (g.white_tex) g.white_tex->lpVtbl->Release(g.white_tex);
    if (g.white_tex3d) g.white_tex3d->lpVtbl->Release(g.white_tex3d);
    if (g.depth) g.depth->lpVtbl->Release(g.depth);
    if (g.ps_cb) g.ps_cb->lpVtbl->Release(g.ps_cb);
    if (g.depth_snapshot_pso)
        g.depth_snapshot_pso->lpVtbl->Release(g.depth_snapshot_pso);
    if (g.depth_snapshot_rootsig)
        g.depth_snapshot_rootsig->lpVtbl->Release(
            g.depth_snapshot_rootsig);
    if (g.rootsig_x) g.rootsig_x->lpVtbl->Release(g.rootsig_x);
    if (g.swap) g.swap->lpVtbl->Release(g.swap);
    if (g_ld_info_queue) {
        g_ld_info_queue->lpVtbl->Release(g_ld_info_queue);
        g_ld_info_queue = NULL;
    }
#if defined(YZ_PERF_PROFILE)
    if (g_ld_profile.adapter) {
        g_ld_profile.adapter->lpVtbl->Release(g_ld_profile.adapter);
        g_ld_profile.adapter = NULL;
    }
#endif
    if (g.dev) g.dev->lpVtbl->Release(g.dev);
    memset(&g, 0, sizeof(g));
    s_tex_index_ready = 0;
    if (dc.verts) { free(dc.verts); dc.verts = NULL; dc.cap_verts = 0; }
    if (dc.refs) { free(dc.refs); dc.refs = NULL; dc.cap_refs = 0; }
    if (dc.compact_verts) {
        free(dc.compact_verts);
        dc.compact_verts = NULL;
        dc.compact_capacity = 0;
    }
    if (dc.cuts) { free(dc.cuts); dc.cuts = NULL; dc.cap_cuts = 0; }
}

#endif /* _WIN32 */
