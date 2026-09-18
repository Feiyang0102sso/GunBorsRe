# 本地 Bot 归并（2026-09-17）

## 范围与方案

按用户要求，把本地合作／PvP 策略、寻路和 Bot 好友配置集中到 `gameplay/brother/bot/`。策略类统一为 `ZLocalCoopBot`、`ZLocalPVPBot`，不保留旧名别名或转发头。原版默认伙伴 `CBrotherAI` 仍留在 `brother/`。

调研确认：`CLevel::FindMatchRoute` 只被 PvP Bot 调用；`FindMatchDestination` 的巡逻／掩体评分也只服务该 Bot。把它们改为 `ZLocalPVPBot` 的私有方法，实现在 `ZLocalPVPBotNavigation.cpp`。关卡仅提供当前移动边界、碰撞数据、原地图路径和通行查询。`FindMatchSupply` 只是转发，删除并直接使用已有 `FindNearestPickup`。保持 A*、碰撞索引、候选评分与随机调用顺序不变。

同时迁入 `data/ZLocalBotFriend.h/.cpp`、`ZLocalBotRoster.cpp`。这些是 Windows 本地身份、配置及独立进度持久化，目录迁移不改变存档位置、格式、BIG 引用或游戏数据来源。

## 任务与验收

1. 迁移 8 个文件，更新类名、声明、include、UI／游戏／测试引用。
2. Bot 接管寻路及战术目标选择，删除关卡中的 Bot 算法接口。
3. 更新当前源码映射；历史记录保留旧名并注明迁移。
4. 构建 Debug Game／Viewer／Tests，运行合作、对战、对战反馈和本地好友配置相关检查。

## 非 Bot 的后续优化判断

- 后续已完成：`ZPlayerActor` 删除，角色脚本、强化状态与模式归回 `CBrother`，完整绑定显式重置，普通换枪保留角色身份与计时器。
- 后续已完成：`ZPlayerEquipment` 删除，装备槽归 `CBrother`，枪械／盔甲自身资源归 `CGun`／`CArmor`，旧躯干动画资源继续保留。结果见 [Brother 所有权归并](brother-ownership-migration.md)。
- 后续已完成：`ZPlayerPart` 与过渡 `ZBrotherRenderer` 均已拆除，角色、枪械、盔甲和弹体分别拥有绘制数据；OpenGL 缓冲／纹理仍使用 `ZMeshBuffer`／`ZTexture`。见 [绘制职责归并](brother-renderer-removal.md)。

Bot 归并当时先独立验证目录迁移；用户随后批准角色所有权归并，现已完成并另行验证，见上述记录。

## 验证结果

- `pwsh -File obj/build-runtime.ps1 -Product Game`：退出码 0，Debug Game／Viewer／Tests 全部构建成功；日志 `obj/bot-migration-build.log`。
- `pwsh -File tests/run.ps1 -Configuration Debug -Case local-live,deathmatch-data,deathmatch,deathmatch-feedback -NoBuild`：退出码 0，4/4 通过，受保护文件变化 0；日志 `obj/bot-migration-tests.log`，汇总 `obj/bot-migration-summary.json`，各项输出 `obj/bot-migration-evidence/`。
- 合作检查包含 10 个 Bot 名单的选择保存／重载、匹配轮换、独立进度及好友强化档位。
- 对比迁移前后的完整对战输出，44 条 Bot 导航记录一致。归属变换还通过 `obj/audit-bot-navigation.py` 核对，寻路与目标选择算法未改动。
- 三个好友／名单文件与迁移前内容相比，仅 include 路径变化；源码与测试无旧 Bot 类型名、旧路径或 `FindMatchRoute`／`FindMatchDestination`／`FindMatchSupply` 残留。
- `git diff --check` 通过。自动运行均静音；本轮未运行 Release 或全量截图基线。
