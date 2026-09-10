# 原工程游戏相关文件与类名

本清单重新核对 [iOS 主程序](../gunbros) 与 [反编译源码](gunbros_3.6.0_IOS.c)，替换旧版文件名推导、重写优先级及完成度记录。

## 范围与读法

- 保留游戏逻辑、菜单、资源、脚本、存档、成长、商店、好友与联机，以及支撑游戏运行的 Glu 自研图形、音频、数学、文件和平台适配代码。原版存在的任务/战役类也保留；出现于本表不表示正式流程会执行。
- 排除广告、推广、遥测、第三方库及通用服务 SDK。游戏侧的购买、好友、奖励接入仍保留；`tools/` 中仅列经核对直接承担商品在线覆盖的实现。
- 文件路径来自 ARMv7 的 `LC_SYMTAB → N_SO`，并用 `N_FUN` 确定函数归属；目录与文件记录合并后规范化 `../`。原工程根为 `/Users/noah.ruffell/Documents/Projects/GunBros1/`，下表目录均相对此根。
- **类名列表示该编译单元实际发出的成员函数归属**，包括结构体、嵌套类型及附带的非模板内联/析构函数；不等于这些类都在对应 `.cpp` 中声明。类名链接指向相同 ARMv7 地址的反编译函数；`仅符号` 表示源码没有该地址的独立函数体。
- 符号不能恢复全部头文件、纯数据结构或已优化掉的类；不由参数类型或文件名补造类。通用容器模板实例不展开。仅有静态函数的部分作用域无法据符号区分 class/namespace，单独注明。
- 为控制表宽，`platform::` 统一缩写 `com::glu::platform::`，其余大小写、拼写及嵌套层级保持符号原名。

共 **342 个文件**：游戏 `src/` 219 个，自研运行基础层 `platform/shared/` 122 个，商品覆盖 `tools/` 1 个。

## `src/`

