# 全资源 Binary Template 阅读入口

范围：`big_360_out/`全部13包，以及`saves/`全部21个原存档。以iOS原程序和实际字节为依据；模板用于辅助理解，不要求010 Editor运行兼容性。

当前 **5586 个实际资源文件**全部归类：**5288 个非空资源、298 个零字节资源的.ref占位**。已提供 **54 份.bt**（包含共享定义、银行入口、标准格式及源码查找说明）；一个格式模板会覆盖多个bin。

按实际扩展名：3429个.bin、1570个PNG、276个WAV、13个文本、298个.ref。另有26个解包CSV元数据文件，不算游戏资源。

非空资源中 **5285 个通过对应结构/格式核对，3 个保留明确的布局异常，0 个缺少解析证据**。通过核对表示长度、计数与结构边界一致，不表示每个字段的游戏语义、每条动画或脚本行为都已证明。

## 从哪里开始读

- 界面布局与动画：[ui_movie.bt](<../_Big_tool/binary template/big_assets/ui_movie.bt>)；具体175个Movie的名字和文件见[UI索引](ui-binary-templates.md)。
- 图像组装：[sprite_global.bt](<../_Big_tool/binary template/big_assets/sprite_global.bt>) → [sprite_archetype.bt](<../_Big_tool/binary template/big_assets/sprite_archetype.bt>) → [sprite_texture_map.bt](<../_Big_tool/binary template/big_assets/sprite_texture_map.bt>) → PNG。
- 商店与实体：[STORE](<../_Big_tool/binary template/big_assets/entries/store_entry.bt>)、[ARMOR](<../_Big_tool/binary template/big_assets/entries/armor_template.bt>)、[GUN](<../_Big_tool/binary template/big_assets/entries/gun_template.bt>)、[POWERUP](<../_Big_tool/binary template/big_assets/entries/powerup_template.bt>)；银行是STORE分类14–16，见[bank_entry.bt](<../_Big_tool/binary template/big_assets/entries/bank_entry.bt>)，没有另造银行磁盘格式。
- 星球/选关：[PLANET](<../_Big_tool/binary template/big_assets/entries/planet_entry.bt>) → MISSION → LEVEL → TILELAYER；玩家解锁/完成情况来自存档。
- 行为脚本：[common.bt](<../_Big_tool/binary template/big_assets/entries/common.bt>)解释CScript容器；[flow_bytecode.bt](<../_Big_tool/binary template/big_assets/flow_bytecode.bt>)解释操作码和操作数；[flow_native_sources.bt](<../_Big_tool/binary template/big_assets/flow_native_sources.bt>)按宿主class/函数ID列出原生参数读取代码。
- 存档：[存档索引和封装说明](save-binary-templates.md)；不要把BIG的4/5字节ObjectRef套到存档8字节内存引用。

## 数据与引擎的边界

BIG包含布局时间线、精灵动画树、图集映射、商店文字/价格/数值表、实体参数、编译脚本、地图和模型；一个bin通常只是其中一种记录或组件。菜单代码负责取哪些条目、建立列表、给用户区域绑定输入、查询存档状态、触发Movie片段；脚本又调用原生移动、碰撞、射击等行为。引擎既有解析器，也有具体游戏逻辑，不能用‘一份bin完整规定一切’或‘数据都在代码里’概括。

可以读到的原版数据应从原资源读取；运行时计算值、派生几何和原生控制逻辑应按源码实现。未知字段保留原位值与查证边界，不能拿重建代码中的手写常量证明原版数据。

## 全类别覆盖表

数量按物理资源文件，不对内容去重。‘空’取resources.csv的原始大小，.ref文字不是原负载。

