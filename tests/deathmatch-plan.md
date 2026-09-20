# Deathmatch PvP 实现与验收

2026-09-14 后续修正：本文原“每命两次商店／手雷／血包”限制现作为 Easy 保留。EXE 旁 `GunBrosRe.cfg` 的 `DMBotLevel=1/2/3` 对应 Easy/Normal/Hard，默认 `1`。Normal 仅标准手雷与血包无限供应，Hard 全部 PvP 合法道具无限供应；二者均不进商店，Live 不受影响。旧值 `2` 现对应 Normal。最后击杀后等待死亡和爆裂完整结束，再停留0.8秒并播放原淡出。最新实现与验收见 [结算动画及难度](../docs/deathmatch-ending-and-difficulty.md)。

日期：2026-09-14。用户已审核并授权实施。本地玩家对 Bot、五张原版地图、五档 MP_MATCH、独立 Bot 与菜单闭环已实现；最终构建和回归结果见文末。

## 需求与建议范围

参考 iOS 3.6.0 原版完成 Deathmatch 对战闭环，新增独立 Bot，包括双枪组合、战术换枪、争夺随机补给、掩体与绕障碍、道具、死亡复活、计分和结算。

本阶段为本地 1 对 1，匹配当前 BROS 激活的 Bot，未选择时使用第一位。2026-09-14 按用户最新要求恢复模拟联网开关限制：Deathmatch 与 Live 均需 `IsConnected=1`，断网显示原不可用弹窗；保留真实资源解锁。真实跨设备联机不在本次范围。下文离线匹配及离线再战属于修正前的历史验收记录，已被本次规则覆盖。

用户已确认以下限制仅作用于新 PvP Bot：每条命成功进入商店最多 2 次；只能主动使用标准手雷和血包，每种最多成功使用 2 次。死亡后在下一次实际出生时重置计数；关闭商店、换枪、拾取补给不能重置。库存不足、冷却未结束或使用被拒绝不扣使用次数。商店请求失败不算进入，成功打开后即计数，不能通过取消购买刷次数。次数上限不等于免费补满库存。

## 已核对的一手依据

以下源码行号均指 `_prep/_IDA_OUT/gunbros_3.6.0_IOS.c`，不是恢复出的原 CPP 行号。

| 内容 | 原版依据 | 实现含义 |
| --- | --- | --- |
| 匹配资源 | `CMPMatch::Template::Init` :395713；`entries/mp_match_template.bt` | 初始装备引用 STORE，临时武器引用 GUN；不能直接混用序号 |
| 开局 | `CLevel::InitDeathMatch` :114880 | 双方使用匹配定义的生命上限与初始双枪，复制击杀阈值和比赛时长 |
| 选择双枪 | `CMPMatch::GetWeaponLoadOut` :396216、:396252 | 经 STORE 实物引用得到 GUN；后一个入口读取各玩家选择的两个候选索引 |
| 随机武器 | `CLevel::GetMPMatchPickupId` :114825 | 0–100 掷骰、累计权重，排除上一次抽中的项；对象 ID 为 5678 加候选序号 |
| 补给落点 | `CEnemySpawner::SpawnMPMatchPickup` :146456 | 由 LEVEL 资源引用和指定路径层生成，不能手写场地坐标 |
| 拾取效果 | `CLevel::OnPickupCollected` 对应 :118325–118369；`CBrother::SetAuxGun` :136986 | 拾取临时武器，记录原武器槽，使用匹配定义的秒数；非永久账户装备奖励 |
| 复活 | `CLevel::RespawnPlayerForDeathMatch` :114940 | 使用脚本指定路径层和离对手最远的节点，恢复选定双枪 |
| 胜负 | `CLevel::DetermineMPMatchResults` :114412；`UpdateNormal` :121497–121558 | 击杀数判定、可选限时、超时比较比分及平局 |
| 道具 | `entries/powerup_template.bt`；`CPowerUpSelector::UpdateMPMatchCoolDownTimers` :183927 | 保留原 POWERUP/Flow 的使用条件、效果、库存和冷却；每命上限属于用户定制规则 |
| 碰撞与场景物 | `entries/prop_template.bt`；现有 `MapPropWorld`、地图碰撞与路径读取 | 使用真实对象层、身体和弹体碰撞，不能用绘制外框伪造掩体 |

已只读核对 `pack3_xga/0xf4e02223/28_MP_MATCH/` 的全部 5 个原始解包样本，按 BT 字段顺序读取，消费长度全部等于文件长度。运行时实现必须从 BIG 读取；这张表仅为核对记录。

