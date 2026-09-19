# Haven 渲染、碰撞与源码位置复查

日期：2026-09-19。以用户本次四张实战截图为线索；iOS 3.6.0 二进制、BIG 和原始消费函数优先。此次没有修改原资源、用户配置或存档。

## host 与 WAV 播放器

播放器没有删除：`src/engine/platform/ZAudioPlayer.h`、`ZAudioPlayer.cpp`、`ZAudioPlayerInternal.h` 负责 WAV 解码、缓存、SDL 音频流与重叠播放。它不是 `ZCombatAudio` 的替代品；两者在不同层。

本次碰撞复查时，`src/gun_bros_re/host/` 有以下四类职责；随后用户指出音频归类问题，`ZCombatAudio.h/.cpp` 已迁回 `gameplay/audio/`，当前 host 只保留前三项。详见 [音频归属纠正](audio-ownership-alignment.md)。

| 类型 | 职责 |
|---|---|
| `ZGameKeys` | Windows 键位与游戏命令映射 |
| `ZGameObserver` | 调用方借用游戏状态的接口；测试断言与选择仍在 tests |
| `ZGameScriptObject` | 尚未恢复完整原虚表继承关系的游戏脚本宿主桥接 |
| `ZCombatAudio` | 将战斗声音事件转成 WAV 播放，管理同帧合并、循环归属、暂停和移动声音重复窗口 |

单独放 host 是为了标明桌面接入／自建桥接与已核对原类的边界；不是新的玩法包，也不是联机房主。这个名字较宽泛，不能仅凭目录名推断里面全是底层平台代码。声音资源读取仍由 `gameplay/audio/SoundEffect.h` 完成，背景音乐由 `CBGM` 管理。

## 玩家弹体遮挡与 Boss 激光

上一阶段修复的是绘制顺序：弹体从地图主体之前的独立阶段，进入原 group/Y 顺序与背景、主体、前景三阶段队列。合法前景遮挡仍保留。

**验收边界纠正：** `MapChecks.cpp` 中穿过建筑的 Infinity Laser 用例没有绑定真实地图碰撞。它隔离并验证绘制顺序，不能证明实战弹体会被墙阻挡，不能作为物理碰撞修复的验收。

Quadcaptain 串珠激光仍未修复。历史视觉规则将 `pack5/BULLET104` 的主体动画从 BIG 指定的 1 改成 0；effect 重构移除该偏离原数据的规则后，串珠重新显现。已核对原 Bind、Draw、真实 Boss Flow 和 BIG：当前实际选择为主体 1、端点 2/3。现有证据尚不能解释与原版实际视觉的差异，因此不恢复按资源特征改选动画的补丁，不宣称 iOS 原版必然也是串珠。原阶段完整记录见 `gameplay-optimization-result.md`。

## 已修复：敌方弹体缺少出生遮挡检查

原 `CBullet::Fire`，反编译行 62194–62245，在射出时检查拥有者中心至枪口的线段；flags `0x20` 选择 level+9928，其余选择 +9932。之后才进入飞行碰撞。模板字段依据 `entries/bullet_template.bt`，地图查询依据 `CLayerCollision::TestCollisionSegment`（125269）。

重建的玩家 `CLevel::EmitBrother` 已做该检查，敌方通用 `SpawnProjectile` 遗漏。后续更新只扫枪口到下一位置；若出生时枪口已跨过墙，便漏掉先前那段障碍。没有按敌人体积跳过建筑的分支，但模型枪口偏移会影响是否跨墙。

复现：真实 `pack7/MAP6`，第 45 波，种子 5489。第 1246 帧，`pack1/ENEMY17` 发出 `pack1/BULLET2`：拥有者 `(1502.26,455.26)`，首次观测弹体 `(1547.24,377.61)`，墙体交点比例约 0.01009。原 2400 帧运行记录 1 次出生穿墙。

最小用例保留该地图、原敌人与原弹体，隔离摄像机剔除。修复前跨墙弹体存活，开阔地对照也存活；修复后前者退场，后者继续飞行。实现将共同检查放入 `CBullet::CheckSpawnCollision`，玩家与敌方都调用，不改伤害、原碰撞标志或资源轮廓。

```powershell
pwsh -File tests/run.ps1 -Configuration Release -Phase Core -Case haven-muzzle -NoBuild
```