| 类别 | 非空 | 空 | 模板 | 核对结果 |
|---|---:|---:|---|---|
| ACHIEVEMENT | 0 | 13 | [empty_sections.bt](<../_Big_tool/binary template/big_assets/entries/empty_sections.bt>) | 无非空样本；见源码/占位说明 |
| ACHIEVEMENTLIST | 0 | 13 | [empty_sections.bt](<../_Big_tool/binary template/big_assets/entries/empty_sections.bt>) | 无非空样本；见源码/占位说明 |
| APP_PROPERTIES_TEXT | 1 | 0 | [text_resource.bt](<../_Big_tool/binary template/big_assets/text_resource.bt>) | 结构/格式核对通过 |
| ARMOR | 233 | 11 | [armor_template.bt](<../_Big_tool/binary template/big_assets/entries/armor_template.bt>) | 结构/格式核对通过 |
| BITMAP_FONT | 13 | 0 | [bitmap_font.bt](<../_Big_tool/binary template/big_assets/bitmap_font.bt>) | 结构/格式核对通过 |
| BULLET | 177 | 7 | [bullet_template.bt](<../_Big_tool/binary template/big_assets/entries/bullet_template.bt>) | 结构/格式核对通过 |
| CHALLENGE | 238 | 7 | [challenge_template.bt](<../_Big_tool/binary template/big_assets/entries/challenge_template.bt>) | 结构/格式核对通过 |
| COUNTS | 13 | 0 | [object_counts.bt](<../_Big_tool/binary template/big_assets/object_counts.bt>) | 结构/格式核对通过 |
| DAILYBONUS | 1 | 12 | [daily_bonus_template.bt](<../_Big_tool/binary template/big_assets/entries/daily_bonus_template.bt>) | 结构/格式核对通过 |
| EMPTY_RESOURCE_PLACEHOLDER | 0 | 8 | [empty_sections.bt](<../_Big_tool/binary template/big_assets/entries/empty_sections.bt>) | 无非空样本；见源码/占位说明 |
| ENEMY | 78 | 5 | [enemy_template.bt](<../_Big_tool/binary template/big_assets/enemy_template.bt>) | 结构/格式核对通过 |
| GUN | 76 | 11 | [gun_template.bt](<../_Big_tool/binary template/big_assets/entries/gun_template.bt>) | 结构/格式核对通过 |
| INIT_DATA | 13 | 0 | [init_data.bt](<../_Big_tool/binary template/big_assets/init_data.bt>) | 结构/格式核对通过 |
| KEYSET | 30 | 0 | [big_keyset.bt](<../_Big_tool/binary template/big_assets/big_keyset.bt>) | 结构/格式核对通过 |
| LEVEL | 23 | 8 | [level_template.bt](<../_Big_tool/binary template/big_assets/entries/level_template.bt>) | 结构/格式核对通过 |
| LEVELPROGRESSION | 0 | 13 | [empty_sections.bt](<../_Big_tool/binary template/big_assets/entries/empty_sections.bt>) | 无非空样本；见源码/占位说明 |
| LEVEL_REFS | 23 | 8 | [level_requirements.bt](<../_Big_tool/binary template/big_assets/level_requirements.bt>) | 结构/格式核对通过 |
| MISSION | 61 | 8 | [mission_entry.bt](<../_Big_tool/binary template/big_assets/entries/mission_entry.bt>) | 结构/格式核对通过 |
| MISSIONOBJECTIVE | 4 | 12 | [mission_objective_entry.bt](<../_Big_tool/binary template/big_assets/entries/mission_objective_entry.bt>) | 结构/格式核对通过 |
| MODEL | 334 | 3 | [mesh.bt](<../_Big_tool/binary template/big_assets/mesh.bt>) | 结构/格式核对通过 |
| MOVIE | 175 | 0 | [ui_movie.bt](<../_Big_tool/binary template/big_assets/ui_movie.bt>) | 结构/格式核对通过 |
| MP_MATCH | 5 | 12 | [mp_match_template.bt](<../_Big_tool/binary template/big_assets/entries/mp_match_template.bt>) | 结构/格式核对通过 |
| NAME_TABLE | 13 | 0 | [name_table.bt](<../_Big_tool/binary template/big_assets/name_table.bt>) | 结构/格式核对通过 |
| PACK_LABEL_UTF8 | 13 | 0 | [text_resource.bt](<../_Big_tool/binary template/big_assets/text_resource.bt>) | 结构/格式核对通过 |
| PARTICLEEFFECT | 249 | 6 | [particle_effect.bt](<../_Big_tool/binary template/big_assets/entries/particle_effect.bt>) | 结构/格式核对通过 |
| PICKUP | 9 | 12 | [pickup_template.bt](<../_Big_tool/binary template/big_assets/entries/pickup_template.bt>) | 结构/格式核对通过 |
| PLANET | 6 | 7 | [planet_entry.bt](<../_Big_tool/binary template/big_assets/entries/planet_entry.bt>) | 结构/格式核对通过 |
| PLATFORM | 0 | 13 | [platform_template.bt](<../_Big_tool/binary template/big_assets/entries/platform_template.bt>) | 无非空样本；见源码/占位说明 |
| PLAYER | 1 | 12 | [player_template.bt](<../_Big_tool/binary template/big_assets/entries/player_template.bt>) | 结构/格式核对通过 |
| PLAYERPROGRESSION | 1 | 12 | [player_progression.bt](<../_Big_tool/binary template/big_assets/entries/player_progression.bt>) | 结构/格式核对通过 |
| PNG | 822 | 0 | [png_texture.bt](<../_Big_tool/binary template/big_assets/png_texture.bt>) | 结构/格式核对通过 |
| PNG_OUTSIDE_GAME_SECTION | 748 | 0 | [png_texture.bt](<../_Big_tool/binary template/big_assets/png_texture.bt>) | 结构/格式核对通过 |
| POWERUP | 20 | 12 | [powerup_template.bt](<../_Big_tool/binary template/big_assets/entries/powerup_template.bt>) | 结构/格式核对通过 |
| PRIZE | 39 | 12 | [prize_entry.bt](<../_Big_tool/binary template/big_assets/entries/prize_entry.bt>) | 结构/格式核对通过 |
| PROP | 265 | 6 | [prop_template.bt](<../_Big_tool/binary template/big_assets/entries/prop_template.bt>) | 264通过，1布局异常 |
| REFINEMENT | 1 | 12 | [refinement_entry.bt](<../_Big_tool/binary template/big_assets/entries/refinement_entry.bt>) | 结构/格式核对通过 |
| SOUNDEFFECT | 128 | 4 | [sound_effect.bt](<../_Big_tool/binary template/big_assets/entries/sound_effect.bt>) | 结构/格式核对通过 |
| SPRITE_ARCHETYPE | 396 | 0 | [sprite_archetype.bt](<../_Big_tool/binary template/big_assets/sprite_archetype.bt>) | 394通过，2布局异常 |
| SPRITE_GLOBAL | 13 | 0 | [sprite_global.bt](<../_Big_tool/binary template/big_assets/sprite_global.bt>) | 结构/格式核对通过 |
| STORE | 339 | 9 | [store_entry.bt](<../_Big_tool/binary template/big_assets/entries/store_entry.bt>) | 结构/格式核对通过 |
| STRING_AGGREGATE | 13 | 0 | [string_pack.bt](<../_Big_tool/binary template/big_assets/string_pack.bt>) | 结构/格式核对通过 |
| TEXTURE_MAP | 396 | 0 | [sprite_texture_map.bt](<../_Big_tool/binary template/big_assets/sprite_texture_map.bt>) | 结构/格式核对通过 |
| TEXTURE_MAP_GLOBAL | 13 | 0 | [sprite_texture_pages.bt](<../_Big_tool/binary template/big_assets/sprite_texture_pages.bt>) | 结构/格式核对通过 |
| TILELAYER | 22 | 8 | [map.bt](<../_Big_tool/binary template/big_assets/maps/map.bt>) | 结构/格式核对通过 |
| TILESET | 6 | 8 | [tileset.bt](<../_Big_tool/binary template/big_assets/maps/tileset.bt>) | 结构/格式核对通过 |
| TUTORIAL | 1 | 12 | [tutorial_entry.bt](<../_Big_tool/binary template/big_assets/entries/tutorial_entry.bt>) | 结构/格式核对通过 |
| WAV | 268 | 2 | [wav_audio.bt](<../_Big_tool/binary template/big_assets/wav_audio.bt>) | 结构/格式核对通过 |
| WAV_OUTSIDE_GAME_SECTION | 8 | 0 | [wav_audio.bt](<../_Big_tool/binary template/big_assets/wav_audio.bt>) | 结构/格式核对通过 |

