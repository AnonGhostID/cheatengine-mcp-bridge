/*
 * CE MCP Bridge Auto-Loader Plugin
 * ---------------------------------
 * Cheat Engine plugin DLL that embeds the full ce_mcp_bridge.lua source and
 * executes it automatically when Cheat Engine starts, using CE's own Lua state.
 *
 * Build: see build.sh
 * SDK:   cepluginsdk.h v6 (CE 6.0+)
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

/* ── Minimal Lua 5.3 declarations (CE bundles its own Lua; we link at runtime) ── */
typedef struct lua_State lua_State;
typedef int (*lua_CFunction)(lua_State *L);
typedef intptr_t lua_KContext;
typedef int (*lua_KFunction)(lua_State *L, int status, lua_KContext ctx);

/* Lua C-API function pointers we resolve from CE's lua53.dll at runtime.
 * NOTE: lua_pcall is a macro in Lua 5.3+; the real export is lua_pcallk. */
typedef int  (*PFN_luaL_loadbufferx)(lua_State *L, const char *buff, size_t sz,
                                     const char *name, const char *mode);
typedef int  (*PFN_lua_pcallk)      (lua_State *L, int nargs, int nresults,
                                     int errfunc, lua_KContext ctx, lua_KFunction k);
typedef void (*PFN_lua_settop)      (lua_State *L, int idx);
typedef const char *(*PFN_lua_tolstring)(lua_State *L, int idx, size_t *len);

static PFN_luaL_loadbufferx fn_luaL_loadbufferx = NULL;
static PFN_lua_pcallk        fn_lua_pcallk       = NULL;
static PFN_lua_settop         fn_lua_settop       = NULL;
static PFN_lua_tolstring      fn_lua_tolstring    = NULL;

/* ── Minimal CE SDK types (subset of cepluginsdk.h we actually need) ── */
#define CESDK_VERSION 6

typedef struct _PluginVersion {
    unsigned int  version;
    char         *pluginname;
} PluginVersion, *PPluginVersion;

typedef lua_State *(__fastcall *CEP_GETLUASTATE)(void);
typedef void       (__stdcall  *CEP_SHOWMESSAGE)(char *message);
typedef int        (__stdcall  *CEP_REGISTERFUNCTION)(int pluginid, int functiontype, void *init);
typedef BOOL       (__stdcall  *CEP_UNREGISTERFUNCTION)(int pluginid, int functionid);