| 局部序号 | 文件 | 字节数 | 尾部四个 u16 的文件偏移 | STORE 候选数 | GUN 候选数 | 生命 | 击杀上限 | 时限（秒） | 复活（秒） |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | `pack3_xga_0225_0x8470.bin` | 105 | 97 | 4 | 4 | 120 | 3 | 0 | 10 |
| 1 | `pack3_xga_0226_0x84bc.bin` | 105 | 97 | 4 | 4 | 250 | 3 | 0 | 12 |
| 2 | `pack3_xga_0227_0x850a.bin` | 88 | 80 | 4 | 3 | 280 | 3 | 0 | 12 |
| 3 | `pack3_xga_0228_0x8550.bin` | 122 | 114 | 4 | 5 | 360 | 3 | 0 | 12 |
| 4 | `pack3_xga_0229_0x85a1.bin` | 127 | 119 | 5 | 5 | 470 | 3 | 0 | 12 |

注意：第 2 项权重为 25、25、25，合计只有 75。原消费者的未命中区间会保留先前候选并继续排除重复，不能擅自把原表改成 100；实现前须结合初始状态确认首轮行为，并明确处理无法产生新候选的异常资源，避免无限循环。三个计数列在现存 5 个样本内一致；中间 i32 列全部为 1，消费者用途仍未知，不能用作伤害或奖励倍率。

地图脚本清单 `pack2_xga_0016_0x286e.flow.txt` 已发现：`@0xD0` 设置复活路径层，`@0x1D6` 抽取 15–30 的刷新等待值，`@0x1EF` 调用 `0x0C14` 生成多人武器补给。其他 pack7、9、11、12 也有该 native。完整地图与 Mission/MP_MATCH 的对应关系须在资源接入阶段继续从引用验证；不能仅凭文件名认定可选地图。

## 实施前缺口（现已接通）

- `BroAIDeathmatch` 名字与用途不符：目前是合作 Live 输入策略，会救援玩家、定时换枪、定时逛店和请求道具。不能直接修改成敌对逻辑而破坏合作模式。
- `StoreMenu.cpp` 仍明确显示 Death Match 未实现；需把入口、匹配、启动和重新比赛真正接通。
- `GameScriptObject::ResolveGameVariable(5)` 固定返回 0，原 PLAYER、POWERUP 等脚本的 Deathmatch 分支没有机会执行。
- `CEnemySpawner` 缺少 native 20（`0x0C14`）；已有普通拾取 native 18、19、21 不能替代它。
- 现有会话依赖合作救援、生存波次及进度保存；PvP 必须使用独立比赛生命周期和计分，避免串入生存结束条件或关卡奖励。
- 原版远端玩家通过网络参与战斗，新 Bot 是 Windows 输入适配，需要补全双方敌对目标、弹体和爆炸归属、受击与死亡处理。

## 已审核实施方案

1. 恢复原 `CMPMatch` 资源读取与运行状态，将模式显式传入 Flow、战斗和菜单。独立维护比赛比分、生命编号、复活倒计时、双枪选择及临时武器状态；复用现有角色、枪械、道具和渲染实现。
2. 保留合作 AI 原行为；新的 PvP Bot 使用独立 `DeathmatchBot`。为避免扩大改动，合作类仍保留旧名 `BroAIDeathmatch`，文档明确其用途。Bot 只提交移动、瞄准、开火、换枪、购物和道具请求，攻击执行原 CBrother/CGun/Flow。
3. 双方从当前匹配的原候选池选择两把枪。Bot 根据已读取的武器属性与可验证的射击表现选互补组合，避免重复选择和频繁抖动；根据射程、对方距离、视线、武器状态决定换枪。复活时恢复本人的选定组合，临时拾取枪不覆盖账户装备。
4. Bot 采用明确的战术状态：寻找对手、交火、争夺补给、退避至掩体、购物、等待复活。使用视线与最近观测位置，不穿墙精确追踪；加入有限反应时间与瞄准误差。绕障碍使用地图导航，脱困基于实际移动受阻；掩体选择检查能否挡弹及能否到达。
5. 接通地图脚本驱动的补给刷新、原 PICKUP 展示、加权抽取与避免连续重复、双方争抢唯一归属、临时武器到期恢复。场景物按原 PROP/Flow 处理碰撞、受损、销毁及掉落。主动血包限制与自动地面拾取分开记录。
6. 新 Bot 的每命购物和道具次数在动作执行入口统一校验，AI、调试请求及菜单共用。只允许标准手雷、血包，禁用其他主动道具及死亡复活道具；死亡统一进入比赛复活流程。道具是否成功由原 Flow/使用流程确认，随后扣次数。
7. 恢复原模式适用的选枪、商店、开场、比分、击杀消息、死亡等待、胜负和平局、REMATCH 与退出。UI 布局、文案、动画从原 Movie/Sprite/KEYSET 绑定；本地匹配提示明确表示 Bot 对战。
8. 平衡性先遵循原匹配档位的统一生命和受限武器池，核对原 PvP 分支对盔甲、熟练度、好友加成及伤害的处理。只对新增 Bot 的反应、误差和战术频率作明确的宿主参数调整；武器或奖励数值如需偏离原版，先用对局证据说明问题再单列方案。

## 阶段任务与验收标准

按阶段实现并验证，不把所有修改积累到最后：

