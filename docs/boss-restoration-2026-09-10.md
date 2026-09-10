# Boss 原版流程恢复（2026-09-10）

## 阶段方案与验收

用户已授权开始恢复 Boss。本阶段修复入场镜头和手雷受击分派，验证四张正式地图的 Boss 入场、装甲、死亡和普通波恢复。音频叠加与 `stboss` 保留为后续范围。

依据：`docs/boss-armor-original-research.md`、`enemy_template.bt`、`entries/common.bt`、`flow_bytecode.bt`，以及四份原 BIG Boss/LEVEL Flow。

- `CEnemy::SetCameraTarget`（iOS :68799）：立即设置镜头模式 2 和敌人世界坐标。当前 native 68 是空实现。
- `CEnemy::HandleCollision`（:71526–71534）、`CBullet::IsGrenade`（:60370）：bit 4 标识的手雷直接接触不触发敌人受击脚本；`OnSplashDamage`（:67945）仍正常触发。当前 ReceiveHit 未区分这两条路径。
- native 59（:72640）仅确认写入 mem+1284/1288/1292/1296。尚未找到完整消费链，不凭参数猜演出。
- 原 `CLevel` native 79 另请求 tutorial 21；现有宿主没有通用 CTutorialManager 显示入口，本阶段不能将 Boss 提示 Movie 等同于这项教程恢复。

## 任务清单

- [x] 加入永久 Boss 专项入口，用真实 BIG 和实际 Session/碰撞链建立失败检查。
- [x] 恢复敌人到关卡镜头的同步调用；避免延后动作改变原执行顺序。
- [x] 恢复手雷直接碰撞与爆炸分派，验证重复接触不拆甲、三次普通爆炸破甲；冰雷仍减装甲，电雷不减装甲。初版方案的“冰雷不拆甲”已按下方一手证据纠正。
- [x] 验证四地图完整 Boss 流程和实际手雷轨迹，运行相关战斗回归，更新结果。

## 结果

已恢复两条原生路径：`CEnemy` native 68 同步调用关卡镜头，`ReceiveHit` 对手雷直接接触返回 Pending，爆炸仍进入原受击 Flow。没有添加 Boss ID 特判或新的装甲数值表。

四图专项使用真实 BIG、LEVEL、Session、HUD Movie 和弹体；强制下一次 Boss 的变量 5 仅在研究检查中设置。普通敌人死亡后继续跑原调度，不直接替换 LEVEL 状态。真实手雷在 Boss 中心静止等待引信，覆盖连续接触与最终爆炸。

| 地图 | 入场到恢复操作（本次模拟） | 镜头目标误差 | 普通雷三次后的部件数 | 首发冰雷 | 首发电雷 |
|---|---:|---:|---|---|---|
| pack2 | 7568 ms | 0 | 3、2、1 | 4→3 | 4→4 |
| pack7 | 5408 ms | 0 | 5、3、2 | 7→6 | 7→7 |
| pack9 | 9408 ms | 0 | 3、2、1 | 4→3 | 4→4 |
| pack12 | 8576 ms | 0 | 3、2、1 | 4→3 | 4→4 |

入场时长是本次 16 ms 步进下的结果，不是写入游戏的常量。每次拆甲均保持原生命值；装甲耗尽后等到原正常行为状态，一发基础伤害 1 的普通弹扣血 4。四图均恢复操作、清除 Boss 标志并进入下一普通波。

### 测试与纠错

- 修复前 `out/boss-before.log`：四图镜头误差 1116～1845 世界单位；重复 5 次直接接触使 pack2/pack12 部件 4→1、pack7 部件 7→2。旧检查还包含固定入场时长和冰雷预期错误，其总失败数不能全部算作游戏缺陷。
- 冰雷旧研究漏追 internal 6 调用 internal 7。pack1 原 bin `0xC6E→0xC7C/0xC82`，pack6 `0xE0D→0xE1B/0xE21` 明确继续减装甲；已纠正原研究文档。当前原 Flow 执行结果吻合，未为迎合旧文档修改脚本。
- 普通枪弹在受击状态确有 2 倍分支；等恢复正常行为后才按 4 倍验收。
- Haven 的地图敌人 120、121 是关卡计数中的两个炮台，LEVEL state 115 死亡回调要求剩余数量为 2。清波检查保留地图对象，仅清动态敌人。后续 `stboss` 也必须保留这些机关，不能无条件杀死所有 CEnemy。

构建：`MSBuild gun_bro_re.slnx /p:Configuration=Release /p:Platform=x64 /m`，退出 0，正式与研究 EXE 均更新。最终日志 `out/boss-final-build.log`；保留既有 MapScene int→float 编译警告。

| 命令（均使用 gun_bros_research.exe） | 退出码 | 日志 |
|---|---:|---|
| `--boss-check --mute` | 0 | `out/boss-after.log` |
| `--arena-check --mute` | 0 | `out/boss-regression-arena.log` |
| `--powerup-play-check --mute` | 0 | `out/boss-regression-powerup.log` |
| `--combat-feedback-check --mute` | 0 | `out/boss-regression-feedback.log` |
| `--native-profile-play-check --mute` | 0 | `out/boss-regression-survival-final.log` |

永久研究入口 78 / `--boss-check`，加入 `test-muted.ps1 -Phase Core`。普通敌人回归覆盖 78 个模板，生存实战覆盖四星球各两波、Horde 两波及原装备/独立存档保存重载。`git diff --check` 与测试脚本 PowerShell 语法解析均退出 0。

综合回归初次在 Haven 空袭检查失败（`out/boss-regression-survival.log`）：旧断言把敌人总数写死为 2，实际还有两座地图炮台。测试改为记录演出开始时数量并验证全程不变，六条空袭路径及四星球综合回归随后通过；未修改空袭运行逻辑。

### 验证边界

- 镜头检查比较真实目标、插值终点与原地图边界约束，未人工逐帧对照 iOS 实机演出；贴边时合法的镜头限位继续保留。
- native 59 和 tutorial 21 仍是上文列出的未恢复项，不能将本轮称为 Boss 全部演出的 1:1 完成。
- 音频叠加和 `stboss` 没有在本阶段实现；专项强制入口不写正式账户，也不改变普通游戏的 Boss 抽签概率。后续用户确认的 `stboss` 实现见 [新增作弊码记录](stboss-2026-09-10.md)。
