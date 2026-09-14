# Powerup 模式过滤

## 方案与验收

本轮补齐商店已标注、战斗尚未执行的模式限制。运行时继续直接读取 BIG，不添加按道具 ID 维护的模式表。

- `entries/store_entry.bt`：STORE 文件偏移 2 的 uint32 为排除模式位，原对象成员偏移为 +8。
- 原 `CStoreAggregator::IsItemExcludedFromGameType`（反编译 :156283–156294）用 `mask & (1 << mode)` 判断排除；商店 `CreateContentSprite`（:150197）使用同一规则显示模式状态。模式下标为单人 0、多人 1、Deathmatch 2。
- 原 `CPowerUpSelector::Bind`（:187378–187449）检查存档装备的商品模式限制，被排除时通过 `CPowerup::IsButtonDefault` 选择默认道具。
- `entries/powerup_template.bt` 与原 `CPowerup::Template::Init`（:187947）确认 Powerup 模板本身没有该模式位。道具能力仍由原 Flow 出口判断。

## 任务

1. 增加三种模式的真实选择器回归，先确认原实现失败。
2. 为商品排除位提供统一查询，战斗选择器与 PowerupScene 使用同一规则。
3. 检查快捷键、轮换、机器人、装备恢复和直接使用入口；禁用道具不得扣库存或启动效果。
4. 执行针对性 Debug 测试并构建 Debug、Release 游戏，记录结果。

验收：模式列表与原商品位一致；已有库存不能绕过限制；跨模式装备恢复使用原默认出口；允许的道具仍正常工作。

## 实现结果

- `CStoreItem::excludedGameModes` 保留原文件读取顺序，并以 `IsExcludedFromGameType` 统一商店标注和战斗查询。
- `PowerupScene` 从原 STORE 专用商品引用取得限制，不使用礼包的包含关系替代规则。可玩道具缺少规则时记录资源位置并停止初始化，不默认放行。
- `IsSupported` 覆盖选择、装备、轮换、快捷键、机器人、立即使用及死亡后使用入口；`Use` 在扣库存、启动脚本前再次检查。
- `GetEquipped` 对模式禁用的存档选项执行原 export 4，恢复允许的默认道具；保留库存。
- 原效果测试中 Speed / Defense / Attack Boost 改在明确的 Deathmatch 上下文执行，同时推进模式冷却计时。旧 Tantrum 清理回调的交叉状态研究仍保留，不将跨模式组合当作正式玩法。

## 验证记录（2026-09-14）

| 检查 | 结果 |
|---|---|
| 修改前 `pwsh -File tests/run.ps1 -Case powerup-selector` | 退出 1；三模式与生前/死后列表共 14 处不符。`tests/powerup-mode-red-selector.log` |
| 修改后 `pwsh -File tests/run.ps1 -Phase OriginalUI,Core -Case powerup-selector,powerup-play` | 退出 0；两项通过。`tests/powerup-mode-final.log` |
| `local-live` / `deathmatch` / `deathmatch-feedback` | 各退出 0。`tests/powerup-mode-regression.log` |
| Debug / Release `GunBrosRe.vcxproj`，x64，`SkipAutoTests=true` | 均退出 0。`tests/powerup-mode-Debug-build.log`、`tests/powerup-mode-Release-build.log`；专项通过上述独立命令运行 |
| 原件保护检查 | `protected-changes=0`；BIG、测试存档样本及用户存档未变更 |

选择器与原 STORE 位逐项对照：单人、多人生前均 13 项，死亡后均 1 项；Deathmatch 生前 9 项，死亡后 0 项。底层测试覆盖已有库存、允许模式选中后切换到禁止模式、两个装备槽的默认恢复、道具轮换，以及禁止使用不扣库存、不启动 Movie。现有购买、点击、装备保存重载和道具实际效果回归通过。已查看选择器截图，未重复全量截图基线。

### 独立的既有测试差异

额外运行 `powerups` 时退出 1：`PowerupCatalogChecks.cpp` 对电击手雷 BULLET 93 预期 3 次脉冲，实际报告为 320 / 1312 / 2320 / 3328 ms 共 4 次。该检查不经过本次新增的 STORE 模式过滤；`PowerupCatalogChecks.cpp`、`PowerupCatalog.cpp`、`CPowerup.cpp`、`CBullet.cpp/.h` 均与本轮开始时一致（`git diff --quiet` 退出 0）。未修改这一独立的脉冲实现或调整其断言，不能将完整 `powerups` 检查宣称为通过。