| 阶段 | 交付 | 验收 |
| --- | --- | --- |
| 1 资源和模式 | 5 档 MP_MATCH、依赖解析、原地图关联、Flow 模式 | 原始字节边界、STORE→GUN 引用、未知列保留、模式隔离 |
| 2 对战闭环 | 敌对伤害、计分、死亡、复活、结束 | 双向普通弹/光束/爆炸命中；死亡只计一次；3 杀结束；限时和平局分支；安全出生 |
| 3 场地和补给 | 原刷箱调用、临时武器、掩体 | 原脚本实际刷新；权重边界；争抢仅一人获得；到期/死亡恢复；身体和子弹碰撞正确 |
| 4 Bot 和限制 | 双枪策略、视线、导航、道具、购物 | 第 3 次动作被拒绝；拒绝不误扣；复活重置；不使用禁用道具；绕障碍及脱困 |
| 5 菜单与结果 | 原 UI 完整流程、对局记录及重开 | 从菜单选模式和武器进入，完成比赛、查看结果、再战和退出；账户写入按原消费依据核对 |
| 6 综合验收 | Debug/Release 构建、实际对局、定向回归 | 多地图/档位/随机种子，交换双方位置检查明显偏置；Solo 和 Live 无行为回归 |

新增永久 `deathmatch` 专项并纳入 `tests/run.ps1`；测试代码仅在 `tests`。适用回归包括 `local-live`、`brother`、`player-death`、`powerup-play`、`powerup-selector`、`pickups`、`prop-combat` 和对应菜单检查。按实际改动决定运行范围，全部自动验证显式 `--mute`，保存命令、退出码、必要截图及原件完整性结果。

不将构建通过或孤立 Bot 能射击认定为完成。最终交付必须走完菜单进入、双向战斗、补给争抢、道具限制、死亡复活、胜负结算和再次开局。

## 实现结果与补充证据

- `Planet.object12` 指向 DM Mission：Cerberus pack2:15→LEVEL8、Haven pack7:11→LEVEL4、Ceres2 pack9:10→LEVEL1、Yeroc Sina pack12:10→LEVEL1、BOKOR pack11:10→LEVEL4；实际 MAP 继续解引用 LEVEL。星图选定 Deathmatch 星球后直接进入匹配和选枪，不经过生存波次页。
- 选枪使用 `GLU_MOVIE_GUN_LAYOUT`、`DEATHMATCH_GUN_CARD`、`DEATHMATCH_GUN_SLOTS` 和 `POWERUP_MENU_NEW`，STORE 提供枪图、名称、简短 POWER 属性和类别。2026-09-14 反馈修正：取消关前选档页面，随机比赛枪组后在关内商店选择双枪，允许关内继续修改。
- `CBrother::HandleDamage` :136705 在 DM 排除 BRO BUFF；保留真实盔甲防御、攻击和枪械熟练度。统一生命来自 MP_MATCH，不随比赛中升级改变上限。没有改写枪械伤害或商品价格。
- `CLevel::OnPlayerKilled` :118371–118710：对手死亡记分、击杀经验取 LEVEL 全局 XP 倍率、积分乘连杀与双方盔甲比例、export11 生成原概率掉落，统计37记录击杀。环境死亡也给另一方记分。比赛计分在同一逻辑帧末判定，支持同时达标平局。
- 初次入场和复活都使用 DM 出生路径，避免 Map 的生存 PLAYER 坐标位于封闭区域。复活同时等待资源倒计时和角色死亡动画完成。辅助枪到期、主动换枪和死亡恢复本人选定双枪；稳定的枪械对象保留旧动画引用和已发出的弹体。
- Bot 采用明确的搜索、交火、补给、掩体、死亡状态；有 240ms 初见反应和有限瞄准误差，保留 3.5s 观测记忆。目标来自原地图路径，A* 对当前地图及动态 PROP 身体碰撞绕行；短步处理贴墙离开，拐点按剩余距离行走。上述数值仅调节新增 Bot 输入策略。
- pack5 POWERUP13 为标准手雷，1/8/9 为三种血包。限制在真正动作入口检查，手雷在原动画发射事件后扣库存与次数；血包合计两次。库存、冷却、失败或取消请求、禁用道具、复活边界都独立处理。地面自动拾取不属于主动血包预算。
- Deathmatch 商店按原 `CPowerUpSelector::Show` 不暂停战场，无 Live 的共享请求锁和750ms延迟；玩家和 Bot 分别阻止自身输入。主动开店没有倒计时，入场/死亡开店有倒计时，死亡 RESUME 调用真实复活。Bot 实际打开才计数，按剩余预算与合并血包库存选择原商品，真实余额/等级不足不购买。
- 结算复用原多人布局和 `MDS_ICON_POSTGAME_MP_DM` 五行：击杀、死亡、矿石、经验、连杀；显示胜负、比分、积分，离线可再战。比赛消费和奖励写入各自账户，不写生存波次、不覆盖永久双枪；重复保存按累计差额入账。

## 验证记录

阶段一 `deathmatch-data` 已通过：五模板边界和引用、每档一万次合法且不连续重复的随机抽取、每命预算、玩家不受限、复活、三杀结束、限时和平局。

