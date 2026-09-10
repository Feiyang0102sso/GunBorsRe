# 原版 Boss 装甲、受伤与出现规律

## 结论

- 四张正式生存地图使用四个不同的 Boss `ENEMY` 资源，但装甲核心规则一致：普通破片手雷命中三次后完全破甲。
- 装甲不是一个叠加到 Boss 身上的“防御力”数值，也不是从每次伤害中减去固定值。原脚本通过两层机制实现：装甲尚存时，普通破片雷只拆装甲而不提交本次生命伤害；装甲耗尽后，普通枪弹的碰撞伤害倍率由默认 `256`（1.0 倍）提高到 `1024`（4.0 倍）。部分受击状态使用 `768`（3.0 倍），属性 0 的命中在该状态使用 `512`（2.0 倍）。
- “每种手雷都会掉一层甲”不成立。当前 3.6.0 资源中，冰雷先走属性 2 分支，施加冰冻后仍调用减装甲函数；电雷走属性 3 分支，不递减装甲计数。普通破片雷走属性 4 的常规破甲分支。

> 2026-09-10 纠错：旧结论“冰雷不拆甲”漏追了 `internal_0x0006 → internal_0x0007`。已用四个真实 Boss 的冰雷爆炸复核：首发部件数分别由 4→3、7→6、4→3、4→3。修复和验证见 [Boss 恢复记录](boss-restoration-2026-09-10.md)。
- 单人模式的 Boss 不是固定每 N 波出现，也不按经过时间出现。每个普通波清空后，关卡脚本按“距上次 Boss 的普通波数”抽签，概率依次为 `0%、0%、0%、5%、10%、15%、20%、40%、70%、100%`。因此两次 Boss 之间必有 4～10 个普通波，数学期望约为 7.57 个普通波。`gameType == 2` 的联机分支改为固定经过 5 个普通波后出 Boss。

## 装甲与伤害链

四个 Boss 资源分别为：

- `pack1_xga_0050_0x5752.bin`
- `pack5_xga_0151_0x7d96.bin`
- `pack6_xga_0014_0x1316.bin`
- `pack8_xga_0022_0x1993.bin`

前三者中的装甲/模型块计数按 `4 → 3 → 2 → 1` 变化。`pack6` 的模型分块更多，变化为 `7 → 5 → 3 → 2`：前两次视觉上各崩掉两个配对块，第三次崩掉一个块，但游戏层面仍然是三次普通破片雷完全破甲。保留下来的 1 块或 2 块是 Boss 本体，不是剩余装甲。

原生碰撞路径位于 `_IDA_OUT/gunbros_3.6.0_IOS.c`：

1. `CEnemy::HandleCollision` 和 `CEnemy::OnSplashDamage` 写入待处理伤害、碰撞属性和默认倍率 `256`；手雷爆炸通过 splash 路径触发 Enemy 的受击 Flow 事件。
2. Boss Flow 使用 `CEnemy.native_0x0731(index)` 检查碰撞属性。冰属性 2 优先，其控制函数还会调用减装甲函数；随后检查手雷属性 4，普通破片雷的拆甲函数会排除电属性 3。
3. 装甲尚存时，拆甲函数递减装甲计数并切换受击动画，但不调用 `CEnemy.native_0x070D()`（`ApplyCollision`），因此这次破甲命中不会扣 Boss 生命。
4. 普通非手雷命中会调用 `ApplyCollision`。装甲耗尽后，Flow 先把 `CEnemy.variable[3]` 设为 `1024`，再提交碰撞。
5. `CEnemy::ResolveCollision` 按 `baseDamage × multiplier / 256` 计算最终值，`CEnemy::Damage` 只执行 `health = max(health - finalDamage, 0)`；原生 `Damage` 本身没有额外的 Boss 防御公式。

所以从玩家体验看，装甲确实“减伤”；从实现看，它是“破甲命中吞伤害 + 破甲后提高受伤倍率”，不是增加 Defense 属性。

四个 Boss 的初始生命还会根据当前 50 波循环中的波段设置为 `100 / 300 / 600 / 900 / 1500`，分别对应 1～10、11～20、21～30、31～40、41～50；`native_0x0732` 最终还会乘关卡的 Enemy health multiplier。

## 手雷属性依据

### 2026-09-10 补充：破甲后的普通雷与枪弹分支不同

上一节的 4 倍是普通枪弹分支，不能推广到普通破片雷。REV10 实测和 BIG Flow 进一步确认：

