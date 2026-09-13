# 作弊码包

游戏窗口内直接连续输入小写字母，不按 Shift/Ctrl/Alt/GUI，无须回车；相邻字母间隔上限为 2.5 秒。Debug、Release 均可用。

| 指令 | 效果 | 适用位置 |
| --- | --- | --- |
| `chm` | 金币 +500,000，Warbucks +500 | 菜单、战斗 |
| `cht` | 推进每日奖励计时一天 | 菜单；战斗保留原有 dailyDayOffset 行为 |
| `chd` | 切换 DebugMode | 菜单、战斗 |
| `chc` | 切换宿主连接状态标记 | 菜单、战斗 |
| `chupdate` | 挑战日推进一天，立即更新 BRO-OPS 列表并保存 | 菜单、战斗 |
| `chxp` | 升至下一级起点，满级后不再增加 | 菜单、战斗 |
| `chlvmax` | 升至 BIG 经验表定义的满级 | 菜单、战斗 |
| `chw` | 解锁四个正式生存星球的全部波次 | 菜单、战斗 |
| `stboss` | 通过原 Flow 回调推进至 Boss | 生存战斗，受原有模式和动画状态限制 |
| `stsuicide` | 进入角色死亡流程 | 战斗 |
| `stbrow` | 请求队友正常换枪动画 | 战斗 |

已删除 `chh`、`chi`；解锁指令统一为 `chw`，满级指令为 `chlvmax`。`chw` 只修改波次进度，不赠送完美波奖励，也不绕过星球本身的等级门槛。

`chupdate` 按原轮换算法从 BIG 生成下一挑战日的任务，清空旧组的进度、参与记录及领奖标记，不补发旧组未领取的奖励。连续输入可继续前进，保存后重启仍保留。它不修改系统时间、签到计时或连接开关；查看联网界面仍需 `chc`。原版周期只向前更新，因此手动推进到未来后，自然刷新需等真实挑战日超过存档中的挑战日。

## 配置与职责

- `CheatConfig.h`：全部指令字符串及识别列表、金币和 Warbucks 数量、升级级数、天数、输入超时、Boss 快进步长和上限。界面提示集中为 `MoneyMessage`（两个 `%u` 分别填入金币、Warbucks）、`LevelMessage`（`%u` 填入升级后等级，两个升级指令共用）、`UnlockMessage`。修改文案时保留占位符，金额和等级自动填入。其余指令目前没有独立界面提示，诊断日志仍在对应实现中。
- `CheatKeys.h`：Shift+C 碰撞显示、Shift+I 信息显示、Shift+M 地图浏览、Shift+T 教程检查，以及地图选择、翻页、确认和返回键位。
- `CheatCodes.cpp`：按配置列表识别指令；不修改游戏状态。
- `CheatActions.cpp`：菜单／战斗指令、经验目标、波次解锁和存档调用。
- `BossCheat.cpp`：原 `SurvivalSession::SkipToBoss` 的宿主快进实现，保留原注释与 Flow 路径。

包位于现有游戏源码内，不新增工程。通用 SDL 事件队列仍归引擎，游戏通过回调注入识别规则和超时配置。

## 依据与验收

阶段方案：集中配置和入口 → 补充经验与波次行为 → 验证输入、战斗状态和原生存档重载。

等级门槛／满级来自 BIG 的 PLAYERPROGRESSION；依据 `entries/player_progression.bt`、原 `CPlayer::AddExperience`（反编译 101185、101250）和 `CPlayerProgress::GetExperienceForLevel`（193308）。战斗调用现有经验接口，更新双方生命上限并保持血量比例；同时更新运行时经验和存档经验。

波次上限来自各正式生存 LEVEL 的 `waveLimit`；依据 `entries/level_template.bt`、原 `CLevel::Bind`（121717）、`CMissionWaveStatus::AddWaves`（192830）和 `saves/GB_save_1003.bt`。复用原生存档的 survivalLevels 引用与 1003 写入路径，缺少原始绑定时报告失败，不手填 500。

定向检查：`pwsh -File tests/run.ps1 -Case progress,debug-input,boss,player-death`。`progress` 覆盖菜单输入、加钱、升级、满级边界、血量比例、四个星球解锁和原生存档重载；`debug-input` 覆盖配置中的所有指令、删除的指令、按键重复及 Shift 快捷键。原始 BIG、原存档样本和真实账号保持只读。

2026-09-13 验收结果：上述四项检查全部通过，退出码均为 0；BIG 满级为 200，满级累计经验为 13,811,105，四个生存关卡上限均为 500 波。测试保护检查发现 0 个原始样本／真实账号变更。Debug、Release 游戏构建通过。

Release 实际窗口已验证连续输入 `chm`、`chxp`、`chlvmax`、`chw`，正常退出并写入独立测试存档。旧 `tests/verify-runtime.ps1` 在已取消的 Viewer `--m1` 参数处停止；本次单独执行其游戏窗口验证部分完成上述检查，未声称旧整套脚本通过。日志保存在 `tests/out/Core/*/logs/`、`tests/out/runtime/`；构建日志为 `obj/cheats-debug-build.log` 和 `obj/cheats-release-build.log`。

## 调试快捷键统一入口（2026-09-13）

Debug 和 Release 均包含地图浏览及完整加载／返回流程。`DebugKeys.h` 统一读取 `GameHostSettings().debugMode`，Shift+M（地图）、Shift+C（碰撞）、Shift+I（信息）、Shift+T（菜单教程）只由此状态控制；不再用测试编译宏裁掉地图功能。启动状态来自 EXE 同目录 `GunBrosRe.cfg` 的 `DebugMode=0/1`，修改文件后重启生效。文字作弊码保持原样，`chd` 仍能切换运行时的同一状态。

地图浏览器内的方向键、Enter 和 Esc 沿用 `CheatKeys.h` 配置。Shift+F3 不再打开地图，普通 M 不触发。地图预览仍使用当前存档的副本，不写回正式进度。

验收：`pwsh -File tests/run.ps1 -Case debug-input,debug-map-profile` 通过（退出码 0），覆盖 0/1 配置、左右 Shift、重复按键、旧键失效、资源加载和存档隔离。`pwsh -File tests/verify-debug-keys.ps1 -Configuration Release` 从真实窗口验证开关、菜单选图进入战斗、战斗内浏览器、碰撞／信息开关、退出地图、教程进入和返回；脚本原样恢复 cfg，使用独立测试存档。Release 中测试代码与截图能力仍按原构建约定排除，调试快捷键和对应功能完整保留。
实际窗口最终验证：`tests/verify-debug-keys.ps1 -Configuration Debug` 和 `-Configuration Release` 均通过，两个版本分别覆盖 `DebugMode=0` 与 `1`，全部正常退出（退出码 0）。测试等待教程返回后的菜单加载完成再关闭窗口，避免把加载中的关闭请求误判为快捷键失败；cfg 在每轮脚本结束后逐字节恢复。
