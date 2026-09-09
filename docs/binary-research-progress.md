# 全资源 Binary Template 研究记录

## 本轮范围与执行顺序

用户授权约两小时研究，开始时间2026-09-09 01:47 UTC，目标截止03:47 UTC。优先实际数据中的未覆盖格式，再深入现有模板中的未知字段。交付辅助理解的`.bt`、逐文件覆盖索引、原代码依据与只读验证结果，不要求010 Editor运行兼容性。

1. 盘点13包全部解压资源、已有模板、`saves/`全部21个文件；保存原存档SHA256基线。
2. 存档：外层封装、记录ID与原类映射、逐字段负载、同步数据和PDST；保留用户原注释，用追加说明纠正有证据的旧推测。
3. BIG：补子弹、粒子、掉落、奖品、道具场景物、关卡、成长、挑战、多人、教程、SpriteGlu及资源索引等格式。
4. 现有模板：补中文用途、读取/消费方法、原内存偏移、原文件名；结构布局只按原代码和实际样本确定。
5. 对实际样本批量检查边界、计数、引用和存档CRC；建立已覆盖、局部覆盖、无样本、未知格式清单。

## 证据规则

- 权威依据：`_IDA_OUT/gunbros_3.6.0_IOS.c`、`gunbros`、实际BIG与存档；重建代码只作定位线索。
- 模板中的源码行号均属于上述反编译文件，原工程文件名有符号或路径证据才列出。
- 不把内存对齐当成所有bin都存在的padding：存档整块内存写出与资源逐字段序列化须区分。
- 一个模板可覆盖同一结构的多个文件；覆盖并不等于每个脚本的全部游戏行为均已理解。
- 原始资源、存档保持只读；研究产物写模板目录、`docs/`、`src/tools/`、`out/binary-research/`。

## 阶段结果

已完成全目录格式覆盖、逐字段补注、只读扫描和最终交叉引用审核。以下数量来自生成器及实际文件清单，不按猜测补齐。

- 13包共5586个资源：3429个bin、1570个PNG、276个WAV、13个文本、298个零字节资源的.ref占位。另有26个解包CSV元数据文件。
- 5288个非空资源中5285个通过结构/格式核对，3个保留布局异常；没有未归类或缺对应解析证据的非空资源。
- 54份`.bt`包含游戏对象、Sprite、Movie、模型地图、资源索引、标准图片声音、Flow和6份存档模板/共享定义。模板用于理解，未执行010 Editor。
- 21个存档全部解析；20个带封装存档CRC-32/BZIP2全部匹配；21个SHA256与研究前基线完全一致。
- 675个含脚本文件、6187个顶层代码块全部按字节边界解码；246个实际原生函数ID对应12种宿主的24个Function/VariableResolver。
- 675份逐文件`.flow.txt`展开语句、嵌套条件/事件、资源槽、变量初值、状态和原生函数证据。不是恢复出的原始源码，也没有执行脚本。
- 原二进制恢复17627个反编译函数导航，其中11360个关联原工程文件路径；原.cpp行号不可得。模板中只把反编译行号标为行号，不冒充原工程行号。

## 主要新增解释

1. 补全BULLET、PARTICLEEFFECT、PLAYER、PLAYERPROGRESSION、ENEMY、PROP、LEVEL、TILESET、CHALLENGE、DAILYBONUS、PRIZE、MP_MATCH、PICKUP、SOUNDEFFECT、TUTORIAL等15类1221条；最初8类740条另行复核。BANK复用STORE，没有虚构独立银行格式。
2. 将枪熟练度第4组表追到升级时的玩家经验奖励；子弹尾部两项追到击退速度与持续毫秒；区分敌人世界模型与UI预览的缩放规则。
3. 粒子补充8通道消费、圆环半径/厚度、发射间隔和寿命单位；碰撞顶点是整数转float，不能当16.16。地图、mesh、动作范围、帧声音和资源依赖也有独立模板。
4. Sprite补充动画/映射方案/贴图页关系、blend标志和全屏填充分支；字体补充有符号char、正文高度、字距/行距、glyph advance和控制字符差别。
5. Flow列出原生case读取实参的完整相关源码、原文件、地址和实物调用示例；额外人工解释枪、掉落与强化参数。特别保留8.8秒、整数秒、整数毫秒的差别。
6. 存档拆出进度、装备、购买集合、关卡完美波位图、击杀/熟练度集合、赠送经验环、22项教程、12槽精炼、签到、47统计、活动报价、内容位图、挑战v5和PDST。
7. 纠正统计语义：单局击杀/波数为历史最大值；LOTTERY只写至少1；BRO_BUFFS重新统计当前达标数；金币余额是u64，但最大金币统计的形参是u32；AUTOAIM枚举对应的实际更新发生在StartAutoFire。
8. 原程序找到22项menuDataCategoryForTutorialType，见[原字节证据](../out/binary-research/tutorial-menu-evidence.json)。唯一TUTORIAL条目若按字符串解析会得到Welcome/跳跃提示，但未发现直接消费者，不据此宣布其正式用途或废弃状态。

