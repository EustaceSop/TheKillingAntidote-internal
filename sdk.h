#pragma once
#include "sdk_types.h"
#include <string>
#include <unordered_map>

namespace sdk
{
    // Init: resolves image base + fills global pointers. Call once from main thread.
    bool Init();

    uintptr_t ImageBase();
    TUObjectArray* GObjects();

    // FName -> string via game's FName::AppendString.
    std::string FNameToString(const FName& name);

    // Find first UObject whose GetFullName() ends with `name`.
    // e.g. "Function TKA_GameState.TKA_GameState_C.Wake_All_Zombies_Up"
    UObject*   FindObject(const char* name);
    UFunction* FindFunction(const char* name);
    UClass*    FindClass(const char* name);

    // Cache a UFunction by name — safe to call every frame; only looks up once.
    UFunction* CachedFn(const char* name);
}
