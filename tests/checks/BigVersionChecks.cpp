/** Exercise automatic format selection through real TOC/BIG readers.
 * Small synthetic archives isolate shifted media sections, string ordinals,
 * mixed versions and malformed directories without depending on old IPAs.
 */
#include "TestOutput.h"
#include "engine/core/CStringToKey.h"
#include "gun_bros_re/data/store/CStoreItem.h"
#include "gun_bros_viewer/scenes/ResourceInfo.h"
#include <cstdio>
#include <fstream>
#include <vector>

namespace {
using Bytes = std::vector<std::uint8_t>;
constexpr unsigned kFirstSectionId = 10;
constexpr unsigned kStringHandle = 0x21010007;

void Append16(Bytes &bytes, unsigned value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
}

void Append32(Bytes &bytes, unsigned value) {
    Append16(bytes, value);
    Append16(bytes, value >> 16);
}

bool WriteBytes(const std::filesystem::path &path, const Bytes &bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    return output.good();
}

void AddPackRecord(Bytes &toc, const std::string &name) {
    const std::string key = name + ":TABLEOFCONTENTS";
    toc.push_back(static_cast<std::uint8_t>(key.size() >> 8));
    toc.push_back(static_cast<std::uint8_t>(key.size()));
    toc.insert(toc.end(), key.begin(), key.end());
    // The TOC payload is resource ID 2. This directory alone is big endian.
    toc.insert(toc.end(), {0, 0, 0, 2});
}

bool MakeFixture(const std::filesystem::path &directory, unsigned types, unsigned sections,
                 bool plain, const std::string &shortName = "pack0_core", bool truncated = false,
                 bool wrongCounts = false) {
    std::string fullName = shortName;
    std::string tocName = "packTOC.dat";
    if (!plain) { fullName += "_xga"; tocName = "packTOC_xga.dat"; }
    std::vector<Bytes> payloads(4 + sections);
    std::vector<unsigned> groups(payloads.size(), kGroupBin);

    // String aggregate: explicit ID 7, 16-bit offset 12, final offset 22.
    Bytes &strings = payloads[0];
    Append16(strings, 0); Append16(strings, 1);
    Append16(strings, 7); Append16(strings, 12); Append32(strings, 22);
    strings.insert(strings.end(), {4, 0, 0, 0, 'H', 'e', 'l', 'l', 'o', 0});
    groups[0] = kGroupStringPack;

    Bytes &init = payloads[2];
    // No locales; aggregate selector 1 points to physical resource ID 1.
    for (unsigned index = 0; index < 4; ++index) { Append32(init, 0); }
    Append32(init, 1); Append32(init, 1);

    Bytes &keyset = payloads[3];
    Append16(keyset, sections + 1);
    groups[3] = kGroupKeyset;
    for (unsigned index = 0; index < sections; ++index) {
        unsigned tag = 0x03000000;
        if (index == types) { tag = 0x02000000; groups[4 + index] = kGroupPng; }
        if (index == types + 1) { tag = 0x09000000; groups[4 + index] = kGroupWav; }
        Append32(keyset, tag | (kFirstSectionId + index));
        payloads[4 + index] = {static_cast<std::uint8_t>(index)};
    }
    Append32(keyset, kStringHandle);
    if (truncated) { keyset.pop_back(); }
    payloads[4 + types] = {'P', 'N', 'G'};
    payloads[5 + types] = {'W', 'A', 'V'};
    payloads[6 + types] = {'M', 'E', 'S', 'H'};
    Bytes &counts = payloads.back();
    counts.assign(types + 1, 1);
    counts[0] = static_cast<std::uint8_t>(types);

    Bytes &names = payloads[1];
    Append32(names, 3);
    Append32(names, CStringToKey(kInitDataResourceName)); Append32(names, 3);
    Append32(names, CStringToKey(kGameTocKeysetName)); Append32(names, 0x05000004);
    Append32(names, CStringToKey(kObjectScriptCountsName));
    unsigned countsId = kFirstSectionId + sections - 1;
    if (wrongCounts) { --countsId; }
    Append32(names, 0x03000000 | countsId);

    const unsigned resourceCount = static_cast<unsigned>(payloads.size());
    const unsigned dataOffset = 48 + resourceCount * 8 + 8;
    unsigned fileSize = dataOffset;
    for (const Bytes &payload : payloads) { fileSize += 4 + static_cast<unsigned>(payload.size()); }
    Bytes big = {'F', 'G', 'I', 'B'};
    Append16(big, 1); Append16(big, 0x80);
    Append32(big, 32); Append32(big, 2);
    Append32(big, 48); Append32(big, resourceCount);
    Append32(big, dataOffset); Append32(big, fileSize - dataOffset);
    Append32(big, 1); Append16(big, 4); Append16(big, 0);
    Append32(big, kFirstSectionId); Append16(big, sections); Append16(big, 4);
    unsigned blockOffset = dataOffset;
    for (unsigned index = 0; index < resourceCount; ++index) {
        Append32(big, groups[index]); Append32(big, blockOffset);
        blockOffset += 4 + static_cast<unsigned>(payloads[index].size());
    }
    Append32(big, 0); Append32(big, fileSize);
    for (const Bytes &payload : payloads) {
        big.insert(big.end(), {4, 0, 0, 0});
        big.insert(big.end(), payload.begin(), payload.end());
    }
    Bytes toc;
    AddPackRecord(toc, fullName);
    return WriteBytes(directory / (fullName + ".big"), big) && WriteBytes(directory / tocName, toc);
}

bool CheckValid(const std::filesystem::path &directory, CGameObjectPack::BigVersion expected) {
    CGameObjectPack::BigVersion detected = CGameObjectPack::BigVersion::Unknown;
    if (!DetectViewerBigVersion(directory.u8string(), detected) || detected != expected) { return false; }
    CResTOCManager toc;
    if (!toc.InitAuto(directory.u8string()) || !toc.Bind()) { return false; }
    CGunBros tables(toc);
    CGameObjectPack &objects = tables.GetObjectPack(0);
    CResPackTOC &pack = *toc.GetPack(0);
    const ZGameSection media[] = {ZGameSection::Png, ZGameSection::Wav, ZGameSection::Mesh};
    const Bytes expectedMedia[] = {{'P', 'N', 'G'}, {'W', 'A', 'V'}, {'M', 'E', 'S', 'H'}};
    for (unsigned index = 0; index < 3; ++index) {
        Bytes payload;
        if (!tables.ReadSectionResource(pack.GetPackHash(), media[index], 0, payload) ||
            payload != expectedMedia[index] || objects.GetSectionSpan(media[index]) != 1) { return false; }
    }
    // An absent newer object type cannot resolve to an older media section.
    for (unsigned number = objects.GetTypeCount() + 1; number <= 28; ++number) {
        if (objects.GetHandle(static_cast<ZGameSection>(number), 0) != 0) { return false; }
    }
    CGameAssetRef stringRef;
    stringRef.packHash = pack.GetPackHash();
    stringRef.assetId = 0;
    if (objects.GetStringHandle(0) != kStringHandle || tables.ReadString(stringRef) != "Hello") { return false; }
    stringRef.assetId = 1;
    if (!tables.ReadString(stringRef).empty()) { return false; }
    return tables.HasLatestBigVersion() == (expected == CGameObjectPack::BigVersion::V1);
}
}