typedef struct _ExportedFunctions {
    int                  sizeofExportedFunctions;
    CEP_SHOWMESSAGE      ShowMessage;
    CEP_REGISTERFUNCTION RegisterFunction;
    CEP_UNREGISTERFUNCTION UnregisterFunction;
    /* There are many more fields — we only need a few, but the struct size
       must match CE's layout so we pad with void* for every remaining slot.
       CE passes us the struct pointer; we only access the first few fields
       plus GetLuaState which is at a known offset in the v5+ struct.
       We use the full-size approach: declare all fields up to GetLuaState. */
    PULONG  OpenedProcessID;
    HANDLE *OpenedProcessHandle;
    void   *GetMainWindowHandle;
    void   *AutoAssemble;
    void   *Assembler;
    void   *Disassembler;
    void   *ChangeRegistersAtAddress;
    void   *InjectDLL;
    void   *FreezeMem;
    void   *UnfreezeMem;
    void   *FixMem;
    void   *ProcessList;
    void   *ReloadSettings;
    void   *GetAddressFromPointer;
    /* hookable Win32 pointers (v1) */
    void   *ReadProcessMemory;
    void   *WriteProcessMemory;
    void   *GetThreadContext;
    void   *SetThreadContext;
    void   *SuspendThread;
    void   *ResumeThread;
    void   *OpenProcess;
    void   *WaitForDebugEvent;
    void   *ContinueDebugEvent;
    void   *DebugActiveProcess;
    void   *StopDebugging;
    void   *StopRegisterChange;
    void   *VirtualProtect;
    void   *VirtualProtectEx;
    void   *VirtualQueryEx;
    void   *VirtualAllocEx;
    void   *CreateRemoteThread;
    void   *OpenThread;
    void   *GetPEProcess;
    void   *GetPEThread;
    void   *GetThreadsProcessOffset;
    void   *GetThreadListEntryOffset;
    void   *GetProcessnameOffset;
    void   *GetDebugportOffset;
    void   *GetPhysicalAddress;
    void   *ProtectMe;
    void   *GetCR4;
    void   *GetCR3;
    void   *SetCR3;
    void   *GetSDT;
    void   *GetSDTShadow;
    void   *setAlternateDebugMethod;
    void   *getAlternateDebugMethod;
    void   *DebugProcess;
    void   *ChangeRegOnBP;
    void   *RetrieveDebugData;
    void   *StartProcessWatch;
    void   *WaitForProcessListData;
    void   *GetProcessNameFromID;
    void   *GetProcessNameFromPEProcess;
    void   *KernelOpenProcess;
    void   *KernelReadProcessMemory;
    void   *KernelWriteProcessMemory;
    void   *KernelVirtualAllocEx;
    void   *IsValidHandle;
    void   *GetIDTCurrentThread;
    void   *GetIDTs;
    void   *MakeWritable;
    void   *GetLoadedState;
    void   *DBKSuspendThread;
    void   *DBKResumeThread;
    void   *DBKSuspendProcess;
    void   *DBKResumeProcess;
    void   *KernelAlloc;
    void   *GetKProcAddress;
    void   *CreateToolhelp32Snapshot;
    void   *Process32First;
    void   *Process32Next;
    void   *Thread32First;
    void   *Thread32Next;
    void   *Module32First;
    void   *Module32Next;
    void   *Heap32ListFirst;
    void   *Heap32ListNext;
    /* advanced Delphi-only ptrs */
    void   *mainform;
    void   *memorybrowser;
    /* v2 */
    void   *sym_nameToAddress;
    void   *sym_addressToName;
    void   *sym_generateAPIHookScript;
    /* v3 */
    void   *loadDBK32;
    void   *loaddbvmifneeded;
    void   *previousOpcode;
    void   *nextOpcode;
    void   *disassembleEx;
    void   *loadModule;
    void   *aa_AddExtraCommand;
    void   *aa_RemoveExtraCommand;
    /* v4 */
    void   *createTableEntry;
    void   *getTableEntry;
    void   *memrec_setDescription;
    void   *memrec_getDescription;
    void   *memrec_getAddress;
    void   *memrec_setAddress;
    void   *memrec_getType;
    void   *memrec_setType;
    void   *memrec_getValue;
    void   *memrec_setValue;
    void   *memrec_getScript;
    void   *memrec_setScript;
    void   *memrec_isfrozen;
    void   *memrec_freeze;
    void   *memrec_unfreeze;
    void   *memrec_setColor;
    void   *memrec_appendtoentry;
    void   *memrec_delete;
    void   *getProcessIDFromProcessName;
    void   *openProcessEx;
    void   *debugProcessEx;
    void   *pause;
    void   *unpause;
    void   *debug_setBreakpoint;
    void   *debug_removeBreakpoint;
    void   *debug_continueFromBreakpoint;
    void   *closeCE;
    void   *hideAllCEWindows;
    void   *unhideMainCEwindow;
    void   *createForm;
    void   *form_centerScreen;
    void   *form_hide;
    void   *form_show;
    void   *form_onClose;
    void   *createPanel;
    void   *createGroupBox;
    void   *createButton;
    void   *createImage;
    void   *image_loadImageFromFile;
    void   *image_transparent;
    void   *image_stretch;
    void   *createLabel;
    void   *createEdit;
    void   *createMemo;
    void   *createTimer;
    void   *timer_setInterval;
    void   *timer_onTimer;
    void   *control_setCaption;
    void   *control_getCaption;
    void   *control_setPosition;
    void   *control_getX;
    void   *control_getY;
    void   *control_setSize;
    void   *control_getWidth;
    void   *control_getHeight;
    void   *control_setAlign;
    void   *control_onClick;
    void   *object_destroy;
    void   *messageDialog;
    void   *speedhack_setSpeed;
    /* v5 */
    void         *ExecuteKernelCode;
    void         *UserdefinedInterruptHook;
    CEP_GETLUASTATE GetLuaState;   /* <── the one we need */
    void         *MainThreadCall;
} ExportedFunctions, *PExportedFunctions;

