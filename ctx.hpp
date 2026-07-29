#pragma once
#include <string_view>

struct bind_t {
    int key = 0;
    int mode = 1;
    bool active = false;
    bool bind = false;

    void updbind();
    bool isact() const;
};

// UI-side state.  Actual cheat toggles live in features::Config and are
// mirrored to/from here in SyncCtxToFeatures().
struct ctx_t {
    bool open = true;
    int  tab  = 0;
    int  sub[6] = {0};

    // --- ESP / visuals ---
    bool espen = false;
    bool box   = false;
    bool name  = true;
    bool hp    = false;
    bool glwen = false;
    float boxclr[3] = { 0.42f, 0.65f, 0.95f };
    float nmclr[3]  = { 1.0f,  1.0f,  1.0f  };
    float glwclr[3] = { 0.8f,  0.2f,  0.8f  };

    // --- Aimbot ---
    bool   aim     = false;
    bool   drawfov = true;
    bind_t aimkey;
    float  fov    = 120.f;
    float  smooth = 3.f;
    int    hitbox = 0;    // 0=head 1=neck 2=chest 3=pelvis
    bool   trig   = false;
    bind_t trigkey;
    int    trigdel = 0;

    // --- Player (TKA) ---
    bool  god          = false;
    bool  infVitality  = false;
    bool  infAmmo      = false;
    bool  noRecoil     = false;
    bool  bigDamage    = false;
    float damageMult   = 100.0f;
    bool  bigBackpack  = false;

    // --- Movement (TKA) ---
    bool  speedHack = false;
    float speedVal  = 1200.0f;
    bool  jumpHack  = false;
    float jumpVal   = 900.0f;
    bool  flyHack   = false;
    bool  noGravity = false;

    // --- World (TKA) ---
    bool  xrayZombies      = false;
    bool  telekillHotkey   = false;
    bind_t telekillKey;
    int    difficulty      = -1;   // -1 = don't change
    bool   applyDifficulty = false;

    // --- Misc UI-only ---
    bool wm = true;
    bool showbinds = false;

    // --- Profile ---
    int  cfgidx = 0;
    char cfgnm[64] = "";
    float cpytime = 0.0f;
};

inline ctx_t ctx;
inline bool keys[256];
inline bool oldkeys[256];
// Edge latch: set by the 5ms input pump on a key-down edge, lives for ~300ms
// so the slower ImGui frame rate doesn't miss it (that was the keybind bug).
inline bool          keyslatch[256];
inline unsigned long keysedge_time[256];

bool presschk(int vk);
bool downchk(int vk);
// Consume a latched edge — used by keybind capture in mkbind.
bool latchchk(int vk);
std::string_view keynm(int vk);