## 三份布局异常


- `big_360_out/pack3_xga/0xf4e02223/pack3_xga_0347_0x213c4b.bin`：unpack_from requires a buffer of at least 21 bytes for unpacking 1 bytes at offset 20 (actual buffer size is 20)。
- `big_360_out/pack4_xga/0xf4e02223/pack4_xga_1155_0x2e445b6.bin`：unpack_from requires a buffer of at least 21 bytes for unpacking 1 bytes at offset 20 (actual buffer size is 20)。
- `big_360_out/pack9_xga/0xf4e02223/20_PROP/pack9_xga_0074_0x41dd.bin`：truncated_original_move_bodies。

两个Sprite原型都只有20字节，当前读取器在offset20需要继续读；没有为了通过检查改用猜测的旧版本布局。PROP的moveCount=4但动作体尾部只剩4字节；保留原文件和失败位置。

## 复核命令与结果

工作目录为项目根，解释器为`D:\Python312\python.exe`；本项目没有`.venv`。下表省略相同解释器前缀，均为实际执行的命令。完整输出、时间和退出码见[verification-log.json](../out/binary-research/verification-log.json)。这些是只读数据扫描，不是游戏功能测试。

| 脚本命令 | 退出码 | 核对范围 |
|---|---:|---|
| `src/tools/catalog_game_entries.py` | 0 | 最初8类740条及分区索引 |
| `src/tools/catalog_ui_movies.py` | 0 | 175个Movie与全部关键帧边界 |
| `src/tools/catalog_save_records.py` | 0 | 21存档、20 CRC、21原件哈希 |
| `src/tools/catalog_binary_sections.py` | 0 | 新增15类1221条；PROP异常单列 |
| `src/tools/catalog_sprite_resources.py` | 0 | 818结构，816通过、2异常；664贴图页引用 |
| `src/tools/catalog_geometry_resources.py` | 0 | 334模型、22地图、23依赖表、13计数表 |
| `src/tools/catalog_resource_containers.py` | 0 | 82资源容器、字符串聚合包及字体 |
| `src/tools/catalog_standard_resources.py` | 0 | 1570 PNG逐块CRC、276 PCM WAV边界、14文本 |
| `src/tools/inventory_binary_resources.py` | 0 | 5586实际资源类型与原文件盘点 |
| `src/tools/catalog_flow_bytecode.py` | 0 | 675文件、6187顶层代码块，0解码错误 |
| `src/tools/index_flow_native_sources.py` | 0 | 24读取器、246实际native ID及原case参数证据 |
| `src/tools/render_flow_disassembly.py` | 0 | 675份可读Flow清单 |
| `src/tools/annotate_binary_source_links.py` | 0 | 54模板的源码、地址、原文件导航 |
| `src/tools/build_binary_coverage.py` | 0 | 实际文件集合与索引集合完全一致，0遗漏 |

## 交付与边界

- [全资源入口](binary-template-catalog.md)按类型链接模板；[存档说明](save-binary-templates.md)按文件ID链接解析结构；[字段用途核对](entry-field-semantics.md)汇总主要消费者；[Flow索引](flow-bytecode-reading.md)逐文件导航。
- 大型数组只记录偏移与长度，不声称所有模型/动画已渲染。部分没有实际样本的分支仅由源码描述。
- 尚未完全确认的包括：PLAYER.reference104、POWERUP两个状态字节、PLANET第三精灵、PROP部分附加参数、PARTICLE通道6/7、字体未明字节、部分宿主变量及原生参数、1001配置尾部、1018记录值、PDST完整状态语义。
- 结构通过与语义确认分开标注。已扫描全部文件不等于穷尽全部游戏逻辑；未知字段没有用手写假数据替代。
- 本轮不改引擎、原BIG、解包资源或存档，不写回游戏验证；保留原模板中用户注释，以补注说明纠正旧推测。
- `_Big_tool/binary template/`及`out/`被当前仓库规则忽略，但产物均在本地真实存在并可直接打开；没有强制加入版本库。

当前模板合计9659行（含原有注释、结构和源码摘录），444个模板到函数的导航关联。该数量只描述产物规模，不作为语义完整率。

## 最终产物核对

- 文档内1718个本地链接全部存在；54个模板的include目标全部存在；675份Flow清单均已生成。
- 逐行比对研究前模板的独立注释行，没有丢失；已保留旧观察，并在旁边解释已证实的修正。
- 关键JSON可读取，模板与关键索引的SHA256已记录在[final-audit.json](../out/binary-research/final-audit.json)。
- `git diff --check`退出0；仅有仓库现有LF/CRLF转换提示。未提交、未强制加入被忽略文件。

最终核对完成时间（UTC）：2026-09-09T03:43:00.565959+00:00。本轮约两小时的调查到此交付，未明事项已逐项保留。
