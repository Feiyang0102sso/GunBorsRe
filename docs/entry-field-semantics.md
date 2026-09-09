# Binary Template 字段用途核对

本轮按用户要求，把用途写在 `.bt` 每个序列化字段旁。以 `_IDA_OUT/gunbros_3.6.0_IOS.c` 的读取器确定边界，再沿运行时成员的赋值、查询和消费函数核定语义；必要时回查 `gunbros` 原始字节。商品展示值、实体行为、界面布局分别追踪。

模板中的 `:数字` 指该反编译文件行号；`runtime64Raw` 等旧名中的数字是原对象的**内存偏移**，不是 bin 内固定偏移。保留旧名便于对应扫描 JSON；是否已确认，以紧邻注释为准。注释包含用途、单位、特殊值、引用目标和消费依据。没有消费者证据的字段写明已查到哪里，不借重建代码的变量名反证原版。

## 阅读入口

本页保留最初八类条目的细查记录，并补充本次全目录研究。全部54份模板与5586个资源的对应关系以[全资源入口](binary-template-catalog.md)为准；全部21份存档见[存档字段说明](save-binary-templates.md)，675个含代码bin见[逐文件Flow清单](flow-bytecode-reading.md)。

| 模板 | 本轮补充重点 |
|---|---|
| [store_entry.bt](<../_Big_tool/binary template/big_assets/entries/store_entry.bt>) | 分类与物品class、模式排除掩码、IAP标识、各展示数值组、隐藏/礼包/抽奖条件 |
| [bank_entry.bt](<../_Big_tool/binary template/big_assets/entries/bank_entry.bt>) | 18条银行商品的字段适用方式、发放数量和双向兑换实例；复用STORE结构 |
| [armor_template.bt](<../_Big_tool/binary template/big_assets/entries/armor_template.bt>) | 装备槽、两组挂件及节点、无mesh时替代PNG、装备脚本写入属性 |
| [gun_template.bt](<../_Big_tool/binary template/big_assets/entries/gun_template.bt>) | 基础开火间隔、持枪移速、索敌角度、暴击判断、各熟练度表、脚本出口及动作覆盖 |
| [powerup_template.bt](<../_Big_tool/binary template/big_assets/entries/powerup_template.bt>) | 不可用按钮动画、输入面板动画、提示粒子引用、冷却秒数及脚本职责 |
| [planet_entry.bt](<../_Big_tool/binary template/big_assets/entries/planet_entry.bt>) | 星图槽位、大小图实际资源取法、任务列表、额外引用与等级门槛 |
| [mission_entry.bt](<../_Big_tool/binary template/big_assets/entries/mission_entry.bt>) | 文本与实际条件的区别、关卡/目标引用、export2条件查询、波次进度锁定 |
| [mission_objective_entry.bt](<../_Big_tool/binary template/big_assets/entries/mission_objective_entry.bt>) | 完成提示、XP与Xplodium奖励以及防重复领取的运行时完成位 |
| [refinement_entry.bt](<../_Big_tool/binary template/big_assets/entries/refinement_entry.bt>) | 分钟转毫秒、收益百分比、两种解锁价格、重复count和门控判断 |
| [common.bt](<../_Big_tool/binary template/big_assets/entries/common.bt>) | 各引用格式、脚本导出/状态/变量/依赖、模型配置、动作范围与帧声音 |
| [ui_movie.bt](<../_Big_tool/binary template/big_assets/ui_movie.bt>) | 画布和时间轴、锚定/伸展、精灵映射、动画计时、声音模式、文字轨的证据限制 |

## 已核定的原始消费链

