# gameplay 全目录优化结果

日期：2026-09-19。基线：`2fb4c8e`。用户授权四小时自主实施，Bot 相关配置归入 `multiplayer/bot/`。实施方案见 [调研与阶段方案](gameplay-z-research-plan.md)。本记录只将实际验证过的行为计入完成项。

## 目录与所有权

路径相对 `src/gun_bros_re/`。gameplay 根目录的 32 个源码文件全部归位，递归源码文件从 169 个变为 164 个；继续使用一个工程和 Game／Viewer／Tests 三个产物。

| 目录 | 负责什么 | 原版对应或边界 |
|---|---|---|
| `gameplay/weapon/` | 枪、弹体、模板／模型资源、追踪、碰撞与绘制 | `CGun`、`CBullet`；模板地址由关卡缓存保持稳定 |
| `gameplay/armor/` | 盔甲模板、脚本、模型 | `CArmor` |
| `gameplay/collision/` | 地图碰撞数据、几何计算与命中消息 | `CCollisionData`、`Collision`；稳定 ID／值消息明确为宿主适配 |
| `gameplay/audio/` | 背景音乐、声音资源引用、战斗音效调度 | `CBGM`、`SoundEffect`、`ZCombatAudio`；共用 SDL 播放器仍在 engine/platform |
| `gameplay/script/` | 游戏 native 分派、Mission 条件查询上下文 | `ScriptResolver`、`CMissionScriptContext` |
| `gameplay/multiplayer/` | 比赛规则、双方统计、本地商店同步 | `CMPMatch`、`ZMultiplayerStatistics`、`ZLiveShopSession` |
| `gameplay/multiplayer/bot/` | 合作／PvP Bot、寻路、档案、名单、难度与生命预算 | 保留自建 Z 身份，不冒充原版网络玩家 |
| `gameplay/` 下的 `game`、`level`、`map`、`enemy`、`brother`、`powerup`、`pickup` | 各原对象业务 | 默认伙伴 `CBrotherAI` 仍在 brother |
| `host/` | 键位、调用方观察接口、脚本虚表桥接 | `ZGameKeys`、`ZGameObserver`、`ZGameScriptObject` |
| `ui/`、`../gun_bros_viewer/scenes/` | 输入盘 UI、地图研究与预览入口 | `CInputPadMeter`、`ZMapViewer` 分别归位 |

原 23 个 gameplay Z 文件：6 个通过合并职责删除，7 个迁至 host／Viewer，10 个保留在 multiplayer；新增集中配置 `ZBotSettings.h` 后，gameplay 中共有 11 个 Z 文件，全部在 multiplayer。没有旧路径转发头。

后续纠正（同日）：用户指出音频应按业务归类，`ZCombatAudio.h/.cpp` 从 host 回归 `gameplay/audio/`。当前 gameplay 为 166 个源码文件，其中 13 个 Z 文件（multiplayer 11、audio 2）。上段及文末数量是该阶段结束时的历史状态；不再将 Z 文件数量作为架构合理性的证明。原播放语义的差异见 [音频归属与原版对齐](audio-ownership-alignment.md)。

| 删除的包装 | 实际归属 |
|---|---|
| `ZBulletResources.h/.cpp`、`ZBulletVisual` | `CBullet::Template::Load` 持有共享模型和 GPU 资源；`CLevel::GetBulletTemplate` 按资源缓存模板 |
| `ZCombatTypes.h` | `CBrother::Vitals`、`CGun::Progress`、`Collision::Hit/Trace/HitResult/ObjectId` |
| `ZCombatGeometry.h` | `Collision.h`，保留原算法中特殊判定 |
| `ZProjectileGeometry.h` | `CBrother::ProjectMuzzle` 与碰撞层的线段／圆查询 |
| `ZProjectileTypes.h` | `CBullet::State/View/World`、`CCollisionData::Scene`；删除玩家前／后弹体阶段枚举 |

`CBullet::World`、`State` 和稳定 ID 是嵌套宿主接口／观察值，不是恢复出的 ARM 对象布局。已有非 Z 文件中的必要 Z 消息类型没有机械改成 C 类型。

## Bot 配置与存档

