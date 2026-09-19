#include "tests/research/ProfileStudy.h"
namespace ProfileImportDetail {
std::uint64_t ReadUInt64(CArrayInputStream &stream) {
    const std::uint64_t low = stream.ReadUInt32();
    return low | (static_cast<std::uint64_t>(stream.ReadUInt32()) << 32);
}

GameObjectRef ReadMemoryRef(CArrayInputStream &stream) {
    GameObjectRef ref;
    ref.packHash = stream.ReadUInt32();
    stream.Skip(2); // Cached runtime pack index is reconciled from the hash.
    ref.localIndex = stream.ReadUInt8();
    stream.Skip(1); // Native structure alignment, not a wire-format field.
    // Original empty slots may retain a core-pack hash with local index 255.
    // Normalize that native sentinel to the rebuilt profile's empty reference.
    if (ref.localIndex == 255) { ref = {}; }
    return ref;
}

bool ReportReference(CGunBros &tables, CResTOCManager &toc, const GameObjectRef &ref,
    unsigned type, std::ofstream &report) {
    if (ref.IsNull() || ref.localIndex == 255) { report << " none"; return true; }
    const int packIndex = toc.GetPackIndexFromHash(ref.packHash);
    if (packIndex < 0) { report << " missing-pack=" << ref.packHash; return false; }
    report << ' ' << tables.GetPackName(ref.packHash) << ':' << unsigned(ref.localIndex) << " type=" << type;
    std::vector<std::uint8_t> payload;
    return tables.ReadSectionResource(ref.packHash, static_cast<ZGameSection>(type + 1), ref.localIndex, payload);
}

}

// Historical importer notes; ImportNative now uses all registered clients.
// The legacy check importer uses the same standard samples as NativeProfile.
// CRefinementManager::BeginRefinement :178519 consumes the QWORD this+44.
// Bullet caches are reconstructed from the equipped guns.
// Server dirty flag and native alignment.
// WasWavePerfected :192487 reads a 4096-bit array after the secondary
// wave value. The disk reference occupies two more bytes than RAM.
// BOKOR is retained in the archive but is not a retail 500-wave planet.
// SaveRestore registration :80431 and CWeaponMastery::SaveToServer :192239
// identify 1013 as weapons3. The older 1005 collection is not current XP.
// Native dirty flag and padding, following the disk reference.
