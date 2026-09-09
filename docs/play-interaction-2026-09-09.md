# PLAY 交互修复

## 方案与验收

用户要求补齐首次进入星图的默认选择、模式选择动画、可点击的返回按钮，并让任务／REV／商店连续拖动及惯性滑动。沿原 BIG Movie 区域、章节、Sprite 和粒子引用实现；Windows 连续拖动作为明确的平台适配，不另造布局表。

任务：复现 → 核对原消费者 → 默认选星球及返回命中 → 连续滚动 → 模式粒子 → 构建、真实输入和回归。

`--play-interaction-check --mute` 修复前退出 1，日志 `out/play-interaction-red.log`：默认 slot=-1、返回仍 page21、小步拖动时间轴不动；模式收拢中间帧存在。以前专项未覆盖这三种输入，资源解析通过不等于菜单行为完整。

## 已核对依据

- `ui_movie.bt`、`sprite_archetype.bt`、`docs/ui-binary-templates.md`：原区域、章节、图标及图集。
- `CMenuMission::SetSelectedIndex`（162296）、OnShow／Update（162368／162434）：默认槽至少为 1，淡入结束后执行待选星球。
- `CMenuMovieControl::HandleTouchInput`（141622）：起点在控件内即捕获连续拖动；CalculateBaseVelocity（141749）从相邻章节区域位移和时长派生速度；UpdatePlaybackSpeed（141086）处理拖动与松手减速。旧宿主 BeginMissionPage 每手势只翻一页，不是这段原逻辑。
- `CMenuMovieMultiplayerOverlay::Bind`（250659）、SetSelection（250261）、LabelCallback（250137）：绑定两层粒子，选择时启动第一层，标签回调在原锚点绘制；旧实现只有文字和图标。`CreateContentParticle`（149304）读取静态表两个 int16 引用。提取器新增 `OriginalModeParticleData.inc`，从原程序 MDS_BUTTON_MP_TOGGLE 得到 pack0_core 粒子 9／13；运行时解析 BIG 的 ParticleEffect，不复制粒子参数。
- 返回按钮是 Movie13，原包 `pack0_core_xga`、handle `0x030004B4`、BIG偏移 `0x2a3287`。唯一触摸区域在 0／400ms 两帧均为 `visible=0`：这是不绘制的输入区域，旧 Regions 调用直接过滤掉了它，这是不能点击的主因。修复独立读取隐藏输入区域，并与绘制保持同一播放时间和原点。

## 结果

- [x] 默认选择、返回按钮及其失败测试。
- [x] 任务／REV／商店连续拖动、惯性及边界验证。
- [x] 原模式效果及正常入口验证。
- [x] Release／Debug 构建退出 0：`out/play-release-final.log`、`out/play-debug-final.log`。Debug 的原 PowerupSelector／HUD 数值转换警告仍存在，本次修改未新增警告。

`MenuScrollMotion`直接跟随指针位移，松手后按原章节派生的速度上限和减速继续移动；`ScrollMissionMovie`将连续位置映射到原 Movie 章节和条目索引，同一次手势可跨任意数量的条目。Windows 适配使用固定 60Hz 参考减速、80ms 静止后不保留抛掷速度，消除桌面帧率差异；到边界停住，不将其声称为完整复刻原版的越界弹性。分类／页面切换清除旧惯性，避免新列表自行漂移。

| 命令（均带 `--mute`） | 结果 |
|---|---|
| `--play-interaction-check` | `out/play-interaction-final.log`，退出0。小步跟手、相同抛掷在16／32ms帧间隔距离一致、REV拖动后由812.5继续到1820末端、波次跨页、两层返回、模式中间帧及粒子均通过。 |
| `--mission-menu-check` | `out/play-mission-regression.log`，退出0；2000个波次按钮、10个Horde按钮及新账户锁定规则通过。 |
| `--planet-menu-check` | `out/play-planet-regression.log`，退出0；四个正式星球选择、原图标布局及离线模式提示通过。 |
| `--package-purchase-check` | `out/play-package-regression.log`，退出0；礼包本次OWNED／重启隐藏、5件自动装备、道具及三件盔甲重新穿戴通过。 |
| `--game-menu-check` | `out/play-store-regression.log`，退出0；商店购买／预览／导航、连续道具购买、兑换及设置回归通过。 |

最终专项截图位于 `out/play-interaction-check/37071000/`。模式效果截图仅绘制模式面板；完整正常窗口另通过 computer-use 实际观察：首次星图自动选 CERBERUS PRIME；Classic 选择出现粒子并收拢；一次拖动从REV1到REV10、波次41–50到1–10；两次点击左侧返回分别收起详情、回到星图；商店拖动跨列且松手后继续移动。测试窗口正常退出，日志 `out/play-manual.log`，账户 `out/play-ui-manual-1788967689/`。

所有测试使用 `out/` 隔离存档和 `--mute`，不修改原 `saves/` 或用户账户。
