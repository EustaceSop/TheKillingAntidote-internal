#include "sdk.h"
#include "offsets.h"
#include <Windows.h>
#include <mutex>

namespace {
    uintptr_t g_ImageBase = 0;
    TUObjectArray* g_GObjects = nullptr;

    using AppendStringFn = void(__thiscall*)(const FName*, FString&);
    AppendStringFn g_AppendString = nullptr;

    using ProcessEventFn = void(*)(void* thisObj, class UFunction* fn, void* params);
    ProcessEventFn g_ProcessEvent = nullptr;

    std::mutex g_FnCacheMx;
    std::unordered_map<std::string, UFunction*> g_FnCache;
}

namespace sdk
{
    bool Init()
    {
        g_ImageBase = (uintptr_t)GetModuleHandleW(nullptr);
        if (!g_ImageBase) return false;

        g_GObjects     = (TUObjectArray*)(g_ImageBase + off::GObjects);
        g_AppendString = (AppendStringFn)(g_ImageBase + off::AppendString);
        g_ProcessEvent = (ProcessEventFn)(g_ImageBase + off::ProcessEvent);

        // sanity: GObjects should have plausible values
        if (g_GObjects->NumElements <= 0 || g_GObjects->NumElements > 20'000'000) return false;
        if (g_GObjects->NumChunks   <= 0 || g_GObjects->NumChunks   > 4096)       return false;

        return true;
    }

    uintptr_t      ImageBase()  { return g_ImageBase; }
    TUObjectArray* GObjects()   { return g_GObjects;  }

    std::string FNameToString(const FName& name)
    {
        if (!g_AppendString) return {};
        FString out{};
        g_AppendString(&name, out);
        std::string s = out.ToString();
        if (out.Data) { /* engine owns the buffer; do not free */ }
        return s;
    }
}

// ---- UObject helpers (must live in this TU because they use g_ globals) ----

std::string UObject::GetName() const
{
    return sdk::FNameToString(NamePrivate);
}

std::string UObject::GetFullName() const
{
    if (!ClassPrivate) return {};

    // "ClassName Outer1.Outer2.ThisName"
    std::string name = GetName();
    const UObject* out = OuterPrivate;
    while (out)
    {
        std::string on = out->GetName();
        name = on + "." + name;
        out  = out->OuterPrivate;
    }
    return ClassPrivate->GetName() + " " + name;
}

bool UObject::IsA(UClass* other) const
{
    for (UStruct* s = (UStruct*)ClassPrivate; s; s = s->SuperStruct)
        if ((UClass*)s == other) return true;
    return false;
}

void UObject::ProcessEvent(UFunction* fn, void* params)
{
    if (!g_ProcessEvent || !fn) return;
    g_ProcessEvent(this, fn, params);
}

namespace sdk
{
    UObject* FindObject(const char* name)
    {
        if (!g_GObjects || !name) return nullptr;
        std::string needle = name;
        int n = g_GObjects->Num();
        for (int i = 0; i < n; ++i)
        {
            UObject* obj = g_GObjects->GetByIndex(i);
            if (!obj || !obj->ClassPrivate) continue;
            std::string full = obj->GetFullName();
            if (full.size() >= needle.size() &&
                full.compare(full.size() - needle.size(), needle.size(), needle) == 0)
                return obj;
        }
        return nullptr;
    }

    UFunction* FindFunction(const char* name) { return (UFunction*)FindObject(name); }
    UClass*    FindClass(const char* name)    { return (UClass*)   FindObject(name); }

    UFunction* CachedFn(const char* name)
    {
        std::lock_guard<std::mutex> lk(g_FnCacheMx);
        auto it = g_FnCache.find(name);
        if (it != g_FnCache.end() && it->second) return it->second;
        // NOTE: nullptr results are deliberately NOT cached permanently —
        // the target package may simply not be loaded yet (main menu vs
        // in-game). Retry every call until it resolves, then keep it.
        UFunction* fn = FindFunction(name);
        g_FnCache[name] = fn;
        return fn;
    }
}
