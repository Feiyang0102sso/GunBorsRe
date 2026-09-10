# 菜单横扫仅限导航分支切换（2026-09-09）

用户反馈：横扫（WIPE）应只在切换菜单大项（PLAY/BROS/BRO-OPS/STORE/REFINERY/OPTIONS/GAMES）时播放。商店内 GUNS/ARMOR/POWER UPS/BANK 互切，以及星球页打开 REVOLUTION 列表，原版都没有横扫条。本轮按原逻辑收窄触发条件。行号均指 `_IDA_OUT/gunbros_3.6.0_IOS.c`。

## 原版依据

| 位置 | 行为 |
|---|---|
| `CMenuSystem::SetBranch` :96614 | 请求的分支就是当前分支（`[+1272] == a2`）时**直接返回**，不碰 WIPE；只有换分支的那条路径执行 `CMovie::SetTime(wipeMovie, 0)` 重放横扫 |
| `CMenuSystem::PushMenu` / `SetMenu` :96666 / :96700 | 先把菜单压入目标分支，再调 `SetBranch(this, branch, 27)`；分支未变时同样走上面的提前返回 |
| `CMenuAction::DoAction` :93478 → :95106 | 商店分类按钮的 action 64（`MDS_BUTTON_STORE_CATEGORIES` 四项均为 64）落到当前菜单自己的 vtable 槽 7，**从不经过 SetBranch** |
| `Transition1/2Callback` :96250/:96279 | 横扫的两幅画面分别取自成员 +1272（旧分支）与 +1276（新分支），本身就说明它是分支级过场 |

结论：横扫属于分支之间的导航。同一分支内换菜单——商店分类、星球页进 REV 列表——都不播放。

## 改动

宿主原条件是 `state.page != previousPage || state.shopCategory != previousCategory`，把页面变化和商店分类变化都当成过场。现改为比较分支：

- 新增 `MenuBranchPage(page)`，把宿主页面归到所属分支（用分支自身的页号表示）。分组沿用 `GameMenu::Header` 里既有的“导航栏点亮哪一项”表，两处现在共用这一个函数，不再各写一份：
  `1/17/18 → 2`（STORE）、`16/19/21/22/23 → 0`（PLAY）、`8/9/11 → 6`（OPTIONS）、`13 → 5`（BRO-OPS）、`29 → 4`（BROS），其余页面即自身。
- 横扫条件改为 `MenuBranchPage(state.page) != MenuBranchPage(previousPage)`，`shopCategory` 不再参与。
- 分支外的页面（14 标题、24 问候、25 兄弟选择、26 精通弹窗、27/28 结算）各自成组，行为与之前一致。

顺带一致化的还有帮助页：6 → 8 同属 OPTIONS 分支，现在也不横扫。

## 验证

`--loading-wipe-check` 原先断言分类切换和星球进 REV **有**横扫，与原版相反，本轮按上述依据改写：分支切换仍要求 `starts=1` 且中点在播；分支内要求 `starts=0`、`active=0`。星球用例去掉了尾部那次商店点击——原先它靠横扫期间输入被屏蔽才停在 REV 页，没有横扫时它只会真的切去商店。

```
[loading-wipe-check] cold-load=1400 midpoint-active=1 time=350 starts=1 expected-sweep=1
[loading-wipe-check] real-shell target=0 page=6 category=0 blocked-store-click=1 failures=0   # STORE→OPTIONS
[loading-wipe-check] real-shell target=1 page=0 category=0 blocked-store-click=1 failures=0   # STORE→PLAY
[loading-wipe-check] cold-load=1400 midpoint-active=0 time=0 starts=0 expected-sweep=0
[loading-wipe-check] real-shell target=2/3/4 page=2 category=1/2/3 blocked-store-click=0 failures=0
[loading-wipe-check] planet-to-REV page=21 active=0 starts=0 expected-sweep=0 failures=0
```

截图：分支切换中点仍有横扫 `out/ui-wipe-menu-0.png`；分类切换 `out/ui-wipe-menu-3.png` 与星球进 REV `out/ui-wipe-revolution.png` 均为完整页面，无横扫条。

菜单回归（均退出 0）：`--game-menu-check`、`--play-interaction-check`、`--header-check`、`--mission-menu-check`、`--store-card-check`、`--options-check`、`--social-check`、`--greeting-check`、`--player-select-check`、`--postgame-menu-check`、`--bank-check`、`--refinery-menu-check`。

`--planet-menu-check` 退出 1，但这与本轮无关：把 `GameFrontEnd.cpp` 还原到 HEAD 重新编译后同样退出 1，且停在同一处（`mode-original-expanded.png` 之后的模式浮层循环，未打印任何 `[planet-check]`）。日志 `out/planet-menu-baseline.log` 与 `out/wipe-regress-planet-menu-check.log` 内容一致。该失败属于既有问题，未在本轮处理。

Release 构建：`MSBuild.exe gun_bro_re.slnx /p:Configuration=Release /p:Platform=x64 /m /v:minimal`，退出 0。

## 验证边界

- 分支归属沿用宿主既有的导航点亮分组，它本身来自原 NAVBAR_MAIN；不属于导航栏的页面（标题、问候、兄弟选择、结算）按现状各自成组，本轮没有为它们找原版分支依据。
- 没有横扫时，进 REV 的首次冷加载会直接停留在上一帧。原版是否在此另有加载指示（`CMenuSystem::SetLoadIndicatorHidden` 存在但消费点未核对）尚未确认，留待单独查。
