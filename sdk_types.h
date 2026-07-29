#pragma once
#include <cstdint>
#include <cmath>
#include <string>

// Minimal x64 UE4.27 reflection types — matches Dumper-7 SDK layouts.

struct FVector
{
    float X = 0, Y = 0, Z = 0;
    FVector() = default;
    FVector(float x, float y, float z) : X(x), Y(y), Z(z) {}
    FVector operator+(const FVector& v) const { return { X + v.X, Y + v.Y, Z + v.Z }; }
    FVector operator-(const FVector& v) const { return { X - v.X, Y - v.Y, Z - v.Z }; }
    FVector operator*(float s)          const { return { X * s,   Y * s,   Z * s   }; }
    float Dot(const FVector& v)         const { return X * v.X + Y * v.Y + Z * v.Z; }
    float Size()                        const { return std::sqrt(X * X + Y * Y + Z * Z); }
    float Dist(const FVector& v)        const { return (*this - v).Size(); }
};

struct FVector2D { float X = 0, Y = 0; };

struct FRotator { float Pitch = 0, Yaw = 0, Roll = 0; };

struct FQuat { float X = 0, Y = 0, Z = 0, W = 0; };

struct FTransform
{
    FQuat    Rotation;
    FVector  Translation;
    uint32_t pad0;
    FVector  Scale3D;
    uint32_t pad1;
};
static_assert(sizeof(FTransform) == 0x30, "FTransform size");

struct FName
{
    int32_t ComparisonIndex = 0;
    int32_t Number          = 0;
    bool operator==(const FName& o) const { return ComparisonIndex == o.ComparisonIndex && Number == o.Number; }
};
static_assert(sizeof(FName) == 8, "FName size");

// FString = TArray<TCHAR>
struct FString
{
    wchar_t* Data     = nullptr;
    int32_t  Count    = 0;
    int32_t  Max      = 0;

    std::wstring ToWString() const { return Data ? std::wstring(Data, Count > 0 ? Count - 1 : 0) : L""; }
    std::string  ToString()  const
    {
        std::wstring w = ToWString();
        return std::string(w.begin(), w.end());
    }
};

template<typename T>
struct TArray
{
    T*      Data  = nullptr;
    int32_t Count = 0;
    int32_t Max   = 0;

    T&       operator[](int i)       { return Data[i]; }
    const T& operator[](int i) const { return Data[i]; }
    bool     IsValidIndex(int i) const { return Data && i >= 0 && i < Count; }
    int      Num() const { return Count; }
};

// TUObjectArray (chunked)  — Dumper-7 Assertions.inl:18-25
struct FUObjectItem
{
    class UObject* Object;
    int32_t        Flags;
    int32_t        ClusterRootIndex;
    int32_t        SerialNumber;
    int32_t        Unknown;
};
static_assert(sizeof(FUObjectItem) == 0x18, "FUObjectItem size");

struct TUObjectArray
{
    FUObjectItem** Objects;
    uint8_t        pad[8];
    int32_t        MaxElements;
    int32_t        NumElements;
    int32_t        MaxChunks;
    int32_t        NumChunks;

    static constexpr int NumElementsPerChunk = 0x10000;

    int Num() const { return NumElements; }

    UObject* GetByIndex(int idx) const
    {
        if (idx < 0 || idx >= NumElements) return nullptr;
        int chunkIdx = idx / NumElementsPerChunk;
        int inChunk  = idx % NumElementsPerChunk;
        if (chunkIdx >= NumChunks) return nullptr;
        FUObjectItem* chunk = Objects[chunkIdx];
        if (!chunk) return nullptr;
        return chunk[inChunk].Object;
    }
};

// UObject layout (UE4 stripped): vtable @0, ObjectFlags @0x08, InternalIndex @0x0C, Class @0x10, Name @0x18, Outer @0x20
class UObject
{
public:
    void**   VTable;          // 0x00
    int32_t  ObjectFlags;     // 0x08
    int32_t  InternalIndex;   // 0x0C
    class UClass* ClassPrivate; // 0x10
    FName    NamePrivate;     // 0x18
    UObject* OuterPrivate;    // 0x20

    std::string  GetName() const;
    std::string  GetFullName() const;
    bool         IsA(UClass* other) const;

    // Call UFunction; params points to arg block laid out per UE reflection
    void ProcessEvent(class UFunction* fn, void* params);
};

class UField : public UObject
{
public:
    UField* Next; // 0x28
};

class UStruct : public UField
{
public:
    uint8_t pad_30[0x10];       // 0x30
    UStruct* SuperStruct;       // 0x40
    UField*  Children;          // 0x48
    int32_t  PropertiesSize;    // 0x50
    // ...
};

class UClass    : public UStruct {};
class UFunction : public UStruct
{
public:
    // Dumper-7 UE4.27 layout: EFunctionFlags@0xB0
    // We don't need internal fields to call via ProcessEvent — only need to pass the UFunction pointer.
};

class UScriptStruct : public UStruct {};
