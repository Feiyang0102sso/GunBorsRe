# data 整体优化结果

日期：2026-09-19。依据已批准的 [实施方案](data-optimization-plan.md)，在本轮开始时的工作区上增量实施；没有回退其他未提交修改。

## 当前职责

`data/` 原有 46 个源码文件归为四个包，目前共 38 个文件，根目录不再散放源码。

| 包 | 文件数 | 职责 |
|---|---:|---|
| `objects` | 6 | 游戏资源引用、类型分区、多版本索引、包内对象持有，以及 `CGunBros` 的资源访问实现 |
| `store` | 5 | 原始商品解析、商品目录和本地商品覆盖规则 |
| `mission` | 5 | Planet、Mission、MissionObjective 资源及生存关卡引用 |
| `profile` | 22 | 存档封装、客户端数据读写、库存、配置、进度及炼化等持久状态 |

`engine/resources` 继续负责 BIG、TOC、解压与原始资源读取，不依赖游戏业务包。`data/objects` 在它之上解释游戏类型与引用，并持有解析对象。菜单展示归 UI，模型与 GPU 加载归 graphics，Windows 存档文件操作归 host，研究探针归 tests。

## Z 文件逐项去向

本轮开始时 `data/` 有 23 个 Z 文件：21 个拆除，2 个模型适配文件迁至 `graphics/`；当前 `data/` 内为 0。没有为了改变前缀而保留别名或转发壳。

下表路径相对 `src/gun_bros_re/`，tests 和 Viewer 路径单独注明。

| 原文件 | 当前实现与变化 |
|---|---|
| `ZPackTables.h` | `application/CGunBros.h`、`data/objects/CGunBrosResources.cpp`；资源会话持有游戏包，包持有解析对象 |
| `ZBigVersions.h` | `data/objects/CGameObjectPack.h/.cpp`；版本与布局归同一索引所有者 |
| `ZWeaponCatalog.h/.cpp` | `gameplay/weapon/CGun.h`、`CGunResources.cpp`；单条模板加载和目录查询归 CGun，Viewer 标签归已有 `src/gun_bros_viewer/ViewerControls.*` |
| `ZArmorCatalog.h/.cpp` | `gameplay/armor/CArmor.h`、`CArmorResources.cpp` |
| `ZPowerupCatalog.h/.cpp` | `gameplay/powerup/CPowerup.h`、`CPowerupResources.cpp` |
| `ZStoreCatalog.h/.cpp` | `data/store/CStoreItemResources.cpp`、`CStoreItemOverride.*`；报价选择归 `ui/content/CStoreAggregator.*`，进度与炼化模板读取归各自原类 |
| `ZMissionCatalog.h/.cpp` | `data/mission/Mission.h`、`MissionResources.cpp`；研究入口留在 tests |
| `ZMissionCatalogInternal.h` | `tests/research/MissionStudyInternal.h` |
| `ZPlanetCatalog.h/.cpp` | `data/mission/Planet.*` 负责资源和关卡引用；展示条目归 `ui/menus/CMenuMission.h`、`CMenuMissionData.cpp` |
| `ZProfileImport.h/.cpp`、`ZProfileImportInternal.h` | 正式导入统一到 `CProfileManager::ImportNative`；只读报告与研究辅助函数归 `tests/research/ProfileStudy.*` |
| `ZProfileStorage.h/.cpp`、`ZProfileStorageInternal.h` | `data/profile/CProfileManager{Archive,Storage,Clients,Native}.*`；进度、炼化、每日奖励读写分归 `CPlayerProgressStorage.cpp`、`CRefinementManagerStorage.cpp`、`CDailyBonusTrackingStorage.cpp` |
| `ZMeshAssets.h/.cpp` | 原名移至 `graphics/`，保留 Windows 模型、PNG 与 GPU 组合加载的适配身份 |

另外删除 `src/engine/resources/ZResourcePacks.h` 的薄转发接口；加载界面直接实现 `CGunBros::LoadProgress`。新增 `host/ZProfileFiles.h/.cpp` 明确承担 Windows 读文件、替换文件和导入来源保护；没有把平台适配冒充原版类。

## 更贴近原版的实际变化

1. **对象跟随包生存。** 对照原 `CGunBros::InitGameObject/GetGameObject`（反编译行 78471、78497）与 `CGameObjectPack::InitGameObject/GetGameObject`（129772、129033），枪、盔甲、强化、商品、任务和星球首次按原引用解析后存入包。重复请求返回同一对象；文本也按包缓存，不再逐次重建 keyset。容器使用宿主的稳定地址存储，没有照搬 ARM 内存布局。
2. **目录与解析不再互相重复。** 装备目录读取各自包内模板，枪名关联复用商品目录；组合商品不能给内部武器错误改名。调用者需要的 `Entry` 是可复制的查询结果，底层资源对象由包持有。
3. **商店不再用研究推测隐藏武器。** 对照 `CStoreAggregator::InitFilteredList`（158999），正式装备筛选依据商品引用与槽位，不再拿自创 `visualOnly` 判定购买资格。该字段只保留作研究诊断。原商品覆盖与排序规则归 `CStoreItemOverride`（232916、233074）。
4. **星球资源与菜单组合分开。** Planet/Mission 引用和原 mapSlot 留在 data；菜单条目、标题与展示组合归 CMenuMission。存档通过真实 PLANET → type 1 MISSION → LEVEL 取得生存关卡，不再依赖菜单结构，也不再使用旧研究导入中按包号或脚本状态数量猜星球的路径。
5. **存档导入与正式读写共用一套封装。** 对照 `CProfileManager::LoadFromDisk/SaveToDisk`（202880、203163）和存档 BT，封装及调度归 manager，已确认客户端的序列化归对应类。未改记录保留原字节，修改记录保留未知字段并重新封装；导入失败不覆盖当前 profile，重新加载保留导入来源保护。Windows 文件替换独立到 host。