`ZBotSettings.h` 统一难度枚举、默认难度、`DMBotLevel` 配置键与合法值，以及每条生命的商店／手雷／医疗包预算。`CMPMatch` 消费此策略；资源价格、装备属性和比赛模板仍来自 BIG。

`ZLocalBotRoster.cpp`、`ZLocalBotFriend.*` 已在 `multiplayer/bot/`。这些是配置解释、策略和档案接入代码；通用原版档案读写仍由 data 提供。运行时用户的 `local-bots.cfg` 和 `local-friends/<身份>/` 保持原有用户目录，避免目录重构造成档案丢失或重建。用户存档不放进源码目录。

## script 的使用者

通用解释器在 `engine/glu/script/`；游戏的 class／native 编号分派在 `gameplay/script/`。CEnemy、CProp、CLevel、CBrother、CGun、CBullet、CArmor、CPickup、CPowerup 仍各自执行 BIG 模板中的 Flow，行为实现留在各自对象。

`CMissionScriptContext` 是 Mission 导出 2 的临时结果收集器，供 `ZPlanetCatalog` 取得等级和资源前置条件。原版 `Mission::FunctionResolver` 的 RTTI（163817）明确命名了该类，原工程 `mission.cpp` 也含此符号；因此保留原名，不将其误认成通用敌人／地图脚本。

游戏模式和随机变量归 `CGame::VariableResolver`。`ZGameScriptObject` 只保留尚未恢复完整原名的虚表桥接及宿主上下文。引擎不 include 游戏对象。引擎的两条虚调用直接 inline，避免与游戏实现的同名 `ScriptResolver.cpp` 在当前工程中生成相同 obj 文件。

脚本随机改为原 `CRandGen` 的 MT19937、闭区间取模和相等边界不抽样；附着同关卡的对象共享随机流。备用种子仅在独立宿主实际抽样时才分配发生器，避免每次生成敌人分配一个不用的 624 项状态数组。原 CGame 变量 0 使用 `GetRandRange(0,1000) >= 500`。这不是对 iOS 全进程随机抽样次序的逐位复现：独立预览可设种子，宿主粒子等随机仍有独立来源。

## 单人和多人统计

`ZMultiplayerStatistics` 目前用于本地 fake connection 合作与 PvP：按波／整局记录双方击杀、助攻、死亡、最佳连杀、完美波、经验和 Xplodium，供 HUD、结算及双方档案消费。它不是网络序列化实现，也不负责全部单人统计。

| 口径 | 生产位置 |
|---|---|
| 单人 HUD／结算击杀总数 | `CLevelRuntime.cpp::GetTotalKills`：已退役敌人的累计值，加仍在容器中的死亡计数，避免尸体动画未结束时漏记 |
| 关卡脚本收到的死亡通知／统计组 | `CLevelEnemyEvents.cpp::OnEnemyKilled` 的 `m_kills`、`m_statisticsKills` |
| 分敌人种类统计、挑战、武器经验 | `RewardEnemy`、`CEnemyCasualty`、挑战六字节键及 `CGun::Progress` |
| 按星球保存的单人击杀／Horde 最佳击杀 | `CGameFlow::UpdatePlayerProgress` 写入 `profile.enemyKills`／`hordeBestKills` |
| PvP 击杀与比分 | `CMPMatch` 和双方统计；档案写入原统计项 37，不增加生存波次 |

## 行为修正及原版证据

1. **弹体层次**：消费 native 9 的 group，恢复 `CBullet::GetZOrder` 的实际发射者与 100 单位距离条件。弹体、建筑、角色、地图粒子进入共同排序；建筑和弹体的背景、主体、前景使用同一排序表。依据 60280、64005、120591、145029、145123。不使用“所有子弹最后画”的补丁。
2. **追踪弹**：模板 +28 和 native 8 是角速度，CGun 模板 +152／native 14 是搜索锥角。恢复持有目标、失效时按 0x2000 重选、最近目标与逐帧限角，不再把转向速率当搜索半径或瞬时转向。依据 61446、128152、136545；目标用稳定 ID 代替裸指针。
3. **直接命中击退**：弹体模板 +120／+124 接回兄弟命中处理，方向来自移动速度；已受力、护盾、免疫与死亡仍由原角色入口控制。依据 BT `bullet_template.bt`、Bind 63644、HandleCollision 137829–137847。零速度弹体不构造无定义的归一化向量。
4. **光束铺贴**：恢复整数长度／步长、source 原点偏移延续、body 原点和 end 偏移，端点复制主体帧；Sprite 动画超界钳制到最后一个槽。依据 Draw 62965–63038、SetAnimation 58861、Update 63519／63532。没有修改 BIG 引用或按弹体 ID 选动画。
5. **音频**：`SoundEffect` 负责原 WAV 引用解析；Windows WAV 去重、忙窗口、循环 owner 和暂停留在 host，清除只写不读的 `loopSound`。原 `CSoundQueue` 是资源预加载队列，不把当前按需加载器改名伪装成它，也不增加空壳。

