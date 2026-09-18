# 源码文件名映射

日期：2026-09-17。路径相对 `src/`。历史研究记录可能仍使用旧名，以此表定位当前文件。

共享特效现统一位于 `gun_bros_re/effects/`，共 28 个源码文件，供 UI 与战斗共同使用。原 `gameplay/` 下同名文件不保留转发头；类名、池与持有关系保持不变。范围与验证见 [目录归并记录](effects-directory-migration.md)。下方历史批次中的旧路径按此迁移定位。

## 拾取物组合层拆分

目录归并：四个拾取物专属源码统一位于 `gun_bros_re/gameplay/pickup/`：`CPickup.h`、`CPickup.cpp`、`CPickupPresentation.cpp`、`CLevelPickups.cpp`。旧路径不保留转发头；下方历史记录按此定位。

目录层亦已清除：`data/ZPickupCatalog.h/.cpp`、`ZPickupEntry` 和 `LoadPickupCatalog` 不再存在。`CPickup::Template::Load` 读取并校验单条 BIG 模板，`CLevel` 按包 hash/局部序号持有模板，实例绑定模板和资源引用；目录标签及报告只在测试侧生成。

2026-09-17：拾取物组合类 `ZPickupScene` 及后续过渡缓存 `ZPickupResources` / `ZPickupVisual` 均已删除。活动实例归 `CPickup`，分配/回收归 `CLevelObjectPool`，调度/通知、模板/Sprite 包/批次归 `CLevel`，帧展开和绘制归 `CSpritePlayer`，商品奖励入口归 `CPlayer::CollectItem`。依据与验证见 [拾取物组合层拆分](pickup-responsibility-migration.md)。

## 夜间特效组合层迁移

`gun_bros_re/gameplay/ZWeaponEffects.h/.cpp` 已删除。下表按职责给出当前文件，不是旧类到一个新类的改名映射；原版依据与验证见 [夜间交接](weapon-effects-night-migration.md)。

| 原职责／状态 | 当前文件或持有者 |
|---|---|
| `ZShot`、弹体 Flow 与状态 | `gun_bros_re/gameplay/CBullet.h/.cpp`；移动／碰撞在 `CBulletProjectile.cpp`，呈现在 `CBulletDrawing.cpp`，附属效果在 `CBulletEffects.cpp` |
| 弹体实例、事件分发与帧调度 | `gun_bros_re/gameplay/level/CLevelEffects.cpp`，实现已有 `CLevel`；没有内部组合 Impl |
| 粒子模板、粒子、播放器和共享池 | `gun_bros_re/effects/CParticleEffect.*`、`CParticle.*`、`CParticleEffectPlayer.*`、`CParticlePool.*` |
| 地图临时粒子、20 效果槽与独立池 | `gun_bros_re/effects/CParticleSystem.*` |
| 原独立效果层的粒子槽 | `gun_bros_re/effects/CEffectLayer.*`，本次只恢复粒子分支 |
| 四槽附属效果、粒子／带状拖尾、电弧 | `gun_bros_re/effects/EffectHolder.h`、`EffectContainer.*`、`ParticleEffectHolder.*`、`TrailEffectHolder.*`、`CRibbonTrailEffect.*`、`CLightningArc.*` |
| 强化的六个播放器及共享池 | `gun_bros_re/gameplay/brother/CBrotherParticles.cpp`，状态归 `CBrother::PowerupParticles`；保留 `CBrotherPowerups.cpp` |
| UI、Powerup 屏幕粒子 | `ui/ZMenuSurface.cpp`、`gameplay/powerup/CPowerupPresentation.cpp` 直接持有共享 `CParticleEffectPlayer` |
| Sprite 展开与纹理缓存 | `engine/glu/sprite/ZSpriteRenderer.*`，复用 `CSpriteIterator` |
| Windows 投影与颜色纹理 | `engine/graphics/ZEffectProjection.h`、`gun_bros_re/effects/ZEffectColors.*` |
| BIG 粒子／弹体缓存 | `gun_bros_re/effects/ZParticleResources.*`、`gun_bros_re/gameplay/ZBulletResources.*` |
| Windows 声音合并、去重、播放时钟 | `gun_bros_re/gameplay/ZCombatAudio.*`；原 native 事件仍由各原类产生 |
| 跨绘制与碰撞的宿主值类型 | `gun_bros_re/gameplay/ZProjectileTypes.h`、`ZProjectileGeometry.h`；拖尾参数位于 `gun_bros_re/effects/ZBulletEffectSettings.h` |

