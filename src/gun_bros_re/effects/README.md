# 共享特效

游戏与 UI 共用的粒子播放、效果实例管理及特效呈现。此目录属于 `gun_bros_re`，仍编入现有单工程的 Game、Viewer、Tests；不单独产出库。

| 职责 | 文件 |
|---|---|
| BIG 粒子模板及缓存 | `CParticleEffect.*`、`ZParticleResources.*` |
| 粒子运动、发射、播放与共享池 | `CParticle.*`、`CParticleEffectPlayer.*`、`CParticlePool.*` |
| 地图临时效果与独立效果层 | `CParticleSystem.*`、`CEffectLayer.*` |
| 附属效果持有与停止 | `EffectHolder.h`、`EffectContainer.*`、`ParticleEffectHolder.*`、`TrailEffectHolder.*` |
| 拖尾、电弧与颜色纹理 | `CRibbonTrailEffect.*`、`CLightningArc.*`、`ZBulletEffectSettings.h`、`ZEffectColors.*` |

菜单、Powerup、角色和弹体在各自对象中决定播放时机与生命周期。共用实现不等于共用所有实例：地图临时系统、地图效果层、UI、角色强化及屏幕道具继续使用各自的持有者和池。

通用 Sprite 展开、批绘制和预览投影在 `engine`；弹体行为、弹体资源缓存及战斗音频在 `gameplay`。BIG 保持唯一资源事实来源，目录中没有人工维护的游戏资源表。

原版依据、目录迁移范围及验证见 [目录归并记录](../../../docs/effects-directory-migration.md)；本目录名是本项目组织方式，不冒充原 iOS 目录或 BIG 包名。