| 字段 | 实际用途与依据 |
|---|---|
| STORE.flagsRaw | `GetItemClass:156298`返回class；奖池按class相等筛选（159590），不是flags位掩码。具体class显示名仍未完整恢复。 |
| STORE.runtime8Raw | `IsItemExcludedFromGameType:156283`按`value & (1 << gameType)`排除模式。 |
| STORE.asset0Raw | `GetIapName:160238`读取字符串并生成IAP产品标识。 |
| STORE.runtime242Raw / singlePurchase / runtime244Raw | 普通列表隐藏过滤（155729）、礼包购买记录（155172、158173）、抽奖候选资格（159554）；三个字节各有用途。 |
| STORE.statGroups[7] | 升级菜单在393188读取下一熟练度列，并显示`IDS_UPGRADE_BUY_BUCKS`；是Warbucks升级费用来源。 |
| GUN.scalar144Fixed16 | `Bind:128862`复制到实例，玩家位移101430乘入该值；16.16持枪移动倍率。 |
| GUN.scalar152Fixed16 | `GetSeekAngle:128950`返回；`FireBullet:128155`在大于0时用于目标寻找。 |
| POWERUP.runtime124Raw | `SetupPowerUps:186300`传给选项；`DrawCoolDownTimer:184127`乘1000计算冷却遮罩，文件单位为秒。 |
| PLANET.layoutSelectorRaw | `CMenuMission::Bind:162800`将原对象+80作为星图数组槽位下标，放入星球图像与名称。 |
| MISSION.runtime64Raw | `IsLocked:164344`对类型1/2比较已完成波次进度，未达到阈值则锁定；不是玩家等级。 |
| MISSIONOBJECTIVE.runtime32Raw / runtime36Raw | `OnMissionObjectiveComplete:117353–117354`分别发放Xplodium与XP。 |
| ScriptState.sequence | `SetState:107323`及`Refresh:107271`依次播放MoveId；不是脚本语句号数组。 |
| ScriptState.enterCode | `Execute:107900`用于进入状态，`Evaluate:107863`还用同一主体评估事件；不能只理解为一次性的进入回调。 |
| MovieSpriteReference.selectorRaw | 边界计算58562将它放入精灵迭代器+1；`SetSprite:58084`用它索引原型内映射方案。它和动画编号分开。 |

商店八组数值的占位符顺序为`POWER / DMG / RPM / SPD / DEF / ATK / COINS / BUCKS`。原构造函数159454依次建立这些token，反编译把宽字符串截为首字母；`gunbros`内UTF-32LE原字节在文件偏移`0x3CB7E8 / 0x3CB800 / 0x3CB810 / 0x3CB820 / 0x3CB830 / 0x3CB840 / 0x3CB850 / 0x3CB868`保存完整文本。同一通用替换逻辑还有`CRIT`，但八组磁盘表之外的特殊分支不能被当成第九组继续读取。

## 仍未完全确认的字段

这些字段均已有边界和目前证据的注释，尚不能赋予确定的最终业务名称：

- POWERUP.runtime30Raw：本批全0，已查加载、使用及选择器，未定位有效消费；runtime112Raw已追到实例与选项状态的复制，但具体开关效果未核定。
- PLANET.sprite44：明确是第三个精灵引用，本批动画号255；两个已确认的星球图像创建函数不用它。object12Raw能对应额外Mission样本，尚无确定启动条件。
- MISSION.runtime66Raw：样本呈0..9轮次顺序，但独立消费者未核定。MISSIONOBJECTIVE.objectiveType本批全0，尚缺枚举分支证据。
- CompiledScript.discardedHeader：原读取器丢弃6字节；secondary表则有加载和存储，尚未核定业务用途。
- MeshMove.unknownRaw：已知动作记录内存+12，已查主要播放函数但未确认消费。
- MovieSpriteReference.frameControlRaw：原读取器保存字节，实际Refresh调用`GetCurrentFrame`按SpriteGlu帧时长计时；未发现这个文件字节控制定帧的依据。
- TextKeyframe五个非时间字段：Init保存到内存帧+30/+32/+36/+34/+38，但本版Refresh只定位关键帧，本批175个Movie无type3样本。保留读写位置，不能据此宣布原版文本功能废弃。

## 全目录研究新增的语义证据

下列信息已写回对应`.bt`字段旁，不只存放在这份摘要。