武器发射和非枪械生成仍保留各自准备路径：枪械需要熟练度、双手枪口、出生碰撞和 OnRemove 回调，敌人／道具生成的上下文不同；二者共享模板缓存、Bind、更新、碰撞与绘制。未为减少表面重复而增加一层多参数转发。

## 两个渲染现象的结论

**玩家激光被地图主体覆盖：已修复并有动态回归。** 新 Haven 用例使用真实 Infinity Laser 的 BIG 弹体，在真实建筑中部穿过。实际 group=3，ZOrder=821，建筑 group=3、ZOrder=732。共同队列与独立预期绘制顺序的像素差为 0；重放旧弹体先画顺序会隐藏 18,152 个可见像素。四张地图的角色遮挡也通过。原版合法的建筑前景遮挡仍保留。

后续复查明确了该用例的边界：场景未绑定真实地图碰撞，验证的是隔离后的绘制排序，不能证明穿墙问题已解决。敌方枪口出生碰撞遗漏、真实 Haven 重放及未解决的敌人穿入问题见 [碰撞复查](haven-collision-investigation.md)。

**Haven Quadcaptain 光束串珠：症状仍存在，不能列为已修复。** 已完成资源、变换、动画消费和真实发射链核对：

- 原 BIG `pack5/BULLET104` 块头偏移 `0x6743`，解压 235 字节，与解包原件一致；引用原型 139、主体动画 1。
- 真实 Haven ENEMY／LEVEL Flow 在攻击开始后观测到 `00267585:104`，body=1、caps=2/3、group=5，观测长度 953.3；并无发射后换到动画 0 的 native。
- 原型 139 有 6 个动画，端点没有越界；一般钳制修正不是串珠根因。精确铺贴与帧同步后亮度低谷仍约峰值 3%。
- 历史“修复”将主体 1 换成 0，是已明确偏离原字节的视觉规则；effect 重构删除该规则后重新出现串珠。此次遵守原版优先，不恢复此规则，也不将当前画面断言为 iOS 原版的实际表现。

若要求将这一现象改成连续粗光束，仍需同版本 iOS Haven 该攻击的实机画面，或另一份原始资源／消费者差异证据来判定。当前本地能确认的是引用与消费链，不能用降低测试阈值证明视觉问题已经修好。测试分别记录绑定正确和亮度测量，二者不混为同一结论。

## 验证记录

自动验证均由 `tests/run.ps1` 传入 `--mute`；BIG、源存档和真实用户数据哈希变化为 0。截图／完整日志保存在忽略目录 `obj/gameplay-z-research-20260919/`。

最终 Debug／Release 均以 `MSBuild GunBrosRe.vcxproj /p:Configuration=<配置> /p:GbProduct=Game /p:SkipAutoTests=true /m` 完成，退出 0，分别生成 Game／Viewer／Tests。保留现有数值转换警告，不宣称零警告。构建日志为上述证据目录中的 `Debug-Game-build.log`、`Release-Game-build.log`。

