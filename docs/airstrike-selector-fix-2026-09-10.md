# 紧急购买文字与空袭流程修复

## 范围与验收

修复紧急购买界面 USE NOW 偏左，以及三种空袭使用时战斗继续运行、立即使用入口缺少警示动画的问题。

同时处理截图中的长余额遮挡 CANCEL：保留原顺序排版，Windows 宿主按 BIG 货币区域和按钮边界计算可用宽度，仅在溢出时等比适配文字，间距取原字库测得的“88”宽度。该处理明确属于宿主溢出适配，不声称是原 iOS 算法。

- [x] 核对原 BT、UI 绑定及 POWERUP Flow。
- [x] 让空袭检查经过实际 SurvivalSession，取得失败证据。
- [x] 修复文字对齐、关卡暂停调用及选择器使用入口。
- [x] 验证三种空袭的快捷/立即使用六条路径及取消流程。
- [x] 构建 Release，回归道具、紧急购买及实际暂停/续玩。

## 原版依据

界面使用 BIG 的 GLU_MOVIE_POWERUP_MENU_NEW、GLU_MOVIE_POWER_UP_LAYOUT、GLU_MOVIE_POWERUP_MENU_NEW_COPY，并非旧手写面板。`ui_movie.bt` 的用户区域提供坐标，MDS_BUTTON_POWERUP_SELECTOR[3] 提供文本。

`CPowerUpSelector::Bind :187086–187108` 建立100×100文本框，使用字体5并启用居中。`DrawUseNow :185311` 将文本框放在区域5中心，`CTextBox::paint :104153` 再逐行居中。重建只执行第一步，随后将文字左对齐到 centerX−50。现于 OriginalPowerupSelector.cpp 恢复逐行居中及词间距，没有修改 BIG 坐标、字库或解析器。

空袭数据为 pack5 POWERUP 0、10、11；样本分别为 `pack5_xga_0365_0x124e5.bin`、`0375_0x129f0.bin`、`0376_0x12acc.bin`。查阅模板为 entries/powerup_template.bt、flow_bytecode.bt、flow_native_sources.bt，可读 Flow 位于 out/binary-research/flow-disassembly。

1. `CPowerup::Use :188704` 按来源选出口：快捷使用为6，选择器为7。此前宿主始终调用6，跳过警示。出口7通过 native15 播放原 pack5 Movie2（1600ms），图案为横向运动的 AIRSTRIKE 文字和人物。
2. 原脚本用 CLevel native64/65 暂停/恢复。此前 CPowerup 未绑定关卡上下文，出现 `level native 64/65 has no level context`，且 CLevel 缺少这两个分支。
3. `CLevel::FunctionResolver :118073` 写暂停字节+275620；`CLevel::Update :121255` 先更新活动道具，再检查暂停。重建却在更新战斗后才更新道具，导致独立 Movie 检查通过而实际战斗继续。
4. `CPowerup::Exit :188138` 才标记完成；现移除把输入面板 event4 额外当作完成的逻辑，交由脚本 native0 收尾。

实现涉及 CPowerup 继承的关卡上下文、CLevel native64/65、PowerupScene 使用来源、PowerupMoviePlayer 警示流程及 SurvivalSession 演出更新分支。MapScene 同时冻结演出中的 HUD/键盘操作、瞄准改变、地图装饰和地砖推进。BGM继续播放，取消/重开释放残留暂停。

伤害保留原 native3：以镜头中心、半径3000、原伤害240/500/1600调用 Splash。屏内敌人由该范围覆盖，不改成全地图清怪，也不新增按道具编号的伤害表。

## 验证

构建 `MSBuild gun_bro_re.slnx /p:Configuration=Release /p:Platform=x64` 退出0；日志 out/airstrike-release-build.log。

统一运行 `bin/x64/Release/gun_bros_research.exe <参数> --mute`：

| 参数 | 结果 |
|---|---|
| --powerup-play-check，扩展后修复前 | 退出1；三种空袭均有非零 moving-frames，见 out/airstrike-red.log |
| --powerup-play-check，修复后 | 退出0；六条路径冻结检查为0，恢复后 moved=1；伤害各一次、库存各扣一次、范围外目标不受击、取消后不补发伤害 |
| --powerup-check | 退出0；20模板、35引用，无失败 |
| --powerup-selector-check | 退出0；15项、14次购买、38个选择操作及弹窗/存档回读通过 |
| --profile-play-check | 退出0；实际暂停/选购控件、BGM及续玩通过，完成至第4波 |

冻结检查还断言敌人位置、玩家生命、敌人数量不变，不只检查暂停标志。日志为 out/airstrike-final.log、out/airstrike-powerup-catalog.log、out/selector-green.log、out/airstrike-profile-play.log。

已查看 out/airstrike-check-0-selector.png 的原警示图、out/ui-original-2026-09-09/selector-original-7.png 的居中文字。保留并扩展既有研究入口。

## 边界

- 警示图在原脚本的选择器“立即使用”路径出现；快捷使用直接进入空袭，没有擅自添加重复警示图。
- 空袭 Movie 完整播放，粒子阶段由原脚本计时。选择器退出/输入面板恢复仍使用已有300ms宿主回调，本轮不声称这些周边收放动画已经逐帧复刻。
- 余额的自适应只改变宿主绘制比例，不修改存档数值、BIG 区域、原字库或按钮坐标；测试账户的长余额已截图核对。