阶段三五图实际 Flow 验收通过。分别完成玩家三杀结束和 Bot 自主三杀结束；Bot 只能通过原武器发射造成伤害。自主测试使用站立玩家和无敌 Bot 隔离寻路/开火能力，不把它当作真人难度或胜率测量。不同地图采用种子42–46，五次完成约2–6分钟模拟时间。

阶段四命令 `pwsh -File tests/run.ps1 -NoBuild -Case deathmatch-data,deathmatch -TimeoutSeconds 600`：2/2通过，退出码0，原件和账户保护检查 `protected-changes=0`。图形专项耗时160.4秒，包含离线匹配、死亡暂停、商店只读、原版结果页及再战按钮；保存于 `tests/out/Core/deathmatch/`，后续运行脚本会清理重建此目录。

最终回归覆盖双向普通弹/光束/爆炸、争抢唯一归属、临时武器到期、环境死亡计分、真实存档保存两次并重载，以及 Solo/Live/道具/碰撞/菜单相关检查。

- Debug 与 Release 的游戏和查看器均构建成功，Debug 测试工程构建成功，退出码均为0。构建日志：`obj/deathmatch-debug-build.log`、`obj/deathmatch-release-build.log`、`obj/deathmatch-final-build-tests.log`。
- 首轮执行 `pwsh -File tests/run.ps1 -NoBuild -Case deathmatch-data,deathmatch,local-live,brother,player-death,powerup-play,powerup-selector,pickups,prop-combat,progress,offline-social,postgame-presentation -TimeoutSeconds 600`。其中10项通过；`offline-social` 的旧断言要求 Deathmatch 断线后取消匹配，与已批准的本地模式冲突，运行在此处停止。日志保存在 `obj/deathmatch-final-test.log`，该轮结果归档为 `obj/deathmatch-regression-results.json`。
- 更新离线检查：Live 断线仍取消匹配，Deathmatch 断线仍可进入本地匹配。随后执行 `pwsh -File tests/run.ps1 -NoBuild -Case deathmatch-data,deathmatch,offline-social,powerup-selector -TimeoutSeconds 600`，4/4通过，退出码0；耗时分别为0.6、122.4、11、1.7秒。日志为 `obj/deathmatch-final-followup.log`，最终结果为 `tests/out/results.json`。
- 两轮累计12项定向检查全部通过，最终复验 `known-data-issues=0`、`protected-changes=0`。`git -c core.safecrlf=false diff --check` 通过。
- 已检查选枪、实际战斗、死亡复活、Bot 商店及结算截图，保存在 `tests/out/Core/deathmatch/`。测试均通过运行器显式静音；未重复执行全量截图基线。

## 已知边界

MP_MATCH 中间 i32 列仍按原值保留，其用途没有新证据，不参与效果计算；BT 不明字段没有被补成手写数据。当前实现针对 iOS 3.6.0 的 BigVersion1。网络协议、真实多人同步、排行榜和好友礼物服务不在本地模式范围。新 Bot 的战术和同时帧裁定是明确的宿主策略；未宣称穷尽所有武器组合、随机种子或测得真人对局平衡。

## 2026-09-14 用户反馈修正

本节替代上轮对关前选枪、共用商店暂停和结算文案的错误描述。范围仍为本地玩家对 Bot；只限制新 Bot 的每命两次购物、两次标准手雷、合计两次血包。

| 反馈 | 实现及依据 | 验收方式 |
| --- | --- | --- |
| 进入后选枪、枪组和警示条 | `CGunBros::GetRandomMpMatchId` :78901 随机枪组；关内 `OriginalDeathmatchSelector.cpp` 绑定原 Movie。`CMPMatch::CreateWeaponLoadOutDescString` :396134 使用 STORE asset[5]、熟练度0，只显示 POWER/数值；类别取 `IDS_SHOP_SORT3` 原连续字符串。原章节3→4、6→2播放选中动画。删除旧关前选档页面。 | 枪卡点击、切换槽位、选中动画截图；连续换枪 |
| 死亡爆裂、无尸体、掉落 | 补全 `CBrother` native18可见性；PLAYER export2选择原爆裂特效；LEVEL export11发放原掉落。pack5 PICKUP0实际脚本给150矿石，PICKUP1给500经验，分别为原文件0352/0353。 | 真正死亡后不可见；解析并执行原掉落脚本 |
| Powerup 冷却 | 执行 POWERUP field124 冷却；原 sprite1:94，根据原帧时间映射冷却进度。冷却中数量徽章和商店 USE NOW 替换为圆形图标，禁止再次使用。 | 标准手雷圆形冷却截图、原道具使用回归 |
| 结算 UI / 动画 | 原 WRAPUP_SCREEN_MP、三项原按钮、胜负文案、KILLS TO WIN、双方状态灯、双列击杀/死亡等五行；DM没有助攻行，特效按原图标映射到正确粒子。 | 16ms逐帧结果页截图、滚动与 REMATCH 点击 |
| 炮塔攻击自己 | 召唤物保存施放者，选敌人目标，子弹判定排除主人；炮塔被删除后在途子弹仍保留归属。 | 实际召唤炮塔、检查敌对目标、直接命中主人被拒绝 |
| 卡顿、身体丢失 | Bot目的地筛选从逐候选最短路改为一次可达性遍历；网格寻路按当前碰撞建立局部边索引。身体绘制查找覆盖所有武器模型库，并跳过加载期间的空槽，避免换补给武器崩溃。 | 五图慢帧测量、连续切换全部补给武器、完整自主对局 |
| 远处对手指示 | Deathmatch复用Live的 `CLevelIndicator` 和原Sprite。 | 五图对局、方位状态检查及截图 |
| 对手血条 | `CLevel::DrawBrotherHealthBar` :120249 原30×4尺寸与绿色，按身体顶端投影；死亡后隐藏。 | 真场景生成血条，相关HUD回归 |
| KILLS/DEATHS与左侧消息 | 右上使用原两行本地计数；左侧使用原字符串与region3、最多五条、每条5000ms。 | HUD计分与原文本截图 |
| 死亡商店与复活动画 | 原关内商店自动打开，计时依据当前MP_MATCH剩余复活时间减1000ms；普通主动开店不限时。RESUME或超时调用PLAYER export8，恢复可见性、原粒子8、3000ms保护。玩家与Bot菜单独立，只阻止自身输入，战场继续。 | 提前RESUME、实际复活、原特效请求与三秒保护检查 |