| 阶段 | 命令／用例 | 结果 |
|---|---|---|
| 调研基线 | Core `weapon-effects,map-occlusion` | 2/2；旧地图用例无弹体，不作为修复证据 |
| 红测试 | 新增 Sprite 超界动画钳制检查 | 修改渲染器前失败，记录在 `cap-red/` |
| 目录／职责／追踪 | Core `weapons,weapon-effects,mines,map-occlusion,gameplay-ownership` | 5/5，退出 0；76 个武器模板通过，含 3 个纯视觉条目 |
| 玩法／档案 | Core `audio-transitions,player-death,local-live,deathmatch-data,deathmatch,deathmatch-feedback,enemies,boss,prop-combat,actor-feedback,powerups,missions,level-flow,progress,profile-play,brother,tutorial,powerup-play` | 18/18，退出 0 |
| 完整三阶段／直接击退／真实 Boss | Core `player-death,boss,map-occlusion,gameplay-ownership` | 4/4，退出 0；四地图击退接受／重复／护盾／致死分支均通过 |
| Debug 最终行为与边界 | Core／Boundary `weapons,weapon-effects,player-death,boss,map-occlusion,gameplay-ownership,final-pack2,final-pack7,final-pack9,final-pack12` | 10/10，退出 0；四地图 500 波配置及边界推进通过，击退恢复检查均为 560 ms |
| Release 长程实战 | `pwsh -File tests/run.ps1 -Configuration Release -Phase LongRun -NoBuild -TimeoutSeconds 1800` | 4/4，退出 0；四张生存地图各完成 500 波，共 2,000 波，受保护文件变化 0 |
| 最终 Debug 产物 | Core／Smoke `gameplay-ownership,debug-input,viewer-controls,weapon-effects,local-live,deathmatch,game-menu` | 7/7，退出 0；配置、Viewer 输入、本地合作／PvP 和正式游戏菜单启动通过 |
| 最终 Release 产物 | Core／Boundary／Smoke／Extended `gameplay-ownership,weapon-effects,final-pack7,game-menu,spawn-performance` | 5/5，退出 0；Haven 曾失败的空袭恢复边界在 Release 复验通过，正式游戏与刷怪性能通过 |

长程验证使用无敌自动驾驶测试玩家，实际刷怪、弹体碰撞、敌人死亡 Flow 和波次完成照常执行；它验证长局稳定性，不代表人工操作难度验收。pack2／pack7／pack9／pack12 分别生成 28,844／28,785／28,590／28,777 个敌人，非法刷怪与屏内刷怪计数均为 0；原始日志和可执行文件哈希在 `release-longrun-green/`。随后只清理了一条重复 include，Debug／Release 三产物增量构建再次退出 0，日志另存 `*-Game-final-incremental-build.log`。

Release 已通过配置、Viewer 控件、地图资源、76 个武器模板、武器效果、地雷、死亡、本地合作、Boss、地图遮挡、共享状态、Bot 名单、交互与性能检查。普通 1200 帧 CPU p95 为 0.862 ms，刷怪场景 p95 为 1.149 ms，峰值敌人数 20，满足既有 16.667 ms 检查预算；该数值不是同硬件同输入的前后性能提升证明。

最后一轮 Release 刷怪性能复验 p95 为 1.202 ms，单帧最大值 36.788 ms，仍通过原 p95 预算；不把该结果等同于所有帧都低于 16.667 ms。

边界测试曾在 Haven 的空袭恢复移动检查失败 2 次。诊断日志确认 LEVEL 允许移动且未暂停，但活跃炮塔触发新增的原版弹体击退，角色正在恢复动作。测试仅在恢复移动探针的 1.6 秒期间开启 2 秒护盾，隔离不属于该断言的敌方命中；暂停、输入和位置断言保留。另增独立四地图击退恢复检查，500 ms 力结束后原 Flow 均在 560 ms 恢复移动。Haven 同一边界用例随后通过；没有修改空袭或原击退行为来迎合测试。

最终构建、定向回归和长程验证已完成；最后两轮证据分别保存于 `debug-final-green/`、`release-final-green/`。目录清单确认 164 个源码文件、根目录 0 个散落文件、11 个 Z 文件全部位于 multiplayer；未发现旧 include 路径或 engine→gameplay、Game→Viewer/Tests 的反向 include，`git diff --check` 通过。旧注释随职责保留；与新证据冲突的历史说明另加纠正，不静默删除。

本轮完成目录与职责优化，不将尚未解决的 Haven Boss 串珠现象、未恢复的宿主虚表原名或未穷尽的原版行为分支计入“完全对齐”。下一步应针对同版本 iOS 的这次 Boss 攻击取得视觉／消费差异证据，再决定修正点；当前不恢复违反原字节的动画选择规则。
