# 2026-09-09 可玩版本优化

## 范围与验收

用户授权修复场景窗口闪切、pack9 炮塔、pack7 尖塔、pack12 解救对话、首星球卡顿，并调整桌面快捷键。原资源只读，游戏数值及布局继续从 BIG 获取。

1. 窗口：由正式程序持有一个窗口与 GL 上下文，视频、加载、菜单、战斗借用；验证同一 SDL 窗口 ID、位置、尺寸及上下文纹理跨场景保留。
2. 地图机关：从实际地图引用、PROP/ENEMY 模板及 Flow 追踪碰撞、显隐、进入事件；验证玩家按真实移动路径能触发，不能只传入“已进入”。
3. 对话：按原 CDialogPopup::Show/Update/Draw（反编译 183446–183740）和 POP_UP Movie 恢复区域、头像、文字与结束时机，移除自创 CONTINUE。
4. 性能：先测同资源、同装备的前后耗时；区分加载尖峰、正式 HUD 和多敌人更新，不以低负载研究入口代替实际体验。
5. 快捷键：桌面适配 Q/E 使用左右 powerup，1 打开战斗购买，2 切换已装备双枪；鼠标按钮保持对应功能。保留 R 重开。

## 基线

- 工作区开始时无未提交修改。
- 最早的 `--performance-check --map pack2 7 --weapon 65` 退出 0，CPU p50=0.470ms、p95=1.137ms、p99=4.258ms、max=34.443ms。**随后发现研究入口忽略地图、武器和起始波次参数，实际使用了默认武器 0**；该记录只保留作历史线索，不能作为武器 65 的对比依据。已修复参数传递。
- 现有窗口由视频、加载、GameMenu、RunSurvival 分别创建，后者标题为 Gun Bros - Survival。
- SurvivalHud::Buttons 为非教程对话额外添加手写 CONTINUE，旧绘制还有手写底板、文本宽度和阅读时长；原 CDialogPopup 无此按钮。

## 任务

- [x] 单窗口与场景切换验证。
- [x] 地图机关原数据与交互修复。
- [x] 原对话重建与回归。
- [x] 恢复原依赖清单的敌人模型预加载，完成同条件测量。
- [ ] 持续卡顿复现：当前同机测量没有复现，不能宣称全面解决。
- [x] 新快捷键、相关回归、更新验收文档与构建产物。

## 实现与一手依据

- **窗口**：`main.cpp` 持有 `CWindow`，视频、加载、菜单和生存关卡借用同一对象。`CWindow::Open` 对已有窗口只清理输入，不重建 SDL 窗口及 GL 上下文。正式窗口标题保持 `Gun Bros`；独立研究查看器仍保留自己的标题。
- **尖塔**：pack7 MAP6 的 LEVEL3 启动 Flow（`pack7_xga_0012_0x2b82.flow.txt`，@0xA2/@0xA8）先启动对象层 2，再切到 3。原 `CMap::SetObjectLayer` :91935 立即调用 `CLayerObject::OnStart` :126249；已生成的 PROP 留在关卡对象池。旧实现只装最后一层，导致层 2 的 PROP33 未生成、地图碰撞却仍存在。现按脚本顺序启动各层，并遵守手动生成标记；显式生成继续走原 tag/对象索引。
- **地图触发**：原 `CPlayer::Move` :100866 在移动碰撞处理后检查 trigger layer，生存模式也执行。原 `CBrother` 构造 :139098 的完整角色半径为 22，墙体碰撞使用半径的一半。旧宿主仅在废弃战役入口检查触发，且用了墙体半径。现统一执行 `UpdateMapInteractions`，不删地图碰撞或放大手写触发区。pack9 MAP0 原层 10 的两组触发数据保持不变；原 LEVEL Flow 决定哪座炮塔可用，不能同时强行解锁两座。
- **对话**：`OriginalDialogPopup.h` 按 `CDialogPopup::Update/Show` :183511/:183585、`CTextBox::Setup/tick` :103058/:104254，读取 `GLU_MOVIE_POP_UP` 的章节 1、用户区域 0/1，绑定原头像 85→86→87 和 font0。文字按原区域换行/分页，50ms 字符播放、页尾超过 1000ms、Movie 退场后完成；三者是原程序逻辑。教程箭头按原 `GLU_MOVIE_TUT_ARROWS` 章节绑定。删除自创 CONTINUE、手填底板与阅读时间估计，空格/Esc 只进入暂停流程。渲染绑定失败保留日志与未完成状态，不假报对话完成。
- **依赖预加载**：`maps/map.bt` 的 RequirementList 与 `RequirementList::Add` :191802 明确每条引用固定为 hash+ordinal，255 仍占字节但跳过消费。旧读取器只计数并丢弃内容。现保留原引用，结合 LEVEL 和 ENEMY Flow 的资源引用（原 `RequirementList::Add` :191754）递归预载敌人模型、纹理和 GL 缓冲。只缓存不可变资源，不运行敌人 Spawn/Flow，不改变随机数或提前生成敌人。特效、声音等其他种类仍由既有消费者处理，不声称已实现全类型预载。
- **桌面键位**：Q/E 左右道具，1 战斗购买，2 双枪切换；R 重开，Esc/空格暂停。G 兼容右侧道具。玩法和消耗仍调用原道具脚本及动画事件，不因快捷键即时改库存。