/* ── Embedded Lua payload ── */
#include "lua_payload.h"

/* ── Plugin globals ── */
static ExportedFunctions g_CE;
static int g_pluginid = -1;

/* ── Helpers ── */

/*
 * Resolve Lua API functions from CE's bundled Lua DLL.
 * CE ships lua53.dll (sometimes lua54.dll) alongside CheatEngine.exe.
 * We try both names; whichever succeeds gives us the exports we need.
 */
static HMODULE try_load_from_ce_dir(const char *filename)
{
    /* First check if it's already loaded (fastest path) */
    HMODULE h = GetModuleHandleA(filename);
    if (h) return h;

    /* Get the directory that CheatEngine.exe lives in */
    char ce_path[MAX_PATH];
    if (GetModuleFileNameA(NULL, ce_path, sizeof(ce_path)) == 0)
        return NULL;

    /* Truncate at last backslash to get directory */
    char *sep = strrchr(ce_path, '\\');
    if (!sep) sep = strrchr(ce_path, '/');
    if (!sep) return NULL;
    *(sep + 1) = '\0';

    /* Append the DLL filename and load */
    char full[MAX_PATH];
    snprintf(full, sizeof(full), "%s%s", ce_path, filename);
    return LoadLibraryA(full);
}

static BOOL resolve_lua_api(void)
{
    /*
     * CE 7.x ships lua53-64.dll and lua53-32.dll next to CheatEngine.exe.
     * We derive that directory at runtime from GetModuleFileNameA(NULL) so
     * the plugin DLL itself can live anywhere.
     */
    static const char *lua_dll_names[] = {
        "lua53-64.dll",
        "lua53-32.dll",
        "lua54-64.dll",
        "lua54-32.dll",
        "lua53.dll",
        "lua54.dll",
        NULL
    };

    HMODULE hLua = NULL;
    for (int i = 0; lua_dll_names[i] != NULL && !hLua; i++) {
        hLua = try_load_from_ce_dir(lua_dll_names[i]);
    }

    if (!hLua) return FALSE;

    fn_luaL_loadbufferx = (PFN_luaL_loadbufferx)GetProcAddress(hLua, "luaL_loadbufferx");
    fn_lua_pcallk       = (PFN_lua_pcallk)       GetProcAddress(hLua, "lua_pcallk");
    fn_lua_settop       = (PFN_lua_settop)       GetProcAddress(hLua, "lua_settop");
    fn_lua_tolstring    = (PFN_lua_tolstring)    GetProcAddress(hLua, "lua_tolstring");

    return (fn_luaL_loadbufferx != NULL && fn_lua_pcallk != NULL);
}

/* Run LUA_PAYLOAD in the given Lua state. Returns NULL on success or an
   error string (valid until next Lua API call). */
static const char *run_payload(lua_State *L)
{
    /* load the chunk */
    int load_rc = fn_luaL_loadbufferx(L, LUA_PAYLOAD, LUA_PAYLOAD_SIZE,
                                       "=ce_mcp_bridge", "t");
    if (load_rc != 0) {
        const char *err = fn_lua_tolstring ? fn_lua_tolstring(L, -1, NULL) : "load error";
        return err ? err : "luaL_loadbufferx failed";
    }

    /* execute the chunk (0 args, 0 results, no error handler) */
    int call_rc = fn_lua_pcallk(L, 0, 0, 0, 0, NULL);
    if (call_rc != 0) {
        const char *err = fn_lua_tolstring ? fn_lua_tolstring(L, -1, NULL) : "exec error";
        return err ? err : "lua_pcall failed";
    }

    return NULL; /* success */
}

