/** BIG keyset: u16 count followed by packed u32 resource handles.
 * big_keyset.bt / CKeysetResource::Load :359730. No runtime pointer array on disk.
 */
#ifndef GUN_BROS_RE_CKEYSETRESOURCE_H
#define GUN_BROS_RE_CKEYSETRESOURCE_H
#include "engine/resources/CResPackTOC.h"
#include "engine/resources/CArrayInputStream.h"
class CKeysetResource {
public:
    bool Load(CResPackTOC &pack, const char *name) {
        std::vector<std::uint8_t> bytes;
        if (!pack.GetResource(pack.GetResValue(name), bytes) || bytes.size() < 2) { return false; }
        CArrayInputStream input(bytes);
        const unsigned count = input.ReadUInt16();
        if (bytes.size() != 2 + count * 4) { return false; }
        handles.clear();
        for (unsigned index = 0; index < count; ++index) { handles.push_back(input.ReadUInt32()); }
        return input.Available() == 0;
    }
    std::vector<unsigned> handles;
};
#endif
