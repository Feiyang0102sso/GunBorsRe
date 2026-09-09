# Gun Bros 逆向重建约定

本文件合并原 `agent.md` 与 `AGENTS.md`，是本项目代理工作约定的唯一维护入口。回复、方案、任务清单和研究文档均使用中文。Implementation Plan, Task List and Thought in Chinese。

## 工作目标与授权

- 最终交付可玩的 Windows 游戏，行为和数据尽量对应 iOS 原版。
- 正式版主线：每个星球仅一张生存地图、四面刷怪，共 50 波 × 10 轮（用户补充）。优先完成这个闭环。战役模式在 1.0.0 前已废弃；可恢复为独立研究入口，不能当作正式版必经流程。
- 用户已明确授权的工作直接推进，不重复请求批准；需要审核的新范围先完成调研并提出具体方案。2026-09-08 的八小时自主推进属于历史阶段授权，记录见 `docs/overnight-progress.md`，不自动延长为其他时段的授权。
- 执行顺序：调研原版依据 → 记录阶段方案与验收标准 → 分解任务 → 实现 → 测试 → 更新结果，再进入下一阶段。
- 每阶段都验证，失败先定位修复；不得将全部改动积累到最后一次测试。
- 必要的独立研究能力做成 milestone 并加入 EXE 菜单；游戏完成后仍永久保留已有里程碑及单项研究入口。
- 常规运行、截图及自动测试统一传入 `--mute`；静音不得关闭游戏逻辑或跳过资源验证。

## 数据来源与复刻要求（必须遵守）

- **原版资源数据必须从 BIG 获取，禁止用硬编码替代。** 正式游戏运行时直接读取 `big/` 的原版归档，按原索引解析资源及依赖；`big_360_out/`用于研究、样本核对和调试，不另建一套手写运行时数据源。
- 此限制包括 UI 坐标/尺寸/锚点、Movie 章节/关键帧/时长、Sprite 动画/图集映射、文本、商品价格与属性、武器/盔甲/强化配置、星球/任务/关卡/刷怪配置、模型与资源引用。凡原资源已有的值，均由读取器取得；不能照截图、凭经验或照现有重建代码重新填表。
- **禁止把硬编码搬进 JSON、CSV、配置文件或生成脚本后称为数据驱动。** 可生成解析缓存和研究索引，但必须能从原 BIG 重建、保留来源与版本关系；它们不能成为人工维护的另一份游戏事实来源。
- **实现前同时查对应 `.bt` 与原反编译代码。** BT 用来理解磁盘结构、字段用途和查找消费函数；它是研究结果，不是最终权威。出现冲突时回查原始字节、读取器和消费者，修正模板及研究记录，不让原资源迁就错误模板。
- **按原源码的组织和执行思路复刻。** 追踪资源加载、对象绑定、状态转换、脚本出口、原生调用及绘制/更新顺序，保留数据与原生逻辑的职责。不能只做视觉近似，也不能把嵌入 Flow 行为改成按物品 ID 分支的手写效果。
- BIG 并不包含所有执行逻辑。原程序明确实现的算法、枚举、协议常量、状态分派与派生计算应按源码实现，并注明依据；这不是许可手写资源表。Windows 输入、渲染、音频等平台适配明确放在适配层，不冒充原版数据或改变玩法逻辑。
- UI 通常由多个 Movie、Sprite、商品/星球等条目、存档状态及菜单代码共同组装。按原引用和绑定流程组合，不能假定一个 bin 包办整页，也不能因组件缺实现而手填整页坐标或动画。
- **找不到或读不懂数据时保留未知，不造数。** 记录原路径、包/类型/局部序号、文件偏移、原值、已查函数和影响；定位缺失、格式异常或未实现分支。必要的临时研究显示须明确标为未验证，不得静默回退到假价格、假布局、假奖励或默认解锁状态并混入正式流程。
- 现有重建代码中的硬编码、历史文档和截图只能作查找线索，不能反证原版。用户当前指令与已核对的一手证据优先于旧计划中的假设；有冲突须在相关记录中说明。
- 类名、文件名尽量对应原版符号；有证据时纠正偏离，同时保留注释、更新引用及工程文件。新增宿主适配层明确职责，不冒充原版类。
- 无法解释的内容记录证据、尝试、影响与待用户核对项；可暂按疑似废案隔离，但不能将缺实现直接断言为原版废案。

## 资料路径与查阅条件

项目根目录：`E:\coding_projects\c_projects\gun_bro_re`。下列路径均相对于此目录；按本次修改涉及的类型查阅，不必每次加载全部资料。

