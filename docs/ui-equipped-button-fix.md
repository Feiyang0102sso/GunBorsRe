# 已装备商品动作按钮修复（2026-09-19）

用户报告 WTF7000 已显示 EQUIPPED，右下角仍显示 EQUIP。上一轮 UI 整理遗漏了无动作按钮状态，旧双枪测试还保留了“金星武器仍显示 EQUIP”的错误预期；当时通过的检查不能证明已装备商品的按钮可见性正确。

## 根因与原版依据

`src/gun_bros_re/ui/menus/CMenuStoreOption.cpp` 的折叠、展开绘制先为已拥有商品选择 EQUIP，仅在可升级时覆盖为 UPGRADE，没有处理已装备且不可升级的情况。BIG 布局和双枪装备判定不是此次故障来源。

下列行号均指 `_prep/_IDA_OUT/gunbros_3.6.0_IOS.c`：

- `CMenuDataProvider::GetElementAction` 的 table 144 分支（153662）：已拥有未装备选择 action 57；已装备仅在可升级时选择 action 58，否则保留无动作的 193。
- `CMenuStoreOption::Bind`（182082）：action 不为 193 才启用按钮。`LevelCallback`（180799）按此状态绘制。
- `CStoreAggregator::CanBeUpgraded`（156746）：仅单个武器对象（type 6）可升级。展开卡此前未检查类型便按包和序号查武器，使盔甲误匹配同序号武器并显示 UPGRADE。
- `PurchaseInfoCallback`（180849）：仅有动作按钮时扣除其宽度。隐藏按钮后需求文本恢复使用完整可用宽度。

卡片仍使用 BIG 的 SHOP_BOX Movie、按钮 Movie 及原区域；结构参考 `ui_movie.bt`，没有新增手写资源布局。

## 修复与验收

折叠和展开卡统一遵循：已装备不可升级无按钮，已装备可升级显示 UPGRADE，已拥有未装备显示 EQUIP。按钮隐藏时取消待派发动作；展开卡按资源类型查询武器，避免跨类型序号碰撞。

新增 `tests/ui/StoreEquippedChecks.cpp`，使用真实 BIG 和实际 GPU 绘制，覆盖 6 种状态的两种布局，共 12 个按钮区域像素检查，以及 2 个选中后装备状态改变的取消检查。修复前复现 6 处失败；首轮修复后剩余展开盔甲失败，由此定位并修复类型检查遗漏。

- Debug、Release 的 Game、Viewer、Tests 六个产物构建成功，退出码均为 0。
- 两种配置分别运行 `pwsh -NoProfile -File tests/run.ps1 -Configuration <配置> -NoBuild -Case store-equipped,dual-weapon,mastery-upgrade,package-purchase`，均 4/4 通过，退出码 0，受保护文件变化为 0。脚本显式使用静音。
- 人工核对实际渲染截图：金星 WTF7000 折叠卡和已装备盔甲展开卡均无动作按钮。
- 本地证据位于 `obj/equipped-button-20260919/`：`before/` 为失败截图，`after-debug/`、`after-release/` 为修复后截图，构建和测试日志在同目录。未重复全量截图基线。

此修复验证商品动作按钮及相关装备、升级、购买流程，不代表所有 UI 行为已完整恢复原版。
