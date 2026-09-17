# 共享特效目录归并

2026-09-17。用户已批准将 UI 与战斗共用的特效实现归入 `src/gun_bros_re/effects/`。

## 范围与依据

这是源码目录整理，保留现有类、对象持有关系与 BIG 读取方式。原符号来源见 `source_tree.md` 中的 `particle.cpp`、`particleEffect.cpp`、`particleEffectPlayer.cpp`、`particleSystem.cpp`、`effectLayer.cpp`、`levelObject.cpp` 和 `glTools.cpp`；粒子磁盘结构依据 `entries/particle_effect.bt`。此前已核对的消费者与行为依据见 [夜间迁移记录](weapon-effects-night-migration.md)。`effects/` 是本项目的目录组织，不表示原 BIG 分包或原 iOS 文件夹名称。

迁移 28 个文件：`CParticle`、`CParticleEffect`、`CParticleEffectPlayer`、`CParticlePool`、`CParticleSystem`、`CEffectLayer`、`CLightningArc`、`CRibbonTrailEffect`、`EffectContainer`、`EffectHolder`、`ParticleEffectHolder`、`TrailEffectHolder`、`ZParticleResources`、`ZEffectColors`、`ZBulletEffectSettings` 对应头文件与实现。

`CBullet`、`CBrother`、`CPowerup`、菜单、`CLevel` 保留原位置；弹体缓存与战斗音频仍在 gameplay，通用 Sprite 绘制及投影仍在 engine。UI 直接使用共享播放器，地图、强化与 UI 各自保留实例与池，不新增统一持有者。

## 任务与验收

1. 保存已有工作区状态与本轮涉及文件的原始内容；移动上述文件，批量更新精确 include 路径。
2. 更新当前源码映射和结果文档，增加目录职责说明；不改资源、算法、数值、类名或已有注释。
3. 对移动前后及引用更新前后的内容逐字节核对，要求只出现已登记的路径替换；确认旧路径无源码引用。
4. 构建 Debug 三产物；运行 `weapon-effects,postgame-presentation,refinery-menu,postgame-menu,powerup-play,player-death,viewer-controls`，覆盖共享特效的 UI、战斗与 Viewer 调用。

## 完成与验证

28 个源码文件已归位；对移动文件及更新引用的文件共 49 个逐字节核对，仅存在登记的 include 路径替换。旧位置文件及源码中的旧 include 引用均已清除，没有转发头、别名或新增工程。迁移前内容、工作区状态与 SHA256 清单保存在 `obj/effects-directory-before/`。

| 验证 | 命令 | 退出码与结果 |
|---|---|---|
| Debug 构建 | `pwsh -File obj/build-runtime.ps1 -Product Game` | 0；Game、Viewer、Tests 均成功 |
| 首轮组合回归 | `pwsh -File tests/run.ps1 -Configuration Debug -Case weapon-effects,postgame-presentation,refinery-menu,postgame-menu,powerup-play,player-death,viewer-controls -NoBuild` | 1；运行至第 4 项时 player-death 失败，后续中止 |
| 死亡检查单独复跑 | `pwsh -File tests/run.ps1 -Configuration Debug -Case player-death -NoBuild` | 0；四个地图断言失败数均为 0 |
| 同一七项组合再次运行 | 同首轮命令，未修改二进制或断言 | 0；7/7 通过，受保护资源变化为 0 |

自动测试通过运行脚本显式传入 `--mute`。构建日志为 `obj/effects-directory-build.log`；首轮日志与证据为 `obj/effects-directory-checks-initial.log`、`obj/effects-directory-evidence/`；单独复跑为 `obj/effects-directory-death-repeat.log`、`obj/effects-directory-death-repeat/`；最终组合为 `obj/effects-directory-checks-final.log`、`obj/effects-directory-final-evidence/`，其中保留各项日志、结果及截图。

首次死亡检查在 pack2 的 scenario=1 出现 5 次断言失败，其余地图通过。相同二进制的单独复跑和最终组合均未复现；具体失败断言及根因尚未确认，不能据此宣称已修复或确定为既有问题。本轮没有通过修改游戏逻辑、测试断言或容差规避失败，原始失败证据保留待后续定位。

当前 Debug 产物可运行，目录迁移完整。未重复全量截图基线，未执行本轮 Release 构建，未提交 Git。此前行为迁移仍待核对的事项继续以夜间迁移记录为准。
