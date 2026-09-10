#include "AddressLibrary.h"
#include "log.h"

#include "f4se_common/f4se_version.h"
#include "f4se_common/Relocation.h"

#include <algorithm>
#include <cstdio>

AddressLibrary g_addressLibrary;

bool AddressLibrary::Load(std::uint32_t runtimeVersion)
{
    m_entries.clear();

    char path[MAX_PATH];
    _snprintf_s(path, sizeof(path), _TRUNCATE,
        "Data\\F4SE\\Plugins\\version-%u-%u-%u-%u.bin",
        GET_EXE_VERSION_MAJOR(runtimeVersion),
        GET_EXE_VERSION_MINOR(runtimeVersion),
        GET_EXE_VERSION_BUILD(runtimeVersion),
        GET_EXE_VERSION_SUB(runtimeVersion));
    m_path = path;

    FILE* f = nullptr;
    if (fopen_s(&f, path, "rb") != 0 || !f)
    {
        logger.Error("AddressLibrary: cannot open %s (is Address Library for F4SE Plugins installed?)\n", path);
        return false;
    }

    std::uint64_t count = 0;
    if (fread(&count, sizeof(count), 1, f) != 1)
    {
        logger.Error("AddressLibrary: %s is truncated (no count)\n", path);
        fclose(f);
        return false;
    }

    // Sanity: the file must be exactly header + count entries.
    _fseeki64(f, 0, SEEK_END);
    const std::int64_t fileSize = _ftelli64(f);
    const std::int64_t expected = (std::int64_t)sizeof(count) + (std::int64_t)count * (std::int64_t)sizeof(Mapping);
    if (count == 0 || count > 0x1000000ull || fileSize != expected)
    {
        logger.Error("AddressLibrary: %s has an unexpected layout (count=%llu, size=%lld, expected=%lld). "
                     "Redownload the Address Library for this game version.\n",
                     path, (unsigned long long)count, (long long)fileSize, (long long)expected);
        fclose(f);
        return false;
    }
    _fseeki64(f, sizeof(count), SEEK_SET);

    m_entries.resize((std::size_t)count);
    const std::size_t got = fread(m_entries.data(), sizeof(Mapping), (std::size_t)count, f);
    fclose(f);
    if (got != (std::size_t)count)
    {
        logger.Error("AddressLibrary: short read on %s\n", path);
        m_entries.clear();
        return false;
    }

    // Databases ship sorted by id; sort anyway so lower_bound is always valid.
    if (!std::is_sorted(m_entries.begin(), m_entries.end(),
        [](const Mapping& a, const Mapping& b) { return a.id < b.id; }))
    {
        std::sort(m_entries.begin(), m_entries.end(),
            [](const Mapping& a, const Mapping& b) { return a.id < b.id; });
    }

    logger.Error("AddressLibrary: loaded %s (%llu ids)\n", path, (unsigned long long)count);
    return true;
}

bool AddressLibrary::FindOffsetById(std::uint64_t id, std::uint64_t& outOffset) const
{
    const Mapping key{ id, 0 };
    auto it = std::lower_bound(m_entries.begin(), m_entries.end(), key,
        [](const Mapping& a, const Mapping& b) { return a.id < b.id; });
    if (it == m_entries.end() || it->id != id)
        return false;
    outOffset = it->offset;
    return true;
}

bool AddressLibrary::FindIdByOffset(std::uint64_t offset, std::uint64_t& outId) const
{
    // Linear; only used for diagnostics / bootstrapping a new game version.
    for (const Mapping& m : m_entries)
    {
        if (m.offset == offset)
        {
            outId = m.id;
            return true;
        }
    }
    return false;
}

std::uintptr_t AddressLibrary::Resolve(std::uint64_t id) const
{
    std::uint64_t offset = 0;
    if (!FindOffsetById(id, offset))
        return 0;
    return (std::uintptr_t)(RelocationManager::s_baseAddr + offset);
}