## 验证记录

所有命令均追加 `--mute`，游戏数据只读，测试账户写入 `out/`。

| 检查 | 结果与证据 |
|---|---|
| `--scene-transition-check` | 退出 0，logo→菜单→战斗→菜单均 generation=1、retained=1；检查 SDL ID、实际窗口位置/尺寸及跨场景纹理。红态见 `out/scene-transition-before.log`，绿态见 `out/optimization-window.log`。永久菜单 69。 |
| `--dialog-check` | 退出 0，真实 pack12 LEVEL0 的 string14 `You Saved a Babe!` 约 4000ms 自动完成；手动关闭模式等待原退场，旧按钮区域不截获点击。截图 `out/original-dialog-0.png`，日志 `out/optimization-dialog.log`。永久菜单 70。 |
| `--survival-check --map pack7 6 --weapon 65` | 退出 0，原对象层 2 生成尖塔；真实移动触发组 0，PROP 消息使状态 2→3，两波、受伤、死亡与重开回归通过。`out/optimization-pack7.log`。 |
| `--survival-check --map pack9 0 --weapon 65` | 退出 0，层 10 的两组触发可从边缘通过真实移动抵达，本轮可用的炮塔 id120 接收消息 1 后状态 2→3。红态两组不响应，见 `out/map9-trigger-before.log`；绿态 `out/optimization-pack9.log`。测试从触发边附近出发，未宣称完成全图出生点导航。 |
| `--survival-check --map pack12 0 --weapon 65` | 退出 0，两波与关卡基本闭环通过，`out/optimization-pack12.log`。 |
| `--profile-play-check` | 退出 0，鼠标购买、装备、暂停和键盘 1/Esc/2/Q/E 走实际输入分派；键盘阶段左右装备不同道具以检查路由，隔离测试账户补一件左道具并设置受伤状态。投掷消耗等待原动画事件，存档后续从第 3 波继续到第 4 波。`out/optimization-controls.log`。 |
| `--native-profile-play-check`、`--original-hud-check`、`--powerup-selector-check`、`--tutorial-check` | 均退出 0；原生账户、HUD 区域与动画、购买保存及教程闭环通过。对应 `out/optimization-*.log`。 |
| `.\test-muted.ps1 -Phase Core` | 20/20 完成，已有资源数据异常 1 项按原预期报告，受保护文件变化 0；`out/validation/20260909-124825-552-Release-Core/`。 |
| Release 构建 | 正式 `gun_bros_re.exe` 与研究 `gun_bros_research.exe` 均生成；`out/optimization-build.log`。 |
| 正式 EXE 启动截图 | `--mute --profile out/optimization-final-profile --menu-page 2 --screenshot out/optimization-final-menu.png` 退出 0。结束前重新核对 58 个原存档/真实账户文件，SHA256 变化 0。 |

## 性能测量与剩余边界

同机 RTX 3050 Ti、1600×1200、关闭垂直同步、16ms 固定模拟、1200 个真实绘制帧：

`gun_bros_research.exe --mute --performance-check --map pack2 7 --weapon 0 --start-wave 45`

| 指标 | 预加载前 | 预加载后 |
|---|---:|---:|
| CPU 中位耗时 | 0.736ms | 0.621ms |
| CPU p95 | 1.573ms | 1.273ms |
| CPU p99 | 7.475ms | 6.847ms |
| CPU 最大耗时 | 16.842ms | 15.578ms |
| 战斗内敌人资源缓存未命中 | 4 | 0 |
| 击杀数 | 3 | 3 |

日志与逐帧 CSV：`out/performance-prime-crowd-before.*`、`out/performance-prime-crowd-after.*`。这些是 CPU 阶段测量，不等于显示器帧率；当前只确认消除了该场景的首次敌人资源加载，并未复现用户描述的持续低帧率。需要进一步明确卡顿发生在移动、开火还是敌人首次出现时，以及实际装备/波次。

pack12 LEVEL0 的另一个 STRING 引用（资源表索引 6，hash=0x01675822、ordinal20）当前读不到文本；本关唯一 `ShowDialog` 调用 @0x3211 使用资源索引 5（string14）。保留未解析引用日志，不编文字，也不将它当作已验证的第二条对话。对话专项覆盖该实际提示与关闭模式，未穷尽所有语言及长文本组合。
