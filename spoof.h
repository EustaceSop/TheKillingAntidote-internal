#pragma once
#include <Windows.h>
#include <winternl.h>

// Creates a thread whose apparent start address points at a freshly-committed
// stub page (random address) instead of our DLL's code. Rip and Rcx are then
// patched via SetThreadContext to run the real entry function with the
// requested argument.
//
// Note: on some AV/anti-cheat, the shifted start-address is treated as more
// suspicious than a normal CreateThread. Prefer this only when actually needed.

using _RtlCreateUserThread = NTSTATUS(NTAPI*)(
    HANDLE ProcessHandle,
    PSECURITY_DESCRIPTOR SecurityDescriptor,
    BOOLEAN CreateSuspended,
    ULONG  StackZeroBits,
    PULONG StackReserved,
    PULONG StackCommit,
    void*  StartAddress,
    void*  StartParameter,
    PHANDLE ThreadHandle,
    void*  ClientID);

inline void SpoofThreadLocation(void* realEntry, HMODULE hModule)
{
    // 1. Allocate a small executable stub page — return-address only needs to
    //    exist; the actual code path is redirected by SetThreadContext.
    LPVOID stub = VirtualAlloc(nullptr, 0x1000,
                               MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!stub) return;
    // put a `ret` opcode there so any accidental fall-through is harmless
    *reinterpret_cast<uint8_t*>(stub) = 0xC3;

    // 2. Create a suspended thread whose StartAddress = stub (fake).
    auto NtCreate = (_RtlCreateUserThread)GetProcAddress(
        GetModuleHandleW(L"ntdll.dll"), "RtlCreateUserThread");
    if (!NtCreate) { VirtualFree(stub, 0, MEM_RELEASE); return; }

    HANDLE hThread = nullptr;
    NTSTATUS st = NtCreate(GetCurrentProcess(), nullptr, TRUE, 0, nullptr, nullptr,
                           stub, hModule, &hThread, nullptr);
    if (!hThread) { VirtualFree(stub, 0, MEM_RELEASE); return; }

    // 3. Redirect RIP -> real entry ; RCX (first arg on Win64) -> hModule.
    CONTEXT ctx{};
    ctx.ContextFlags = CONTEXT_INTEGER | CONTEXT_CONTROL;
    GetThreadContext(hThread, &ctx);
    ctx.Rip = reinterpret_cast<ULONG64>(realEntry);
    ctx.Rcx = reinterpret_cast<ULONG64>(hModule);
    ctx.ContextFlags = CONTEXT_INTEGER | CONTEXT_CONTROL;
    SetThreadContext(hThread, &ctx);

    // 4. Go.
    ResumeThread(hThread);
    CloseHandle(hThread);
    // stub page intentionally leaked — it must remain valid for the reported StartAddress.
}
