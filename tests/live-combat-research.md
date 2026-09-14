# Live 战斗统计与奖励依据

日期：2026-09-14。主证据为 `_prep/_IDA_OUT/gunbros_3.6.0_IOS.c` 与 `_prep/gunbros` ARMv7 指令。本文仅记录研究结果，未修改实现。GameType=2 为 Live 合作，3 为 Death Match；不能把二者的杀人计分和敌人击杀奖励混用。

## 可直接实现的规则

1. 双方各有独立波统计与整局统计：击杀、助攻、完美波、Xplodium、死亡、XP；最佳连杀另存。不要把世界敌人死亡总数填进双方的击杀栏。
2. 助攻位不是“玩家 0/1”，而是**本地玩家两个枪槽 0/1**。另一侧完成击杀后，本地曾命中的每个枪槽分别触发一次助攻，所以一只敌人可能贡献两个助攻。
3. 常规 Live 枪击的敌人 XP 与矿归击杀玩家，按该玩家自己的倍率计算。非击杀玩家不自动拿一份均分 XP/矿。助攻转发武器熟练度 XP，未发现固定百分比账户 XP 分成；不得捏造。补查确认这些击杀/助攻方法没有 Flow export，详见下方补证。
4. 本地玩家实际受到伤害后，本地波变为非完美，并重置自己的连杀。队友受伤没有直接破坏本地完美标记的证据。双方的完美波字段独立传送。
5. 完美波额外矿量为 `max(1, floor(本波已得矿 × LEVEL.perfectWaveRewardPercent / 100))`，随后走该玩家原 `AddXplodium`，仍受其运行时矿倍率/余数处理。
6. 救援完成奖励救援者 **10 XP**；被救方走原 Revive 脚本出口，不把双方都奖励 10 XP，也不能把 `OnRevive` 直接解释成恢复固定满血。
7. 单人倒地不立即结束 Live；原状态机等待双方完成死亡/复活选择状态与统计同步。双方状态都为 5 时禁止新增 XP/矿入账，避免飞行子弹继续刷奖励。
8. 原波结算计时 **15000 ms**，与多人商店的 10 秒限制不同。波段在波结算结束时清空，整局累计保留。

## 资源、伤害与归属

已查模板：`_prep/_Big_tool/binary template/big_assets/enemy_template.bt`、`entries/level_template.bt`，以及现有 `CEnemy`/`CLevel` 原消费者。

`CEnemy::Template::Init :67174` 按顺序读取两个 u16 奖励：XP、Xplodium。`Bind :73381` 将前者放到实例 `mem+912`，后者放到 `mem+876`，并将助攻位 `mem+936` 清零。敌人原始奖励必须来自 BIG；不能按敌人 ID 手填奖励。

反编译里没有独立命名的 `CEnemy::OnDamage` 实现；实际入口是 `CEnemy::Damage :71552`。它先扣血并限制到 0，保存伤害来源；生命为 0 时先调来源的击杀回调（符合条件时），再调用敌人脚本死亡 export 1，最后 `CLevel::OnEnemyKilled`。活着时调用来源的伤害回调。Flow 死亡与原生奖励的顺序需要保留。

`CEnemy::ResolveCollision :71596` 的 `:71617–71628` 检查有效子弹和本地角色来源，然后执行 `assistMask |= 1 << GetGunConfigurationID(...)`。`TestAssist :67294` 只是对 `mem+936` 按位取值；本研究没有发现这里存在“最近几秒参与才有助攻”的时间窗。不能自行添加时间衰减。

`CLevel::OnEnemyKilled :119306` 解析伤害来源、子弹的所有者和枪槽。以**常规角色枪击**分支为例：

