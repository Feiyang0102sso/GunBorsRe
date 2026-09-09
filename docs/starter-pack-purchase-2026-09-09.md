# Starter Pack 购买流程纠正

用户明确确认：购买后自动装备礼包内容；本次运行保留 OWNED，退出重启后隐藏；仅在 GUNS 出现。本记录纠正前轮“购买后立即移除”的错误理解，不扩展历史八小时任务。

## 调研、方案与验收

失败测试先从 BIG 加载真实礼包，以缺少源存档目录的新账户，通过折叠卡和展开卡分别点击购买。两种入口均发放 5/5 件装备，但装备配置为 0/5：`out/package-purchase-red.log`，退出 1。库存并非完全未发放；自动装备缺失和专属盔甲不可见共同造成了“装备不到账”。

方案分为三项：记录本次会话购买状态以延后隐藏；恢复原购买后的装备动作；应用原本地商品覆盖规则，使已拥有的隐藏盔甲可见。验收覆盖真实点击、两种活动枪槽、道具数量、重复购买、切分类、同进程重载、冷启动重载与再次穿戴。

## 一手依据

- BIG：pack3 STORE 91，引用 `2520451:91`，`singlePurchase=1`，35 个对象引用。包含两把枪、三件 Hunter 盔甲以及三组各十件道具。价格、对象顺序、盔甲部位均从 BIG 读取；实现不按名称或上述 ID 分派。
- 结构依据：`_Big_tool/binary template/big_assets/entries/store_entry.bt`、`armor_template.bt`、`_Big_tool/binary template/saves/save_payloads.bt`；原存档 1001 为装备配置、1002 为库存、1018 为礼包购买集合。
- 原反编译 `CStoreAggregator::EquipItem`（156082）：按商品对象顺序装备前两把枪，从当前活动槽交替；盔甲调用 `SetArmor`。`CPlayerConfiguration::SetGun/SetArmor`（171658–171750）避免重复装备同一个对象，盔甲部位来自模板。
- `CMenuAction::DoAction`（94575、94626）：购买成功后接装备动作 57，单纯调用 `AcquireItem` 不会完成自动装备。
- `CStoreItemOverride::OverrideItem`（233021–233084）：非禁用、非单次商品在拥有内容后，将负排序值覆盖为 10000；单次已购商品覆盖为隐藏。Hunter 三件独立 STORE 原排序均为 -1。10000 是原生派生算法常量，不是替代 BIG 的资源表。
- `GetItemStatus`（155288）单次礼包已购状态为 OWNED；动作分派（153661，case 144）不给已购礼包再次购买／装备按钮。隐藏时机以用户确认的原版实测为准；宿主用不序列化的会话状态保留本次卡片。

## 实现

- `CProfileManager`：成功购买同时记入原购买集合和会话集合；会话集合不写存档，普通重载保留，Reset 后加载表示冷启动。
- `StoreCatalog::GetStoreDisplayOrder`：按库存派生展示排序，保留原资源值和禁用字段。
- `GameFrontEnd::EquipStoreItem`：复刻礼包装备动作；完成后与库存、购买记录一起保存。列表仅在 GUNS 插入未隐藏礼包，已购保留 OWNED 且无购买按钮；展开购买不强制关闭详情。
- `--package-purchase-check`：独立附属研究入口，使用真实商店绘制和点击；测试账户位于 `out/package-purchase-check/`。未修改源 `saves/` 或实际 `userdata/saves/`。

## 验证记录

- [x] 修复前两入口稳定失败，证明原测试只查库存会遗漏自动装备。
- [x] 修复后两入口均发放并装备 5/5 件，三组道具数量准确；本次 OWNED、分类限定、重启隐藏、装备重载、拒绝重复扣款：`out/package-purchase-green.log`，退出 0。
- [x] 原生存档回归：`out/package-native-regression.log`，退出 0。
- [x] 进度／交易回归：`out/package-progress-regression.log`，退出 0。
- [x] Splash、角标和礼包分类流程回归：`out/package-ui-regression.log`，退出 0。
- [x] 最终 Release／Debug GUI 与研究工具构建，退出 0：`out/package-release-final.log`、`out/package-debug-final.log`。
- [x] 最终 `--package-purchase-check --mute`，退出 0：`out/package-purchase-final.log`。两种入口、两种活动枪槽全部通过；每个账户从 ARMOR 已购列表实际点击三次 EQUIP，重新穿戴三件 Hunter 盔甲，failures=0。点击已消失的 BUY 按钮区域也未再次扣款或发放。

最终截图：`out/package-purchase-check/34488687/{0,1}/after-purchase.png`、`after-restart.png`、`armor-re-equipped.png`。已逐项核对 OWNED 卡、Hunter 套装及重新装备结果；该专项只绘制商店面板，未绘制公共顶部导航。正常完整菜单截图保留在 `out/ui-feedback-2026-09-09/`。

旧版本未写入 1018 的历史购买，仍不根据已有装备猜测或补造购买记录。