## 既有迁移记录

2026-09-17 Bot 归并：本地策略统一为 `gameplay/brother/bot/ZLocalCoopBot.*`、`ZLocalPVPBot.*`；原 `level/ZDeathmatchNavigation.cpp` 改为同目录的 `ZLocalPVPBotNavigation.cpp`，寻路与巡逻／掩体选择现由 Bot 自己实现。原 `data/ZLocalBotFriend.*`、`ZLocalBotRoster.cpp` 同时迁入。`CBrotherAI` 仍是原版伙伴策略，留在 `brother/`。后续历史段落中的旧名按此映射，详见 [Bot 归并记录](bot-directory-migration.md)。

2026-09-17 角色所有权归并：所有过渡 `ZPlayer*` 模块已删除。`CBrother` 直接持有脚本、强化状态与装备槽，`CBrotherBody.cpp`、`CBrotherEquipment.cpp`、`CBrotherResources.cpp`、`CBrotherDrawing.cpp` 分别实现身体装配、装备操作、模板加载与绘制；`CGunDrawing.cpp`／`CArmorDrawing.cpp` 加载各自资源。`ZBrotherRenderer.h/.cpp` 随后也已拆除，根目录无 Z 文件，绘制直接归 `CBrother`；通用模型加载归 `data/ZMeshAssets.*`，裸模型浏览归 Viewer 的 `BrotherPreview.*`。详见 [Brother 所有权归并](brother-ownership-migration.md)。下述 13 文件是此前目录归并时的数量。

Brother 核心目录：`CBrother.h/.cpp`、`CBrotherPowerups.cpp`、`CPlayer.h/.cpp`、`ZPlayerModel.h/.cpp`、`CBrotherAI.h/.cpp`、`ZLocalCoopBot.h/.cpp`、`ZDeathmatchBot.h/.cpp` 共 13 个文件统一位于 `gun_bros_re/gameplay/brother/`。保留 `CBrotherPowerups.cpp` 的独立文件；数据、关卡与共享战斗模块不随之移动。

Powerup 本体目录：`CPowerup.h/.cpp`、`CPowerupActions.cpp`、`CPowerupPresentation.cpp` 统一位于 `gun_bros_re/gameplay/powerup/`。角色入口保留在 `gameplay/brother/CBrotherPowerups.cpp`，关卡调度保留在 `gameplay/level/CLevelPowerups.cpp`；选择器、Catalog 与测试仍在各自职责目录。

职责拆分新增的 `CPlayer`、`CMovieObject`、`CMovieChapter`、`ZHudResources`、`ZHudState`、`ZMapResources`、`ZMapPropWorld`、`CMenuInviteFriends` 和 `CMenuIncentives` 见 [重构结果](source-alignment-result.md)。表内的 Z 表示当前自建表示或适配，不表示数据来源不是原版。

Movie 完成通知：`engine/glu/movie/CMovie.*` 内的嵌套 `Playback` 保存每个使用实例的播放状态；`ui/CPowerUpSelectorPresentation.cpp` 是现有选择器的分文件实现，负责商品隐藏／主框关闭；`CInputPadControls.cpp` 负责操作界面恢复。没有新增顶层播放管理器，也没有把 UI 回调放进共享渲染缓存。

