#include "gun_bros_viewer/scenes/ResourceInfo.h"
#include "gun_bros_re/data/CGameObjectPack.h"
#include "engine/resources/CResTOCManager.h"
#include <cstdio>

bool DetectViewerBigVersion(const std::string &bigDirectory, BigVersion &version, bool detailed) {
    version = BigVersion::Unknown;
    CResTOCManager toc;
    if (!toc.InitAuto(bigDirectory) || !toc.Bind()) { return false; }
    BigVersion detected = BigVersion::Unknown;
    for (unsigned index = 0; index < toc.GetPackCount(); ++index) {
        CResPackTOC &pack = *toc.GetPack(index);
        CGameObjectPack objects;
        if (!objects.Init(pack)) { return false; }
        if (detected == BigVersion::Unknown) { detected = objects.GetBigVersion(); }
        if (detected != objects.GetBigVersion()) {
            std::printf("[big-version] mixed formats: %s has BigVersion=%u, expected %u\n",
                pack.GetShortName().c_str(), static_cast<unsigned>(objects.GetBigVersion()), static_cast<unsigned>(detected));
            return false;
        }
        if (detailed) {
            std::printf("[big-version] %s BigVersion=%u types=%u sections=%u strings=%u\n",
                pack.GetShortName().c_str(), static_cast<unsigned>(detected),
                objects.GetTypeCount(), objects.GetSectionCount(), objects.GetStringCount());
        }
    }
    version = detected;
    std::printf("[big-version] auto selected BigVersion=%u packs=%u; format family only\n",
        static_cast<unsigned>(version), toc.GetPackCount());
    return version != BigVersion::Unknown;
}