依据同时包括 `big_keyset.bt`、`object_counts.bt`、`entries/{gun_template,armor_template,powerup_template,store_entry,planet_entry,mission_entry}.bt`、`saves/GB_save_profile.bt` 和 `save_payloads.bt`。行号指 `_prep/_IDA_OUT/gunbros_3.6.0_IOS.c`，不是恢复出的原 cpp 行号。

## 验证记录

构建入口为单一 `GunBrosRe.vcxproj`，`GbProduct=Game` 同时产出 Game、Viewer、Tests；测试通过 `tests/run.ps1` 显式静音并检查受保护输入哈希。本轮未重复全量截图基线。

| 阶段 | 验证 | 结果 |
|---|---|---|
| 包归位 | Debug Tests；resources、big-version、missions、progress | 构建退出 0，4/4 通过，受保护文件变化 0 |
| 对象与目录 | Debug Tests；big-version、weapons、armor-data、powerups、missions、progress、store-equipped、native-profile | 构建退出 0，8/8 通过，受保护文件变化 0 |
| 存档拆分 | Debug Tests；original-saves、original-profile、native-profile、native-profile-play、daily-bonus、progress | 构建退出 0，6/6 通过，受保护文件变化 0 |
| 最终 Debug | Game、Viewer、Tests；自动 progress、big-version、viewer-controls 与 Viewer smoke | 构建退出 0，自动检查通过 |
| 最终 Debug 专项 | BIG 版本、武器、盔甲、强化、任务、进度、每日奖励、菜单、原存档、原版存档运行、商品卡片、装备与双持等 | 17/17 通过，退出 0，受保护文件变化 0；menu 分别在 Core/UI 执行 |
| 最终 Release 构建 | Game、Viewer、Tests | 构建退出 0 |
| 最终 Release 专项 | big-version、weapons、armor-data、powerups、missions、progress、native-profile、store-equipped | 8/8 通过，退出 0，受保护文件变化 0 |

本轮实施及验收完成。所有阶段测试均通过；原始输入哈希未变化。

新增 `tests/checks/GameObjectCacheChecks.cpp` 归入 progress 检查，以真实 BIG 验证对象地址稳定、重复请求不重读、商品目录复用、独立所有者隔离和空引用处理。

命令与完整日志保存在本地 `obj/data-optimization-20260919/`；阶段三详细报告归档为 `stage3-test-details.zip`，最终 Debug 日志、结果与输入哈希归档为 `final-debug-test-details.zip`，Release 结果及装备检查截图归档为 `final-release-test-details.zip`。`data-before.zip`、`sources-before.zip` 保存本轮开始时源码状态，不作为运行时输入。

构建命令（`Configuration` 分别为 Debug、Release）：

```powershell
& 'D:/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe' GunBrosRe.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:GbProduct=Game /v:minimal /nologo
```

最终 Debug 专项命令：

```powershell
pwsh -NoProfile -File tests/run.ps1 -Configuration Debug -Case progress,big-version,weapons,armor-data,powerups,missions,store-equipped,store-cards,dual-weapon,menu,native-profile,original-saves,original-profile,daily-bonus,deathmatch-data,native-profile-play -NoBuild
```

最终 Release 专项命令：

```powershell
pwsh -NoProfile -File tests/run.ps1 -Configuration Release -Phase Core,OriginalUI -Case progress,big-version,weapons,armor-data,powerups,missions,native-profile,store-equipped -NoBuild
```

静态核对：旧包装 include／调用清零（历史注释保留）；engine 不依赖 gun_bros_re；data 不依赖 UI／tests；`git diff --check` 退出 0。构建仍有图形等其他模块既有的数值转换警告，本轮 data 与新资源实现文件未报告编译警告。

## 保留的边界

- 包内类型缓存目前覆盖上述六类模板和文本；模型、粒子、地图等已有生命周期没有在本轮全部重写为原版通用对象分派。
- `Entry`、`Archive` 及部分 profile 字段仍是宿主查询／持久化投影。原程序完整客户端注册、虚表关系与远程同步没有全部恢复，不把拆分后的文件名视为完成证明。
- `CGunBros` 当前实现资源会话职责，没有宣称恢复原程序全部应用单例和生命周期。
- `ZGameSection` 等仍有意义的研究类型名保留；本轮目标是删掉多余模块和归还职责，不是把所有 Z 类型机械改名。
- `ZMeshAssets` 和 `ZProfileFiles` 保留平台适配身份。原始 BIG、存档样本不改写，也没有新增人工维护的游戏资源表。