- 本地击杀调用 `CBrother::OnEnemyKilled`，再用原枪配置给武器熟练度/击杀记录归属。
- 非本地击杀进入 `:119520–119547`：对本地枪槽 0 和 1 分别 TestAssist，命中的槽各调用 `OnEnemyKilledAssist` 并将波助攻/总助攻加一。
- 若两个槽都未参与，仍调用 `OnEnemyKilledByBro`，让当前枪及已装备盔甲接收原助攻类方法调用，但**不增加数值助攻统计**。这里并不触发 Flow export。
- `CBrother::OnEnemyKilledAssist :137885` 对指定枪槽调用 `CGun::OnEnemyKilledAssist`，随后对存在的各盔甲调用 `CArmor::OnEnemyKilledAssist`。两个枪槽都参与时，盔甲通知也会重复两次，这是原控制流，不是每个敌人简单去重一次。

环境伤害、反射子弹、手雷、非角色来源还有独立分支与统计集合；本研究确认这些分支不能粗暴全部算作普通枪击，但未完成所有来源的逐条实物验证，不应宣称这些边界已全部复刻。

## XP、矿与分数分离

`OnEnemyKilled :119587–119663` 从敌人实例奖励、关卡倍率、敌人条目倍率、该端玩家盔甲倍率、促销倍率、好友增益倍率推导奖励，并用 `ceilf` 取整；XP 对应 armor multiplier 3、矿对应 4。最终取值应使用现有原资源与消费者，不从 UI 分数倒推奖励。

`GameType==2` 的分支在 `:119665` 附近。非本地击杀跳过本地 `AddExperience`，并用 `v144` 阻止 `:119757` 的本地 `AddXplodium`。本地击杀经 `:119786` 调 XP，继续本地计分与矿路径。因此本地 bot 应以独立“本地玩家视角”对称执行自己的账户奖励，不能让它仅作为普通单人 AI 共享玩家账户奖励。

分数另有连杀公式：本地击杀为 `2 × XP奖励 × (当前连杀+1)`，然后连杀加一；另一侧击杀的该分支只记一次倍率分值，不推动本地连杀。原分数还限制到 3000000000。分数不是矿，也不是实际入账 XP；局内最终统计必须读取各自实际累计值。

`CPlayer::AddXplodium :101116` 用运行时百分比和余数累积：`scaled = oldRemainder + percent × amount`，矿增加 `scaled / 100`，保留 `% 100`。`AddExperience :101185` 还负责原等级门槛等逻辑；不能在统计结构上直接 `xp += enemyXP` 就替代账户消费者。

两函数都检查 Live 双方状态：本地 `mem+287552==5` 且远端 `mem+287556==5` 时不再入账。这里只确认原状态条件；不要把每一个血量为 0 的瞬间都等同状态已经为 5。

## StatisticPacket

原 `StatisticPacket::ImplementSerialization :122468` 明确逐项写出；包类型为 `0x0D`，发送见 `UpdateMultiplayerStatistics :115164`。内存有对齐，**线上负载没有那些 padding**。以下是 payload 相对偏移，均为宽度/原值记录；本研究未重新核对 PacketBuffer 端序实现，不能仅凭 ARM 主机端序声称网络端序已验证。

| 字段 | 原结构内存偏移 | 序列化偏移 | 字节 |
|---|---:|---:|---:|
| 本波击杀 | 0 | 0 | 2 |
| 本波助攻 | 2 | 2 | 2 |
| 本波完美标记 | 4 | 4 | 1 |
| 本波 Xplodium | 8 | 5 | 4 |
| 本波死亡 | 12 | 9 | 2 |
| 本波 XP | 16 | 11 | 4 |
| 整局击杀 | 20 | 15 | 2 |
| 整局助攻 | 22 | 17 | 2 |
| 整局完美波数 | 24 | 19 | 1 |
| 整局 Xplodium | 28 | 20 | 4 |
| 整局死亡 | 32 | 24 | 2 |
| 整局 XP | 36 | 26 | 4 |
| 最佳连杀 | 40 | 30 | 2 |

总序列化大小 **32 字节**，原结构占 44 字节，不能 `memcpy(sizeof(struct))` 发送。

`UpdateMultiplayerStatistics`：

