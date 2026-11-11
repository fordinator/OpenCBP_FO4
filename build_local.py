#!/usr/bin/env python3
import os
import sys
import subprocess
import concurrent.futures

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
XWIN_DIR = os.path.expanduser("~/.xwin")
BUILD_DIR = os.path.join(SCRIPT_DIR, "build_x64")
BIN_DIR = os.path.join(SCRIPT_DIR, "x64", "Release")

CLANG_CL = "clang-cl"
LLVM_LIB = "llvm-lib"

CFLAGS_BASE = [
    "-fuse-ld=lld",
    "-winsysroot", XWIN_DIR,
    "/O2",
    "/std:c++17",
    "/EHsc",
    "/MT",  # MultiThreaded release CRT
    "/FI", "common/IPrefix.h",
    f"-I{SCRIPT_DIR}",
    f"-I{os.path.join(SCRIPT_DIR, 'f4se')}",
    "-DNDEBUG",
    "-DNOMINMAX",
    "-Wno-invalid-offsetof",
    "-Wno-invalid-token-paste",
    "-Wno-pragma-once-outside-header",
    "-Wno-microsoft-goto",
    "-Wno-self-assign-field",
    "-Wno-return-type",
    "-Wno-typename-missing",
    "-Wno-deprecated-declarations",
]

def compile_file(src, obj, extra_flags=None):
    os.makedirs(os.path.dirname(obj), exist_ok=True)
    flags = list(CFLAGS_BASE)
    if extra_flags:
        flags.extend(extra_flags)
    cmd = [CLANG_CL] + flags + ["-c", src, f"-Fo{obj}"]
    res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if res.returncode != 0:
        print(f"FAILED: {src}\n{res.stdout}\n{res.stderr}", file=sys.stderr)
        return False
    return True

def create_lib(lib_path, objs):
    cmd = [LLVM_LIB, f"/out:{lib_path}"] + objs
    res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if res.returncode != 0:
        print(f"FAILED to create {lib_path}\n{res.stdout}\n{res.stderr}", file=sys.stderr)
        return False
    return True

