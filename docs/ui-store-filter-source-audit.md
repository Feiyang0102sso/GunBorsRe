# 商店筛选谓词与真实资源案例

> 研究快照：本文中的“当前实现”和旧路径对应调查时点。相关修复已实施，最终归属、仍未恢复的部分及验收以 [UI 重组结果](ui-optimization-result.md) 为准。

日期：2026-09-19。范围：只读核对 `CStoreAggregator::InitFilteredList`、`GetItemStatus`、`SetRootCategory` 与当前 `ui/content/CStoreAggregator.cpp`；仅新增本研究文档，未修改实现/测试，未构建。所有原源码行号均指 `_prep/_IDA_OUT/gunbros_3.6.0_IOS.c`。

## 结论

当前的“类别相交，再与 Owns 相交”不符合原筛选。原类别单选排除已拥有和已装备；类别加持有位则包含该类别的未拥有和已拥有/已装备；仅持有位返回持有内容。默认 ALL 还包含独立的已装备位。

零价道具排除没有原代码依据，但真实资源中的唯一零价道具已经通过 hidden 和 displayOrder 隐藏，不能称为已证实的商品漏项。可删除多余价格判断，保持资源隐藏规则。

## 状态值先于筛选

原函数 `GetItemStatus:155261`，获取时缓存为可选，不改变以下业务优先级：

1. singlePurchase 非零：由 `CPackageOfferMgr::IsPackagePurchased` 返回状态 3 或 -1；不以礼包全部对象的持有情况代替。
2. 正好一个对象：枪在两个枪槽任一个已装备，或盔甲已装备，先返回 4；其他非道具对象有原库存记录且持有字节为真，返回 3。道具不以普通库存记录返回 3。
3. 非单对象且 `CountNonConsumables >= 2`：逐一核对枪/盔甲，全部已装备或已拥有时返回 3；不能仅检查第一件。
4. 剩余一般分支：商品 category16 返回 6；其他商品未达到 requiredLevel 返回 5；`CanItemBeAcquired:155150` 为假返回 7。
5. 可取得且达到等级时，按运行时商品 mem+252：值 1→状态0、2→状态1、3→状态2，其他→状态-1。

**状态0/1/2不是金币/Warbucks/免费枚举。** mem+252 的默认值为0（构造160208、Reset159785），由 `CStoreItemOverride::OverrideItem:233136` 根据动态标签改为1/2/3；它不在 STORE 磁盘布局中。当前无该覆盖时应保留 -1，不能由价格造出标签。标签字符串名称本研究未恢复，不给0/1/2发明名字。

`GetElementAction:153679–153702` 对 -1/0/1/2/6 产生购买动作56，状态3产生装备57，状态4按 CanBeUpgraded 产生升级58。状态5/7不产生该购买按钮动作。

筛选本身仅特别区分 **3与4**；-1/0/1/2，以及5/6/7/9，均先走一般的类别匹配规则，没有另按“能否买得起/是否等级锁定”从列表排除。显示不可购买按钮和隐藏整条商品是不同职责。

## root、ALL 与两个筛选位

原构造158975：枪 `R=0x7F`，盔甲`0x380`，道具`0x3C00`，银行`0x1C000`。这是源码枚举掩码，实体类型不能代替商品 category。

- bit18=`0x40000`：加入持有状态，含已装备；道具等特定类别使用数量查询。
- bit19=`0x80000`：独立加入已装备状态。
- `SetRootCategory:159378` 设置 `F = F | R | 0xC0000`，不是只设置 category。
- `RemoveItemFilterCriteria` 的 root 重载159332会移除 root 类别位及18/19；`ProcessFilterChange:178968` 离开 ALL 时使用这个流程。
- 当前约定 `filterAll=true` 时映射 `F=R|0xC0000`，`filterAll=false` 时 `F=shopFilter`。非零 shopFilter 不再偷偷补类别位或18/19。
- 必须保留独立的 ALL 状态：最后一个筛选项取消后真实 `F=0` 表示无选项，不等于 ALL。原 `ProcessFilterChange` 没有自动恢复全部的分支。

## 完整选择谓词

下述对应159129–159217。先排除不属于 R 的商品；特殊 root=5 研究入口绕过此筛选，不应塞进正式四类 UI 的默认分支。

定义：

- `C = (F & (1 << item.category)) != 0`
- `selectedCategories = F & R`
- `partial = selectedCategories != 0 && selectedCategories != R`
- `ownedEnabled = (F & 0x40000) != 0`
- `equippedEnabled = (F & 0x80000) != 0`
- `ownedStatus = status == 3 || status == 4`
- `hasCount = category在10..14且objectCount>0且原库存数量查询第一对象返回非零`。这里原范围确实包含14；银行14通常无对象所以不会命中。不能将所有类别都改用第一对象的 Owns。

按以下基础分支实现，顺序对应原代码：

```text
include = C

if ownedEnabled:
    if C or not partial:
        if ownedStatus or hasCount:
            include = true
else:
    if status == 3:
        include = false

if status == 4:
    if equippedEnabled:
        if not partial:
            include = true
    else:
        if not ownedEnabled:
            include = false
```

