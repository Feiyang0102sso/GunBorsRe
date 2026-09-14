# 本地多好友与 BRO BUFF 原版依据

日期：2026-09-14。范围：可编辑本地好友名单、等级与装备配置、BRO BUFF 计算与实际消费。本文只记录研究与接入建议，不表示实现完成。源码行号均指 `_prep/_IDA_OUT/gunbros_3.6.0_IOS.c`，不是原 `.cpp` 行号。

## 结论

原版 BRO BUFF 按**好友列表总数**解锁，不按好友等级、在线人数或选中出战的兄弟计算。`CFriendPowerManager::GetAggregateLevel :234020` 虽然叫 level，返回的是传入的好友数量；`GetFriendCount :198813` 读取远端好友列表数量。默认兄弟不属于该好友列表。Windows 本地好友加入列表后计入这个数量，符合用户本轮明确授权的本地服务替代方案。

`CalculateAggregates :234455` 本身没有联网或好友在线状态判断。`CProfileManager::HandleFriendListUpdate :202779–202807` 在好友列表更新成功后重算，`CGunBros` 菜单恢复路径 `:79723–79724` 也重算。由此可以在本地名单加载后立即计算，并在正常离线单人中使用；不能把“fake connection 当前关闭”当作自动清零依据。原版断网启动究竟恢复多少远端好友取决于 NGS 列表缓存，本地没有该服务器，不应伪造其缓存协议。

特别注意：**一个好友只开启每日金币 +10%**，不会马上提高战斗 XP；三个好友才开启战斗 XP +10%，九个好友才开启矿 +10%。不能为了“看得出已激活”而给一个好友所有加成。

## 原生阈值与公式

`CGunBros::Init :80477–80488` 明确调用 `Reset(10)` 和十次 `AddPercentModifier`。这张表是原程序算法常量，不是 BIG 商品属性，允许按原代码实现：

| 好友数阈值 | 类型枚举 | 新增百分比 | 含义 |
|---:|---:|---:|---|
| 1 | 7 | 10 | 每日奖励金币 |
| 2 | 2 | 5 | 移动速度 |
| 3 | 5 | 10 | 战斗经验 |
| 4 | 1 | 10 | 防御 |
| 5 | 3 | 10 | 精炼产出效率 |
| 6 | 7 | 10 | 每日奖励金币 |
| 7 | 2 | 5 | 移动速度 |
| 8 | 0 | 10 | 伤害 |
| 9 | 6 | 10 | Xplodium |
| 10 | 1 | 5 | 防御 |

`CalculateAggregates :234479–234496` 清零八类加成，依次处理 requirement ≤ friendCount 的条目，并把同类型百分比相加。`IsActive :234026` 使用相同阈值判断。`GetPercentMultiplier :234065` 返回 `100 + bonus[type]`，不是只返回 bonus。超过十个好友不会产生额外档位。

十好友最终汇总：伤害 +10%、防御 +15%、速度 +10%、精炼效率 +10 个百分点、经验 +10%、矿 +10%、每日金币 +20%。类型 4 掉落率有枚举和原文本，但本版本初始化表没有任何条目，始终无加成，汇总列表也跳过它（`CMenuFriends::FriendPowerSummaryCallback :195644` 根据最大倍率是否为 100 决定）。

| 消费者 | 原版计算及边界 |
|---|---|
| `CBrother::GetDamageMultiplier :136462–136516` | 盔甲伤害系数形成后，再乘 `P[0] / 100`。明确 `GameType != 3`，Death Match 不应用。 |
| `CBrother::HandleDamage :136670–136703` | 在现有 frenzy 防御处理后，伤害乘 `100 / P[1]`，不是直接减去 bonus%；之后才处理盔甲减伤。明确 `GameType != 3`。 |
| `CPlayer::UpdateMovement :101384–101445` | 原移动速度、枪与熟练度速度系数之后乘 `P[2] / 100`。此处没有与伤害相同的 GameType 3 排除，不能自行推断排除。 |
| `CLevel::OnEnemyKilled :119306`，特别 `:119641–119655` | 经验、矿分别在原 LEVEL / 盔甲 / promo 系数基础上乘 `P[5]/100` 和 `P[6]/100`，最后各自 `ceil`。不能对已取整奖励再乘，也不能把击杀奖励发给未获得击杀者。 |
| `CPlayer::GetAggregateXplodiumMultiplier :101139–101176` | 组合盔甲矿系数、限时 promo、`P[6]` 和 LEVEL 的矿百分比。接入奖励与拾取时要区分现有流程，防止同笔矿重复乘两次。 |
| `CRefinementManager::GetIntervalEfficiency :178164` 和 `BeginRefinement :178519–178552` | 原 BIG 基础效率 **加上** `(P[3] - 100) / 100.0`。提交槽位时保存计算后的效率，进行中的精炼不随好友列表变化重新计价。不是基础效率乘 1.1。 |
| `CDailyBonusTracking::CommitBonus :209500–209546` | 先原 `AwardPrize`，再加 `floor(prize.coins * P[7] / 100) - prize.coins`。只加普通金币，不加稀有币、经验或物品。 |
| `UpdateAndDrawMultiplier :161123–161190` | UI 经验/矿百分比组合 promo 与好友系数后取 `ceil`。画面显示必须用实际消费同源倍率，不能只亮好友卡片。 |

原 `CFriendPowerManager` 是进程内本地账户管理器；线上另一玩家的个人朋友列表不通过本文件中的加成公式取得。给本地模拟 bot 分配什么好友关系属于宿主配置，不能声称原版会把本地玩家的朋友圈自动复制给真人。应避免将加成写进 bot 的寻路/换枪逻辑；公共角色和奖励系统消费明确的账户加成即可。