- 波击杀取本地敌人击杀分类集合之和，并加入累计击杀；这不是无限次可重复调用而不改变累计的纯查询方法。
- 助攻在敌人死亡时直接累加波/总两个 u16。
- 本波矿和 XP 由当前有效累计值减去上次累计快照得出，再更新总值。矿的有效累计表达式含中途消费补偿项，因此不要只用“结束钱包减开始钱包”展示奖励。
- 死亡来自 `CBrother::HandleDamage :136789–136790`：生命归零且符合本地角色条件时，波死亡/总死亡各加一。
- 完成波时设本波 perfect；若成立，累计 perfect byte 加一。
- 最佳连杀取当前连杀、之前最佳与统计最佳的较大值。
- 发送后设置本地统计 ready；接收端 `:230104–230143` 反序列化至独立远端结构，并设置远端 ready，不覆盖本地账户数值。Death Match 另有 `SyncWaveStatisticsWithHost`，不应套到 Live。

### 初始化与波重置

初始清理见 `CLevel` 初始化路径 `:120887–120913`，本地/远端总量、波量及 ready 均被清零。

`OnWaveCleared :116991` 先更新统计，再置 15000 ms 的波结算等待；结算 UI 接收本地和远端统计指针。`UpdateNormal :121363–121390` 倒计时结束后清本波矿、死亡、XP、perfect、ready，并用 `this+79110*4` 的 u32 清本波 kills+assists 两个 u16；累计段保留。`OnWaveCleared :117025` 还清该波敌人分类集合，随后重置受伤标记并推进波次。

不要在刚收到统计包、每次绘制 UI、每次开商店时清零整局统计；也不要重复调用带累计副作用的结算收集方法。

## 完美波：已修正一个反编译错误

`IsCurrentWavePerfect :114250` 检查 `mem+310929==0`；`OnWaveCleared :116971` 用同一标记。反编译 `OnPlayerDamaged :115924` 却显示写 `&loc_4BE90`（数值 310928），而相邻 310928 被拾取记录环使用，不能照该伪 C 字面实现。

已只读反汇编 `_prep/gunbros` 核对：

```text
0008ed34  movw r2, #0xbe91
0008ed3c  movt r2, #4
0008ed40  mov  r1, #1
0008ed44  strb r1, [r4, r2]
```

实际偏移为 **0x4BE91 = 310929**，原程序正确写独立受伤标记。命令：`D:/Python312/python.exe _prep/tools/disassemble_original.py CLevel15OnPlayerDamaged --max-bytes 128`，退出码 0。项目没有 `.venv`，使用现有研究 Python 与已有 `_prep/out/research-python` 依赖，没有安装或改写工具。

`CBrother::HandleDamage :136797–136799` 只在受伤对象等于当前端本地玩家时调用 `OnPlayerDamaged`；该方法对非 Death Match 保存最佳连杀并清当前连杀。无敌/护盾处理位于实际扣血前，不能把被挡下的命中当作失去完美波。

完美奖金的百分比来自 LEVEL 最后的 u16，模板 `entries/level_template.bt`。原 `:116983–116988` 用本波矿增量乘百分比、整数除 100，最低 1，再调 `AddXplodium`。没有发现 Live 将双方受伤标记先做 OR 的证据。

## 复活与最终失败

`SetRevivePercent :115322` 存救援进度。进度精确等于 1 时：

- `a3=0`：当前端是救援者，调用 `CPlayer::AddExperience(...,10)` 并播放显示/日志。
- `a3=2`：接收远端救援进度后，当前端是被救者，调用 `CBrother::OnRevive(...,0)`。

接收进度的包类型是 `0x0B`，见 `:230081–230101`。`CBrother::OnRevive :135970` 调 `CallScriptExportFunction(...,1,7,...)`，因此恢复血量、动画与状态应由原对应 Flow 消费，不该凭这一个函数捏造“满血复活”。本研究未额外重做救援半径与时长证据。