| 何时查阅 | 路径与用途 |
|---|---|
| 读取任何游戏资源 | `big/`：原始 BIG 与包目录；`big_360_out/`：对应解包样本及解包 CSV，核对物理文件与原归档位置。 |
| 新增或修改任何资源读取器 | `docs/binary-template-catalog.md`：全类型入口；`_Big_tool/binary template/big_assets/`：格式模板；`_Big_tool/binary template/big_assets/entries/`：实体/商品条目；`_Big_tool/binary template/big_assets/maps/`：地图。 |
| 核对字段含义和原实现 | `gunbros`：带符号的 iOS 主程序；`_IDA_OUT/gunbros_3.6.0_IOS.c`：反编译代码；`_IDA_OUT/source_tree.md`：原工程结构；`docs/entry-field-semantics.md`：字段消费证据。 |
| 修改 UI、菜单或动画 | `docs/ui-binary-templates.md`；`_Big_tool/binary template/big_assets/ui_movie.bt`及同目录的`sprite_*.bt`、`bitmap_font.bt`；同时核对原菜单的创建、绑定、输入和刷新代码。 |
| 修改脚本或实体行为 | `docs/flow-bytecode-reading.md`；`_Big_tool/binary template/big_assets/entries/common.bt`、`_Big_tool/binary template/big_assets/flow_bytecode.bt`、`_Big_tool/binary template/big_assets/flow_native_sources.bt`；`out/binary-research/flow-disassembly/`：按实际 bin 解码的可读清单。 |
| 修改存档、解锁、进度、库存或奖励 | `saves/`：原件；`docs/save-binary-templates.md`：ID与原类对应；`_Big_tool/binary template/saves/`：封装及负载模板；必须同时查对应客户端的读写、同步和消费方法。实际目录名为`saves/`，不是`save/`。 |
| 查某个 bin 的偏移/数值/覆盖状态 | `out/binary-research/coverage.json`指向各字段目录；`out/game-entry-catalog.json`、`out/ui-movie-catalog.json`及`out/binary-research/*-catalog.json`保留逐文件证据。 |
| 查原函数、文件或重新验证研究结果 | `out/binary-research/source-index.json`、`out/binary-research/template-source-links.json`；`src/tools/catalog_*.py`等研究工具；`docs/binary-research-progress.md`记录命令、异常及验证边界。 |
| 开始一个实现阶段 | `PLAN.md`、`docs/overnight-progress.md`：总体状态与阶段交接；`docs/acceptance.md`：可玩验收；`docs/original-name-map.md`：原名和宿主适配边界。 |
| 修改武器、盔甲或战斗 | 分别查`docs/player-weapon.md`、`docs/armor.md`、`docs/arena.md`；盔甲双模型并非总是两位兄弟变体。 |

- 模板中的源码行号指 `_IDA_OUT/gunbros_3.6.0_IOS.c`；原工程文件路径来自符号，不等于恢复了原 `.cpp` 行号。`mem+N`是运行时成员偏移，解析索引的`offset`才是该文件内偏移，不得混用。
- 包 hash、运行时包索引、类型内 ordinal、资源 handle、逻辑 ID、物理文件号和 BIG 偏移分别处理。不能从文件名或单个字节猜包号；存档引用也不能直接套用 BIG 的引用结构。
- `_Big_tool/binary template/`和`out/`可能被 Git 忽略，先检查本地实际文件；索引缺失时按研究记录重新生成。BT 用于辅助理解，没有通过010 Editor执行验证；结构核对通过也不代表每个字段用途已经证实。
- `10-17-2011/`是内容不完整且有 bug 的官方旧 PC 版，用户估计约2.4.0但版本未证实；`flow scripts/`版本未知。两者仅供补充参考，不能覆盖当前 iOS 二进制、BIG 和实物存档证据。

## 编码与文档

- KISS，可读性优先；使用含义明确的命名和基础控制流，精准处理异常，保留用户全部注释。
- 必要的注释和 docstring 说明用途、原版依据、单位与原因；解析/行为实现注明对应 BT、原类/函数及源码位置，区分已确认、推断与未知。必要日志用简明英文，并带资源定位信息。
- Python 使用项目 `.venv`（存在时先执行`.\.venv\Scripts\Activate.ps1`），路径优先 `pathlib`；避免列表/字典/集合推导式、三元运算与 lambda。不用无意义的try-catch掩盖错误。
- 有依据的代码常量统一组织，Python大量常量集中在文件开头；资源数据仍从BIG读取，不能以“常量集中管理”为由复制原数据表。
- 项目技能放 `.agents/skills/<skill_name>/`。

## 验收与交接

- 资源解析改动须核对原样本的字段顺序、宽度、字节序、计数、引用和文件边界；遵循原磁盘序列化，不能把运行时内存对齐直接当磁盘padding。发现异常保留原值，不靠跳字节或猜长度制造“解析成功”。
- 行为和UI改动须能说明“数据来自哪个原资源、由哪个原函数消费、复刻代码在哪里执行”，并验证相关实际流程。移除相关硬编码时检查是否仍有静默默认值或按ID补丁掩盖缺数据。
- 测试保留命令、退出码和必要截图；数据缺失、未验证行为和版本差异明确列出。
- 不把成功构建、模型查看器或空场地战斗等同于完成游戏；可玩验收需覆盖进入关卡、移动射击、敌人调度、受伤死亡、重开和进度反馈。
- 优先完成闭环，再补细节；保留每阶段已有验证能力。`big/`、`big_360_out/`、`saves/`及`gunbros`原件只读，实验使用副本；原版资源及存档不提交版本库。