def main():
    os.makedirs(BUILD_DIR, exist_ok=True)
    os.makedirs(BIN_DIR, exist_ok=True)

    # 1. common_vc14
    common_srcs = [
        "common/IBufferStream.cpp",
        "common/IDataStream.cpp",
        "common/IDebugLog.cpp",
        "common/IDirectoryIterator.cpp",
        "common/IErrors.cpp",
        "common/IEvent.cpp",
        "common/IFileStream.cpp",
        "common/IInterlockedLong.cpp",
        "common/IMutex.cpp",
        "common/IPrefix.cpp",
        "common/ISegmentStream.cpp",
        "common/ISingleton.cpp",
        "common/ITimer.cpp",
        "common/ITypes.cpp",
    ]
    common_objs = [os.path.join(BUILD_DIR, f"{os.path.splitext(f)[0]}.obj") for f in common_srcs]

    # 2. f4se_common
    f4se_common_srcs = [
        "f4se/f4se_common/BranchTrampoline.cpp",
        "f4se/f4se_common/Relocation.cpp",
        "f4se/f4se_common/SafeWrite.cpp",
        "f4se/f4se_common/Utilities.cpp",
    ]
    f4se_common_objs = [os.path.join(BUILD_DIR, f"{os.path.splitext(f)[0]}.obj") for f in f4se_common_srcs]

    # 3. f4se (Game headers/RTTI)
    f4se_cbp_srcs = [
        "f4se/f4se/GameAPI.cpp",
        "f4se/f4se/GameData.cpp",
        "f4se/f4se/GameExtraData.cpp",
        "f4se/f4se/GameForms.cpp",
        "f4se/f4se/GameRTTI.cpp",
        "f4se/f4se/GameTypes.cpp",
        "f4se/f4se/GameUtilities.cpp",
        "f4se/f4se/NiNodes.cpp",
        "f4se/f4se/NiObjects.cpp",
        "f4se/f4se/NiTypes.cpp",
        "f4se/f4se/PapyrusArgs.cpp",
        "f4se/f4se/PapyrusInterfaces.cpp",
        "f4se/f4se/PapyrusNativeFunctions.cpp",
        "f4se/f4se/PapyrusValue.cpp",
        "f4se/f4se/PapyrusVM.cpp",
    ]
    f4se_flags = [
        "-DWIN32",
        "-D_WINDOWS",
        "-D_USRDLL",
        "-DF4SE_EXPORTS",
        "-DRUNTIME",
        "-DRUNTIME_VERSION=0x010B0F00",
    ]
    f4se_objs = [os.path.join(BUILD_DIR, f"{os.path.splitext(f)[0]}.obj") for f in f4se_cbp_srcs]

    # 4. CBPSSE & DetourXS
    cbp_srcs = [
        "detourxs-master/detourxs.cpp",
        "CBPSSE/ActorUtils.cpp",
        "CBPSSE/AddressLibrary.cpp",
        "CBPSSE/config.cpp",
        "CBPSSE/FrameHook.cpp",
        "CBPSSE/log.cpp",
        "CBPSSE/main.cpp",
        "CBPSSE/NavmeshPrecutFix.cpp",
        "CBPSSE/PapyrusOCBP.cpp",
        "CBPSSE/scan.cpp",
        "CBPSSE/SimObj.cpp",
        "CBPSSE/Thing.cpp",
    ]
    cbp_flags = [
        f"-I{os.path.join(SCRIPT_DIR, 'CBPSSE')}",
        f"-I{os.path.join(SCRIPT_DIR, 'detourxs-master')}",
        "-D_USRDLL",
        "-DPLUGIN_EXAMPLE_EXPORTS",
        "-DRUNTIME",
        "-DRUNTIME_VERSION=0x010B0F00",
    ]
    cbp_objs = [os.path.join(BUILD_DIR, f"{os.path.splitext(f)[0]}.obj") for f in cbp_srcs]

    all_tasks = []
    for s, o in zip(common_srcs, common_objs):
        all_tasks.append((os.path.join(SCRIPT_DIR, s), o, ["-D_LIB"]))
    for s, o in zip(f4se_common_srcs, f4se_common_objs):
        all_tasks.append((os.path.join(SCRIPT_DIR, s), o, ["-D_LIB"]))
    for s, o in zip(f4se_cbp_srcs, f4se_objs):
        all_tasks.append((os.path.join(SCRIPT_DIR, s), o, f4se_flags))
    for s, o in zip(cbp_srcs, cbp_objs):
        all_tasks.append((os.path.join(SCRIPT_DIR, s), o, cbp_flags))

    print(f"Compiling {len(all_tasks)} translation units with {os.cpu_count()} threads...")
    with concurrent.futures.ThreadPoolExecutor(max_workers=os.cpu_count()) as executor:
        futures = {executor.submit(compile_file, s, o, f): s for s, o, f in all_tasks}
        for future in concurrent.futures.as_completed(futures):
            src = futures[future]
            if not future.result():
                print(f"Compilation failed for {src}", file=sys.stderr)
                sys.exit(1)

    print("All translation units compiled successfully.")

    # Create static libraries
    common_lib = os.path.join(BUILD_DIR, "common_vc14.lib")
    f4se_common_lib = os.path.join(BUILD_DIR, "f4se_common.lib")
    f4se_lib = os.path.join(BUILD_DIR, "f4se_1_11_240.lib")

    print(f"Archiving {common_lib}...")
    if not create_lib(common_lib, common_objs): sys.exit(1)

    print(f"Archiving {f4se_common_lib}...")
    if not create_lib(f4se_common_lib, f4se_common_objs): sys.exit(1)

    print(f"Archiving {f4se_lib}...")
    if not create_lib(f4se_lib, f4se_objs): sys.exit(1)

    # Link cbp.dll
    target_dll = os.path.join(BIN_DIR, "cbp.dll")
    lde_lib = os.path.join(SCRIPT_DIR, "detourxs-master", "LDE64x64.lib")
    def_file = os.path.join(SCRIPT_DIR, "CBPSSE", "exports.def")

    print(f"Linking {target_dll}...")
    link_cmd = [
        CLANG_CL,
        "-fuse-ld=lld",
        "-winsysroot", XWIN_DIR,
        "/LD", # Create DLL
        f"-Fe{target_dll}",
    ] + cbp_objs + [
        common_lib,
        f4se_common_lib,
        f4se_lib,
        lde_lib,
        "/link",
        f"/DEF:{def_file}",
        "/OPT:REF",
        "/OPT:ICF",
        "/SUBSYSTEM:WINDOWS",
    ]

    res = subprocess.run(link_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if res.returncode != 0:
        print(f"Link FAILED:\n{res.stdout}\n{res.stderr}", file=sys.stderr)
        sys.exit(1)

    print(f"SUCCESS: Built {target_dll} successfully!")
    return 0

if __name__ == "__main__":
    sys.exit(main())