`OnPlayerKilled :118371` 在 Live 下先打开原复活/道具选择流程：可打开时本地状态为 4，否则为 5，并同步状态包 `0x0C`。`UpdateNormal :121559–121644` 在本地状态 5、远端状态 5 后等待双方统计 ready，再进入 `OnLevelFailed`；本地仍有可用复活选择时不能提前最终结算。由此，“双方血量都等于零”可作为需要结束的线索，但完整复刻必须保留复活选择与同步收尾状态。

## 验证边界

本次读取了对应 BT、原反编译消费者和完美标记原 ARM 指令。没有修改 BIG、原始存档或主程序；没有新建网络协议实现。常规枪击归属、双枪助攻、统计字段、奖励冻结、复活 XP 和个人完美标记有直接证据。反射/环境/手雷等全部归属分支、网络字节序、救援半径及原 Flow 的全部复活细节不在本次已完成验证范围内。

## 补证：击杀回调没有 Flow export

本节纠正最初记录的“assist Flow 出口”措辞。已同时查 `entries/gun_template.bt`、`entries/armor_template.bt` 与原程序：

- `CGun::OnEnemyKilled :127880`：`mem+164` 击杀数加一，然后 `mem+216` 熟练度 XP 加传入的奖励值；没有调用脚本。
- `CGun::OnEnemyKilledAssist :127888`：仅 `mem+216` 熟练度 XP 加传入的奖励值；没有击杀加一，没有调用脚本。
- `CArmor::OnEnemyKilled` 与 `OnEnemyKilledAssist` 在反编译中仅有 idb 声明。原 ARM 符号地址分别 `0xF57C4`、`0xF57C8`，两者都只有 `1e ff 2f e1 / bx lr`，即空函数。不能给它们臆造一个 export。
- `CBrother::OnEnemyKilled :137903` 按命中子弹保存的枪对象匹配槽 0/1，匹配不到两把装备枪则直接返回；匹配后调用枪，再按原四甲内存槽顺序 1144、1312、1480、1648 调用存在的盔甲。
- `OnEnemyKilledAssist :137885` 用传入枪槽；`OnEnemyKilledByBro :137867` 用配置的当前枪槽。都按枪→四甲顺序转发。

实现所需参数是 `奖励XP:uint32`、`命中枪引用或枪槽`、`击杀/助攻/无参与队友击杀类别`。不需要添加任何击杀 Flow export 参数。原 GUN 既有出口仅装备 0、开火 1、子弹移除 2；ARMOR 装备出口 0 也不等于击杀出口。

只读验证命令：`disassemble_original.py CArmor13OnEnemyKilled --max-bytes 220` 与 `disassemble_original.py CArmor19OnEnemyKilledAssist --max-bytes 220`，均退出 0。

## 补证：死亡后的复活道具选择器

模板 `entries/powerup_template.bt` 的 `runtime112Raw`（原 Template+112）经 `Bind :187270` 复制至每个 option 的 +114。`CanShowAfterDeathPowerups :183895` 要求存在 `option+114!=0` 且 `option+104库存>0`；`SetupPowerUps :186307` 将 option 可见性设为“该类别等于本次 afterDeath 参数”。因此该字节已经有明确的死亡前/死亡后选择类别消费者，不应再视为完全未知字段，更不能按复活道具 ordinal 写筛选补丁。

`CInputPad::ShowPowerUpSelector :90310` 在 `afterDeath=1` 且不是 Death Match 时先检查上述库存条件；无候选返回 0。有候选才继续设置 Live 的 10000 ms 限时、发送类型 5 的选择器包（afterDeath byte 与各道具库存快照），使死亡状态走 4。Death Match 绕过该检查并有自己的重生倒计时逻辑。

实际选择/使用仍要满足当前可见、库存至少 1、`CanUse` export 1 返回 1、`CanUseFromSelector` export 2 返回 1、选项冷却为 0，证据 `:186021–186043`。是否允许装备单独查 export 0；是否扣库存查 export 3，不能仅凭类别字节扣库存。

