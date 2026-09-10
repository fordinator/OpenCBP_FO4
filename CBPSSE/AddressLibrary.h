#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Minimal, dependency-free reader for the "Address Library for F4SE Plugins"
// database (Nexus 47327). F4SE-flavoured databases live at
//     Data\F4SE\Plugins\version-<major>-<minor>-<build>-<sub>.bin
// and use the uncompressed layout:
//     uint64 count;
//     struct { uint64 id; uint64 offset; } entries[count];   // sorted by id
// An ID is stable across game versions; the offset is what moves. Resolve the
// ID at runtime instead of baking an RVA into the DLL.
class AddressLibrary
{
public:
    struct Mapping
    {
        std::uint64_t id;
        std::uint64_t offset;
    };

    // runtimeVersion is the packed F4SE runtime version (F4SEInterface::runtimeVersion).
    bool Load(std::uint32_t runtimeVersion);

    bool IsLoaded() const { return !m_entries.empty(); }
    std::size_t Count() const { return m_entries.size(); }
    const std::string& Path() const { return m_path; }

    bool FindOffsetById(std::uint64_t id, std::uint64_t& outOffset) const;
    bool FindIdByOffset(std::uint64_t offset, std::uint64_t& outId) const;

    // Module base + offset for the ID, or 0 when the ID is missing.
    std::uintptr_t Resolve(std::uint64_t id) const;

private:
    std::vector<Mapping> m_entries;
    std::string m_path;
};

extern AddressLibrary g_addressLibrary;