| 字段/结构 | 核定用途与消费证据 |
|---|---|
| GUN.masteryTables[3] | 跨熟练度门槛时追加玩家经验，`CGun::XPChanged :128380—128401`用表值乘经验基数再除100；结算基数来自本局玩家`ExperienceDelta :75967`，不是武器经验或暴击倍率。 |
| BULLET.mem+120 / +124 | 固定16.16的击退速度幅值和u16持续毫秒；碰撞137831—137847传给`CBrother::SetForce`，Update135201—135228按毫秒推进并用cos衰减。 |
| ENEMY.mem+132 / +134 | 前者参与世界模型缩放67563，后者绘制UI预览时除100并按视口缩放68564—68575；两字段不能使用同一换算规则。 |
| PLAYER.modelScale | 原读取器134585只把u16转float；Bind135793与Draw134969传递使用，没有在读文件时除100。 |
| PLAYERPROGRESSION.levelValues | 磁盘u32、加载193270截为u16、角色升级101253再按int16读取，用作基础最大生命；不能把磁盘宽度跟着内存改为u16。 |
| PRIZE.percentFixed16 / attributes bit1 | 好友使用奖励候选资格与随机接受阈值；`GetRandomFriendUsagePrizeIdx :210388—210405`先扰乱顺序，反复检查bit1并比较随机数。不是货币倍率或归一化权重。 |
| CHALLENGE.flags bit0/bit1 | bit0令局结束时未达到要求的局部计数清零241198—241214；bit1要求gameType=2，241622—241624的参数来自CGame+12、SetGameType74343。 |
| CHALLENGE.mem+120 / +122 | 挑战期间增加的玩家等级数与好友数，241296/241305使用与存档1017起始值的差；不是一个u32字段。 |
| CHALLENGE完成率 | UpdateChallengeStatusData241318—241332把有效条件的比例求平均、乘100、封顶100；不要自行改成逐条件全满足。 |
| PARTICLEEFFECT | 标量采用IEEE754 float；8个通道已确认0横缩放、1纵缩放、2整体缩放、3透明度、4旋转角、5速度倍率。圆环的末两数是厚度范围，不是角度。 |
| MP_MATCH | 掉落权重、辅助武器持续秒数，以及初始生命、击杀目标、比赛秒数、重生秒数；原重生路径乘1000后截u16的行为也保留。第二组并列表仍未发现完整消费。 |
| Sprite全局图元 | type=17走FillScreen59251，其余分支走FillRectAlpha；blendFlags的bit7、bit6决定原SetBlendMode参数59178—59192，59262—59276恢复，不凭位号猜混合枚举名。 |
| BitmapFont | 字符宽高、字距、行距在消费者中按有符号char使用；GetHeight395123、GetWidth395189、PaintText395375分开说明尺寸与前进距离。 |
| TUTORIAL与存档1007 | 唯一17字节模板没有覆盖22个教程弹窗。原程序ARMv7地址0x3C5180保存22项menuDataCategoryForTutorialType，ShowTutorial210850按存档槽索引选择菜单category。 |
| Flow参数单位 | CGun的0x0805是8.8秒，0x080F是整数毫秒；CPowerup护盾/Frenzy是8.8秒，而0x0F16自动开火使用整数秒。不能给所有时长统一除256。 |

扩展范围中的未知项还包括：PLAYER.reference104、PARTICLE通道6/7、PROP动作附加控制字段、部分Sprite替换模式的业务名称、PRIZE其余属性位、1001配置尾部、1018记录末值。模板记载已找到的读取、复制和消费者边界。三份结构异常另在全资源入口逐文件列出。

## 验证范围

任务顺序：核对原读取器与消费链 → 就地补注释 → 检查字段布局和完整文件边界。验收目标是每个字段可在模板内直接读到用途或明确的未知原因；不要求运行010 Editor。

只读扫描命令：

```powershell
D:\Python312\python.exe src/tools/catalog_game_entries.py
D:\Python312\python.exe src/tools/catalog_ui_movies.py
```

本轮两条命令均退出0。8类非空条目共740条完整读取：ARMOR 233、GUN 76、POWERUP 20、STORE 339、PLANET 6、MISSION 61、MISSIONOBJECTIVE 4、REFINEMENT 1；Movie 175个完整读取，无剩余字节。各文件与字段偏移分别见`out/game-entry-catalog.json`和`out/ui-movie-catalog.json`。

上述740是初始八类小计；新增15类1221个条目由`catalog_binary_sections.py`核对，其他Sprite、地图、模型、资源容器与标准格式有各自读取器。完整复核命令与退出码见[本轮记录](binary-research-progress.md)，不能把最初两条命令当作全部5586文件的检查。

此检查验证原文件的字节边界，不能代替字段语义证据，也不是010 Editor执行验证。原始BIG、解包文件和游戏运行时代码未修改。