证据差异：用户视频是2025年OpenSpy恢复的Android版本；当前工程资源为iOS 3.6.0。五套MP_MATCH原复活时间分别为10/12/12/12/12秒，不能复制视频数字覆盖原表；商店显示扣除原动画/退出预留时间后的倒计时。当前BIG确实有经验掉落，不能因视频未出现就删掉该脚本分支。排行榜与远端加好友不属于本地模式，保留原按钮外观，不伪造远端结果。

故障证据：`obj/deathmatch-feedback-red-3.log` 复现炮塔瞄准主人、尸体未隐藏，最慢逻辑帧505ms。分段测量确认最慢约509ms主要来自目的地筛选中的重复最短路查询；修复后五图同样输入最慢约11–31ms。这是Debug逻辑帧测量，不代表全游戏GPU帧率承诺。

首轮综合回归的 `postgame-presentation`、`player-death`、`local-live`、`deathmatch-data` 通过；长流程 `deathmatch` 捕获加载中空模型槽访问异常。修复后直接运行 `--deathmatch-check --mute` 五图完整对局通过，退出码0，记录为 `obj/deathmatch-feedback-followup.log`。原始失败结果另存 `obj/deathmatch-feedback-first-results.json`；这次失败已纳入连续补给换枪回归。

最终验证：Debug 测试工程、Debug 游戏与 Release 游戏均构建成功，退出码0；日志分别为 `obj/deathmatch-feedback-build.log`、`obj/deathmatch-feedback-debug-build.log`、`obj/deathmatch-feedback-release-build.log`。

执行 `pwsh -File tests/run.ps1 -NoBuild -Case deathmatch-data,deathmatch-feedback,deathmatch,local-live,player-death,powerup-play,powerup-selector,postgame-presentation,offline-social -TimeoutSeconds 600`，9/9通过，退出码0，`known-data-issues=0`、`protected-changes=0`。命令日志为 `obj/deathmatch-feedback-final-regression.log`，逐项结果为 `tests/out/results.json`。五图反馈检查的最慢逻辑帧分别为11.759、28.793、29.900、15.896、28.177ms；该指标只覆盖本专项的Debug逻辑更新。

已目视核对 `tests/out/Core/deathmatch/` 中的 `deathmatch-shop.png`、`deathmatch-gun-selected.png`、`deathmatch-cooldown.png` 和 `deathmatch-results.png`，确认简短枪卡、原槽位高亮、圆形冷却与结算特效位置。孤立商店测试使用黑色背景；实际场景截图在菜单入场动画起始帧捕获，不能用它判定菜单的最终展开位置。未重复执行全量截图基线。

## 后续过场与按钮反馈

本阶段修正开场滑动提示、道具使用消息、比赛结束淡出、商店默认页，以及单人/多人/Deathmatch结算按钮间距。沿用已授权复刻范围：先核对原函数和BIG，再按实际调用链补事件，最后进行定向回归。

