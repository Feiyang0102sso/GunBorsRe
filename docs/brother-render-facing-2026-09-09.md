# AI 兄弟渲染黑斑与出生朝向修复（2026-09-09）

用户反馈：战斗中 AI 兄弟一身黑斑；出生朝向也不对，星球 2 应与玩家出生方向一致。两处都在宿主侧，未改动原资源、原存档或正式账户。下列行号均指 `_IDA_OUT/gunbros_3.6.0_IOS.c`。

## 一、黑斑：兄弟画在深度测试之外

`runtime/MapScene.cpp` 的战斗主循环里，`DrawModels` 会先 `glClear(GL_DEPTH_BUFFER_BIT)`、开深度测试画玩家，返回前又关掉；敌人在随后的 `glEnable(GL_DEPTH_TEST)` 之后绘制。只有 AI 兄弟夹在两者中间，是唯一没有深度测试的三维角色。

`DrawPlayer` 依次提交躯干、腿、枪和三件盔甲。没有深度测试（也就没有深度写入）时，这些部件与模型自身的背面按提交顺序互相覆盖，模型内侧的暗面盖住正面——就是截图里的黑斑。修复是把 `glEnable(GL_DEPTH_TEST)` 提到兄弟绘制之前，让他与玩家、敌人共用同一个深度缓冲，绘制顺序不变。

## 二、出生朝向：地图 PLAYER 对象的附加 u16 就是角度

消费链（全部有源码依据）：

| 位置 | 行为 |
|---|---|
| `CLayerObject::InitializeObjects` :126603 | PLAYER 实例的附加缓冲按 12 字节分配，磁盘只写首个 u16 |
| 同函数 :126620—:126624 | 类型 15 时把附加缓冲首个 dword 写进对象层成员 +76，再用实例 x/y 覆盖 +80/+84 |
| 关卡开始 :120846 / :121005 | 把 `{+76, +80, +84}` 作为 `{facing, x, y}` 交给两兄弟的 Spawn |
| `CBrother::Spawn` :135887 | 取 `*(uint16*)a2` 存入成员 +1984，即 `CBrother::SetAngle` :134326 的同一成员 |

即：玩家与 AI 兄弟出生时用的是**同一个**地图角度值，兄弟不是固定朝上。20 张原图核对结果：多数 PLAYER 对象写 180，其中一张写 0，其余整个附加字段缺席（原版此时读到未初始化内存，本移植保持 0）。星球 2 的 pack7 地图 6 写的是 180。

本移植原先把这个 u16 命名为 `playerConfiguration` 且无人消费，兄弟 `Reset` 把 `facing` 硬置 0。现改为：

- `CLayerObject` 读出 `playerSpawnFacing` 与 `hasPlayerSpawnFacing`（`maps/map.bt` 的 `playerValue` 注释同步补上这条消费证据）。
- `MapScene::LoadPlacedPlayers` 写入 `PlacedPlayer::facingDegrees`，日志追加 `facing`/`authored`。
- `SurvivalSession::Restart(x, y, facingDegrees)` 同时设 `CombatScene::facing` 与 `ResetBrotherPosition`，`CBrotherAI::Reset` 收下 `startFacing`。

兄弟的出生偏移仍是宿主原有的“沿八方向找空位”，未改；原版是固定 +40/+20（:120849），属另一处待核对差异。桌面端玩家的朝向随后立即被鼠标接管，这是宿主输入适配，不改变出生值本身。

## 验证

研究 EXE 为 `bin/x64/Release/gun_bros_research.exe`，命令均退出 0。

| 命令 | 结果 |
|---|---|
| `--brother --map pack7 6 --screenshot ... --advance 1500 --mute` | 日志 `[m3] pack0_core player 0 at 1076 256 -- scale 83 facing 180 authored=1`，两兄弟同向并肩 |
| `--brother-check --mute` | 全部 `failures=0`，含死亡/复活、目标 31、射击 93 |
| `--survival-check --mute` | 无非零 failures |
| `--native-profile-play-check --mute` | 四星球各两波，日志 `out/brother-fix-native-play.log` |

黑斑对照（同一地图、同一帧、只差深度测试开关）：修复前 [out/brother-fix-before.png](../out/brother-fix-before.png)，修复后 [out/brother-fix-after.png](../out/brother-fix-after.png)。

Release 构建：`MSBuild.exe gun_bro_re.slnx /p:Configuration=Release /p:Platform=x64 /m /v:minimal`，退出 0，正式与研究 EXE 均已更新。

## 验证边界

- 缺附加字段的四张地图（含星球 1 的 pack2 地图 7）没有可用的出生角度，仍为 0；这是数据缺失，不是推断值。
- 附加缓冲的其余 10 字节原版从未写盘，也未在磁盘上出现，不当作 padding 断言。
- 本轮只覆盖战斗主循环的兄弟绘制；兄弟的出生偏移方向仍与原版 +40/+20 不同，留待单独核对。