## 异常与验证边界

- [pack3_xga_0347_0x213c4b.bin](<../big_360_out/pack3_xga/0xf4e02223/pack3_xga_0347_0x213c4b.bin>): unpack_from requires a buffer of at least 21 bytes for unpacking 1 bytes at offset 20 (actual buffer size is 20)
- [pack4_xga_1155_0x2e445b6.bin](<../big_360_out/pack4_xga/0xf4e02223/pack4_xga_1155_0x2e445b6.bin>): unpack_from requires a buffer of at least 21 bytes for unpacking 1 bytes at offset 20 (actual buffer size is 20)
- [pack9_xga_0074_0x41dd.bin](<../big_360_out/pack9_xga/0xf4e02223/20_PROP/pack9_xga_0074_0x41dd.bin>): truncated_original_move_bodies
- 两个Sprite原型只确认与当前读取器不兼容；尚未确认是其他版本、损坏还是未消费资源。异常后的字段值是失败位置记录，不是认可的有效结构。
- PROP异常尾部原样保留；moveCount=4而后面只有4字节，原读取器至少需要20字节。没有通过改长度规则掩盖异常。
- PLATFORM虽然无当前非空样本，原读取器明确读取7字节SpriteGluRef；模板单列为源码分支。ACHIEVEMENT/ACHIEVEMENTLIST无工厂分配分支；LEVELPROGRESSION仅找到4字节运行时Progression对象，没有虚构磁盘表。
- 几何解析核对索引范围及每帧字节边界，大型float数组保留偏移/元素数，未逐个渲染或证明所有浮点值在游戏里可用。PNG逐块CRC通过；WAV全部为PCM，核对RIFF尺寸和块对齐。
- Sprite映射/原型依赖包级计数；不能脱离global文件仅靠单个bin推出所有外部参数。UI Movie的文字轨、地图Movie层/PathLink区域等无本批样本的分支以源码证据标注。
- 本次未修改游戏实现、原BIG或原存档；没有运行010 Editor。原模板的用户注释保留，新增‘修正’说明指出旧猜测的边界。