- 红测命令：`bin/Debug/GunBrosTests.exe --deathmatch-feedback-check --mute --fixtures E:/coding_projects/c_projects/gun_bro_re/tests/fixtures/saves --test-output E:/coding_projects/c_projects/gun_bro_re/tests/out/dm-presentation-red`，退出码1。日志 `obj/deathmatch-presentation-red.log`：默认POWER UPS为false，实际图案间距40，原要求4。
- `CMenuMovieButton::Init` :144990 将region1的图案尺寸保存到+48/+50；region0是较大的触摸区域。`DrawModeToggleButtons` :185346 按图案宽度加4排列，`CMenuPostGame::CategoryCallback` :165166使用图案宽度加2。旧代码误用了region0尺寸。
- `CInputPad::OnDeathMatchStart` :89958 使用原START1/START2文本和WAVE_CLEARED动画；结束回调 :90035 才打开商店。`CPowerUpSelector::Show` :186531进入SetState(0)，每次打开默认POWER UPS，关内仍保留GUNS切换。
- `CInputPad::OnDeathMatchUsePowerUp` :87783给本地道具使用添加原console消息；`powerup_template.bt`与原消费确认field124为每种道具自己的冷却秒数，0表示无该配置冷却。
- `CGame::SetMissionWrapUp` :75619 载入原GLU_MOVIE_MISSION_END，完成后才进入结果页；不手写淡出曲线或等待时长。

验收：用图案边缘检查真实按钮间距，覆盖反复开店和换枪；开场提示完成后才自动开店；成功使用手雷和另一种道具各一条消息，失败使用无消息；结束后战场冻结、原过场完整播放、随后进入结果；复验Solo/Live和原菜单。

用户已澄清间距范围仅为各模式结算页的OVERVIEW、CASUALTIES/LEADERBOARDS、ADD BRO；不修改星图模式选择。

已完成连接：`SurvivalSession::Restart`→`SurvivalHud::BeginDeathmatch`→原WAVE_CLEARED完成事件→关内商店；成功道具消费/手雷发射→`PowerupScene::CommitMatchUse`→HUD原消息；比赛判定结束→冻结世界→原MISSION_END→`IsReadyForResults`才允许宿主进入结算。当前原Movie时长分别为2000ms和1500ms，运行时直接读取。显式重开商店会重置默认页，覆盖在GUNS页被击杀、未经历隐藏帧的情形。

原POWERUP冷却核对：标准手雷3秒，炮塔18秒，Speed/Defense/Attack Boost各15秒，Shield20秒，Tantrum30秒，Auto Aim120秒。三种血包、空袭、复活、Shock/Freeze Grenade的该配置为0；是否可用仍由库存及原CanUse脚本决定，不能把0解释为任意状态都能使用。

首次新增检查有一个测试假设错误：半血使用小血包后未满血，第二次仍允许使用，因为该道具冷却为0。旧断言错误地要求第二次被拒绝，导致`deathmatch-feedback`退出1。已改为先核对其冷却确实为0，再在满血状态验证原CanUse拒绝；手雷则单独验证3秒冷却内拒绝。保留首次结果于 `obj/deathmatch-presentation-first-results.json`，这次失败不是生产血包逻辑故障。

另一次综合执行中，旧完整对局专项的定向致死断言失败，原日志没有记录受击返回值及保护状态；原日志与结果保存在 `obj/deathmatch-presentation-fatal-hit.log`、`obj/deathmatch-presentation-second-results.json`。增加详细受击状态后，用原时钟随机流直接重跑五图通过（`obj/deathmatch-presentation-hit-probe.log`），未再次复现该次失败，因此不把它归因于本轮UI修改。检查同时发现关卡脚本随机流未像Bot一样固定，现仅在Deathmatch专项中将两者都固定为42–46；实际游戏保留时钟随机。保留详细断言用于后续定位，未放宽伤害或胜负要求。

独立执行 `bin/Debug/GunBrosTests.exe --powerup-check --mute --test-output E:/coding_projects/c_projects/gun_bro_re/obj/deathmatch-powerup-evidence`，退出码0。原20项脚本查询验证：普通主动道具在使用入口扣量，手雷/炮塔类在发射事件扣量；两条实际路径都连接HUD消息。原查询和冷却报告为 `obj/deathmatch-powerup-evidence/powerup-check.txt`。

最终结果：

- Debug测试工程、Debug游戏、Release游戏构建均退出0；日志为 `obj/deathmatch-presentation-build.log`、`obj/deathmatch-presentation-debug.log`、`obj/deathmatch-presentation-release.log`。
- 固定种子的五图完整对局直接复验退出0，日志为 `obj/deathmatch-presentation-seeded.log`。
- 最新运行器命令：`pwsh -File tests/run.ps1 -NoBuild -Case deathmatch,deathmatch-feedback,powerup-play,powerup-selector,postgame-menu -TimeoutSeconds 600`，5/5通过、退出0，`known-data-issues=0`、`protected-changes=0`。日志为 `obj/deathmatch-presentation-followup-regression.log`，逐项结果为 `tests/out/results.json`。
- 加上此前已通过且未受后续测试种子修改影响的 `postgame-presentation`、`player-death`、`local-live`、`deathmatch-data`，本阶段9项定向检查分别通过；前四项记录见 `obj/deathmatch-presentation-final-regression.log`，不把曾中断的整轮写成9/9成功。
- 五图实际使用消息统计：小血包5条、手雷5条、炮塔5条，每次成功使用恰好一条；另有孤立UI手雷示例1条。拒绝的重试未产生额外消息。五图结束后更新停止，1500ms原淡出按16ms步长完成后才允许结果页。
- 已目视确认商店、单人及Deathmatch结算的图案间距；滑动提示和淡出截图分别为 `tests/out/Core/deathmatch/deathmatch-intro.png`、`deathmatch-fade.png`，商店、枪页、道具消息、结算截图在同目录。淡出专项用单色场验证alpha，战场冻结另由真实五图检查验证。
- `git -c core.safecrlf=false diff --check`通过。未执行全量截图基线；前述偶发致死断言保留记录，尚未确认其原始触发状态。

