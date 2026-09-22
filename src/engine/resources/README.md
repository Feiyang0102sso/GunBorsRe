# Engine Resources

This directory implements the runtime path from the original Gun Bros resource
archives to decoded bytes and desktop textures. It reads the original files in
`big/`; it does not maintain a second hand-written resource database.

## Main flow

```text
packTOC_<artset>.dat
        |
        v
CResTOCManager                 all packs and cross-pack routing
        |
        v
CResPackTOC                    one pack's name table and initialization data
        |
        v
CBigFileReader                 BIG table lookup, block reads, decompression
        |
        +---- ordinary handle ------> top-level resource
        |
        +---- aggregate handle -----> CAggregateResource ----> sub-resource
```

`CResourceLoader` and `CImagePool` sit above this archive path. They schedule
model and image work and reuse live GPU textures; they do not define the BIG
format.

## Files

| File | Purpose |
| --- | --- |
| `CAggregateResource.h/.cpp` | Declares and implements indexed aggregate resources, sub-resource lookup, and the shared raw/zlib resource-block decoder. |
| `CArrayInputStream.h/.cpp` | Declares and implements bounded, little-endian sequential reads over decompressed resource payloads. |
| `CBigFileReader.h/.cpp` | Declares and implements BIG headers, table1/table2 lookup, resource block reads, decompression, and aggregate-handle resolution. |
| `CImagePool.h/.cpp` | Loads PNG resources into repeating `ZTexture` objects and weakly reuses them by pack, handle, and OpenGL context. |
| `CResourceLoader.h/.cpp` | Registers pack ranges, reads model bytes, queues functions and images, cancels requests, and executes work in approximately 15 ms slices. |
| `CResPackTOC.h/.cpp` | Loads one pack's Name Table and `___INIT_DATA`, installs locale/aggregate tables, and maps hashed names to handles or scalar values. |
| `CResTOCManager.h/.cpp` | Reads big-endian `packTOC_<artset>.dat`, creates and binds all packs, identifies the core pack, and routes pack hashes. |
| `IDSNames.inc` | Preserves recovered original `IDS_*` symbolic names as a C++ initializer fragment and generator input. It is not runtime data. |
| `IDSNames.csv` | Generated catalog of each recovered name, its hash, matching pack and handle, string ID, resolved current-BIG text, and resolution status. |

## Format invariants

- `packTOC*.dat` is big-endian; BIG archives and their resource payloads are
  little-endian.
- A logical resource ID is not a table2 index or a file offset. Table1 maps a
  logical ID range to table2.
- The value after a `packTOC` record selects that pack's Pack Name Table, not
  `___INIT_DATA`.
- A Pack Name Table maps `CStringToKey(name)` to a value. Most values are
  handles, but named count entries may be plain scalars.
- Ordinary handles address top-level resources. Handles with bit 29 set address
  a sub-resource inside an aggregate selected through initialization data.
- Missing or unknown values must remain visible; readers must not invent
  offsets, IDs, or fallback resource data.

## Binary templates

The maintained 010 Editor templates are format references for the runtime
readers. Check them together with original bytes and the original consuming
functions when changing a parser.

| Template | Related runtime code |
| --- | --- |
| [`pack_toc.bt`](<../../../tools/binary template/big_assets/big_archive/pack_toc.bt>) | `CResTOCManager`: pack names and Pack Name Table logical IDs. |
| [`big_archive.bt`](<../../../tools/binary template/big_assets/big_archive/big_archive.bt>) | `CBigFileReader`: BIG header, table1, table2, resource blocks, and footer. |
| [`big_name_table.bt`](<../../../tools/binary template/big_assets/big_archive/big_name_table.bt>) | `CResPackTOC`: hashed names and their handle/scalar values. |
| [`big_init_data.bt`](<../../../tools/binary template/big_assets/big_archive/big_init_data.bt>) | `CResPackTOC::LoadInitData` and `CBigFileReader`: locale and aggregate lookup tables. |
| [`big_keyset.bt`](<../../../tools/binary template/big_assets/big_archive/big_keyset.bt>) | Handle encoding used by game keysets and resource references. |
| [`string_pack.bt`](<../../../tools/binary template/big_assets/string_pack.bt>) | `CAggregateResource`: the indexed aggregate layout used by the string pack. |

## IDS name catalog

`IDSNames.inc` contains comma-separated string literal expressions. It is not a
constant by itself and therefore has no `=`. Its valid C++ use is as an
initializer fragment:

```cpp
static constexpr const char *const kIdsNames[] = {
#include "engine/resources/IDSNames.inc"
};
```

The runtime does not need this array. 
