#include "NavmeshPrecutFix.h"
#include "AddressLibrary.h"
#include "OCBP_AddressIDs.h"
#include "log.h"
#include "f4se_common/SafeWrite.h"
#include "f4se_common/Relocation.h"

#define XBYAK_NO_OP_NAMES
#include "xbyak/xbyak.h"

#include <windows.h>
#include <cstdint>
#include <cstring>

namespace
{
    constexpr std::size_t kTrampolinePageSize = 4096;

    alignas(8) volatile LONG64 s_navmeshOobCount = 0;
    void* s_trampolineMem = nullptr;

    /**
     * Generates the x86-64 bounds-checked remap loop trampoline using vendored
     * Xbyak so both MSVC and clang-cl build without inline assembly.
     */
    struct NavmeshRemapCode : Xbyak::CodeGenerator
    {
        NavmeshRemapCode(void* buf, std::size_t bufSize, std::uintptr_t resumeAddr, volatile LONG64* oobCounter)
            : Xbyak::CodeGenerator(bufSize, buf)
        {
            Xbyak::Label loopTop;
            Xbyak::Label exitLoop;
            Xbyak::Label resumeLabel;

            auto emitEdge = [&](int disp, std::uint32_t flagMask)
            {
                Xbyak::Label inBounds;
                Xbyak::Label writeEdge;
                Xbyak::Label skipEdge;

                movzx(eax, word[r8 + r9 + disp]);
                cmp(ax, r11w);
                je(skipEdge);
                movzx(ecx, ax);
                mov(edx, r11d);
                cmp(ecx, dword[rbp - 0x28]);
                jb(inBounds);

                // OOB precut neighbor index: keep 0xFFFF sentinel (r11d) and record catch.
                mov(rax, reinterpret_cast<std::size_t>(oobCounter));
                lock();
                inc(qword[rax]);
                jmp(writeEdge);

                L(inBounds);
                mov(rax, qword[rbp - 0x38]);
                movzx(edx, word[rax + rcx * 2]);

                L(writeEdge);
                and_(dword[r8 + r9 + 0x10], flagMask);
                mov(word[r8 + r9 + disp], dx);

                L(skipEdge);
            };

            L(loopTop);
            test(r10, r10);
            jz(exitLoop, T_NEAR);
            mov(rax, qword[rsi]);
            test(rax, rax);
            jz(exitLoop, T_NEAR);
            mov(r9, qword[rax + 0x50]);
            test(r9, r9);
            jz(exitLoop, T_NEAR);

            emitEdge(6, 0xFFFFFFFEu);
            emitEdge(8, 0xFFFFFFFDu);
            emitEdge(10, 0xFFFFFFFBu);

            add(r8, 0x18);
            sub(r10, 1);
            jnz(loopTop, T_NEAR);

            L(exitLoop);
            jmp(ptr[rip + resumeLabel]);

            L(resumeLabel);
            dq(resumeAddr);

            ready();
        }
    };
}

std::uint64_t GetNavmeshOobCount()
{
    return static_cast<std::uint64_t>(InterlockedCompareExchange64(&s_navmeshOobCount, 0, 0));
}

bool InstallNavmeshPrecutFix(std::uint32_t runtimeVersion)
{
    if (s_trampolineMem)
    {
        return true;
    }

    if (!g_addressLibrary.IsLoaded() && !g_addressLibrary.Load(runtimeVersion))
    {
        logger.Error("NavmeshPrecutFix: AddressLibrary not loaded\n");
        return false;
    }

    const std::uint64_t id = OCBP_AddressIDs::kDynamicNavmesh_ApplyActivePrecutsToNavmesh;
    const std::uintptr_t funcBase = g_addressLibrary.Resolve(id);
    if (!funcBase)
    {
        logger.Error("NavmeshPrecutFix: Failed to resolve ID %llu\n", (unsigned long long)id);
        return false;
    }

    // Offset of the remap loop within function ID 2301453
    const std::uintptr_t hookAddr = funcBase + 0x6C0;  // 0x141E3A1C0
    const std::uintptr_t resumeAddr = funcBase + 0x73A; // 0x141E3A23A

    // Verify byte pattern at hookAddr (first 7 bytes: mov rax, [rsi]; mov r9, [rax + 0x50])
    const unsigned char expected[7] = { 0x48, 0x8B, 0x06, 0x4C, 0x8B, 0x48, 0x50 };
    if (std::memcmp((const void*)hookAddr, expected, sizeof(expected)) != 0)
    {
        logger.Error("NavmeshPrecutFix: byte mismatch at 0x%p, skipping patch\n", (void*)hookAddr);
        return false;
    }

    void* trampolineMem = VirtualAlloc(nullptr, kTrampolinePageSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!trampolineMem)
    {
        logger.Error("NavmeshPrecutFix: VirtualAlloc failed (%lu)\n", (unsigned long)GetLastError());
        return false;
    }

    try
    {
        NavmeshRemapCode code(trampolineMem, kTrampolinePageSize, resumeAddr, &s_navmeshOobCount);
    }
    catch (const Xbyak::Error& e)
    {
        logger.Error("NavmeshPrecutFix: Xbyak codegen error: %s\n", e.what());
        VirtualFree(trampolineMem, 0, MEM_RELEASE);
        return false;
    }

    DWORD oldProtect = 0;
    VirtualProtect(trampolineMem, kTrampolinePageSize, PAGE_EXECUTE_READ, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), trampolineMem, kTrampolinePageSize);
    s_trampolineMem = trampolineMem;

    // The loop spans exactly 122 bytes (0x7A bytes from 0x6C0 to 0x73A).
    // We write a 14-byte 64-bit absolute indirect jump at hookAddr:
    //   FF 25 00 00 00 00 [64-bit target address]
    // and fill the remaining 108 bytes with 0x90 (NOP).
    constexpr std::size_t kPatchSize = 122;
    unsigned char patchBuf[kPatchSize];
    std::memset(patchBuf, 0x90, kPatchSize);

    // JMP [RIP + 0]
    patchBuf[0] = 0xFF;
    patchBuf[1] = 0x25;
    patchBuf[2] = 0x00;
    patchBuf[3] = 0x00;
    patchBuf[4] = 0x00;
    patchBuf[5] = 0x00;

    const std::uint64_t trampolineAddr = reinterpret_cast<std::uint64_t>(trampolineMem);
    std::memcpy(&patchBuf[6], &trampolineAddr, sizeof(trampolineAddr));

    SafeWriteBuf(hookAddr, patchBuf, kPatchSize);

    logger.Error("NavmeshPrecutFix: successfully patched DynamicNavmesh at 0x%p -> trampoline 0x%p (resume 0x%p)\n",
                 (void*)hookAddr, (void*)trampolineAddr, (void*)resumeAddr);
    return true;
}