## 首次出生、死亡指示与商店／油桶核对（2026-09-14）

范围沿用已授权的本地Deathmatch修复。先核对原流程，再实现待出生状态和原出生点算法，验证双方开店和油桶范围伤害；不把用户明确标为不确定的规则改成猜测。

- 原 `CLevel::OnStart` :120748–120903 在DM分支不调用双方Spawn，保存距MAP玩家位置最远的路径点及其对端；`RespawnPlayerForDeathMatch` :114940 首次使用保存点，再次出生且对手存活才选离对手最远的点。`CLayerPathLink::FindFarthestNode` :166874 只比较未锁节点距离，没有随机抽样。MAP位置、朝向与路径节点来自BIG，结构见 `maps/map.bt`。
- `CRemotePlayer::Update` :229788 显式排除DM的type6 HELP，死亡时仍使用type4/5头像。未首次出生的对手不运行该更新。
- `CPowerUpSelector::Show` :186489 仅非DM暂停世界；`CInputPad::ShowPowerUpSelector` :90310–90410 的GameType3直接本地开店，库存消息属于GameType2 Live。开店停止购物者操作，对手可继续行动，无对手商店画面。
- 油桶需区分 `CProp::FunctionResolver` native7→普通碰撞过滤爆炸与native10→`FireSplashDamageKnockBack` :123456，后者直接检查双方中心距离，不遍历敌人。参数继续由实际PROP脚本读取。

验收任务：未确认时无身体、移动、射击、拾取、锁定和伤害；确认后采用所选枪并播放原export8；不得提前计死亡或重置每命预算；死亡屏外头像不能变HELP，Live救援保留；首次保存两端、复活随对手位置选择最远未锁节点；原道具冷却、商店、完整对局与油桶定向回归。

红测：`--deathmatch-feedback-check --mute`，`obj/deathmatch-entry-red.log`退出1，明确打印`actor spawned before equipment confirmation`。实现待出生后五图同专项退出0，`obj/deathmatch-entry-green.log`；已覆盖等待12秒仍不可击中、Bot先入场仍不锁定待选装玩家、确认采用所选枪、原复活保护、双方开店分别停操作和死亡头像。

油桶补证与修正：

- 两种当前DM地图中实际激活的油桶，PROP资源为`00267582:0`和`01675822:22`。原脚本native7分别为半径300、伤害15／20、owner1；native10均为半径100、伤害150、击退300。数值是执行BIG内Flow得到的日志，不加入运行时手写表。
- 只直接触发PROP脚本时未多算；使用真实玩家子弹触发后，`obj/deathmatch-barrel-shot-red.log`退出1，玩家／Bot实际伤害为60／174，而按两者原护甲应为60／150。区别由lastDamager归属触发，新增回归保留真实Trace→ApplyHit→PROP Flow→爆炸链。
- 根因：`CBrother::CanCollide` :135310 对直接BROTHER／PROP来源返回false，不能把它与“该玩家发射的CBullet”混同。现保留PROP native7来源类型，native10独立执行双方中心半径检查，不额外打敌人、不扩大到角色碰撞半径，也不额外套玩家攻击倍率。原150近距离伤害保留。
- `CLevel::OnStart` :120884 设置首次等待计时，`CLevel::Update` :121701 会在计时到期调用同一出生函数。因此首次选装仍保留原倒计时自动入场；RESUME可提前确认。在此之前不是隐形且可交互的角色，也不计作一次死亡。

本阶段验证结果：

- `pwsh -File tests/run.ps1 -NoBuild -Case deathmatch-feedback,deathmatch,local-live,prop-combat,actor-feedback -TimeoutSeconds 600`，退出0，5/5通过，`known-data-issues=0`、`protected-changes=0`。运行器日志`obj/deathmatch-entry-regression.log`，结构化结果`tests/out/results.json`。
- 五图反馈覆盖待出生、选枪确认、双方分别开店、屏外死亡头像、原复活动画／保护、出生节点最远性和结束淡出；真实玩家击爆油桶后的伤害为60／150，与原护甲计算一致；100半径外的角色不因额外碰撞半径而受伤。
- 五图完整对局均通过三杀判定、Bot自主作战、死亡复活、补给、伤害归属、重开和每命限制；合作Live原有商店同步／救援检查通过。道具与敌人受击链通过独立油桶和动画回归。
- 测试工程构建`obj/deathmatch-entry-final-build.log`和Debug游戏构建`obj/deathmatch-entry-debug.log`均退出0。`git -c core.safecrlf=false diff --check`通过；本阶段未重跑全量截图基线。
- Release游戏构建退出0，日志`obj/deathmatch-entry-release.log`；`bin/Debug/GunBrosRe.exe`和`bin/Release/GunBrosRe.exe`均已更新。