/* ── CE Plugin exports ── */

BOOL __stdcall CEPlugin_GetVersion(PPluginVersion pv, int sizeofpluginversion)
{
    pv->version    = CESDK_VERSION;
    pv->pluginname = (char *)"CE MCP Bridge Auto-Loader v1.0";
    return TRUE;
}

BOOL __stdcall CEPlugin_InitializePlugin(PExportedFunctions ef, int pluginid)
{
    char msg[512];

    g_pluginid = pluginid;

    /* Sanity-check the exported functions struct size */
    if (ef->sizeofExportedFunctions != (int)sizeof(ExportedFunctions)) {
        /*
         * Size mismatch usually means we're running against a CE version whose
         * ExportedFunctions layout differs from what we compiled against.
         * We still proceed — the fields we actually use (ShowMessage, GetLuaState)
         * are in the stable section of the struct, so this is a soft warning.
         */
        /* Uncomment to make this a hard failure instead:
        MessageBoxA(NULL,
            "CE MCP Bridge Plugin: ExportedFunctions size mismatch.\n"
            "This plugin was built for a different CE version.",
            "CE MCP Bridge Plugin", MB_OK | MB_ICONWARNING);
        return FALSE;
        */
    }

    /* Copy the entire CE function table */
    g_CE = *ef;

    /* Resolve Lua API from CE's bundled Lua DLL */
    if (!resolve_lua_api()) {
        char ce_path[MAX_PATH] = "<unknown>";
        GetModuleFileNameA(NULL, ce_path, sizeof(ce_path));
        char errmsg[1024];
        snprintf(errmsg, sizeof(errmsg),
            "CE MCP Bridge Plugin:\nCould not resolve Lua API.\n\n"
            "Looked for lua53-64.dll / lua53-32.dll / lua53.dll / lua54.dll\n"
            "in the CE directory and already-loaded modules.\n\n"
            "CE exe path detected:\n%s\n\n"
            "Please report this path so the correct DLL name can be added.",
            ce_path);
        MessageBoxA(NULL, errmsg, "CE MCP Bridge Plugin - Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }

    /* Get CE's live Lua state */
    if (g_CE.GetLuaState == NULL) {
        MessageBoxA(NULL,
            "CE MCP Bridge Plugin:\nGetLuaState is NULL.\n"
            "This CE build does not expose GetLuaState (requires CE 6.0+).",
            "CE MCP Bridge Plugin - Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }

    lua_State *L = g_CE.GetLuaState();
    if (L == NULL) {
        MessageBoxA(NULL,
            "CE MCP Bridge Plugin:\nGetLuaState() returned NULL.",
            "CE MCP Bridge Plugin - Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }

    /* Execute the embedded bridge script */
    const char *err = run_payload(L);
    if (err != NULL) {
        snprintf(msg, sizeof(msg),
            "CE MCP Bridge Plugin:\nFailed to start bridge.\n\nLua error:\n%s", err);
        MessageBoxA(NULL, msg, "CE MCP Bridge Plugin - Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }

    /* Success */
    MessageBoxA(NULL,
        "CE MCP Bridge started successfully!\n\n"
        "The MCP bridge is now listening on:\n"
        "\\\\.\\pipe\\CE_MCP_Bridge_v99\n\n"
        "You can now connect your AI agent.",
        "CE MCP Bridge Plugin", MB_OK | MB_ICONINFORMATION);

    return TRUE;
}

BOOL __stdcall CEPlugin_DisablePlugin(void)
{
    /* The Lua bridge manages its own teardown via StopMCPBridge().
       We could call it here via the Lua state but CE is likely shutting down,
       so we simply return TRUE and let CE clean up. */
    return TRUE;
}

/* ── DLL entry point ── */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    (void)hinstDLL;
    (void)lpvReserved;

    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hinstDLL);
            break;
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}