int RunBigVersionCheck() {
    const std::filesystem::path root = TestOutput::Path("big-version-fixtures");
    int failures = 0;
    const unsigned types[] = {28, 27, 26};
    for (unsigned index = 0; index < 3; ++index) {
        const auto directory = root / ("format-" + std::to_string(index + 1));
        if (!MakeFixture(directory, types[index], types[index] + 5, false) ||
            !CheckValid(directory, static_cast<CGameObjectPack::BigVersion>(index + 1))) { ++failures; }
    }
    const auto plain = root / "plain";
    if (!MakeFixture(plain, 26, 31, true) || !CheckValid(plain, CGameObjectPack::BigVersion::V3)) { ++failures; }
    const auto invalid = root / "invalid";
    CGameObjectPack::BigVersion detected = CGameObjectPack::BigVersion::V1;
    if (!MakeFixture(invalid, 27, 33, false) || DetectViewerBigVersion(invalid.u8string(), detected) ||
        detected != CGameObjectPack::BigVersion::Unknown) { ++failures; }
    if (!MakeFixture(invalid, 28, 33, false, "pack0_core", true) ||
        DetectViewerBigVersion(invalid.u8string(), detected)) { ++failures; }
    if (!MakeFixture(invalid, 28, 33, false, "pack0_core", false, true) ||
        DetectViewerBigVersion(invalid.u8string(), detected)) { ++failures; }
    if (!MakeFixture(invalid, 25, 30, false) || DetectViewerBigVersion(invalid.u8string(), detected)) { ++failures; }
    // Prefer XGA even if it is corrupt; never silently load a different set.
    const auto priority = root / "xga-priority";
    if (!MakeFixture(priority, 26, 31, true) || !WriteBytes(priority / "packTOC_xga.dat", {}) ||
        DetectViewerBigVersion(priority.u8string(), detected)) { ++failures; }
    const auto mixed = root / "mixed";
    if (!MakeFixture(mixed, 28, 33, false) || !MakeFixture(mixed, 26, 31, false, "pack1")) { ++failures; }
    Bytes toc;
    AddPackRecord(toc, "pack0_core_xga"); AddPackRecord(toc, "pack1_xga");
    if (!WriteBytes(mixed / "packTOC_xga.dat", toc) || DetectViewerBigVersion(mixed.u8string(), detected)) { ++failures; }
    std::printf("[big-version-check] three-formats, plain/XGA, media, strings, malformed/mixed failures=%d\n", failures);
    return failures;
}