## 逐文件和逐字段证据

[完整覆盖JSON](../out/binary-research/coverage.json)逐文件给出类型、模板、验证状态与字段目录位置；使用`path`搜索文件名即可。

| 证据 | 内容 |
|---|---|
| [game-entry-catalog.json](../out/game-entry-catalog.json) | entries数组；使用path匹配原资源，含实际数值/偏移或格式验证信息 |
| [ui-movie-catalog.json](../out/ui-movie-catalog.json) | movies数组；使用path匹配原资源，含实际数值/偏移或格式验证信息 |
| [section-catalog.json](../out/binary-research/section-catalog.json) | entries数组；使用path匹配原资源，含实际数值/偏移或格式验证信息 |
| [geometry-catalog.json](../out/binary-research/geometry-catalog.json) | entries数组；使用path匹配原资源，含实际数值/偏移或格式验证信息 |
| [sprite-catalog.json](../out/binary-research/sprite-catalog.json) | entries数组；使用path匹配原资源，含实际数值/偏移或格式验证信息 |
| [container-catalog.json](../out/binary-research/container-catalog.json) | entries数组；使用path匹配原资源，含实际数值/偏移或格式验证信息 |
| [standard-catalog.json](../out/binary-research/standard-catalog.json) | entries数组；使用path匹配原资源，含实际数值/偏移或格式验证信息 |
| [flow-bytecode-catalog.json](../out/binary-research/flow-bytecode-catalog.json) | 675个含代码文件，6187个顶层代码块，包含嵌套语句和原生调用位置 |
| [flow-native-sources.json](../out/binary-research/flow-native-sources.json) | 实际原生调用ID、原类、读取参数代码与源码行号 |
| [全部Flow可读清单](flow-bytecode-reading.md) | 675份逐文件伪代码，保留指令偏移、操作数token、状态/依赖、已证实native参数注释 |
| [template-source-links.json](../out/binary-research/template-source-links.json) | 每份模板的原函数、ARMv7地址、N_SO原工程文件路径 |
| [source-index.json](../out/binary-research/source-index.json) | 原二进制符号与反编译函数导航 |

模板中`:行号`均指`_IDA_OUT/gunbros_3.6.0_IOS.c`；`mem+N`是原对象内存偏移；JSON的`offset`才是对应文件的偏移。原工程文件名来自调试符号，原.cpp行号未恢复，二者不能混写。

完整执行命令与退出码见[本轮记录](binary-research-progress.md)。