## 首次默认视角与GUNS入口纠正（2026-09-14）

用户确认：加载后保留默认地图区域，初始选装完成再定位角色／切换跟随视角；首次商店直接显示GUNS，后续开店优先POWER UPS。

补充证据：`DeathMatchIntroSequenceCallback` :90035 调用`ShowPowerUpSelector(this,1,1,1)`；最后一个参数保存于InputPad+6352，传至`CPowerUpSelector::Show`的a5，再写到selector+3800（:186527）。`SetState(0)` :185707 控制展开动画，并非POWER UPS标签；进入状态2时读取+3800，值1选GUNS、值0选POWER UPS（:185725）。纠正本文件前文“SetState(0)总是进入POWER UPS”的错误解读。

执行方案：保留MAP初始位置和加载时的相机区域，待首次确认才计算并应用原路径出生点；玩家待出生期间保持相机位置，Bot先出生不触发玩家视角变化；确认后恢复原Camera模式0的跟随。商店沿用原Movie，只增加初始页参数，后续ResetSelector仍默认POWER UPS。

验收：五图进入时坐标仍为MAP默认位置，等待选装期间相机不动；确认后角色定位且视角跟随；首次商店绘制实际枪卡，后续开店和死亡重开默认道具页，原标签紧密间距及点击换页保留。首次相机红测`obj/deathmatch-entry-camera-red.log`退出1，明确检测到`moved to spawn before equipment confirmation`。

首次页红测：`obj/deathmatch-entry-tabs-red.log`退出1，实际绘制后打印`initial shop did not open GUNS`。修复在`ResetSelector`保留原调用的初始模式参数，绑定时仅消费一次；因此用户手动换页不会被每帧重置。初始坐标则只记录MAP来源，出生确认时才计算路径端点并应用；`SurvivalSession::UpdateCamera`在本地待出生期间保留加载相机，`RespawnDeathmatch(0)`恢复原Camera模式0及其已有插值。

验证：`pwsh -File tests/run.ps1 -NoBuild -Case deathmatch,deathmatch-feedback,powerup-selector -TimeoutSeconds 600`退出0，3/3通过，`known-data-issues=0`、`protected-changes=0`。完整对局69.7秒、五图反馈25.2秒、原商店1.8秒；日志`obj/deathmatch-entry-camera-regression.log`，结果`tests/out/results.json`。

五图均检查默认MAP坐标不被提前改写、12秒待选装视角保持不动、Bot先生成后的4秒仍不改变本地视角、确认后定位到原最远路径节点并恢复跟随。首次枪页截图`tests/out/Core/deathmatch/deathmatch-initial-guns.png`已目视确认选中GUNS和四张原枪卡；后续正常及死亡重开商店仍显示POWER UPS，标签间距和切换检查保持通过。测试工程构建日志`obj/deathmatch-entry-camera-build.log`退出0。

Debug与Release游戏构建均退出0，日志分别为`obj/deathmatch-entry-camera-debug.log`和`obj/deathmatch-entry-camera-release.log`，两个运行版本已更新。`git -c core.safecrlf=false diff --check`通过。

## PvP Bot 药包平衡（2026-09-19）

用户要求普通每7.5秒、困难每5秒最多使用一个药包，两者上限暂设99，参数集中成常量供试玩调节。沿用现有每命预算，复活和重开重置，首次需要回血时可立即使用。

方案与任务：在 `ZBotSettings.h` 集中四个参数；`CMPMatch` 统一检查每命额度和成功使用后的冷却；困难随机道具入口同样检查药包限制，其他道具规则保留。验收覆盖冷却结束前1毫秒、99次后拒绝、复活/重开恢复以及真实Flow药包入口。原资源仍由BIG加载；已核对 `mp_match_template.bt` 和 `CMPMatch::Template::Init` :395900–395903，这些本地Bot调节值不属于原资源字段。

验证结果：`pwsh -NoProfile -File tests/run.ps1 -Configuration Debug -Case deathmatch-data,deathmatch -TimeoutSeconds 600` 退出0，2/2通过；`-NoBuild -Case deathmatch-feedback` 退出0，Bot普通/困难真实Flow与随机入口在五张地图通过。运行器统一传入 `--mute`，两轮均无受保护资源变更。日志为 `obj/pvp-health-balance-tests.log`、`obj/pvp-health-balance-feedback-tests.log`。Debug、Release的Game构建退出0，日志为 `obj/pvp-health-balance-debug-build.log`、`obj/pvp-health-balance-release-build.log`。首次沙箱内MSBuild受FileTracker访问限制，改为获准的沙箱外构建后通过。`git -c core.safecrlf=false diff --check` 通过。实际对战强度留待用户试玩调节。