该简化保留原语义：已装备且bit19启用时，只在partial类别筛选下遵循include，非partial时直接放行；未启用bit19但bit18启用时仍按include处理已装备项；两个位均关则排除已装备项。

**之后仍要执行的条件，不能被上述 include 绕过：**

1. include为真时检查 `excludedGameModes & exclusionFilter`；交集非零排除。
2. 通过后调用 `OverrideItem`，再检查 hidden242==1 或 displayOrder<0；原代码不是先检查负排序再调用 override。
3. IAP value32==1 的特殊产品字符串后缀筛选在159218–159246，当前未完整复刻，本次不补猜测。
4. 按 `SortFilteredList:155433` 排序；当前配对排序是否覆盖原附加优先级尚未完整核对。

| 原F | 未拥有同类别 | 已拥有未装备同类别 | 已装备同类别 | 已拥有其他类别 |
|---|---|---|---|---|
| R\|18\|19（ALL） | 包含 | 包含 | 包含 | root内包含 |
| 仅一个类别位 | 包含 | 排除 | 排除 | 排除 |
| 同类别位\|18 | 包含 | 包含 | 包含 | 排除（partial=true） |
| 仅18 | 排除 | 包含 | 包含 | root内包含 |
| 仅19 | 排除 | 排除 | 包含 | 只有已装备的包含 |
| 0 | 排除 | 排除 | 排除 | 排除 |

表中18/19代表对应位，而不是十进制掩码。若类别选择恰好覆盖整个R，partial=false，持有/装备的特别行为应按谓词处理，不要把它等同于独立ALL选中状态。

## 已核对原始字节的真实案例

先从 `_prep/out/game-entry-catalog.json` 筛选339个非空STORE，再直接读取以下原解包样本核对；没有改动样本。字段偏移对应 `entries/store_entry.bt`，这些记录都恰好221字节，但读取器不能因此假设通用定长。

| 商品定位 | 实体引用 | category / order / hidden | 价格 | 验证用途 |
|---|---|---|---|---|
| pack0_core_xga STORE[0]，handle0x03000130，physical49，BIG偏移0x701a | type6，hash1482249506，ordinal0 | 0 / 1 / 0 | 0 / 0 | 默认枪已装备→status4；ALL出现，类别0单选隐藏，类别0+18恢复。原显示排序本来就是1，不依赖override。 |
| pack3_xga STORE[0]，handle0x0300013C，physical61，BIG偏移0x4de0 | type6，hash2520453，ordinal0 | 0 / 13 / 0 | 225 / 0 | 在测试副本中拥有但不装备→status3；类别0单选隐藏；类别0+18包含。未拥有时类别0+18仍包含，当前AND Owned实现会错误排除。 |
| pack3_xga STORE[1]，handle0x0300013D，physical62，BIG偏移0x4e50 | type6，hash2520453，ordinal1 | 0 / 49 / 0 | 8000 / 0 | 与前一条同类别作未拥有对照；仅18排除，同类别0+18包含。 |
| pack3_xga STORE[4]，handle0x03000140，physical65，BIG偏移0x4fc8 | type6，hash2520453，ordinal4 | 1 / 10 / 0 | 0 / 0 | 已拥有类别1对象在仅18中包含；在类别0+18且partial=true时排除。也说明零价本身不代表隐藏。 |
| pack3_xga STORE[79]，handle0x0300018B，physical140，BIG偏移0x6d33 | type17，hash2520453，ordinal13 | 10 / -1 / 1 | 0 / 0 | 唯一零价道具；原hidden=1足够隐藏，删除价格特判也不能使它出现在商店。 |

上述pack3商品的mode mask均0、singlePurchase均0。资源完整路径为 `_prep/big_360_out/<pack>/0xf4e02223/23_STORE/<pack>_<physical四位>_<BIG偏移>.bin`。

本组单对象记录的直接字节偏移：category0、mode2(u32)、实体type7、hash8(u32)、ordinal12、coins15(u32)、bucks19(u32)、displayOrder216(i16)、hidden218、singlePurchase219。

SHA256校验：

- core STORE0：C400F61C3742CE72D21BB5CBE6D11027A03E4DEA250638EAE2A4B8D82388D067
- pack3 STORE0：5363736E5D0E9B2B3B775049B87DF1A2F5A2D61E75CA268B8E5C613BC69D88BC
- pack3 STORE1：26B18236D622BBA4415D828716CB62F40A5781EC86FD0B1105DFC47DE06793A5
- pack3 STORE4：4F0B70E980431C4F5ACA07E4C76360FC85068908C3C6452F7EE873FD54AD9BE8
- pack3 STORE79：AC836E1521307D04BFBB99691FB81E47EC6AD9ABAE9273EDF553A5789EC25EEE

## 验证建议与边界

使用现有真实BIG加载器取得上述对象，仅在测试临时profile中改变拥有、装备和道具数量；不要手写替代资源表，不修改原样本存档。按表比较完整商品引用，不依赖屏幕位置和假设条目下标。另覆盖“取消最后选项”得到空过滤列表，而非恢复ALL。

本研究没有运行原iOS UI，结论是原源码分支推演与原资源字节核对。完整联网覆盖数据、IAP产品尾缀和排序优先级尚未验证。单项已确认筛选谓词不能当作CStoreAggregator全部行为已经复刻。
