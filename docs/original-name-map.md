# 原版符号与重建命名对照

2026-09-08。依据带符号 iOS 程序及 `_IDA_OUT/gunbros_3.6.0_IOS.c`。原编译单元文件名常为 `bullet.cpp`，类名为 `CBullet`；当前工程沿用已有“类名作为文件名”的统一方式，没有把所有文件批量改成小写。

## 原名保留与本晚补齐

下列文件均位于 `src/gun_bros_re/`。表中“原名”只表明符号对应，不能据此认为全部原方法已经实现。

| 原版符号 | 当前文件 | 对应职责／边界 |
|---|---|---|
| `CArmor` | `gun_bros/CArmor.*` | 模板、挂点、装备脚本和五项属性 |
| `CPlayerConfiguration` | `gun_bros/CPlayerConfiguration.h` | 双枪、四盔甲槽及原默认配置 |
| `CPlayerProgress` | `gun_bros/CPlayerProgress.*` | 等级、经验和最大生命 |
| `CStoreItem` | `gun_bros/CStoreItem.*` | 原商品字段和资源引用 |
| `CRefinementManager` | `gun_bros/CRefinementManager.*` | 炼化槽、解锁、计时与产出 |
| `CBrother` | `gun_bros/CBrother.*` | 持枪状态、射击、投掷、强化、自动瞄准与炮塔活动标志 |
| `CBrotherAI` | `gun_bros/CBrotherAI.*` | 独立跟随、搜索和射击策略 |
| `CTargetingController` | `gun_bros/CTargetingController.*` | 本晚补 type 2 敌人搜索；未覆盖其他目标类别，随机序列为宿主固定种子 |
| `CStunController` | `gun_bros/CStunController.*` | 冰冻／电击控制器及原毫秒计时 |
| `CBullet` | `gun_bros/CBullet.*` | 弹体原脚本与碰撞回调；生成对象编号和强制池参数保留，固定容量池尚未重建 |
| `CPickup` / `CPowerup` / `CProp` | `gun_bros/CPickup.*` 等 | 三种不同原资源类别，不能混为通用 item |
| `CLevel` / `CEnemySpawner` | `gun_bros/CLevel.*`、`CEnemySpawner.*` | 原关卡脚本与调度；缺少的 native 调用仍显式记录 |
| `CCamera` | `gun_bros/CCamera.*` | 原一秒余弦缩放和中途重定向；视口倍数由宿主投影处理，位置缓动／震动仍未完整复刻 |
| `CLayerPathMesh` / `CLayerPathLink` | `gun_bros/CLayerPathMesh.*`、`CLayerPathLink.*` | 原导航层结构及锁节点操作；宿主寻路不等于原完整群聚算法 |
| `Mission` / `MissionObjective` / `Planet` | `gun_bros/Mission.h` 等 | 无 `C` 前缀是原符号，保留原名 |
| `CCrc32` | `engine/CCrc32.h` | 原高位左移 CRC，不能替换为普通反射 CRC32 |
| `CProfileManager` | `gun_bros/CProfileManager.*` | 保留领域原名；当前写入的是重建版独立文本格式，**不是原版二进制保存器** |

## 已纠正的含义

- `CArmor::m_flag4` → `m_slot`：原 `CPlayerConfiguration::SetArmor` 证明是装备槽。
- 盔甲模型附带字节 → `m_attachmentNode`：原 `CArmor::Bind` 证明是挂点，不能按两位兄弟编号解释。
- 弹体 native 9 → `GetZOrderGroup`：这是绘制分组；完整原分组排序仍待复刻。
- 冰冻和电击采用原 `CStunController` 名称，不使用泛化“暂停整个敌人脚本”的替代语义。
- `Mission` 的未知字段继续保留偏移含义；仅看到值 0–9，不能擅自命名为已证实的难度等级。

## 有意保留的宿主名称

`runtime/CombatScene`、`SurvivalSession`、`PlayerModel`、`GameFrontEnd`、各 `*Catalog`、`PickupScene`、`PowerupScene`、`OriginalProfile`，以及 `milestones/` 检查器属于本工程的装配、桌面适配或验证层。

- `GameFrontEnd` 不是原 `CMenuMission`／Flash 菜单的逐方法移植。
- `OriginalProfile` 读取并解释原 `NativeCProfileManager::SaveToDisk` 外层记录，导入后由本地 `CProfileManager` 写独立副本；不能将其重命名成已经完全重建的 `NativeCProfileManager`。
- `WeaponEffects` 和 `EnemyCombat` 是已有宿主组合层，虽然放在 `gun_bros/`，没有据此宣称存在同名原类。
- `SurvivalPilot` 是移动／射击输入测试驾驶器，不属于原游戏 AI；无敌检查也不代表普通玩家难度验证。

将来发现更精确的原对应时，再连同引用、工程文件和相关文档一起改名。保留用户原注释，用新增更正说明记录旧推断，避免破坏研究历史。