| 结果 | 跨墙后存活 | 加入开阔地对照后的总存活 | 退出码 |
|---|---:|---:|---:|
| 修复前 | 1 | 2 | 1 |
| 修复后 | 0 | 1 | 0 |

相同第 45 波实战重放，出生穿墙计数从 1 降至 0。这里只证明已复现的发射路径修复，不等于所有敌方弹体／地图情况穷尽通过。

## 未解决：敌人进入建筑范围

两套碰撞数据与导航数据必须分开：

- 原 `CProp::Template::Init`（123346）依次读取实体、弹体轮廓；`GetCollision`（123369）与 `GetBulletCollision`（123384）分别消费。两者本来不同。
- 第 41 波重放中的 `pack7/PROP3` 原点 `(1376,411)`，实体右下边缘经过 `(1515,593)`，弹体轮廓右侧约为 x=1502–1503，底边也更靠上。因此进入实体轮廓的弹体不一定穿过其弹体阻挡轮廓。
- 同处原导航 cell116 使用顶点 `(1511,591)`，已经位于上述实体轮廓内。首个敌人进入时 flock 为零；仅关闭群聚避让不能消除问题。
- 原 `CEnemy::UpdatePathFinder`（69968）直接按路径和群聚向量更新位置；`TestCollisions`（73079）没有调用玩家的地图圆形推离。因此没有擅自把玩家墙体推离器套给所有敌人，也没有收缩原导航网格。

固定 2400 帧、每帧 16ms 的诊断结果：

| 场景 | 进入实体轮廓的不同敌人 | 最大中心穿入深度 | 最长连续停留 |
|---|---:|---:|---:|
| 第 45 波，正常群聚，出生碰撞修复后 | 20 | 19.887 地图单位 | 8.288 秒 |
| 第 45 波，仅测试关闭群聚 | 22 | 13.877 地图单位 | 21.200 秒 |

两次运行轨迹会分叉，不能直接把差值解释成群聚的净影响。对照只能排除“关闭群聚即可解决”的假设；没有在正式游戏关闭群聚。第 41 波初始复现还记录 4 个敌人进入实体轮廓、29 枚弹体进入实体轮廓边缘，其中观测点均不在该建筑的弹体轮廓内部。

这不是仅凭截图判断的层级问题；真实中心位置确实进入了轮廓。尚不能把它归结为一个已确认的重建算法错误，也不能以资源边界不重合为由宣称卡住表现正确。下一步需进一步隔离具体敌人的路径 cell 转换与停步距离，并与原指令级执行或同版本实机轨迹比较。

这三个研究用例保留显式失败条件，通过 `-Case` 单独选择，不加入默认 Core 回归集；不会把尚未消除的穿入改成“通过”：

```powershell
pwsh -File tests/run.ps1 -Configuration Release -Phase Core -Case haven-collision -NoBuild
pwsh -File tests/run.ps1 -Configuration Release -Phase Core -Case haven-artillery -NoBuild
pwsh -File tests/run.ps1 -Configuration Release -Phase Core -Case haven-artillery-no-flock -NoBuild
```

研究均退出 1，表示穿入仍存在。无敌仅用于维持观察，正常敌人／弹体逻辑继续执行。原日志、截图与资源保护检查归档于忽略目录 `obj/haven-collision-20260919/`：`initial-red`、`geometry-red`、`birth-probe-red`、`muzzle-red`、`muzzle-green`、`birth-fix-replay`、`no-flock-study`。

## 最终验证

Debug Game 构建成功，同时产出 Viewer、Tests。Debug 定向回归 `haven-muzzle,gameplay-ownership,weapon-effects,mines,prop-combat` 全部通过（5/5，退出 0）。

Release 定向回归 `haven-muzzle,weapon-effects,mines,boss,map-occlusion,prop-combat,enemies,local-live,deathmatch` 全部通过（9/9，退出 0）。两组资源／存档／配置保护检查均为 0 变化。Boss 用例通过仅表示原攻击流程、资源与生命周期检查通过，不代表串珠视觉已经修复。

最终 Release Game、Viewer、Tests 三产物构建成功，退出 0；本次测试日志归档到 `release-regression-green` 与 `debug-regression-green`。没有重新跑四地图 500 波长程检查。

新 Release 正式 EXE 的 `Smoke/game-menu` 启动检查也通过（退出 0，资源保护变化 0），归档 `release-smoke-green`。`git diff --check` 通过。