`OptionUse :184707` 先绑定实际使用者，将运行时使用方式设为 2，成功进入 `CLevel::UsePowerup` 后调用 `CPowerup::Use`；后者在 `mem+988==2` 时执行 export 7，否则 export 6。export 7/6 无显式形参，但需要正确绑定使用者 `mem+984`、使用方式 `mem+988` 和选择器指针。成功使用后按 export 3 决定是否扣使用者库存，再执行选择器视觉/冷却流程。远端快照的数量变化不应重复扣本地玩家账户。

### 当前原版 Resurrection 条目

实际 POWERUP 列表中只有 `pack5 / ordinal 7` 的类别字节为 1，对应物理样本 `_prep/big_360_out/pack5_xga/0xf4e02223/18_POWERUP/pack5_xga_0372_0x1289d.bin`，文件偏移 154 的值为 1。名字引用为 packHash 2520453、assetOrdinal 470；此定位仅作研究证据，运行时仍按资源读入与类别筛选。

该条目 Flow 清单 `pack5_xga_0372_0x1289d.flow.txt`：

- exports 0–7 对应 functions 0–7；0 返回 0，1/2/3 返回 1，4 返回 0，5/6 为空，7 进入 state 0。
- state 0 请求 HideOnlyItems，收到对应隐藏事件后转 state 1；state 1 请求 HideSelector，收到选择器隐藏事件后转 state 2。
- state 2 依次 `CPowerup.native_0x0F15()`、播放资源槽 0 的声音、`native_0x0F0D()` 输入面板动画；完成事件后 `CLevel.native_0x0541()` 再 `CPowerup.native_0x0F00()` 结束。

复活不能在用户刚点按钮时跳过这条状态/动画顺序直接完成。

### 复活原因参数与 PLAYER Flow

`CPowerup::FunctionResolver case0x15 :188482` 的反编译漏掉了虚调用参数。原 ARM 指令：

```text
0010c3bc  ldr r0, [r6, #0x3d8]   ; actual user
0010c3c0  mov r1, #1             ; resurrection powerup reason
0010c3c4  ldr r4, [r0]
0010c3c8  ldr r4, [r4, #0xcc]
0010c3cc  blx r4
```

只读核对 `CPlayer` 虚表：符号 VA `0x403660`，地址点 +8，虚表 +204 的目标是 `0xB01C0 = CBrother::OnRevive`；+220 是 `CPlayer::GetBrotherType`，本地 CPlayer 返回 0。故实际调用是 **`OnRevive(1)`**，而附近救援调用 **`OnRevive(0)`**。

`OnRevive :135970` 发同步标志 1 的 PLAYER export 7，三个脚本参数为 `reason,0x7FFF,0x7FFF`。`CallScriptExportFunction :137093` 在多人活跃状态下由本地角色同步 PacketFunctionCall（类型 2），再执行本地脚本；远端代理不能自行重复发送。

原 `pack0_core / PLAYER ordinal0`（物理 `0037_0x6726.bin`）export 7：参数非零进入 state 15，参数零进入 state 16。两状态均先调用 `CBrother.native_0x060A(100)`，原 native case10 `:138930` 将生命恢复至最大值；这是已核对的满血依据。state 15 额外含 `CLevel.native_0x053A(25)`、`CBrother.native_0x060D(3200,2000)`，state 16 不含这两项。native0x060D在 `:138983` 用第二参数作范围、第一参数乘 10 作伤害，再调用 `FireSplashDamageForceAttribute(...,attribute=2,source=使用者)`；不得简化为无条件删除全图所有敌人。

两状态先关闭原角色变量 0/1，播放原移动/复活动画和保护时间；等对应动画事件再重新开启变量并返回正常 state 1。实现接口需要保留 `OnRevive(reason)`、实际使用者、原脚本事件及使用者自己的账户，不能把救援与复活道具合并成无参数的“设满血”。
