# 双枪装备与标识修复

## 范围与依据

用户要求：同时保留两把不同的枪，两把已装备枪都显示 EQUIPPED；切换当前枪不改变装备组合，不能把另一槽已装备枪再次覆盖到当前槽。

原版依据：

- `CStoreAggregator::GetItemStatus`（反编译 :155261，枪械分支约 :155316）用 `IsGunEquipped(..., -1)` 查询全部枪槽。
- `CPlayerConfiguration::IsGunEquipped` :170480 搜索两槽，匹配时返回槽号，未匹配返回 -1。
- `CPlayerConfiguration::SetGun` :171692 的非强制路径先查询两槽；该枪已经装备时不覆盖任何槽。
- `entries/store_entry.bt`：STORE 商品的实体引用与枪械分类分开；`saves/save_payloads.bt`：两枪引用和当前槽分别保存。原 BIG、原存档只读。

## 复现、修复与验收

先将商店原有的单槽状态判断提取为共同函数，保持旧行为，添加 `--dual-weapon-check`。测试使用 BIG 的三个实际单枪商品、独立原生存档副本，点击原 Movie 区域中的装备按钮。

红态 `out/dual-weapon-before.log` 退出 1：当前槽为 0、1 时均 `stamps=1`；重复点击另一把已装备枪后均 `correct=0 distinct=0`，共四项失败。对已装备金牌枪测试 EQUIP 按钮，确保保护来自写入逻辑，不能仅靠隐藏按钮防重复。

修复：

1. 商店折叠卡、展开卡及旧装备入口共用双槽状态判断，EQUIPPED 与当前手持枪分离。
2. `CPlayerConfiguration` 增加原名的双槽查询和非强制 `SetGun` 保护；普通商品、礼包与旧装备入口共用，不能直接把另一槽枪覆盖过来。
3. 保留现有 2 键切枪方式，只改变当前槽，不重写两把装备枪。

绿态 `out/dual-weapon-after.log`：两种当前槽均 `stamps=2`，重复装备保持 A/B，换装第三枪只替换当前槽，原生保存重载一致。专项永久保留为研究菜单 **71**，并加入 `test-muted.ps1 -Phase OriginalUI`。

验收命令统一 `--mute`：

- `--dual-weapon-check`：双标识、实际按钮输入、防重复、第三枪替换、当前槽及双枪保存重载。
- `--profile-play-check`：进入战斗、鼠标与 2 键切枪、两侧道具、进度保存与续关。
- `--store-card-check`：原卡片状态、展开/收起、预览和商店切枪按钮。
- `--package-purchase-check`：礼包购买、装备分配、原生保存重载。

上述四项最终均退出 0，日志依次为 `out/dual-weapon-after.log`、`out/dual-weapon-combat.log`、`out/dual-weapon-cards.log`、`out/dual-weapon-packages.log`。两种当前槽的实际卡片截图为 `out/dual-weapon-slot-0.png`、`out/dual-weapon-slot-1.png`；`git diff --check` 通过。

构建日志 `out/dual-weapon-build.log`；正式与研究 Release EXE 均更新。测试写入 `out/`，不替用户选择武器，也不改写已存在的真实存档。若旧错误已把某存档写成重复双枪，重新装备一把不同枪即可恢复组合；不能凭空猜测被覆盖的原武器。