## BIG、BT 与存档复核

- 阈值与类别来自上述原生初始化；名称来自 core BIG：`CFriendPowerManager::Init :234195` 逐一查 `IDS_FRIEND_POWER_DAMAGE/ARMOUR/SPEED/REFINERY_OUTPUT/DROP_RATE/EXPERIENCE/XPLODIUM/DAILY_BONUS`。不要再写一套手填价格、属性或 UI 字符串。
- BRO BUFF 页面继续使用原 Movie。研究索引定位：`GLU_MOVIE_BROBUFF_MENU` 为 core Movie 97、handle `0x03000508`、物理 180、归档偏移 `0x2a887c`；`GLU_MOVIE_BROBUFF_BOX` 为 Movie 98、handle `0x03000509`、物理 181、偏移 `0x2a898e`。索引为 `_prep/out/ui-movie-catalog.json`。已查 `big_assets/ui_movie.bt`：10 字节头、变长对象/关键帧、type 6 用户区域；区域编号与对象编号不能混用。
- 等级数据来自 core `PLAYERPROGRESSION` section 17 ordinal 0，handle `0x03000125`，样本 `_prep/big_360_out/pack0_core_xga/0xf4e02223/17_PLAYERPROGRESSION/pack0_core_xga_0038_0x6adb.bin`。本次只读核对 1620 字节：u16 经验计数 201 + 201 个 u32；u16 血量计数 201 + 201 个 u32；两个 i32 尾字段，边界正好 1620。SHA256 `502A32ED95D5B29B4DA725BAA6F9B6452910164B810FD4F74CA40504BE8417D3`。
- 对应 BT 为 `big_assets/entries/player_progression.bt` 与 `common.bt`：经验表是增量，原 `GetExperienceForLevel :193308` 累加；血量磁盘 u32、读取截 u16。配置 `level` 必须调用已有 `CPlayerProgress::Template::GetExperienceForLevel` 换算，不复制等级门槛或按等级线性造 XP/生命。
- `big_assets/entries/common.bt` 的 `ObjectRef` 是包 hash + 类型内 ordinal；类型由装备槽决定。用户文件只保存资源引用/装备状态，不允许写伤害、价格、模型路径替代 BIG。
- 已查 `saves/save_payloads.bt` 的 `PlayerConfigurationPayload` 和 `FriendDataPayload`：1001 为单个账户装备，1006 是 XP 礼物环。完整好友身份/名字/多个独立账户仍应保留为 Windows 扩展文件，不占用 1006。更完整来源见 [好友存档研究](live-friend-save-research.md)。

## 当前代码与最小接入建议

当前 `LocalBotFriend` 只有一个固定 Identity、公开 profile、selected 布尔；`OriginalSocialContent.cpp` 已复制原生十阶表用于展示，但好友数、汇总百分比仍写 0，列表固定两项。`GameFrontEnd.cpp`、`SurvivalRuntime.cpp`、`LocalOnlineServices` 仍使用单个指针或静态 BotName。

1. 本地名单采用带版本的文本 `.cfg` 即可。工程现无通用 JSON 库；`HostSettings.cpp` 已使用 `getline`、`key=value`、注释和 `istringstream`，`CProfileManager.cpp` 也已有基础文本读写。建议 section `[稳定身份]` 循环装载，名字取等号右边完整文本而不是按空格截断，不增加解析依赖。顶层保存选中身份/轮换游标。单项支持名字、level、brother、两枪、四盔甲槽及必要库存；装备值只有合法 BIG 引用。
2. 每个身份保留独立 `local-friends/<identity>/` 原格式账户。名单是用户可编辑的身份/初始化配置，战斗进度、买道具扣费、库存继续保存到各自账户。明确每次重读配置哪些字段覆盖账户：不能每次启动都将上次获得 XP 或消耗库存重置；可用每项配置版本决定显式重新应用。原单 bot 目录直接复用避免丢已有进度。
3. 名单加载后 `CProfileManager` 或共享 `CFriendPowerManager` 保持运行时好友数量。把原十阶表从 Social 展示移至公共 manager，由页面阈值、summary、战斗、每日、精炼共同读取。默认兄弟不计数；每个唯一有效 localbot 计一名；不按出战选择/配置等级/在线开关变动。
4. BROS 列表用真实数量循环，头像、名字、等级、预览模型都从当前条目读取；当前选择用稳定身份持久化，避免用户重排配置后选中另一个人。匹配可按游标轮换所有有效条目，返回统一好友记录，AI 不负责读取名单。
5. 实际接入点：`CombatScene.cpp` 的奖励 `ceil`、`ApplyHit` 中双方出入伤害分支、移动系数；`CDailyBonusTracking::CommitBonus`；`CRefinementManager::BeginRefinement` 和对应预览；`OriginalSocialContent.cpp` 的 +0%、0 friends、固定两项。现好友日志/菜单测试是“只有展示”，必须增加实际数值回归证明加成生效。

建议验证：0/1/3/5/10/11 好友档位；同类百分比相加；等级和出战选择不改 Buff；不同 bot 名字/装备/账号隔离；配置重排后选中身份不变；读错引用明确报错；1 好友每日金币取整且其他奖励不变；5 好友精炼只对新提交槽加效率；10 好友伤害、受伤、移动、经验/矿实际数值；原存档 1006 与样本资源不被扩展数据污染。

本文没有改实现，没有启动游戏，也未声称验证了 NGS 服务端好友缓存或真人远端账户加成同步。