- pack2/7/9 的 Boss 裸甲后继续走 internal 5，普通雷直接 `ApplyCollision`，伤害倍率保持 256；普通雷的 BIG 爆炸伤害为 100，再乘玩家装备攻击倍率。
- pack12 使用 `pack5 ENEMY:2`，其 `pack5_xga_0151_0x7d96.bin @0xACD` 在装甲计数 ≤1 时先执行 `native_0x0732(1)`，再于 `@0xAD3` 提交碰撞。原消费者 `CEnemy::ResolveFunctionLocally :72546` 将血量设为 `1 × Level health multiplier`。因此 REV10 先降到 10 血，再承受普通雷；一雷死亡确实来自原资源，没有通过增加血量或跳过 Flow 改写它。
- REV10 波1与波50的生命分别为 1000/15000（pack2/7/12）；Haven/pack9 因原 LEVEL 的 REV 偏移为 1200/18000。不能把“REV10”直接等同于每波 15000 血。

新增检查覆盖四图 REV10 首尾波、前三颗雷只破甲、第四颗雷实际扣血。结果与依据见 [Boss 与道具修复记录](boss-powerup-fixes-2026-09-10.md)。

当前 BIG 中三种手雷弹体的碰撞 flags 为：

- 普通破片雷：`19`（包含 bit 4，不包含 bit 2/3）
- 电雷：`6171`（包含 bit 3 和 bit 4）
- 冰雷：`23`（包含 bit 2 和 bit 4）

这与 Boss Flow 的判断顺序吻合：冰雷进入冻结/控制分支后继续减装甲，电雷进入电击/控制分支，普通破片雷按常规路径拆甲。例如 pack1 的冰雷在原 bin `0xC6E` 调用 internal 7，随后 `0xC7C` 递减 local 10，`0xC82` 更新部件数。pack6 冰雷首次移除一个部件，与普通雷首次移除配对的两个部件不同，不能混用同一条破甲序列。旧版 `flow scripts/game.link` 没有列出属性 4，版本早于当前资源，只能作为辅助，不可覆盖当前 BIG 和反编译消费者证据。

## Boss 出现规律

四张正式地图的 LEVEL Flow 都包含同一张十项概率表：

`[0, 0, 0, 5, 10, 15, 20, 40, 70, 100]`

普通波敌人清空后，脚本调用 `CGame.native_0x0401(1, 100)`。原 `CGame::FunctionResolver` 将其交给 `Utility::Random`，而 `CRandGen::GetRandRange` 明确生成包含上下界的 1～100。若随机数不大于当前概率，则把下一场标记为 Boss 并把间隔计数清零；否则计数加一。

单人模式的实际间隔分布为：4 波 5%、5 波 9.5%、6 波 12.825%、7 波 14.535%、8 波 23.256%、9 波 24.4188%、10 波 10.4652%。Boss 只在普通波结束边界插入，不会在战斗中因墙钟时间到了而刷出。

进入 Boss 场时，LEVEL Flow 调用 `CLevel.native_0x054F()`；原 `CLevel::FunctionResolver` 随后调用 `CInputPad::OnBossWaveStart` 并显示 tutorial 21。该调用负责 Boss 波提示/演出，不负责决定出现概率。

## 一手证据入口

- Boss Flow：`out/binary-research/flow-disassembly/pack1_xga_0050_0x5752.flow.txt`、`pack5_xga_0151_0x7d96.flow.txt`、`pack6_xga_0014_0x1316.flow.txt`、`pack8_xga_0022_0x1993.flow.txt`
- 正式 LEVEL Flow：`out/binary-research/flow-disassembly/pack2_xga_0014_0x19f3.flow.txt`、`pack7_xga_0011_0x1e62.flow.txt`、`pack9_xga_0008_0x1bf7.flow.txt`、`pack12_xga_0011_0x125b.flow.txt`
- 原生消费者：`_IDA_OUT/gunbros_3.6.0_IOS.c` 中的 `CGame::FunctionResolver`、`Utility::Random`、`CRandGen::GetRandRange`、`CEnemy::HandleCollision`、`CEnemy::OnSplashDamage`、`CEnemy::ResolveCollision`、`CEnemy::Damage`、`CEnemy::ResolveFunctionLocally`、`CLevel::FunctionResolver`
- 格式依据：`_Big_tool/binary template/big_assets/enemy_template.bt`、`flow_bytecode.bt`，以及 `docs/flow-bytecode-reading.md`

## 边界

- 上述结论针对当前项目保存的 iOS 3.6.0 主程序与对应 BIG。
- `CLevel.variable[5]` 还保留一个强制下一次 Boss 概率为 100% 的分支，但四份正式脚本只看到初始化/消费，尚未找到把它置 1 的当前资源路径；不能把它当作常规出怪规律。
- `gameType == 2` 已由 `CGame::VariableResolver` 证实为一种联机模式标志；本记录不进一步猜测其 UI 名称。