| 文件名 | 类名／类型作用域 |
|---|---|
| `CFontMgr.cpp` | [CFontMgr](gunbros_3.6.0_IOS.c#L56656) |
| `CGameApp.cpp` | [CGameApp](gunbros_3.6.0_IOS.c#L52346)；[CNetMessageEnvelope](gunbros_3.6.0_IOS.c#L55562)；[CNetMessageQueue](gunbros_3.6.0_IOS.c#L55973)；[CSimpleStream](gunbros_3.6.0_IOS.c#L55787)；[platform::components::CAppProperties](gunbros_3.6.0_IOS.c#L55762)；[platform::components::CHash](gunbros_3.6.0_IOS.c#L54864)；[platform::components::CTypedVariableTable](gunbros_3.6.0_IOS.c#L55641)；[platform::components::ICLicenseMgr](gunbros_3.6.0_IOS.c#L55371)；[platform::components::ICMediaPlayer](gunbros_3.6.0_IOS.c#L55404)；[platform::components::ICMoviePlayer](gunbros_3.6.0_IOS.c#L55437)；[platform::framework::CApp](gunbros_3.6.0_IOS.c#L54846)；[platform::framework::CAppExecutor](gunbros_3.6.0_IOS.c#L53415)；[platform::framework::CAppFactory](gunbros_3.6.0_IOS.c#L54834)；[platform::graphics::CRenderSurfaceBuffer](gunbros_3.6.0_IOS.c#L55622)；[platform::graphics::ICShaderProgram::ParameterTable](gunbros_3.6.0_IOS.c#L55726)；[platform::systems::CEventPool](gunbros_3.6.0_IOS.c#L56316)；[platform::systems::CMessage](gunbros_3.6.0_IOS.c#L56250)；[platform::systems::CMessagePool](gunbros_3.6.0_IOS.c#L56147)；[platform::systems::CRegistry](gunbros_3.6.0_IOS.c#L54907)；[platform::systems::CRegistry::Accelerator](gunbros_3.6.0_IOS.c#L54881)；[platform::systems::CRegistryAccelerateHandleQuery](gunbros_3.6.0_IOS.c#L55157)；[platform::systems::CRegistryElement](gunbros_3.6.0_IOS.c#L55140)；[platform::systems::CResourceManager_v1::CConsecutiveResourceIdItr](gunbros_3.6.0_IOS.c#L54920) |
| `CGameProfiler.cpp` | 未确认类名；仅 N_SO 文件记录；未找到具名 N_FUN，类名未知。 |
| `COptionsMgr.cpp` | [COptionsMgr](gunbros_3.6.0_IOS.c#L51313)；[platform::components::CStrChar](gunbros_3.6.0_IOS.c#L51496)；[platform::components::Color_Palette](gunbros_3.6.0_IOS.c#L51544)；[platform::components::ICFileMgr](gunbros_3.6.0_IOS.c#L51513)；[platform::core::CClass](gunbros_3.6.0_IOS.c#L51481)；[platform::systems::CEventListener](gunbros_3.6.0_IOS.c#L51563) |
| `CResBank.cpp` | [platform::components::CKeysetResource](gunbros_3.6.0_IOS.c#L51252)；[platform::components::CSingleton](gunbros_3.6.0_IOS.c#L51288)；[platform::components::CStrWChar](gunbros_3.6.0_IOS.c#L51271)；仅观察到所列基础类析构符号；不能据文件名补写 CResBank。 |
| `CSaveGameMgr.cpp` | [CSaveGameMgr](gunbros_3.6.0_IOS.c#L56428) |
| `CUtility.cpp` | [CAppVersion](gunbros_3.6.0_IOS.c#L51612)；[CUtility](gunbros_3.6.0_IOS.c#L51594)；[platform::components::CAppProperties](gunbros_3.6.0_IOS.c#L52225)；[platform::core::ICStdUtil](gunbros_3.6.0_IOS.c#L52306) |
| `drawSurface.cpp` | [platform::graphics::CBlitUtil](gunbros_3.6.0_IOS.c#L111899)；主实现为自由函数 drawSurface；CBlitUtil 为附带析构符号。 |

## `src/NGClient/`

| 文件名 | 类名／类型作用域 |
|---|---|
| `CFunctor.cpp` | [FriendDataFunctor](gunbros_3.6.0_IOS.c#L195399)；[ProfileManagerFunctor](gunbros_3.6.0_IOS.c#L195370) |
| `CGunBrosFactory.cpp` | [CGunBrosFactory](gunbros_3.6.0_IOS.c#L204664) |

## `src/cocoa/`

| 文件名 | 类名／类型作用域 |
|---|---|
| `AppDelegate_mm.mm` | [AppDelegate](gunbros_3.6.0_IOS.c#L111958) |
| `AppViewController.mm` | [AppViewController](gunbros_3.6.0_IOS.c#L222150) |
| `AppView_mm.mm` | [AppView](gunbros_3.6.0_IOS.c#L113604) |
| `AppleInterface.mm` | [AppleInterface](gunbros_3.6.0_IOS.c#L113342)；[Hardware](gunbros_3.6.0_IOS.c#L113366) |

## `src/gluMovie/`

| 文件名 | 类名／类型作用域 |
|---|---|
| `embededMovie.cpp` | [CEmbededMovie](gunbros_3.6.0_IOS.c#L108010)；[CMovieObject](gunbros_3.6.0_IOS.c#L108220) |
| `movie.cpp` | [CMovie](gunbros_3.6.0_IOS.c#L108257) |
| `movieChapter.cpp` | [CMovieChapter](gunbros_3.6.0_IOS.c#L109532)；`CMovieObject`（仅符号 `0x00083E3C`） |
| `movieEmptyRegion.cpp` | [CMovieEmptyRegion](gunbros_3.6.0_IOS.c#L182312) |
| `movieFill.cpp` | [CMovieFill](gunbros_3.6.0_IOS.c#L139821) |
| `movieObject.cpp` | [CMovieObject](gunbros_3.6.0_IOS.c#L109633) |
| `movieRegion.cpp` | [CMovieRegion](gunbros_3.6.0_IOS.c#L109751) |
| `movieSoundSet.cpp` | [CMovieSoundSet](gunbros_3.6.0_IOS.c#L110138) |
| `movieSprite.cpp` | [CMovieSprite](gunbros_3.6.0_IOS.c#L110238) |
| `movieText.cpp` | [CMovieText](gunbros_3.6.0_IOS.c#L111015) |
| `movieTiledSprite.cpp` | [CMovieTiledSprite](gunbros_3.6.0_IOS.c#L111098) |

## `src/gluScript/`

| 文件名 | 类名／类型作用域 |
|---|---|
| `script.cpp` | [CScript](gunbros_3.6.0_IOS.c#L105985)；[CScriptState](gunbros_3.6.0_IOS.c#L106704) |
| `scriptCode.cpp` | [CScriptCode](gunbros_3.6.0_IOS.c#L106762) |
| `scriptCondition.cpp` | [CScriptCondition](gunbros_3.6.0_IOS.c#L106933) |
| `scriptEvent.cpp` | [CScriptEvent](gunbros_3.6.0_IOS.c#L107073) |
| `scriptFunction.cpp` | [CScriptFunction](gunbros_3.6.0_IOS.c#L107110) |
| `scriptInterpreter.cpp` | [CScriptInterpreter](gunbros_3.6.0_IOS.c#L107176) |
| `scriptResolver.cpp` | [ScriptResolver](gunbros_3.6.0_IOS.c#L107580)（静态函数作用域，class/namespace 未确认） |
| `scriptResult.cpp` | [CScriptResult](gunbros_3.6.0_IOS.c#L107686) |
| `scriptReturn.cpp` | [CScriptReturn](gunbros_3.6.0_IOS.c#L107706) |
| `scriptState.cpp` | [CScriptState](gunbros_3.6.0_IOS.c#L107728) |
| `scriptVariable.cpp` | [CScriptVariable](gunbros_3.6.0_IOS.c#L107914) |

## `src/gunbros/`

| 文件名 | 类名／类型作用域 |
|---|---|
| `CAchievementsMgr.cpp` | [CAchievementsMgr](gunbros_3.6.0_IOS.c#L223702) |
| `CGKFriendRequestComposeViewController.cpp` | 未确认类名；函数入口 [CGKFriendRequestComposeViewController.cpp（静态初始化）](gunbros_3.6.0_IOS.c#L244445)；仅静态初始化函数。 |
| `CGKFriendRequestComposeViewController.mm` | [CGKFriendRequestComposeViewController](gunbros_3.6.0_IOS.c#L244541)；[FriendRequestComposeViewController](gunbros_3.6.0_IOS.c#L244457) |
| `CMultiplayerMgr.cpp` | [CMultiplayerMgr](gunbros_3.6.0_IOS.c#L228322) |
| `CPackageOfferMgr.cpp` | [CPackageOfferMgr](gunbros_3.6.0_IOS.c#L396292) |
| `KillTracker.cpp` | [CKillTracker](gunbros_3.6.0_IOS.c#L224791) |
| `Label.cpp` | [CLabel](gunbros_3.6.0_IOS.c#L414010) |
| `LocalNotificationMgr.cpp` | [CLocalNotificationMgr](gunbros_3.6.0_IOS.c#L232571)；[InactivityInfo](gunbros_3.6.0_IOS.c#L232559)；含回归奖励发放逻辑，因此保留。 |
| `LocalNotificationMgr.mm` | [CLocalNotificationMgr](gunbros_3.6.0_IOS.c#L231745)；与上一文件共同实现 CLocalNotificationMgr。 |
| `MessageComposerViewController.cpp` | 未确认类名；函数入口 [MessageComposerViewController.cpp（静态初始化）](gunbros_3.6.0_IOS.c#L244436)；仅静态初始化函数。 |
| `MessageComposerViewController.mm` | [CMessageComposerViewController](gunbros_3.6.0_IOS.c#L244307)；[MessageComposerViewController](gunbros_3.6.0_IOS.c#L244020) |
| `NetworkParams.cpp` | 未确认类名；函数入口 [NETPARAMS](gunbros_3.6.0_IOS.c#L249362)；全局 NETPARAMS() 返回 gParams；不能据文件名补写类。 |
| `PurchaseManager.mm` | [CInAppPurchasableProduct](gunbros_3.6.0_IOS.c#L211825)；[SPurchaseManager](gunbros_3.6.0_IOS.c#L211772) |
| `StoreAutoPreview.cpp` | [CStoreAutoPreview](gunbros_3.6.0_IOS.c#L297561) |
| `StoreItemOverride.cpp` | [CStoreItemOverride](gunbros_3.6.0_IOS.c#L232736) |
| `armor.cpp` | [CArmor](gunbros_3.6.0_IOS.c#L176460)；[CArmor::Template](gunbros_3.6.0_IOS.c#L176438) |
| `bgm.cpp` | [CBGM](gunbros_3.6.0_IOS.c#L59493) |
| `brother.cpp` | [CBrother](gunbros_3.6.0_IOS.c#L134119)；[CBrother::CGrenadeHolder](gunbros_3.6.0_IOS.c#L139120)；[CBrother::Template](gunbros_3.6.0_IOS.c#L134571)；[CGun](gunbros_3.6.0_IOS.c#L139377) |
| `brotherAI.cpp` | [CBrotherAI](gunbros_3.6.0_IOS.c#L139396) |
| `bullet.cpp` | [AnchorBulletHit](gunbros_3.6.0_IOS.c#L60242)；[CBrother](gunbros_3.6.0_IOS.c#L64190)；[CBrother::PacketSeekingWeapon](gunbros_3.6.0_IOS.c#L64069)；[CBullet](gunbros_3.6.0_IOS.c#L60254)；[CBullet::Template](gunbros_3.6.0_IOS.c#L60448)；[CLightningArc](gunbros_3.6.0_IOS.c#L64242)；[EffectContainer](gunbros_3.6.0_IOS.c#L64420)；`IDrawable`（仅符号 `0x00035364`）；[ILevelObject](gunbros_3.6.0_IOS.c#L63843)；[ScriptStorage](gunbros_3.6.0_IOS.c#L64146)；[platform::graphics::CGraphics](gunbros_3.6.0_IOS.c#L64117) |
| `camera.cpp` | [CCamera](gunbros_3.6.0_IOS.c#L64447) |
| `challengeInfoOverlay.cpp` | [CChallengeInfoOverlay](gunbros_3.6.0_IOS.c#L297137) |
| `challengeManager.cpp` | [CChallengeManager](gunbros_3.6.0_IOS.c#L238134)；[CChallengeManager::Template](gunbros_3.6.0_IOS.c#L239005) |
| `challengeProgressData.cpp` | [CChallengeProgressData](gunbros_3.6.0_IOS.c#L297764) |
| `collision.cpp` | [Collision](gunbros_3.6.0_IOS.c#L65444)（静态函数作用域，class/namespace 未确认） |
| `collisionData.cpp` | [CCollisionData](gunbros_3.6.0_IOS.c#L142446) |
| `contentTracker.cpp` | [CContentTracker](gunbros_3.6.0_IOS.c#L224886)；[CContentTracker::UserData](gunbros_3.6.0_IOS.c#L224990)；[CContentTracker::UserData::PerPackData](gunbros_3.6.0_IOS.c#L225374)；[CContentTracker::UserData::PerPackData::PerObjectTypeData](gunbros_3.6.0_IOS.c#L225025) |
| `controlStick.cpp` | [ControlStick](gunbros_3.6.0_IOS.c#L248851) |
| `dailyBonusTracking.cpp` | [CDailyBonusTracking](gunbros_3.6.0_IOS.c#L208796)；[CDailyBonusTracking::Template](gunbros_3.6.0_IOS.c#L209142) |
| `debug.cpp` | 未确认类名；仅 N_SO 文件记录；未找到具名 N_FUN，类名未知。 |
| `dialogPopup.cpp` | [CDialogPopup](gunbros_3.6.0_IOS.c#L183433) |
| `effectLayer.cpp` | [CEffectLayer](gunbros_3.6.0_IOS.c#L66452)；[CEffectLayer::ParticleEffect](gunbros_3.6.0_IOS.c#L67116)；[CEffectLayer::TextEffect](gunbros_3.6.0_IOS.c#L67080) |
| `enemy.cpp` | [CActor](gunbros_3.6.0_IOS.c#L73602)；[CCollisionData](gunbros_3.6.0_IOS.c#L73774)；[CEnemy](gunbros_3.6.0_IOS.c#L67206)；[CEnemy::CollisionInformation](gunbros_3.6.0_IOS.c#L73614)；[CEnemy::Template](gunbros_3.6.0_IOS.c#L67174)；[CMoveSetMesh](gunbros_3.6.0_IOS.c#L73908)；[TargetNode](gunbros_3.6.0_IOS.c#L73703)；[vec2](gunbros_3.6.0_IOS.c#L73989) |
| `enemySpawner.cpp` | [CEnemySpawner](gunbros_3.6.0_IOS.c#L146035)；[CNetworkEnemySpawner](gunbros_3.6.0_IOS.c#L147304)；[COffscreenSpawnLocationFilter](gunbros_3.6.0_IOS.c#L147156)；[IEnemySpawner](gunbros_3.6.0_IOS.c#L146632)；[IEnemySpawnerScriptInterface](gunbros_3.6.0_IOS.c#L146152)；[SpawnPacket](gunbros_3.6.0_IOS.c#L147125) |
| `engine.cpp` | [Engine](gunbros_3.6.0_IOS.c#L133971)（静态函数作用域，class/namespace 未确认） |
| `flock.cpp` | [CFlock](gunbros_3.6.0_IOS.c#L170242) |
| `friendData.cpp` | [CFriendData](gunbros_3.6.0_IOS.c#L204978) |
| `friendPowerManager.cpp` | [CFriendPowerManager](gunbros_3.6.0_IOS.c#L234020) |
| `friendsManager.cpp` | [CAutoBroNotifyFunctor](gunbros_3.6.0_IOS.c#L201010)；[CFriendDataManager](gunbros_3.6.0_IOS.c#L198759)；[CFriendsManagerNotifyFunctor](gunbros_3.6.0_IOS.c#L200968) |
| `game.cpp` | [CArmor](gunbros_3.6.0_IOS.c#L76895)；[CBrother](gunbros_3.6.0_IOS.c#L77099)；[CGame](gunbros_3.6.0_IOS.c#L74326)；[CGun](gunbros_3.6.0_IOS.c#L76907)；[CInputPad](gunbros_3.6.0_IOS.c#L77173)；[CInputPad::PeripheralHUD](gunbros_3.6.0_IOS.c#L76998)；[CParticlePool](gunbros_3.6.0_IOS.c#L77050)；[INetworkObject](gunbros_3.6.0_IOS.c#L76920)；[ITransition](gunbros_3.6.0_IOS.c#L76901)；[platform::core::ICStdUtil](gunbros_3.6.0_IOS.c#L77042) |
| `gameAssetRef.cpp` | [CGameAssetRef](gunbros_3.6.0_IOS.c#L191877)；[CGameSpriteGluRef](gunbros_3.6.0_IOS.c#L191859) |
| `gameFlow.cpp` | [CGameFlow](gunbros_3.6.0_IOS.c#L77286) |
| `gameObject.cpp` | [IGameObject::GameObjectRef](gunbros_3.6.0_IOS.c#L191893)；[IGameObject::GameObjectTypeRef](gunbros_3.6.0_IOS.c#L191914)；观察到 IGameObject 的嵌套引用类型。 |
| `gameObjectPack.cpp` | [CArmor::Template](gunbros_3.6.0_IOS.c#L130503)；[CBullet::Template](gunbros_3.6.0_IOS.c#L130542)；[CGameObjectPack](gunbros_3.6.0_IOS.c#L129033)；[CGun::Template](gunbros_3.6.0_IOS.c#L130667)；[CPlatform::Template](gunbros_3.6.0_IOS.c#L130145)；[CProp::Template](gunbros_3.6.0_IOS.c#L130689)；[SoundEffect](gunbros_3.6.0_IOS.c#L130176)；[TileSet](gunbros_3.6.0_IOS.c#L130243)；[Tutorial](gunbros_3.6.0_IOS.c#L130212) |
| `glTools.cpp` | [CLightningArc](gunbros_3.6.0_IOS.c#L242730)；[CMeshLine](gunbros_3.6.0_IOS.c#L242742)；[CMeshLine::CVertexBuffer](gunbros_3.6.0_IOS.c#L242714)；[CRibbonTrailEffect](gunbros_3.6.0_IOS.c#L242724) |
| `gluMovie.cpp` | [CMenu](gunbros_3.6.0_IOS.c#L77489)；实际符号归属 CMenu；不能补写 CGluMovie。 |
| `gun.cpp` | [CDummyTarget](gunbros_3.6.0_IOS.c#L128956)；[CGun](gunbros_3.6.0_IOS.c#L127880)；[CGun::BulletSource](gunbros_3.6.0_IOS.c#L128950)；[CGun::Template](gunbros_3.6.0_IOS.c#L127712) |
| `gunbros.cpp` | [CGameFlow](gunbros_3.6.0_IOS.c#L85398)；[CGunBros](gunbros_3.6.0_IOS.c#L77538)；[CPlayerProgress](gunbros_3.6.0_IOS.c#L84833)；[CSaveRestoreInterface](gunbros_3.6.0_IOS.c#L80913) |
| `imagePool.cpp` | [CImagePool](gunbros_3.6.0_IOS.c#L85420) |
| `input.cpp` | [CInput](gunbros_3.6.0_IOS.c#L85779) |
| `inputPad.cpp` | [CInputPad](gunbros_3.6.0_IOS.c#L86290)；[CInputPad::Base](gunbros_3.6.0_IOS.c#L87488)；[CInputPad::ChallengeInfoOverlay](gunbros_3.6.0_IOS.c#L89408)；[CInputPad::IComponent](gunbros_3.6.0_IOS.c#L91400)；[CInputPad::PeripheralHUD](gunbros_3.6.0_IOS.c#L86297)；[CInputPad::PowerUpSelector](gunbros_3.6.0_IOS.c#L89357) |
| `inputPadMeter.cpp` | [CInputPadMeter](gunbros_3.6.0_IOS.c#L130721) |
| `interpolator.cpp` | [CInterpolator](gunbros_3.6.0_IOS.c#L91559) |
| `layerCamera.cpp` | [CLayerCamera](gunbros_3.6.0_IOS.c#L127663) |
| `layerCollision.cpp` | [CLayerCollision](gunbros_3.6.0_IOS.c#L125241) |
| `layerMovie.cpp` | [CLayerMovie](gunbros_3.6.0_IOS.c#L126049) |
| `layerObject.cpp` | [CLayerObject](gunbros_3.6.0_IOS.c#L126133) |
| `layerPathLink.cpp` | [CLayerPathLink](gunbros_3.6.0_IOS.c#L166451)；[DistanceList](gunbros_3.6.0_IOS.c#L167261) |
| `layerPathMesh.cpp` | [CLayerPathMesh](gunbros_3.6.0_IOS.c#L167381) |
| `layerTile.cpp` | [CLayerTile](gunbros_3.6.0_IOS.c#L126759) |
| `level.cpp` | [CBrother](gunbros_3.6.0_IOS.c#L122775)；[CLevel](gunbros_3.6.0_IOS.c#L114212)；[CLevel::Template](gunbros_3.6.0_IOS.c#L114770)；[CLevelObjectPool](gunbros_3.6.0_IOS.c#L122616)；[CProp](gunbros_3.6.0_IOS.c#L122601)；[CStatisticEnemy](gunbros_3.6.0_IOS.c#L122531)；[StatisticPacket](gunbros_3.6.0_IOS.c#L122468) |
| `levelIndicator.cpp` | [CLevelIndicator](gunbros_3.6.0_IOS.c#L191302) |
| `levelObject.cpp` | [AnchorPosition](gunbros_3.6.0_IOS.c#L294504)；[AnchorTransform](gunbros_3.6.0_IOS.c#L294520)；[CRibbonTrailEffect](gunbros_3.6.0_IOS.c#L295349)；[EffectContainer](gunbros_3.6.0_IOS.c#L294598)；[EffectContainerPair](gunbros_3.6.0_IOS.c#L294579)；[EffectHolder](gunbros_3.6.0_IOS.c#L294533)；[ParticleEffectHolder](gunbros_3.6.0_IOS.c#L294557)；[TrailEffectHolder](gunbros_3.6.0_IOS.c#L294563)；含锚点、效果持有者等多个类型；不能补写 CLevelObject。 |
| `levelObjectPool.cpp` | [CLevelObjectPool](gunbros_3.6.0_IOS.c#L145287)；[CParticleEffectProp](gunbros_3.6.0_IOS.c#L145792)；[CPickup](gunbros_3.6.0_IOS.c#L145875) |
| `levelTag.cpp` | [CLevelTag](gunbros_3.6.0_IOS.c#L183175) |
| `linkPathFinder.cpp` | [CLinkPathFinder](gunbros_3.6.0_IOS.c#L168567) |
| `mainScreen.cpp` | [MainScreen](gunbros_3.6.0_IOS.c#L91786)（静态函数作用域，class/namespace 未确认） |
| `map.cpp` | [CMap](gunbros_3.6.0_IOS.c#L91816)；[CParticlePool](gunbros_3.6.0_IOS.c#L92882)；[CParticleSystem](gunbros_3.6.0_IOS.c#L92860)；[IGameObject](gunbros_3.6.0_IOS.c#L92759)；[ILayerPath](gunbros_3.6.0_IOS.c#L92845)；[IMapLayer](gunbros_3.6.0_IOS.c#L92753) |
| `menu.cpp` | [CMenu](gunbros_3.6.0_IOS.c#L92948) |
| `menuAction.cpp` | [CMenuAction](gunbros_3.6.0_IOS.c#L92977) |
| `menuChallengeOption.cpp` | [CMenuChallengeOption](gunbros_3.6.0_IOS.c#L237340) |
| `menuChallenges.cpp` | [CMenuChallenges](gunbros_3.6.0_IOS.c#L235043) |
| `menuDataProvider.cpp` | [CMenuDataProvider](gunbros_3.6.0_IOS.c#L148357) |
| `menuFriendOption.cpp` | [CMenuFriendOption](gunbros_3.6.0_IOS.c#L197766) |
| `menuFriendOptionGroup.cpp` | [CMenuFriendOptionGroup](gunbros_3.6.0_IOS.c#L233415) |
| `menuFriendPowerOption.cpp` | [CMenuFriendPowerOption](gunbros_3.6.0_IOS.c#L234640) |
| `menuFriends.cpp` | [CMenuFriends](gunbros_3.6.0_IOS.c#L195456) |
| `menuGameResources.cpp` | [CMenuGameResources](gunbros_3.6.0_IOS.c#L171917)；[CMenuGameResources::CResourceMeter](gunbros_3.6.0_IOS.c#L172390)；[CMenuGameResources::CTransferEffect](gunbros_3.6.0_IOS.c#L173545) |
| `menuGreeting.cpp` | [CMenuGreeting](gunbros_3.6.0_IOS.c#L207809) |
| `menuIconOption.cpp` | [CMenuIconOption](gunbros_3.6.0_IOS.c#L175548) |
| `menuInviteFriends.cpp` | [CMenuInviteFriends](gunbros_3.6.0_IOS.c#L247680) |
| `menuList.cpp` | [CMenuList](gunbros_3.6.0_IOS.c#L140118) |
| `menuListOption.cpp` | [CMenuListOption](gunbros_3.6.0_IOS.c#L143983)；[CMenuOption](gunbros_3.6.0_IOS.c#L144348) |
| `menuLotteryPopup.cpp` | [CMenuLotteryPopup](gunbros_3.6.0_IOS.c#L396390) |
| `menuLotterySelection.cpp` | [CMenuLotterySelection](gunbros_3.6.0_IOS.c#L414118) |
| `menuMesh.cpp` | [CMenuMesh](gunbros_3.6.0_IOS.c#L168773) |
| `menuMeshEnemy.cpp` | [CMenuMeshEnemy](gunbros_3.6.0_IOS.c#L169021) |
| `menuMeshOption.cpp` | [CMenuMeshOption](gunbros_3.6.0_IOS.c#L176083) |
| `menuMeshPlayer.cpp` | [CMenuMeshPlayer](gunbros_3.6.0_IOS.c#L169198) |
| `menuMidPopup.cpp` | [CMenuMidPopup](gunbros_3.6.0_IOS.c#L392429) |
| `menuMissionInfo.cpp` | [CMenuMissionInfo](gunbros_3.6.0_IOS.c#L188880) |
| `menuMissionOption.cpp` | [CMenuMissionOption](gunbros_3.6.0_IOS.c#L189803) |
| `menuMissions.cpp` | [CMenuMission](gunbros_3.6.0_IOS.c#L161015) |
| `menuMovieButton.cpp` | [CMenuMovieButton](gunbros_3.6.0_IOS.c#L144360) |
| `menuMovieControl.cpp` | [CMenuMovieControl](gunbros_3.6.0_IOS.c#L140724) |
| `menuMovieMultiplayerOverlay.cpp` | [CMenuMovieMultiplayerOverlay](gunbros_3.6.0_IOS.c#L250020) |
| `menuMovieQueuedOverlay.cpp` | [CMenuMovieQueuedOverlay](gunbros_3.6.0_IOS.c#L242299) |
| `menuMovieScrollBar.cpp` | [CMenuMovieScrollBar](gunbros_3.6.0_IOS.c#L221427) |
| `menuNavigationBar.cpp` | [CMenuNavigationBar](gunbros_3.6.0_IOS.c#L143030) |
| `menuOption.cpp` | [CMenuOption](gunbros_3.6.0_IOS.c#L169732) |
| `menuOptionGroup.cpp` | [CMenuOptionGroup](gunbros_3.6.0_IOS.c#L174820) |
| `menuPlayerSelect.cpp` | [CMenuPlayerSelect](gunbros_3.6.0_IOS.c#L194860) |
| `menuPopupPrompt.cpp` | [CMenuPopupPrompt](gunbros_3.6.0_IOS.c#L206290) |
| `menuPostGame.cpp` | [CMenuPostGame](gunbros_3.6.0_IOS.c#L164541) |
| `menuPostGameOption.cpp` | [CMenuPostGameOption](gunbros_3.6.0_IOS.c#L249708) |
| `menuSplash.cpp` | [CMenuSplash](gunbros_3.6.0_IOS.c#L160301) |
| `menuStack.cpp` | [CMenuStack](gunbros_3.6.0_IOS.c#L147562) |
| `menuStore.cpp` | [CMenuStore](gunbros_3.6.0_IOS.c#L178697) |
| `menuStoreOption.cpp` | [CMenuStoreOption](gunbros_3.6.0_IOS.c#L180478) |
| `menuStoreOptionGroup.cpp` | [CMenuStoreOptionGroup](gunbros_3.6.0_IOS.c#L233898) |
| `menuSystem.cpp` | [CMenuSystem](gunbros_3.6.0_IOS.c#L96038) |
| `menuUpgradePopup.cpp` | [CMenuUpgradePopup](gunbros_3.6.0_IOS.c#L392452)；[ItemUpgradeInfo](gunbros_3.6.0_IOS.c#L394280) |
| `mesh.cpp` | [CMesh](gunbros_3.6.0_IOS.c#L97705)；[CMesh::Frame](gunbros_3.6.0_IOS.c#L98687) |
| `meshAnimationController.cpp` | [CMeshAnimationController](gunbros_3.6.0_IOS.c#L98708) |
| `meshCamera.cpp` | [CMeshCamera](gunbros_3.6.0_IOS.c#L98863) |
| `meshPathFinder.cpp` | [CMeshPathFinder](gunbros_3.6.0_IOS.c#L168383) |
| `mission.cpp` | [CMissionScriptContext](gunbros_3.6.0_IOS.c#L164513)；[Mission](gunbros_3.6.0_IOS.c#L163769) |
| `missionHighScore.cpp` | [CMissionHighScore](gunbros_3.6.0_IOS.c#L233284) |
| `missionObjective.cpp` | [MissionObjective](gunbros_3.6.0_IOS.c#L175980) |
| `missionObjectivePrompt.cpp` | [CMissionObjectivePrompt](gunbros_3.6.0_IOS.c#L139695) |
| `missionObjectiveStatus.cpp` | [CMissionObjectiveStatus](gunbros_3.6.0_IOS.c#L193175) |
| `missionWaveStatus.cpp` | [CMissionWaveStatus](gunbros_3.6.0_IOS.c#L192413)；[MissionWaveInfo](gunbros_3.6.0_IOS.c#L192822) |
| `moveSet.cpp` | [CMoveSet](gunbros_3.6.0_IOS.c#L99445) |
| `moveSetAnimController.cpp` | [CMoveSetAnimController](gunbros_3.6.0_IOS.c#L154783) |
| `moveSetMesh.cpp` | [CMoveSetMesh](gunbros_3.6.0_IOS.c#L122930) |
| `moveSetMeshController.cpp` | [CMoveSetMeshController](gunbros_3.6.0_IOS.c#L134034) |
| `movieOverlay.cpp` | [CMovieOverlay](gunbros_3.6.0_IOS.c#L182991) |
| `mpMatch.cpp` | [CMPMatch](gunbros_3.6.0_IOS.c#L395618)；[CMPMatch::Template](gunbros_3.6.0_IOS.c#L395646) |
| `networkObject.cpp` | [INetworkObject](gunbros_3.6.0_IOS.c#L231721) |
| `packetBuffer.cpp` | [PacketBuffer](gunbros_3.6.0_IOS.c#L249368) |
| `particle.cpp` | [CParticle](gunbros_3.6.0_IOS.c#L133114) |
| `particleEffect.cpp` | [CParticleEffect](gunbros_3.6.0_IOS.c#L130974)；[CParticleEmitter](gunbros_3.6.0_IOS.c#L131182) |
| `particleEffectPlayer.cpp` | [CParticleEffectPlayer](gunbros_3.6.0_IOS.c#L131269) |
| `particleEmitter.cpp` | [CParticleEmitter](gunbros_3.6.0_IOS.c#L131811)；[CParticleSpawnPatternCircle](gunbros_3.6.0_IOS.c#L132157)；[CParticleSpawnPatternLine](gunbros_3.6.0_IOS.c#L132036)；[CParticleSpawnPatternRect](gunbros_3.6.0_IOS.c#L132092)；[CParticleSpawnVelocityLinear](gunbros_3.6.0_IOS.c#L132287)；[CParticleSpawnVelocityRadial](gunbros_3.6.0_IOS.c#L132229)；[ParticleInterpolator](gunbros_3.6.0_IOS.c#L132332) |
| `particleSystem.cpp` | [CParticleSystem](gunbros_3.6.0_IOS.c#L133841) |
| `pickup.cpp` | [CPickup](gunbros_3.6.0_IOS.c#L99655)；[CPickup::Template](gunbros_3.6.0_IOS.c#L99591) |
| `planet.cpp` | [Planet](gunbros_3.6.0_IOS.c#L169908) |
| `platform.cpp` | [CPlatform](gunbros_3.6.0_IOS.c#L154460) |
| `player.cpp` | [CBrother](gunbros_3.6.0_IOS.c#L101592)；[CBrother::PacketRespawn](gunbros_3.6.0_IOS.c#L101569)；[CPlayer](gunbros_3.6.0_IOS.c#L100166) |
| `playerConfiguration.cpp` | [CPlayerConfiguration](gunbros_3.6.0_IOS.c#L170474) |
| `playerProgress.cpp` | [CPlayerProgress](gunbros_3.6.0_IOS.c#L193289)；[CPlayerProgress::ProgressData](gunbros_3.6.0_IOS.c#L193380)；[CPlayerProgress::Template](gunbros_3.6.0_IOS.c#L193209) |
| `playerStatistics.cpp` | [CPlayerStatistics](gunbros_3.6.0_IOS.c#L220165) |
| `powerUpSelector.cpp` | [CPowerUpSelector](gunbros_3.6.0_IOS.c#L183797)；[CPowerup](gunbros_3.6.0_IOS.c#L187928) |
| `powerup.cpp` | [CPowerup](gunbros_3.6.0_IOS.c#L187973)；[CPowerup::Template](gunbros_3.6.0_IOS.c#L187947) |
| `prize.cpp` | [CPrize](gunbros_3.6.0_IOS.c#L204703) |
| `prizeManager.cpp` | [CPrizeManager](gunbros_3.6.0_IOS.c#L209697) |
| `profileManager.cpp` | [CProfileManager](gunbros_3.6.0_IOS.c#L201063)；[platform::framework::CCore](gunbros_3.6.0_IOS.c#L204249) |
| `progression.cpp` | [Progression](gunbros_3.6.0_IOS.c#L164519) |
| `prop.cpp` | [CMoveSet](gunbros_3.6.0_IOS.c#L125069)；[CProp](gunbros_3.6.0_IOS.c#L123363)；[CProp::Template](gunbros_3.6.0_IOS.c#L123346) |
| `propertiesOverride.cpp` | [CPropertiesOverride](gunbros_3.6.0_IOS.c#L392251) |
| `purchases.cpp` | [CPurchases](gunbros_3.6.0_IOS.c#L183747) |
| `refinementManager.cpp` | [CRefinementManager](gunbros_3.6.0_IOS.c#L176716)；[CRefinementManager::CRefinementSlot](gunbros_3.6.0_IOS.c#L178471)；[CRefinementManager::Template](gunbros_3.6.0_IOS.c#L177773) |
| `remotePlayer.cpp` | [CRemotePlayer](gunbros_3.6.0_IOS.c#L229552) |
| `renderQueue.cpp` | [CRenderQueue](gunbros_3.6.0_IOS.c#L145064) |
| `requirement.cpp` | [RequirementList](gunbros_3.6.0_IOS.c#L191696) |
| `resPackTOC.cpp` | [CResPackTOC](gunbros_3.6.0_IOS.c#L132351) |
| `resTOCManager.cpp` | [CResTOCManager](gunbros_3.6.0_IOS.c#L132617) |
| `resourceLoader.cpp` | [CResourceLoader](gunbros_3.6.0_IOS.c#L101917) |
| `scalarFloat.cpp` | 未确认类名；仅 N_SO 文件记录；未找到具名 N_FUN，不能补写 CScalarFloat。 |
| `serializer.cpp` | [Deserializer](gunbros_3.6.0_IOS.c#L249630)；[Serializer](gunbros_3.6.0_IOS.c#L249605) |
| `simpleStream.cpp` | [CSimpleStream](gunbros_3.6.0_IOS.c#L102716) |
| `soundEffectLoop.cpp` | [CSoundEffectLoop](gunbros_3.6.0_IOS.c#L142289) |
| `soundQueue.cpp` | [CSoundQueue](gunbros_3.6.0_IOS.c#L102742) |
| `storeAggregator.cpp` | [CStoreAggregator](gunbros_3.6.0_IOS.c#L154835)；[platform::components::CStrWCharBuffer](gunbros_3.6.0_IOS.c#L159425) |
| `storeItem.cpp` | [CStoreItem](gunbros_3.6.0_IOS.c#L159542) |
| `storeSpinMgr.cpp` | [CStoreSpinMgr](gunbros_3.6.0_IOS.c#L399703) |
| `stunController.cpp` | [CStunController](gunbros_3.6.0_IOS.c#L394408) |
| `targetingController.cpp` | [CTargetingController](gunbros_3.6.0_IOS.c#L223153) |
| `textBox.cpp` | [CTextBox](gunbros_3.6.0_IOS.c#L103058) |
| `timerQueue.cpp` | [CTimerQueue](gunbros_3.6.0_IOS.c#L104366) |
| `tutorialManager.cpp` | [CTutorialManager](gunbros_3.6.0_IOS.c#L210464) |
| `utility.cpp` | [Utility](gunbros_3.6.0_IOS.c#L104376)（静态函数作用域，class/namespace 未确认） |
| `weaponMastery.cpp` | [CWeaponMastery](gunbros_3.6.0_IOS.c#L192201) |

## `src/platformLocal/`

| 文件名 | 类名／类型作用域 |
|---|---|
| `CBitmapFont.cpp` | [platform::graphics::CBitmapFont](gunbros_3.6.0_IOS.c#L394506) |
| `CDrawUtil.cpp` | [CDrawUtil](gunbros_3.6.0_IOS.c#L381872) |
| `CResourceManager_v1.cpp` | [platform::components::CIdToObjectRouter](gunbros_3.6.0_IOS.c#L331184)；[platform::systems::CResourceFactory](gunbros_3.6.0_IOS.c#L331294)；[platform::systems::CResourceManagerLegacy](gunbros_3.6.0_IOS.c#L331144)；[platform::systems::CResourceManager_v1](gunbros_3.6.0_IOS.c#L329732) |

## `src/purchase/`

| 文件名 | 类名／类型作用域 |
|---|---|
| `IAPInterface.mm` | [SIAPInterface](gunbros_3.6.0_IOS.c#L210897) |
| `PurchaseHandler.mm` | [PurchaseHandler](gunbros_3.6.0_IOS.c#L211034) |
| `PurchaseHandlerDelegate.mm` | [PurchaseHandlerDelegate](gunbros_3.6.0_IOS.c#L211452) |
| `PurchaseObserver.mm` | [PurchaseObserver](gunbros_3.6.0_IOS.c#L211549) |

## `src/spriteGlu3/`

| 文件名 | 类名／类型作用域 |
|---|---|
| `spriteGlu.cpp` | [CSpriteGlu](gunbros_3.6.0_IOS.c#L56870)；[TexturePack](gunbros_3.6.0_IOS.c#L57993) |
| `spriteIterator.cpp` | [CSpriteIterator](gunbros_3.6.0_IOS.c#L58022) |
| `spritePlayer.cpp` | [CSpritePlayer](gunbros_3.6.0_IOS.c#L58340) |

## `platform/shared/adpcm/src/`

| 文件名 | 类名／类型作用域 |
|---|---|
| `CADPCMInputStream.cpp` | [platform::adpcm::CADPCMInputStream](gunbros_3.6.0_IOS.c#L300082) |
| `adpcm.cpp` | 未确认类名；函数入口 [adpcm_decoder](gunbros_3.6.0_IOS.c#L299971) |

## `platform/shared/arm/src/`

| 文件名 | 类名／类型作用域 |
|---|---|
| `ARM_math.cpp` | 未确认类名；函数入口 [platform::arm::smult16](gunbros_3.6.0_IOS.c#L300406) |

## `platform/shared/cocoa/src/`

| 文件名 | 类名／类型作用域 |
|---|---|
| `CApplet_mm.mm` | [CApplet](gunbros_3.6.0_IOS.c#L300412) |
| `CCore_Cocoa.cpp` | [CCore_Cocoa](gunbros_3.6.0_IOS.c#L351806)；[platform::framework::ICCore](gunbros_3.6.0_IOS.c#L351794) |
| `CCore_Cocoa_mm.mm` | [CCore_Cocoa](gunbros_3.6.0_IOS.c#L300628) |
| `CDebug_Cocoa_mm.mm` | [platform::core::ICDebug](gunbros_3.6.0_IOS.c#L301050) |
| `CFileMgr_Cocoa.cpp` | [CFileMgr_Cocoa](gunbros_3.6.0_IOS.c#L301334)；[platform::components::ICFileMgr](gunbros_3.6.0_IOS.c#L301322) |
| `CFileMgr_Cocoa_mm.mm` | [CFileMgr_Cocoa](gunbros_3.6.0_IOS.c#L301167) |
| `CFile_Cocoa.cpp` | [CFile_Cocoa](gunbros_3.6.0_IOS.c#L301059) |
| `CGraphicsAbstractionManager_Cocoa.cpp` | [platform::graphics::CVertexBuffer](gunbros_3.6.0_IOS.c#L302317)；[platform::graphics::ICBuffer](gunbros_3.6.0_IOS.c#L302300)；[platform::graphics::ICDisplayProgram](gunbros_3.6.0_IOS.c#L301806)；[platform::graphics::ICGraphicsAbstractionManager](gunbros_3.6.0_IOS.c#L301891)；[platform::graphics::ICIndexBuffer](gunbros_3.6.0_IOS.c#L301978)；[platform::graphics::ICRasterizerState](gunbros_3.6.0_IOS.c#L302164)；[platform::graphics::ICRenderSurface](gunbros_3.6.0_IOS.c#L302089)；[platform::graphics::ICShader](gunbros_3.6.0_IOS.c#L302056)；[platform::graphics::ICShaderProgram](gunbros_3.6.0_IOS.c#L302014)；[platform::graphics::ICVertexBuffer](gunbros_3.6.0_IOS.c#L301945) |
| `CGraphics_OGLES2_Cocoa.cpp` | [CGraphics_OGLES2_Cocoa](gunbros_3.6.0_IOS.c#L381982)；[platform::graphics::CGraphics_OGLES](gunbros_3.6.0_IOS.c#L382256)；[platform::systems::CEvent](gunbros_3.6.0_IOS.c#L382346) |
| `CGraphics_OGLES_Cocoa.cpp` | [CGraphics_OGLES_Cocoa](gunbros_3.6.0_IOS.c#L382381) |
| `CGraphics_OGLES_Cocoa_mm.mm` | 未确认类名；函数入口 [CGraphics_OGLES_Cocoa_GetEAGLVersion](gunbros_3.6.0_IOS.c#L382671) |
| `CGraphics_OGLES_EAGL.cpp` | [CGraphics_OGLES_EAGL](gunbros_3.6.0_IOS.c#L301721) |
| `CGraphics_OGLES_EAGL_mm.mm` | 未确认类名；函数入口 [CGraphics_OGLES_EAGL_CreateContext](gunbros_3.6.0_IOS.c#L301650) |
| `CGyroscope_Cocoa_mm.mm` | [CGyroscope_Cocoa](gunbros_3.6.0_IOS.c#L302359) |
| `CLicenseMgr_Cocoa.cpp` | [CLicenseMgr_Cocoa](gunbros_3.6.0_IOS.c#L302545)；[platform::components::ICLicenseMgr](gunbros_3.6.0_IOS.c#L302524) |
| `CMediaPlayer_Cocoa.cpp` | [BackgroundTrackMgr](gunbros_3.6.0_IOS.c#L304621)；[CALPoolObject](gunbros_3.6.0_IOS.c#L304335)；[CMediaPlayer_Cocoa](gunbros_3.6.0_IOS.c#L302659)；[CSoundEvent_Cocoa](gunbros_3.6.0_IOS.c#L302606)；[CVibrationEvent_Cocoa](gunbros_3.6.0_IOS.c#L302620)；[platform::components::CSoundEvent](gunbros_3.6.0_IOS.c#L304561)；[platform::components::CVibrationEvent](gunbros_3.6.0_IOS.c#L304409)；[platform::components::ICMediaPlayer](gunbros_3.6.0_IOS.c#L304549) |
| `CMoviePlayer_Cocoa_mm.mm` | [CMovieEvent_Cocoa](gunbros_3.6.0_IOS.c#L305003)；[CMoviePlayer_Cocoa](gunbros_3.6.0_IOS.c#L305015)；[GluMovieController](gunbros_3.6.0_IOS.c#L304935)；[OverlayView](gunbros_3.6.0_IOS.c#L305027)；[platform::components::CMovieEvent](gunbros_3.6.0_IOS.c#L304919)；[platform::components::ICMoviePlayer](gunbros_3.6.0_IOS.c#L305666) |
| `CRenderSurface_OGLES_Window_Cocoa.cpp` | [CRenderSurface_OGLES_Window_Cocoa](gunbros_3.6.0_IOS.c#L306138)；[platform::graphics::CRenderSurface_OGLES_Texture](gunbros_3.6.0_IOS.c#L306717)；`platform::graphics::CRenderSurface_OGLES_Texture_FBO`（仅符号 `0x0021B9C4`）；`platform::graphics::CRenderSurface_SW`（仅符号 `0x0021B9B8`）；[platform::graphics::ICRenderSurface](gunbros_3.6.0_IOS.c#L306711) |
| `CSocket_Cocoa.cpp` | [CSocket_Cocoa](gunbros_3.6.0_IOS.c#L306736)；[platform::network::ICSocket](gunbros_3.6.0_IOS.c#L306724) |
| `CStdUtil_Cocoa.cpp` | [CStdUtil_Cocoa](gunbros_3.6.0_IOS.c#L307452) |
| `NPMalloc.cpp` | 未确认类名；函数入口 [np_malloc](gunbros_3.6.0_IOS.c#L307595) |
| `NPMem.cpp` | 未确认类名；函数入口 [np_memset](gunbros_3.6.0_IOS.c#L307613) |
| `main_mm.mm` | 未确认类名；函数入口 [main](gunbros_3.6.0_IOS.c#L307580) |

## `platform/shared/components/src/`

| 文件名 | 类名／类型作用域 |
|---|---|
| `CAggregateResource.cpp` | [platform::components::CAggregateResource](gunbros_3.6.0_IOS.c#L355548) |
| `CArrayInputStream.cpp` | [platform::components::CArrayInputStream](gunbros_3.6.0_IOS.c#L355858) |
| `CArrayOutputStream.cpp` | [platform::components::CArrayOutputStream](gunbros_3.6.0_IOS.c#L356049) |
| `CBigFileReader.cpp` | [platform::components::CBigFileReader](gunbros_3.6.0_IOS.c#L356201) |
| `CBinary.cpp` | [platform::components::CBinary](gunbros_3.6.0_IOS.c#L356953) |
| `CColor.cpp` | [platform::components::CColor](gunbros_3.6.0_IOS.c#L357300)；[platform::components::Color_ARGB_fixed](gunbros_3.6.0_IOS.c#L357058)；[platform::components::Color_Palette](gunbros_3.6.0_IOS.c#L357672) |
| `CCrc32.cpp` | [platform::components::CCrc32](gunbros_3.6.0_IOS.c#L357828) |
| `CExecutable.cpp` | [platform::components::CExecutable](gunbros_3.6.0_IOS.c#L357939) |
| `CFileInputStream.cpp` | [platform::components::CFileInputStream](gunbros_3.6.0_IOS.c#L357989) |
| `CFileOutputStream.cpp` | [platform::components::CFileOutputStream](gunbros_3.6.0_IOS.c#L358300) |
| `CFileUtil.cpp` | [platform::components::CFileUtil](gunbros_3.6.0_IOS.c#L358433) |
| `CHash.cpp` | [platform::components::CHash](gunbros_3.6.0_IOS.c#L359126) |
| `CInputStream.cpp` | [platform::components::CInputStream](gunbros_3.6.0_IOS.c#L359309) |
| `CKeysetResource.cpp` | [platform::components::CKeysetResource](gunbros_3.6.0_IOS.c#L359730) |
| `CMedia.cpp` | [platform::components::CMedia](gunbros_3.6.0_IOS.c#L359843) |
| `CMediaPlayer.cpp` | [platform::components::CMediaEvent](gunbros_3.6.0_IOS.c#L360112)；[platform::components::CMediaPlayer](gunbros_3.6.0_IOS.c#L360558)；[platform::components::CMediaPlayer3d](gunbros_3.6.0_IOS.c#L361225)；[platform::components::CSoundEvent](gunbros_3.6.0_IOS.c#L360119)；[platform::components::CSoundEvent3d](gunbros_3.6.0_IOS.c#L360375)；[platform::components::CSoundEventPCM](gunbros_3.6.0_IOS.c#L360214)；[platform::components::CSoundEventStreamingADPCM](gunbros_3.6.0_IOS.c#L361451)；[platform::components::CVibrationEvent](gunbros_3.6.0_IOS.c#L363342) |
| `CMoviePlayer.cpp` | [platform::components::CMovieEvent](gunbros_3.6.0_IOS.c#L363675)；[platform::components::CMoviePlayer](gunbros_3.6.0_IOS.c#L363824) |
| `COutputStream.cpp` | [platform::components::COutputStream](gunbros_3.6.0_IOS.c#L364305) |
| `CPool.cpp` | [platform::components::CPool](gunbros_3.6.0_IOS.c#L364487) |
| `CProperties.cpp` | [platform::components::CProperties](gunbros_3.6.0_IOS.c#L364725) |
| `CStrChar.cpp` | [platform::components::CStrChar](gunbros_3.6.0_IOS.c#L365524) |
| `CStrWChar.cpp` | [platform::components::CStrWChar](gunbros_3.6.0_IOS.c#L365890) |
| `CStrWCharBuffer.cpp` | [platform::components::CStrWCharBuffer](gunbros_3.6.0_IOS.c#L366528) |
| `CTypedVariableTable.cpp` | [platform::components::CTypedVariableTable](gunbros_3.6.0_IOS.c#L366658)；[platform::components::CTypedVariableTable::Entry](gunbros_3.6.0_IOS.c#L368072)；[platform::components::CTypedVariableTable::Stack](gunbros_3.6.0_IOS.c#L368150) |
| `CVorbis.cpp` | 未确认类名；函数入口 [platform::components::DecodeVorbisBitstream](gunbros_3.6.0_IOS.c#L368303) |
| `CZipInputStream.cpp` | [platform::components::CZipInputStream](gunbros_3.6.0_IOS.c#L368318) |

## `platform/shared/core/src/`

| 文件名 | 类名／类型作用域 |
|---|---|
| `CGenUtil.cpp` | [platform::core::CGenUtil](gunbros_3.6.0_IOS.c#L369793) |
| `CLinkList.cpp` | [platform::core::CLinkList](gunbros_3.6.0_IOS.c#L369926)；[platform::core::CLinkListNode](gunbros_3.6.0_IOS.c#L369887) |
| `CRandGen.cpp` | [platform::core::CRandGen](gunbros_3.6.0_IOS.c#L370208) |
| `CStringToKey.cpp` | 未确认类名；函数入口 [platform::core::CStringToKey](gunbros_3.6.0_IOS.c#L370405) |
| `CSystemEventQueue.cpp` | [platform::core::CSystemEventQueue](gunbros_3.6.0_IOS.c#L370521) |
| `CUtf.cpp` | [platform::core::CUtf](gunbros_3.6.0_IOS.c#L370654) |
| `bvsprintf.cpp` | 未确认类名；函数入口 [bvsprintf_s](gunbros_3.6.0_IOS.c#L368708) |

## `platform/shared/framework/src/`

| 文件名 | 类名／类型作用域 |
|---|---|
| `CApp.cpp` | [platform::framework::CApp](gunbros_3.6.0_IOS.c#L374779)；[platform::framework::CAppConfig](gunbros_3.6.0_IOS.c#L375159) |
| `CAppExecutor.cpp` | [platform::framework::CAppExecutor](gunbros_3.6.0_IOS.c#L307674) |

## `platform/shared/graphics/2d/src/`

| 文件名 | 类名／类型作用域 |
|---|---|
| `CFont.cpp` | [platform::graphics::CFont](gunbros_3.6.0_IOS.c#L351738)；[platform::graphics::ICFont](gunbros_3.6.0_IOS.c#L351777) |
| `CGraphics2d_OGLES.cpp` | [platform::graphics::CGraphics2d_OGLES](gunbros_3.6.0_IOS.c#L375719)；[platform::graphics::CGraphics2d_OGLES::Matrix](gunbros_3.6.0_IOS.c#L381755)；[platform::graphics::ICGraphics2d](gunbros_3.6.0_IOS.c#L381738) |
| `CTextParser.cpp` | 未确认类名；仅 N_SO 文件记录；未找到具名 N_FUN，类名未知。 |

## `platform/shared/graphics/src/`

| 文件名 | 类名／类型作用域 |
|---|---|
| `CBlit.cpp` | [platform::graphics::CBlit](gunbros_3.6.0_IOS.c#L331365) |
| `CBlitUtil.cpp` | [platform::graphics::CBlitUtil](gunbros_3.6.0_IOS.c#L336166) |
| `CDIB.cpp` | [platform::graphics::CDIB](gunbros_3.6.0_IOS.c#L336372) |
| `CDisplayProgram.cpp` | [platform::graphics::CDisplayProgram](gunbros_3.6.0_IOS.c#L336998)；[platform::graphics::CDisplayProgram::Instruction::Opcode](gunbros_3.6.0_IOS.c#L338816)；[platform::graphics::ICDisplayProgram](gunbros_3.6.0_IOS.c#L339841)；[platform::graphics::ICDisplayProgram::Mode](gunbros_3.6.0_IOS.c#L336690) |
| `CDisplayProgram_OGLES.cpp` | [platform::graphics::CDisplayProgram_OGLES](gunbros_3.6.0_IOS.c#L336568) |
| `CGraphics.cpp` | [platform::graphics::CGraphics](gunbros_3.6.0_IOS.c#L339875)；[platform::graphics::ICGraphics](gunbros_3.6.0_IOS.c#L340776)；[platform::graphics::ICGraphicsAbstract](gunbros_3.6.0_IOS.c#L340759)；[platform::graphics::ICGraphicsResource](gunbros_3.6.0_IOS.c#L339860) |
| `CGraphicsAbstractionManager.cpp` | [platform::graphics::CGraphicsAbstractionManager](gunbros_3.6.0_IOS.c#L340882)；[platform::graphics::ICGraphics](gunbros_3.6.0_IOS.c#L340818)；[platform::graphics::ICGraphics2d](gunbros_3.6.0_IOS.c#L341016)；[platform::graphics::ICGraphicsAbstractionManager](gunbros_3.6.0_IOS.c#L340847) |
| `CGraphics_OGLES.cpp` | [platform::graphics::CGraphics_OGLES](gunbros_3.6.0_IOS.c#L382906) |
| `CGraphics_OGLES2.cpp` | [platform::graphics::CGraphics_OGLES2](gunbros_3.6.0_IOS.c#L388194) |
| `CIndexBuffer.cpp` | [platform::graphics::CIndexBuffer](gunbros_3.6.0_IOS.c#L341063) |
| `CPNG.cpp` | [platform::graphics::CPNG](gunbros_3.6.0_IOS.c#L341313) |
| `CRasterizerState.cpp` | [platform::graphics::CRasterizerState_v1](gunbros_3.6.0_IOS.c#L342526)；[platform::graphics::ICRasterizerState](gunbros_3.6.0_IOS.c#L342881) |
| `CRasterizerState_OGLES.cpp` | [platform::graphics::CRasterizerState_v1_OGLES](gunbros_3.6.0_IOS.c#L341376) |
| `CRenderSurface.cpp` | [platform::graphics::CRenderSurface](gunbros_3.6.0_IOS.c#L345817)；[platform::graphics::ICRenderSurface](gunbros_3.6.0_IOS.c#L346800) |
| `CRenderSurfaceBuffer.cpp` | [platform::graphics::CRSBFrag](gunbros_3.6.0_IOS.c#L350121)；[platform::graphics::CRenderSurfaceBuffer](gunbros_3.6.0_IOS.c#L346819)；[platform::graphics::CRenderSurfaceBufferMipmap](gunbros_3.6.0_IOS.c#L346977) |
| `CRenderSurface_OGLES2_Texture_FBO.cpp` | [platform::graphics::CRenderSurface_OGLES2_Texture_FBO](gunbros_3.6.0_IOS.c#L390004) |
| `CRenderSurface_OGLES_Targetable.cpp` | [platform::graphics::CRenderSurface_OGLES_Targetable](gunbros_3.6.0_IOS.c#L342953) |
| `CRenderSurface_OGLES_Texture.cpp` | [platform::graphics::CRenderSurface_OGLES](gunbros_3.6.0_IOS.c#L345354)；[platform::graphics::CRenderSurface_OGLES_Texture](gunbros_3.6.0_IOS.c#L343585)；[platform::graphics::CRenderSurface_SW](gunbros_3.6.0_IOS.c#L345347) |
| `CRenderSurface_OGLES_Texture_FBO.cpp` | [platform::graphics::CRenderSurface](gunbros_3.6.0_IOS.c#L343559)；[platform::graphics::CRenderSurface_OGLES_Texture_FBO](gunbros_3.6.0_IOS.c#L343265) |
| `CRenderSurface_SW.cpp` | [platform::graphics::CRenderSurface_SW](gunbros_3.6.0_IOS.c#L345373) |
| `CShader.cpp` | [platform::graphics::CShader](gunbros_3.6.0_IOS.c#L350320)；[platform::graphics::ICShader](gunbros_3.6.0_IOS.c#L350542) |
| `CShaderProgram.cpp` | [platform::graphics::CShaderProgram](gunbros_3.6.0_IOS.c#L391161)；[platform::graphics::ICShaderProgram](gunbros_3.6.0_IOS.c#L392232) |
| `CShaderProgram_OGLES.cpp` | [platform::graphics::CShaderProgram_OGLES](gunbros_3.6.0_IOS.c#L350559) |
| `CShaderProgram_OGLES2.cpp` | [platform::graphics::CShaderProgram_OGLES2](gunbros_3.6.0_IOS.c#L390348) |
| `CShader_OGLES.cpp` | [platform::graphics::CShader_OGLES](gunbros_3.6.0_IOS.c#L350237) |
| `CShader_OGLES2.cpp` | [platform::graphics::CShader_OGLES2](gunbros_3.6.0_IOS.c#L390157) |
| `CVertex.cpp` | [platform::graphics::CVertex::Attribute::Id](gunbros_3.6.0_IOS.c#L350969) |
| `CVertexBuffer.cpp` | [platform::graphics::CVertex::Decl](gunbros_3.6.0_IOS.c#L351650)；[platform::graphics::CVertexBuffer](gunbros_3.6.0_IOS.c#L351168) |

## `platform/shared/math/src/`

| 文件名 | 类名／类型作用域 |
|---|---|
| `CMath.cpp` | 未确认类名；仅 N_SO 文件记录；未找到具名 N_FUN，类名未知。 |
| `CMathFixed.cpp` | [platform::math::CMathFixed](gunbros_3.6.0_IOS.c#L370850) |
| `CMatrix2d.cpp` | [platform::math::CMatrix2d](gunbros_3.6.0_IOS.c#L370882) |
| `CMatrix2dx.cpp` | [platform::math::CMatrix2dx](gunbros_3.6.0_IOS.c#L370891) |
| `CMatrix4d.cpp` | [platform::math::CMatrix4d](gunbros_3.6.0_IOS.c#L370900) |
| `CMatrix4dh.cpp` | [platform::math::CMatrix4dh](gunbros_3.6.0_IOS.c#L371334) |
| `CVector3d.cpp` | [platform::math::CVector3d](gunbros_3.6.0_IOS.c#L371823) |

## `platform/shared/network/src/`

| 文件名 | 类名／类型作用域 |
|---|---|
| `CHttpDataChunk.cpp` | [platform::network::CHttpDataChunk](gunbros_3.6.0_IOS.c#L325670) |
| `CHttpTransport.cpp` | [platform::network::CHttpTransport](gunbros_3.6.0_IOS.c#L325720)；[platform::network::HttpRequestInfo](gunbros_3.6.0_IOS.c#L327230) |
| `CWUtil.cpp` | [platform::network::CWUtil](gunbros_3.6.0_IOS.c#L327246) |

## `platform/shared/systems/src/`

| 文件名 | 类名／类型作用域 |
|---|---|
| `CEvent.cpp` | [platform::systems::CEvent](gunbros_3.6.0_IOS.c#L371851) |
| `CEventListener.cpp` | [platform::systems::CEventListener](gunbros_3.6.0_IOS.c#L372016)；[platform::systems::CSystem](gunbros_3.6.0_IOS.c#L372238) |
| `CExecutableRegistry.cpp` | [platform::systems::CExecutableRegistry](gunbros_3.6.0_IOS.c#L372253) |
| `CMessage.cpp` | [platform::systems::CMessage](gunbros_3.6.0_IOS.c#L372343) |
| `CRegistry.cpp` | [platform::systems::CRegistry](gunbros_3.6.0_IOS.c#L372410) |
| `CRegistryElement.cpp` | [platform::systems::CRegistryElement](gunbros_3.6.0_IOS.c#L372675) |
| `CResource.cpp` | [platform::systems::CResource](gunbros_3.6.0_IOS.c#L372714) |
| `CResourceBigFile.cpp` | [platform::systems::CResourceBigFile](gunbros_3.6.0_IOS.c#L372780) |
| `CResourceBinary.cpp` | [platform::systems::CResourceBinary](gunbros_3.6.0_IOS.c#L372918) |
| `CResourceDIB.cpp` | [platform::systems::CResourceDIB](gunbros_3.6.0_IOS.c#L373053) |
| `CResourceFactory.cpp` | [platform::systems::CResourceFactory](gunbros_3.6.0_IOS.c#L373180) |
| `CResourceFont.cpp` | [platform::systems::CResourceFont](gunbros_3.6.0_IOS.c#L373518) |
| `CResourceKeyset.cpp` | [platform::systems::CResourceKeyset](gunbros_3.6.0_IOS.c#L373651) |
| `CResourceManager.cpp` | [platform::systems::CResourceManagerLegacy](gunbros_3.6.0_IOS.c#L375298)；[platform::systems::ICResourceManager](gunbros_3.6.0_IOS.c#L375393) |
| `CResourceMedia.cpp` | [platform::systems::CResourceMedia](gunbros_3.6.0_IOS.c#L373786) |
| `CResourcePalette.cpp` | [platform::systems::CResourcePalette](gunbros_3.6.0_IOS.c#L373922) |
| `CResourceRenderSurface.cpp` | [platform::systems::CResourceRenderSurface](gunbros_3.6.0_IOS.c#L374057) |
| `CResourceShader.cpp` | [platform::systems::CResourceShader](gunbros_3.6.0_IOS.c#L374300) |
| `CResourceShaderProgram.cpp` | [platform::systems::CResourceShaderProgram](gunbros_3.6.0_IOS.c#L374455) |
| `CResourceStrWChar.cpp` | [platform::systems::CResourceStrWChar](gunbros_3.6.0_IOS.c#L374645) |

## `tools/gServe/src/storeOveride/`

| 文件名 | 类名／类型作用域 |
|---|---|
| `CNGSJSONData.cpp` | [CNGSJSONData](gunbros_3.6.0_IOS.c#L295420)；[CNGSJSONDataRequestFunctor](gunbros_3.6.0_IOS.c#L295399) |

## 核对边界

`N_SO` 恢复的是编译单元，不是原始头文件树。表中链接行号仅属于当前反编译 `.c`，不是原工程 `.cpp/.mm` 行号。无独立函数体的条目保留未知，不能据此断言没有类或属于废案。

广告排除范围包括 `src/PlayHaven/`、`src/adManager/`、AdColony、Tapjoy、FeaturedApp 接入；`CEventLog.cpp/.mm` 为遥测。第三方 JPEG、PNG、zlib、ASIHTTPRequest 等实现不列出，Glu 自己的资源/解码接口仍保留。

容易混淆的边界已经核对消费代码：

- `menuIncentives.cpp` 的 [Init](gunbros_3.6.0_IOS.c#L293181) 绑定 AdColony/Tapjoy 广告奖励；`offerManager.cpp` 的 [推广状态](gunbros_3.6.0_IOS.c#L222321) 混有邀友计数，本次按推广模块排除。
- `pushNotificationManager.cpp` 的 [Register](gunbros_3.6.0_IOS.c#L216490) 为推送接入，排除；`LocalNotificationMgr.cpp` 的 [HandleInactivityBonus](gunbros_3.6.0_IOS.c#L232623) 直接发放回归奖励，保留。
- `CPackageOfferMgr.cpp` 的 [AddItem](gunbros_3.6.0_IOS.c#L396370) 保存礼包购买条目，保留；`CNGSJSONData.cpp` 被 [CStoreItemOverride](gunbros_3.6.0_IOS.c#L232981) 消费，作为商品覆盖实现保留。Glu 的通用账户、跨 SKU 奖励协议、消息和服务 SDK 不展开，不将其误称为第三方广告。

核对统计：554 个文件—类型关联可定位反编译函数体；4 个关联仅有主程序函数符号。重复出现的类保留在每个实际编译单元下。

交叉检查：按上述范围筛选后，ARMv6 与 ARMv7 的 N_SO 文件集合均为 342 个且完全一致；文件行无重复，全部表内函数链接已按地址和函数签名核对。

输入 SHA-256（用于识别本表对应的二进制与反编译版本）：

- `gunbros`：`987d78475fc2261c066884f91dccde18a4c3d2f5d8fe403e701f84687178a6cd`
- `_IDA_OUT/gunbros_3.6.0_IOS.c`：`f4876601b73ad8a41506544939174d2de0b585d64e1894cba37e352f7c5560f1`
