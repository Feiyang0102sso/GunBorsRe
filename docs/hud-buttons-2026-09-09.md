# 战斗 HUD 按钮按压反馈

## 方案与验收

用户要求恢复红色感叹号、蓝色切枪按钮的按下动画。仅修改原 HUD 的状态绑定，继续使用 BIG 的 Movie 区域和 Sprite；维持按下触发一次的操作，不增加猜测的缩放或动画时间。

任务：核对原按压资源及输入事件 → 建立画面差异专项 → 绑定按压状态 → 验证按住、释放和道具选择器退出 → 构建并记录结果。

验收：点击功能保持；按住不重复触发；蓝色按钮按住变为按压帧，松开／移出恢复；红色按钮在道具选择器打开期间保持按压，关闭后恢复。位置、图像、帧时长从原 BIG 读取。

## 原版依据与缺失原因

- 模板：`ui_movie.bt` 的用户区域；`sprite_archetype.bt` 的动画／帧组合。
- `CInputPad::Base::Bind`（反编译 :88320）：HUD_PAD_IPAD 区域 2/3 分别绑定红色按钮／切枪按钮，core Sprite 原型 1 初始化动画 27/35。
- `CInputPad::Base::DrawWeapons`（:87488）：在区域左下角绘制原 Sprite。
- `CInput::HandleTouchDown`（:85898）、`Refresh`（:85927 附近）：事件 1 是首次按下，后续保持为 2。
- `CInputPad::Base::UpdateInput`（:88480）：首次按下触发切枪／道具选择器；保持触摸切枪区域时用动画 36，无触摸则回到 35。
- `CInputPad::Base::SetState`（:87545）：状态 7（道具选择器）设置动画 28，状态 8 退出恢复 27。
- BIG 中 core 原型 1 对应研究样本 `pack0_core_xga_0406_0x1b2bebc.bin`（逻辑 ID 1535）；运行时经原 SpriteGlu TOC 加载，并非读取研究样本。

现有代码无条件绘制动画 27/35，按压资源没有接到绘制状态。专项通过真实 `Pointer` → `DrawOriginalControls` → OpenGL 帧缓冲比较复现；修复前两个按钮画面都不变。

原型样本偏移 4146/4152/4230/4236 分别为动画 27/28/35/36；均为单步 100ms，分别引用组合帧 24/25/40/39。原效果是上下状态切换，并无多帧缩放补间。实现沿用原 Sprite 绘制器，预热新增的 28/36，避免首次点击时加载图集。

## 验证记录

- Release 构建：`out/hud-button-red-build.log`，退出 0。
- `gun_bros_research.exe --original-hud-check --mute`：`out/hud-button-red.log`，退出 1；两个 `pressed-pixels-changed=0`，原区域／仪表验证仍通过。
- 修复后 Release 构建：`out/hud-button-build.log`，退出 0，GUI 主程序与附属研究程序均更新。
- 同一 `--original-hud-check --mute`：`out/hud-button-check.log`，退出 0；两个按钮 `pressed-pixels-changed=1 restored=1`，按住不重复触发、移出恢复、暂停取消蓝色按压、红色松手保持／关选择器恢复均通过。原 5 个输入区域、20 种道具图标和仪表回归通过。
- `--powerup-selector-check --mute`：`out/hud-button-selector-regression.log`，退出 0；14 次购买、38 个选择按钮、提示及隔离存档重载通过。
- `--pause-check --mute`：`out/hud-button-pause-regression.log`，退出 0；暂停菜单输入、返回及设置重载通过。
- 图像核对：`out/ui-original-2026-09-09/hud-button-idle-{0,1}.png` 与 `hud-button-pressed-{0,1}.png`，红／蓝按压帧分别有原版下陷与发光效果；释放后的完整 HUD 像素与点击前一致。

本次验证为真实 BIG、HUD 输入和 OpenGL 绘制专项，未声称完成整场战斗人工操作或全 HUD 的 1:1 验收。未改原资源及用户存档。