2026-09-16：新增 `gun_bros_re/gameplay/CParticle.*` 和 `CParticleEffectPlayer.*`，接收 `ZMapParticles`、`ZWeaponEffects` 内重复的粒子行为。两者均有原符号依据；本批恢复范围及未恢复的原粒子池边界见 [运行行为实施记录](runtime-alignment-roadmap.md#六实施记录r09-共享粒子核心2026-09-16)。

同日第二批新增 `gun_bros_re/gameplay/CParticlePool.*`；播放器持有活动粒子，池按原所有者共享，宿主保留绘制缓存。停止与回收边界见 [粒子池实施记录](runtime-alignment-roadmap.md#七实施记录r09-粒子池与停止语义2026-09-16)。

同日第三批：所有 `gameplay/CLevel*` 文件归入 `gameplay/level/`。`ZEnemyCombat.h` 已删除，部件、状态和动作归入 `CEnemy::Part/CombatState/Action`，不保留旧类型别名。`CEnemyCombat.cpp`、`CLevelCoop.cpp`、`CLevelDeathmatch.cpp` 是原类的分文件实现，不是三个新建 C 类。

| 本批旧路径 | 当前路径 |
|---|---|
| `gun_bros_re/gameplay/ZEnemyCombat.cpp` | `gun_bros_re/gameplay/CEnemyCombat.cpp` |
| `gun_bros_re/gameplay/ZEnemyCombat.h` | 已并入 `gun_bros_re/gameplay/CEnemy.h` |
| `gun_bros_re/gameplay/ZLiveCombat.cpp` | `gun_bros_re/gameplay/level/CLevelCoop.cpp` |
| `gun_bros_re/gameplay/ZDeathmatchCombat.cpp` | `gun_bros_re/gameplay/level/CLevelDeathmatch.cpp` |
| `gun_bros_re/gameplay/ZDeathmatchNavigation.cpp` | `gun_bros_re/gameplay/brother/bot/ZLocalPVPBotNavigation.cpp`，桌面 Bot 寻路与战术目标选择 |
| `gun_bros_re/gameplay/CLevel*.h/.cpp` | 同名文件统一放在 `gun_bros_re/gameplay/level/` |

第四批：`ZPowerupMoviePlayer.h/.cpp` 已删除，脚本、Movie、屏幕粒子及完成事件归同一个 `CPowerup`；私有图形数据与方法实现放在 `CPowerupPresentation.cpp`。旧 `GetMoviePlayer` 调用改为 `GetPresentedPowerup`，不保留兼容播放器对象。

第五批：删除 `ZPowerupScene::Use` 的按编号执行器分流；角色 native、实时查询和投掷预约收归 `CPowerupActions.cpp`，与 `CPowerupPresentation.cpp` 实现同一个 `CPowerup`。宿主保留库存、冷却和本地 Bot 政策，本批未额外删除 Z 文件。

第六批：`ZPowerupScene.h/.cpp` 已删除，无替代宿主类。选项、装备和库存视图归 `ui/CPowerUpSelectorInventory.cpp`；使用条件归 `gameplay/brother/CBrotherPowerups.cpp`；活动更新、清理及库存提交归 `gameplay/level/CLevelPowerups.cpp`；本地策略归已有 `ZDeathmatchBot.cpp`、`ZLocalCoopBot.cpp`。正式玩家使用 InputPad 内同一个选择器与资源目录，关卡直接持有执行中的 `CPowerup` 引用。旧 `GetPresentedPowerup` 已改为选择器的 `GetPowerup`。

| 本批旧路径 | 当前路径 |
|---|---|
| `gun_bros_re/gameplay/ZPowerupMoviePlayer.cpp` | `gun_bros_re/gameplay/powerup/CPowerupPresentation.cpp`，实现 `CPowerup` 成员 |
| `gun_bros_re/gameplay/ZPowerupMoviePlayer.h` | 已并入 `gun_bros_re/gameplay/powerup/CPowerup.h`，图形细节不进入公共头文件 |

| 旧路径 | 当前路径 |
|---|---|
| `engine/core/CMatrix4d.cpp` | `engine/core/ZMatrix4d.cpp` |
| `engine/core/CMatrix4d.h` | `engine/core/ZMatrix4d.h` |
| `engine/core/Paths.h` | `engine/core/ZPaths.h` |
| `engine/glu/movie/MoviePlayback.cpp` | `engine/glu/movie/ZMoviePlayback.cpp` |
| `engine/glu/movie/MovieRenderer.cpp` | `engine/glu/movie/ZMovieRenderer.cpp` |
| `engine/glu/movie/MovieRenderer.h` | `engine/glu/movie/ZMovieRenderer.h` |
| `engine/glu/movie/MovieResources.cpp` | `engine/glu/movie/ZMovieResources.cpp` |
| `engine/glu/script/CScriptResolver.cpp` | `engine/glu/script/ScriptResolver.cpp` |
| `engine/glu/script/CScriptResolver.h` | `engine/glu/script/ScriptResolver.h` |
| `engine/glu/sprite/CSpriteGluArchetype.cpp` | `engine/glu/sprite/ZSpriteArchetype.cpp` |
| `engine/glu/sprite/CSpriteGluArchetype.h` | `engine/glu/sprite/ZSpriteArchetype.h` |
| `engine/graphics/CMarkerBatch.cpp` | `engine/graphics/ZMarkerBatch.cpp` |
| `engine/graphics/CMarkerBatch.h` | `engine/graphics/ZMarkerBatch.h` |
| `engine/graphics/CMeshBuffer.cpp` | `engine/graphics/ZMeshBuffer.cpp` |
| `engine/graphics/CMeshBuffer.h` | `engine/graphics/ZMeshBuffer.h` |
| `engine/graphics/CPNG.cpp` | `engine/graphics/ZPNG.cpp` |
| `engine/graphics/CPNG.h` | `engine/graphics/ZPNG.h` |
| `engine/graphics/CQuadBatch.cpp` | `engine/graphics/ZQuadBatch.cpp` |
| `engine/graphics/CQuadBatch.h` | `engine/graphics/ZQuadBatch.h` |
| `engine/graphics/CShaderProgram.cpp` | `engine/graphics/ZShaderProgram.cpp` |
| `engine/graphics/CShaderProgram.h` | `engine/graphics/ZShaderProgram.h` |
| `engine/graphics/CTexture.cpp` | `engine/graphics/ZTexture.cpp` |
| `engine/graphics/CTexture.h` | `engine/graphics/ZTexture.h` |
| `engine/graphics/PNGEncode.cpp` | `engine/graphics/ZPNGEncode.cpp` |
| `engine/graphics/PNGEncode.h` | `engine/graphics/ZPNGEncode.h` |
| `engine/platform/CAudioPlayer.cpp` | `engine/platform/ZAudioPlayer.cpp` |
| `engine/platform/CAudioPlayer.h` | `engine/platform/ZAudioPlayer.h` |
| `engine/platform/CAudioPlayerInternal.h` | `engine/platform/ZAudioPlayerInternal.h` |
| `engine/platform/CMediaDecoder.cpp` | `engine/platform/ZMediaDecoder.cpp` |
| `engine/platform/CMediaDecoder.h` | `engine/platform/ZMediaDecoder.h` |
| `engine/platform/CWindow.cpp` | `engine/platform/ZWindow.cpp` |
| `engine/platform/CWindow.h` | `engine/platform/ZWindow.h` |
| `engine/platform/GLLoader.cpp` | `engine/platform/ZGLLoader.cpp` |
| `engine/platform/GLLoader.h` | `engine/platform/ZGLLoader.h` |
| `engine/platform/IWindowOverlay.h` | `engine/platform/ZWindowOverlay.h` |
| `engine/platform/Paths.cpp` | `engine/platform/ZPaths.cpp` |
| `engine/platform/Startup.cpp` | `engine/platform/ZStartup.cpp` |
| `engine/platform/Startup.h` | `engine/platform/ZStartup.h` |
| `engine/resources/ResourcePacks.h` | `engine/resources/ZResourcePacks.h` |
| `gun_bros_re/Config.h` | `gun_bros_re/ZConfig.h` |
| `gun_bros_re/HostSettings.cpp` | `gun_bros_re/ZHostSettings.cpp` |
| `gun_bros_re/HostSettings.h` | `gun_bros_re/ZHostSettings.h` |
| `gun_bros_re/LocalOnlineServices.cpp` | `gun_bros_re/ZLocalOnlineServices.cpp` |
| `gun_bros_re/LocalOnlineServices.h` | `gun_bros_re/ZLocalOnlineServices.h` |
| `gun_bros_re/StartupSequence.cpp` | `gun_bros_re/ZStartupSequence.cpp` |
| `gun_bros_re/StartupSequence.h` | `gun_bros_re/ZStartupSequence.h` |
| `gun_bros_re/StartupSequenceInternal.h` | `gun_bros_re/ZStartupSequenceInternal.h` |
| `gun_bros_re/data/ArmorCatalog.cpp` | `gun_bros_re/data/ZArmorCatalog.cpp` |
| `gun_bros_re/data/ArmorCatalog.h` | `gun_bros_re/data/ZArmorCatalog.h` |
| `gun_bros_re/data/BigVersions.h` | `gun_bros_re/data/ZBigVersions.h` |
| `gun_bros_re/data/LocalBotFriend.cpp` | `gun_bros_re/gameplay/brother/bot/ZLocalBotFriend.cpp` |
| `gun_bros_re/data/LocalBotFriend.h` | `gun_bros_re/gameplay/brother/bot/ZLocalBotFriend.h` |
| `gun_bros_re/data/LocalBotRoster.cpp` | `gun_bros_re/gameplay/brother/bot/ZLocalBotRoster.cpp` |
| `gun_bros_re/data/MissionCatalog.cpp` | `gun_bros_re/data/ZMissionCatalog.cpp` |
| `gun_bros_re/data/MissionCatalog.h` | `gun_bros_re/data/ZMissionCatalog.h` |
| `gun_bros_re/data/MissionCatalogInternal.h` | `gun_bros_re/data/ZMissionCatalogInternal.h` |
| `gun_bros_re/data/NativeProfile.cpp` | `gun_bros_re/data/ZProfileStorage.cpp` |
| `gun_bros_re/data/NativeProfile.h` | `gun_bros_re/data/ZProfileStorage.h` |
| `gun_bros_re/data/NativeProfileInternal.h` | `gun_bros_re/data/ZProfileStorageInternal.h` |
| `gun_bros_re/data/OriginalProfile.cpp` | `gun_bros_re/data/ZProfileImport.cpp` |
| `gun_bros_re/data/OriginalProfile.h` | `gun_bros_re/data/ZProfileImport.h` |
| `gun_bros_re/data/OriginalProfileInternal.h` | `gun_bros_re/data/ZProfileImportInternal.h` |
| `gun_bros_re/data/PackTables.h` | `gun_bros_re/data/ZPackTables.h` |
| `gun_bros_re/data/PickupCatalog.cpp` | `gun_bros_re/data/ZPickupCatalog.cpp` |
| `gun_bros_re/data/PickupCatalog.h` | `gun_bros_re/data/ZPickupCatalog.h` |
| `gun_bros_re/data/PlanetCatalog.h` | `gun_bros_re/data/ZPlanetCatalog.h` |
| `gun_bros_re/data/PowerupCatalog.cpp` | `gun_bros_re/data/ZPowerupCatalog.cpp` |
| `gun_bros_re/data/PowerupCatalog.h` | `gun_bros_re/data/ZPowerupCatalog.h` |
| `gun_bros_re/data/StoreCatalog.cpp` | `gun_bros_re/data/ZStoreCatalog.cpp` |
| `gun_bros_re/data/StoreCatalog.h` | `gun_bros_re/data/ZStoreCatalog.h` |
| `gun_bros_re/data/WeaponCatalog.cpp` | `gun_bros_re/data/ZWeaponCatalog.cpp` |
| `gun_bros_re/data/WeaponCatalog.h` | `gun_bros_re/data/ZWeaponCatalog.h` |
| `gun_bros_re/gameplay/BroAIDeathmatch.cpp` | `gun_bros_re/gameplay/brother/bot/ZLocalCoopBot.cpp` |
| `gun_bros_re/gameplay/BroAIDeathmatch.h` | `gun_bros_re/gameplay/brother/bot/ZLocalCoopBot.h` |
| `gun_bros_re/gameplay/CombatGeometry.h` | `gun_bros_re/gameplay/ZCombatGeometry.h` |
| `gun_bros_re/gameplay/CombatScene.cpp` | 已按职责并入 `CLevelRuntime.cpp`、`CLevelWorld.cpp`、`CLevelActors.cpp`、`CLevelProjectiles.cpp`、`CLevelCombat.cpp` |
| `gun_bros_re/gameplay/CombatScene.h` | 已并入 `gun_bros_re/gameplay/level/CLevel.h`，不保留兼容门面 |
| `gun_bros_re/gameplay/CombatTypes.h` | `gun_bros_re/gameplay/ZCombatTypes.h` |
| `gun_bros_re/gameplay/DeathmatchBot.cpp` | `gun_bros_re/gameplay/brother/bot/ZLocalPVPBot.cpp` |
| `gun_bros_re/gameplay/DeathmatchBot.h` | `gun_bros_re/gameplay/brother/bot/ZLocalPVPBot.h` |
| `gun_bros_re/gameplay/DeathmatchCombat.cpp` | `gun_bros_re/gameplay/ZDeathmatchCombat.cpp` |
| `gun_bros_re/gameplay/DeathmatchNavigation.cpp` | `gun_bros_re/gameplay/brother/bot/ZLocalPVPBotNavigation.cpp` |
| `gun_bros_re/gameplay/EnemyCombat.cpp` | `gun_bros_re/gameplay/CEnemyCombat.cpp` |
| `gun_bros_re/gameplay/EnemyCombat.h` | 已并入 `gun_bros_re/gameplay/CEnemy.h` |
| `gun_bros_re/gameplay/EnemyModel.cpp` | `gun_bros_re/gameplay/ZEnemyModel.cpp` |
| `gun_bros_re/gameplay/EnemyModel.h` | `gun_bros_re/gameplay/ZEnemyModel.h` |
| `gun_bros_re/gameplay/GameScriptObject.cpp` | `gun_bros_re/gameplay/ZGameScriptObject.cpp` |
| `gun_bros_re/gameplay/GameScriptObject.h` | `gun_bros_re/gameplay/ZGameScriptObject.h` |
| `gun_bros_re/gameplay/IPropWorld.h` | `gun_bros_re/gameplay/ZPropWorld.h` |
| `gun_bros_re/gameplay/LiveCombat.cpp` | `gun_bros_re/gameplay/ZLiveCombat.cpp` |
| `gun_bros_re/gameplay/LiveShopSession.h` | `gun_bros_re/gameplay/ZLiveShopSession.h` |
| `gun_bros_re/gameplay/MapLoading.cpp` | `gun_bros_re/gameplay/ZMapLoading.cpp` |
| `gun_bros_re/gameplay/MapParticles.cpp` | `gun_bros_re/gameplay/ZMapParticles.cpp` |
| `gun_bros_re/gameplay/MapPropWorld.cpp` | `gun_bros_re/gameplay/ZMapPropWorld.cpp` |
| `gun_bros_re/gameplay/MapRendering.cpp` | `gun_bros_re/gameplay/ZMapRendering.cpp` |
| `gun_bros_re/gameplay/MapScene.h` | `gun_bros_re/gameplay/ZMapScene.h` |
| `gun_bros_re/gameplay/MapWorld.cpp` | `gun_bros_re/gameplay/ZMapWorld.cpp` |
| `gun_bros_re/gameplay/MapWorldInternal.h` | `gun_bros_re/gameplay/ZMapWorldInternal.h` |
| `gun_bros_re/gameplay/MultiplayerStatistics.h` | `gun_bros_re/gameplay/ZMultiplayerStatistics.h` |
| `gun_bros_re/gameplay/PickupScene.cpp` | `gun_bros_re/gameplay/ZPickupScene.cpp` |
| `gun_bros_re/gameplay/PickupScene.h` | `gun_bros_re/gameplay/ZPickupScene.h` |
| `gun_bros_re/gameplay/PlayerModel.cpp` | `CBrotherBody.cpp`、`CBrotherEquipment.cpp`、`CBrotherResources.cpp`、`CBrotherDrawing.cpp`；枪械与盔甲加载归 `CGunDrawing.cpp`、`CArmorDrawing.cpp` |
| `gun_bros_re/gameplay/PlayerModel.h` | 游戏入口为 `gameplay/brother/CBrother.h`，绘制接口也在 `CBrother.h`，同目录 `CBrotherDrawing.h` 仅定义私有数据；不存在旧模型类型或转发头 |
| `gun_bros_re/gameplay/PowerupMoviePlayer.cpp` | `gun_bros_re/gameplay/powerup/CPowerupPresentation.cpp` |
| `gun_bros_re/gameplay/PowerupMoviePlayer.h` | 已并入 `gun_bros_re/gameplay/powerup/CPowerup.h` |
| `gun_bros_re/gameplay/PowerupScene.cpp` | 组合层已删除，分归 `CPowerUpSelectorInventory.cpp`、`CBrotherPowerups.cpp`、`level/CLevelPowerups.cpp` 与现有 Bot 策略 |
| `gun_bros_re/gameplay/PowerupScene.h` | 组合层已删除，接口归对应原对象，无别名或兼容头 |
| `gun_bros_re/gameplay/SurvivalGameContext.h` | `gun_bros_re/gameplay/ZSurvivalGameContext.h` |
| `gun_bros_re/gameplay/SurvivalInputDriver.cpp` | `gun_bros_re/gameplay/ZSurvivalInputDriver.cpp` |
| `gun_bros_re/gameplay/SurvivalInputDriver.h` | `gun_bros_re/gameplay/ZSurvivalInputDriver.h` |
| `gun_bros_re/gameplay/SurvivalLoop.cpp` | `gun_bros_re/gameplay/ZSurvivalLoop.cpp` |
| `gun_bros_re/gameplay/SurvivalProgress.cpp` | `gun_bros_re/gameplay/ZSurvivalProgress.cpp` |
| `gun_bros_re/gameplay/SurvivalRuntime.h` | `gun_bros_re/gameplay/ZSurvivalRuntime.h` |
| `gun_bros_re/gameplay/SurvivalScenario.h` | `gun_bros_re/gameplay/ZSurvivalScenario.h` |
| `gun_bros_re/gameplay/SurvivalSession.cpp` | `gun_bros_re/gameplay/CGame.cpp`、`CLevel.cpp` |
| `gun_bros_re/gameplay/SurvivalSession.h` | `gun_bros_re/gameplay/CGame.h`、`CLevel.h` |
| `gun_bros_re/gameplay/WeaponEffects.cpp` | `gun_bros_re/gameplay/ZWeaponEffects.cpp` |
| `gun_bros_re/gameplay/WeaponEffects.h` | `gun_bros_re/gameplay/ZWeaponEffects.h` |
| `gun_bros_re/ui/GameFrontEnd.cpp` | `gun_bros_re/ui/ZGameFrontEnd.cpp` |
| `gun_bros_re/ui/GameFrontEnd.h` | `gun_bros_re/ui/ZGameFrontEnd.h` |
| `gun_bros_re/ui/GameFrontEndInternal.h` | `gun_bros_re/ui/ZGameFrontEndInternal.h` |
| `gun_bros_re/ui/GreetingMenu.cpp` | `gun_bros_re/ui/ZGreetingMenu.cpp` |
| `gun_bros_re/ui/LivePostGame.cpp` | `gun_bros_re/ui/ZLivePostGame.cpp` |
| `gun_bros_re/ui/LiveWaveOverlay.cpp` | `gun_bros_re/ui/CInputPadLiveWave.cpp` |
| `gun_bros_re/ui/LoadingScreen.h` | `gun_bros_re/ui/ZLoadingScreen.h` |
| `gun_bros_re/ui/LocalOnlineMenus.cpp` | `gun_bros_re/ui/ZLocalOnlineMenus.cpp` |
| `gun_bros_re/ui/MenuFlow.cpp` | `gun_bros_re/ui/ZMenuFlow.cpp` |
| `gun_bros_re/ui/MenuInternal.h` | `gun_bros_re/ui/ZMenuInternal.h` |
| `gun_bros_re/ui/MenuNavigation.cpp` | `gun_bros_re/ui/ZMenuNavigation.cpp` |
| `gun_bros_re/ui/MenuPreview.cpp` | `gun_bros_re/ui/ZMenuPreview.cpp` |
| `gun_bros_re/ui/MenuSurface.cpp` | `gun_bros_re/ui/ZMenuSurface.cpp` |
| `gun_bros_re/ui/MenuWipe.h` | `gun_bros_re/ui/ZMenuWipe.h` |
| `gun_bros_re/ui/MissionMenu.cpp` | `gun_bros_re/ui/ZMissionMenu.cpp` |
| `gun_bros_re/ui/ModeOverlayCallbacks.h` | `gun_bros_re/ui/ZModeOverlayCallbacks.h` |
| `gun_bros_re/ui/OptionsMenu.cpp` | `gun_bros_re/ui/ZOptionsMenu.cpp` |
| `gun_bros_re/ui/OriginalDeathmatchSelector.cpp` | `gun_bros_re/ui/CPowerUpSelectorDeathmatch.cpp` |
| `gun_bros_re/ui/OriginalDialogPopup.h` | `gun_bros_re/ui/CDialogPopup.h` |
| `gun_bros_re/ui/OriginalLivePostGameParticleData.inc` | `gun_bros_re/ui/CMenuLivePostGameParticleData.inc` |
| `gun_bros_re/ui/OriginalLoadingSplash.h` | `gun_bros_re/ui/CMenuSplash.h` |
| `gun_bros_re/ui/OriginalMenuData.h` | `gun_bros_re/ui/ZMenuData.h` |
| `gun_bros_re/ui/OriginalMenuData.inc` | `gun_bros_re/ui/ZMenuData.inc` |
| `gun_bros_re/ui/OriginalModeParticleData.inc` | `gun_bros_re/ui/CMenuModeParticleData.inc` |
| `gun_bros_re/ui/OriginalNavigationData.inc` | `gun_bros_re/ui/CMenuSystemNavigationData.inc` |
| `gun_bros_re/ui/OriginalPostGameParticleData.inc` | `gun_bros_re/ui/CMenuPostGameParticleData.inc` |
| `gun_bros_re/ui/OriginalPowerupSelector.cpp` | `gun_bros_re/ui/CPowerUpSelector.cpp` |
| `gun_bros_re/ui/OriginalPromotionPopup.h` | `gun_bros_re/ui/ZPromotionPopup.h` |
| `gun_bros_re/ui/OriginalRefineryParticleData.inc` | `gun_bros_re/ui/CMenuRefineryParticleData.inc` |
| `gun_bros_re/ui/OriginalSocialContent.cpp` | `gun_bros_re/ui/ZSocialContent.cpp` |
| `gun_bros_re/ui/OriginalSplashData.inc` | `gun_bros_re/ui/CMenuSplashData.inc` |
| `gun_bros_re/ui/OriginalTextLayout.h` | `gun_bros_re/ui/ZTextLayout.h` |
| `gun_bros_re/ui/PlanetMenu.cpp` | `gun_bros_re/ui/ZPlanetMenu.cpp` |
| `gun_bros_re/ui/PostGameCardCallbacks.h` | `gun_bros_re/ui/ZPostGameCardCallbacks.h` |
| `gun_bros_re/ui/PostGameMenu.cpp` | `gun_bros_re/ui/ZPostGameMenu.cpp` |
| `gun_bros_re/ui/RefineryMenu.cpp` | `gun_bros_re/ui/ZRefineryMenu.cpp` |
| `gun_bros_re/ui/StoreMenu.cpp` | `gun_bros_re/ui/ZStoreMenu.cpp` |
| `gun_bros_re/ui/StoreRegionClip.h` | `gun_bros_re/ui/ZStoreRegionClip.h` |
| `gun_bros_re/ui/SurvivalChallenges.cpp` | `gun_bros_re/ui/CInputPadChallenges.cpp` |
| `gun_bros_re/ui/SurvivalControls.cpp` | `gun_bros_re/ui/CInputPadControls.cpp` |
| `gun_bros_re/ui/SurvivalHud.cpp` | `gun_bros_re/ui/CInputPad.cpp` |
| `gun_bros_re/ui/SurvivalHud.h` | `gun_bros_re/ui/CInputPad.h` |
| `gun_bros_re/ui/SurvivalNotices.cpp` | `gun_bros_re/ui/CInputPadNotices.cpp` |
| `gun_bros_re/ui/SurvivalPause.cpp` | `gun_bros_re/ui/CInputPadPause.cpp` |
