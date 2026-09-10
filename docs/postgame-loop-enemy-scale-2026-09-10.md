# 结算循环闪光与僵尸缩放修复

## 范围与验收

用户反馈：Perfect Waves闪光播放一轮后停止；普通僵尸在结算和战斗中偏小。修复原执行链，卡片尺寸和ENEMY原始缩放参数仍从BIG读取。

- [x] 将原第77项检查延长至30秒，断言末尾三秒仍有粒子和逐帧像素变化。
- [x] 对七种僵尸的菜单/战斗两种出生路径，与原MoveSet帧筛选规则作差分检查。
- [x] 修复有限时长粒子的循环调度，以及敌人模型加载时遗漏的MoveSet参数。
- [x] Release构建、实际结算截图、敌人/武器/结算/续玩回归。

## 原版证据与原因

### Perfect Waves循环

`CParticleEffectPlayer`构造 :131269把循环标志设为1；`Update` :131499在效果时长结束后推进下一周期，并保留发射器间隔余量。周期由 `CParticleEffect::Init` :131032取所有发射器结束毫秒的最大值；1ms效果按原消费者关闭循环。读取格式已核对 `entries/particle_effect.bt`。

上一轮只实现了start/end均为-1的无限发射器。Perfect Waves引用core type11:4，是有限周期效果，播放器未重启；Best Streak引用type11:14的无限发射器所以不受影响。为 `StartPersistentEffect`增加明确的loop参数，结算控件按原构造行为启用。保留其他调用方原有的一次性/持续行为，不按图标ID拼特效。

### 敌人缩放

原消费链：`CEnemy::Template::Load` :68895 → `CMoveSetMesh::LoadMesh` :123157（:123178传入MoveSet）→ `CMesh::Init` :97705（:97952筛掉未使用帧）→ `CMesh::ComputeBounds` :98184（第一张实际保留的顶点帧）。格式核对 `enemy_template.bt`、`mesh.bt`，未改模板数值或磁盘读取格式。

`LoadEnemyConfigs`漏传MoveSet，导致未使用的文件首帧参与包围盒计算。四种普通男僵尸的首帧横向尺寸异常大，由此压低战斗归一化倍率，并改变结算的矩形适配倍率。恢复原参数后，两种出生路径都使用正确的保留帧。

| 样本 | 修复前包围盒X/Y/Z | 修复后包围盒X/Y/Z | 战斗/UI原模板值 |
|---|---|---|---|
| pack11 ENEMY1 Zomboy | 207.83 / 55.56 / 186.58 | 99.45 / 56.99 / 180.45 | 110 / 25 |
| pack11 ENEMY2 Zomdude | 同上 | 同上 | 105 / 25 |
| pack11 ENEMY3 Zombrat | 同上 | 同上 | 100 / 25 |
| pack11 ENEMY4 Zomguy | 同上 | 同上 | 105 / 25 |
| pack11 ENEMY6/7 ZomMom/Zomgrrl | 34.09 / 23.50 / 57.62 | 相同 | 105或110 / 100 |
| pack11 ENEMY8 Cuttles | 51.84 / 48.01 / 55.50 | 相同 | 95 / 150 |

普通男僵尸inverseExtent由0.00481变为0.00554，战斗线性尺寸增大约15.2%。原生SetScaleFactor的1/256消费核对正确，未调整。结算沿原 `GetBoundsInternal` :67314、`DrawUI` :68451，用同一Movie矩形适配后乘各自uiScalePercent；没有放大/缩小框，也没有把不同敌人的模型强制设为相同大小。

## 红绿与结果

- `out/postgame-loop-before.log`：延长到30秒后退出1；Perfect Waves末尾三秒变化帧0、粒子0，其他图标持续变化。
- `out/enemy-scale-red.log`：原MoveSet差分退出1；四种普通男僵尸×两出生路径共8失败，女僵尸和Cuttles不失败。
- 修复后末尾三秒四图标均151个变化帧，最大粒子数6/10/35/44；七种僵尸×两出生路径原包围盒差分均通过。

命令统一为 `bin/x64/Release/gun_bros_research.exe --mute` 追加下列参数，均退出0：

| 参数 | 日志 |
|---|---|
| `--postgame-presentation-check` | `out/loop-scale-postgame-presentation-check.log` |
| `--arena-check` | `out/loop-scale-arena-check.log` |
| `--weapon-effects-check` | `out/loop-scale-weapon-effects-check.log` |
| `--postgame-menu-check` | `out/loop-scale-postgame-menu-check.log` |
| `--profile-play-check` | `out/loop-scale-profile-play-check.log` |

实际卡片截图：`out/enemy-scale-casualties-0.png`、`-1.png`、`-2.png`。已检查普通男僵尸、女僵尸和Cuttles的渲染。截图使用隔离结果样本，计数为测试值；原存档只读。

构建：MSBuild `gun_bro_re.slnx /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo`，退出0，日志 `out/postgame-loop-scale-build.log`。第77项继续保留，新增长时循环与原模型缩放验证。尚未与原iOS逐帧对齐随机粒子或逐像素对齐敌人动画。
