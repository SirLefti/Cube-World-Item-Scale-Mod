// Cube World Alpha - Item Scale Mod
//
// Every item model the UI draws (inventory, equipment grid, equipment slots,
// vendor list, hotbar, tooltips, world-space item labels) grows and shrinks
// with the game's resolution, but the UI elements themselves are fixed in size.
// This causes items to be drawn too big for the UI scaling, increasingly on
// higher resolution displays. This mod renders items in a constant size
// relative to the UI elements on any resolution.

// ---- build requirements -----------------------------------------------------
#include <stdint.h>   // UINTPTR_MAX - pointer width, checked portably

#if !defined(_WIN32)
#  error "Target must be Windows: this is a DLL injected into Cube.exe. Cross-compile, e.g. i686-w64-mingw32-g++."
#endif
#if UINTPTR_MAX != 0xFFFFFFFFu
#  error "Target must be 32-bit: Cube.exe is PE32."
#endif
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 8
#  error "GCC 8 or newer: older GCC silently IGNORES __attribute__((naked)) on x86 and builds a broken DLL."
#endif

#include <windows.h>

// Scale multiplier at REF_HEIGHT. 1.0 is vanilla size, which means at
// REF_HEIGHT, we already scale down a bit, because items do clip already at
// this resolution.
static const float SCALE = 0.87f;

// The reference render height, where our scale applies as-is. If the game
// resolution is higher or lower, the scale is adjusted.
static const int REF_HEIGHT = 1080;

// ---- the hook site ----------------------------------------------------------
//
// DrawItemModel @ 0x004758C0, __thiscall + 6 cdecl-pushed args:
//
//   void __thiscall DrawItemModel(void* renderer,   // ecx
//                                 float  screen_x,  // [esp+0x04] on entry
//                                 float  screen_y,  // [esp+0x08]
//                                 void*  ctx,       // [esp+0x0C]
//                                 float  scale,     // [esp+0x10]  <-- we scale this
//                                 Item*  item,      // [esp+0x14]
//                                 float  z_offset); // [esp+0x18]
//
// `scale` is used for nothing but multiplying the model matrix's 3x3 part, so
// it is a clean uniform scale. Every UI surface that draws an item goes
// through this one function, so hooking the callee covers all nine call sites.
static const uint32_t RVA_DRAW_ITEM = 0x000758C0;
static const uint32_t OFFSET_HEIGHT = 0x120; // offset where we find the height in the renderer
static const int HOOK_LEN = 9; // push ebp / mov ebp,esp / sub esp,0x1a8

// The exact bytes we expect to replace - a mismatch means a different build.
static const BYTE EXPECTED_BYTES[HOOK_LEN] = {
    0x55,                               // push ebp
    0x8B, 0xEC,                         // mov ebp, esp
    0x81, 0xEC, 0xA8, 0x01, 0x00, 0x00  // sub esp, 0x1a8
};

extern "C" void OnDrawItemModel(float* scale, const BYTE* renderer)
{
    int height = *(const int*)(renderer + OFFSET_HEIGHT);
    if (height <= 0) { // avoid div by zero
        *scale *= SCALE;
    } else {
        *scale *= SCALE * REF_HEIGHT / height;
    }
}

extern "C" {
    uint32_t jmp_back = 0;
}

// Naked: no prologue, so the game's registers and stack stay untouched.
// EFLAGS and xmm are not saved on purpose - we sit on the function's first
// instruction, where a caller expects neither to survive the call. The
// overwritten prologue is replayed at the end, before jumping back past it.
extern "C" __attribute__((naked)) void HookStub()
{
    asm("pushad");                              // push registers to stack

    asm("lea eax, [esp+0x30]");                 // &scale: [esp+0x10] on entry, +0x20 for the pushad
    asm("push ecx");                            // renderer (this), for the window height
    asm("push eax");                            // pass it by pointer, so the callee can scale it
    asm("call _OnDrawItemModel");
    asm("add esp, 8");                          // cdecl, caller cleans

    asm("popad");                               // pops stack back to registers
    asm("push ebp");                            // original code
    asm("mov ebp, esp");                        // original code
    asm("sub esp, 0x1a8");                      // original code
    asm("jmp dword ptr [_jmp_back]");           // -> mov eax, ds:0x76aa78 (stack cookie)
}

static bool WriteHook(BYTE* location, void* target)
{
    DWORD oldProtection;
    if (!VirtualProtect(location, HOOK_LEN, PAGE_EXECUTE_READWRITE, &oldProtection)) {
        return false;
    }
    location[0] = 0xE9; // jmp rel32
    *(uint32_t*)(location + 1) = (uint32_t)target - (uint32_t)location - 5;
    for (int i = 5; i < HOOK_LEN; i++) location[i] = 0x90; // no-op the rest of the prologue
    VirtualProtect(location, HOOK_LEN, oldProtection, &oldProtection);
    FlushInstructionCache(GetCurrentProcess(), location, HOOK_LEN);
    return true;
}

extern "C" __declspec(dllexport) BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason != DLL_PROCESS_ATTACH) {
        return TRUE;
    }

    uint32_t base = (uint32_t)GetModuleHandle(NULL);
    BYTE* site = (BYTE*)(base + RVA_DRAW_ITEM);

    // Refuse to patch anything that is not the expected Cube.exe build.
    if (memcmp(site, EXPECTED_BYTES, HOOK_LEN) != 0) {
        return TRUE;
    }

    jmp_back = base + RVA_DRAW_ITEM + HOOK_LEN;
    WriteHook(site, (void*)&HookStub);
    return TRUE;
}
