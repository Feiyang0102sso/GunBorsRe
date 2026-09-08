# 原始源码树还原与重写优先级

## 当前重写进度（2026-09-07）

本文件下方的 P0–P3 是原工程的重写优先级，**不是完成度**。当前按可运行结果区分：

| 状态 | 已落地内容／后续工作 |
|---|---|
| 已完成基础能力 | BIG 资源读取、地图／精灵／模型展示、脚本解释器、模型动画和玩家／敌人拼装（M0–M3.8） |
| 已有可操作闭环 | GameView 移动、地图碰撞、相机、装备切换及鼠标开火（M4.1）；角色动作手感仍需校准 |
| 本轮新增 | 菜单 16 Arena：全 ENEMY 目录、独立多实例、选敌／通用行为、双方弹体伤害、受击／死亡、玩家生命、红绿血条和调试操作 |
| 已验证范围 | 78 个敌人条目（76 有脚本、2 无脚本）；73 把可开火武器实伤害通过，另外 3 个武器纯展示条目单列 |
| 后续战斗工作 | Boss 特殊机制、手雷眩晕、击杀奖励；真实关卡的路径／导航、刷怪器与波次整合，特殊弹体表现继续还原 |
| 后续外围系统 | 正式菜单流程、玩家成长／进度存档等；原版运营、联网及 iOS 绑定仍按下方 P3 排除 |

敌人现在已超出“把动作拼起来”的阶段，但缺脚本废案不会自动补成有效敌人，
全目录通用运行通过也不等于所有 Boss 和关卡机制完成。
操作和验收边界见 [Arena 使用说明](../docs/arena.md)，总里程碑见 [PLAN.md](../PLAN.md)。

从 `gunbros` (ARMv7 slice) 的 `LC_SYMTAB` 中提取。二进制未剥离符号，且保留了 stab 调试条目：
`N_SO` (0x64) 给出源文件路径，`N_OSO` (0x66) 给出它编译成的 `.o` / `.a` 归属。

- 工程根：`/Users/noah.ruffell/Documents/Projects/GunBros1/`
- 编译单元：**540**（工程内）+ 141（外部 SDK 源码）
- 只含 `.cpp` / `.mm` / `.m` / `.c`；头文件 `.h` / `.inl` 不进符号表
- 命名映射：`bullet.cpp` → `CBullet`，小写驼峰文件名 → `C` + 首字母大写

---

## 一、原工程布局

按**代码所有权**分三块，不是按文件类型。每个可复用模块自带 `src/`，全工程共 18 个 `src/`。

```
GunBros1/
├── src/                  游戏本体      GunBros 团队自己写的
├── platform/shared/      引擎          引擎组维护，多个游戏共用
└── tools/                服务 / SDK    第三方原样解压 + Glu 服务组
```

目录名里的版本号（`TapjoyConnect_v8.1.10`、`PlayHaven_v1.10.4`）说明第三方包整目录替换升级，所以不能塞进自己的 `src/`。

**重写时不必沿用这个结构**——它的复杂度来自多团队协作和第三方包管理，单人重写一个游戏时这些约束都不成立。一个 `src/` 底下按职责分即可。

---

## 二、分级图例

| 级别 | 含义 | 数量 |
|---|---|---:|
| **P0** | 核心。必须理解并重写，游戏的本体 | ~95 |
| **P1** | 需要，但实现可整体替换（现代库 / SDL3 / GL 3.3） | ~75 |
| **P2** | 需要，但可以推后。不影响"先跑起来" | ~80 |
| **P3** | **完全忽略**。运营、变现、联网、iOS 绑定 | ~430 |

**680 个文件里，真正要动脑子的约 250 个。**

---

## 三、引擎层 `platform/shared/`（130）

| 模块 | 数量 | 级别 | 说明 |
|---|---:|:---:|---|
| `components/src/` | 26 | **P0** | `CBigFileReader` 在这里。流、哈希、内存池、字符串、`CTypedVariableTable` |
| `systems/src/` | 20 | **P0** | 资源管理器、`CResourceBigFile`、注册表、事件系统 |
| `core/src/` | 7 | **P0** | `CClass`、`CVector`、`CLinkList`、`CStringToKey`、`CRandGen` |
| `math/src/` | 7 | **P0** | `CMatrix4d`、`CVector3d`、`CMathFixed`、`SinLUT` |
| `graphics/src/` | 28 | **P1** | 照着读懂设计，换 GL 3.3 Core 重写 |
| `graphics/2d/src/` | 3 | **P1** | 2D 批处理 |
| `framework/src/` | 2 | **P1** | `CApp` / `CCore` 生命周期，换 SDL3 |
| `adpcm/src/` | 2 | **P1** | 若 `.big` 里有 ADPCM 音频则需要，否则删 |
| `ljpeg/src/` | 25 | **P3** | 内嵌 libjpeg，换系统库 |
| `lpng/src/` | 11 | **P3** | 内嵌 libpng，换系统库 |
| `zlib/src/` | 6 | **P3** | 内嵌 zlib（只有 inflate），换系统库 |
| `cocoa/src/` | 26 | **P3** | iOS 平台绑定，换 SDL3 |
| `network/src/` | 3 | **P3** | HTTP |
| `arm/src/` | 1 | **P3** | `smult16` 定点乘法汇编，用普通 C |

**引擎层真正要重写的是前 8 行，约 95 个文件。** 后 6 行 72 个直接删。

---

## 四、游戏层 `src/`（219）

### P0 — 核心（按重写顺序排列）

**资源与容器**（第一个里程碑，扒清楚这些就能读 `.big`）
```
resTOCManager.cpp  resPackTOC.cpp  resourceLoader.cpp  gameAssetRef.cpp
gameObjectPack.cpp  serializer.cpp  simpleStream.cpp  imagePool.cpp  requirement.cpp
```

**主循环与框架**
```
engine.cpp  game.cpp  gameFlow.cpp  gameObject.cpp  gunbros.cpp
camera.cpp  renderQueue.cpp  timerQueue.cpp  utility.cpp  scalarFloat.cpp
CGameApp.cpp  CResBank.cpp  CFontMgr.cpp  drawSurface.cpp  CUtility.cpp
```

**关卡与地图**
```
level.cpp  map.cpp  levelObject.cpp  levelObjectPool.cpp  levelTag.cpp
layerTile.cpp  layerCollision.cpp  layerObject.cpp  layerCamera.cpp
layerPathLink.cpp  layerPathMesh.cpp
collision.cpp  collisionData.cpp  linkPathFinder.cpp  meshPathFinder.cpp
```

**战斗实体**
```
brother.cpp  brotherAI.cpp  player.cpp  playerConfiguration.cpp
enemy.cpp  enemySpawner.cpp  flock.cpp
bullet.cpp  gun.cpp  armor.cpp  powerup.cpp  pickup.cpp  prop.cpp  planet.cpp
targetingController.cpp  stunController.cpp
```

**动画与特效**
```
moveSet.cpp  moveSetAnimController.cpp  interpolator.cpp  effectLayer.cpp
particle.cpp  particleEffect.cpp  particleEffectPlayer.cpp
particleEmitter.cpp  particleSystem.cpp
```

**输入**
```
input.cpp  inputPad.cpp  controlStick.cpp
```

### P1 — 需要但实现可换（~20）

```
mesh.cpp  meshAnimationController.cpp  meshCamera.cpp        3D 顶点动画，非骨骼
moveSetMesh.cpp  moveSetMeshController.cpp                   模型动作绑定
glTools.cpp  platform.cpp  debug.cpp                         平台/调试
textBox.cpp  Label.cpp                                       文本渲染
bgm.cpp  soundQueue.cpp  soundEffectLoop.cpp                 音频
gluMovie.cpp  layerMovie.cpp  (src/gluMovie/ 11 个)          UI 动画系统
(src/gluScript/ 11 个)                                       脚本解释器
(src/spriteGlu3/ 3 个)                                       精灵系统
```

> `gluScript` / `gluMovie` / `spriteGlu3` 是三个独立子系统，先跳过——地图和角色能画出来之后再回头啃。

### P2 — 推后（~80）

**菜单系统（40 个）** — 占游戏层近 1/5，但对"跑起来"零贡献
```
menu.cpp menuAction.cpp menuList.cpp menuStack.cpp menuSystem.cpp menuOption.cpp
menuOptionGroup.cpp menuDataProvider.cpp menuNavigationBar.cpp menuSplash.cpp
menuStore.cpp menuMissions.cpp menuChallenges.cpp menuFriends.cpp menuPostGame.cpp
menuMesh*.cpp menuMovie*.cpp ...（其余同名前缀）
mainScreen.cpp  dialogPopup.cpp  movieOverlay.cpp  levelIndicator.cpp
```

**任务与进度**
```
mission.cpp  missionObjective.cpp  missionObjectivePrompt.cpp
missionObjectiveStatus.cpp  missionWaveStatus.cpp  missionHighScore.cpp
progression.cpp  playerProgress.cpp  playerStatistics.cpp  profileManager.cpp
challengeManager.cpp  challengeProgressData.cpp  challengeInfoOverlay.cpp
```

**经济与奖励**
```
storeItem.cpp  storeAggregator.cpp  storeSpinMgr.cpp
prize.cpp  prizeManager.cpp  dailyBonusTracking.cpp  refinementManager.cpp
weaponMastery.cpp  powerUpSelector.cpp  CAchievementsMgr.cpp
KillTracker.cpp  inputPadMeter.cpp  tutorialManager.cpp  CSaveGameMgr.cpp
COptionsMgr.cpp  CGameProfiler.cpp
```

### P3 — 完全忽略（~50）

联网多人、社交、广告、内购、推送、遥测，以及全部 `.mm` / `.m`：
```
CMultiplayerMgr.cpp  mpMatch.cpp  remotePlayer.cpp  networkObject.cpp
packetBuffer.cpp  NetworkParams.cpp  (src/NGClient/ 2)
friendsManager.cpp  friendData.cpp  friendPowerManager.cpp
CGKFriendRequestComposeViewController.*  MessageComposerViewController.*
AdColonyInterface.cpp  FeaturedAppMgr.cpp  offerManager.cpp
CPackageOfferMgr.cpp  StoreAutoPreview.cpp  StoreItemOverride.cpp
propertiesOverride.cpp  contentTracker.cpp  CEventLog.*
purchases.cpp  PurchaseManager.mm  (src/purchase/ 4)
pushNotificationManager.cpp  LocalNotificationMgr.*
cocoa/TapjoyInterface.mm  cocoa/TapjoyManager.mm
(src/cocoa/ 4)  (src/adManager/ 2)  (src/PlayHaven/ 2)  (src/platformLocal/ 3)
```

---

## 五、`tools/` 与外部 SDK — 整体忽略（281）

跨游戏复用，但复用的是**运营和变现基础设施**，不是引擎能力。和引擎平级，各自被游戏层调用。

| 模块 | 数量 | 是什么 |
|---|---:|---|
| `tools/gServe/` | 67 | Glu 账号后端（NGS）：登录、好友、锦标赛、Facebook、GameCenter、推送 |
| `tools/TapjoyConnect_v8.1.10/` | 33 | 广告变现 |
| `tools/PlayHaven_v1.10.4/` | 22 | 广告变现 |
| `tools/MessagingQueue/` | 10 | 推送消息 |
| `tools/glu_games_network/` | 7 | 社区界面（那批散 PNG 图标的消费者） |
| `tools/AdColony/` | 1 | 视频广告 |
| 外部 SDK 源码 | 141 | AdMarvel 79 / AdColony 49 / Millennial 22 / AdMob 35 / Flurry 13 |

**这 281 个文件一行都不用看。**

---

## 六、建议的里程碑

1. **读得到资源** — `resTOCManager` + `resPackTOC` + `CBigFileReader` + `CResourceBigFile`，
   目标：打开 `packTOC_xga.dat` → 加载 `pack0_core_xga.big` → 解出一张 PNG 显示在窗口里。
   这条链走通，容器格式、寻址、zlib 解压四块地基全部验证过。
2. **画得出地图** — `map` + `layerTile` + `level` + 图块集，得到一张静态大图。
3. **动得起来** — `brother` + `input` + `camera` + `moveSet`，角色能跑。
4. **打得起来** — `bullet` + `enemy` + `collision` + `particle`。
5. 之后再考虑菜单、进度、3D 模型。

---

## 七、完整文件清单

按原始目录分组，见下。

### `platform/shared/adpcm/src/`  (2)

```
CADPCMInputStream.cpp adpcm.cpp
```

### `platform/shared/arm/src/`  (1)

```
ARM_math.cpp
```

### `platform/shared/cocoa/src/`  (26)

```
ASIHTTPRequest.m CApplet_mm.mm CCore_Cocoa.cpp CCore_Cocoa_mm.mm CDebug_Cocoa_mm.mm CFileMgr_Cocoa.cpp CFileMgr_Cocoa_mm.mm CFile_Cocoa.cpp CGraphicsAbstractionManager_Cocoa.cpp CGraphics_OGLES2_Cocoa.cpp CGraphics_OGLES_Cocoa.cpp CGraphics_OGLES_Cocoa_mm.mm CGraphics_OGLES_EAGL.cpp CGraphics_OGLES_EAGL_mm.mm CGyroscope_Cocoa_mm.mm CLicenseMgr_Cocoa.cpp CMediaPlayer_Cocoa.cpp CMoviePlayer_Cocoa_mm.mm CPushNotification_Cocoa_mm.mm CRenderSurface_OGLES_Window_Cocoa.cpp CSocket_Cocoa.cpp CStdUtil_Cocoa.cpp NPMalloc.cpp NPMem.cpp NSHTTPCookieAdditions.m main_mm.mm
```

### `platform/shared/components/src/`  (26)

```
CAggregateResource.cpp CArrayInputStream.cpp CArrayOutputStream.cpp CBigFileReader.cpp CBinary.cpp CColor.cpp CCrc32.cpp CExecutable.cpp CFileInputStream.cpp CFileOutputStream.cpp CFileUtil.cpp CHash.cpp CInputStream.cpp CKeysetResource.cpp CMedia.cpp CMediaPlayer.cpp CMoviePlayer.cpp COutputStream.cpp CPool.cpp CProperties.cpp CStrChar.cpp CStrWChar.cpp CStrWCharBuffer.cpp CTypedVariableTable.cpp CVorbis.cpp CZipInputStream.cpp
```

### `platform/shared/core/src/`  (7)

```
CGenUtil.cpp CLinkList.cpp CRandGen.cpp CStringToKey.cpp CSystemEventQueue.cpp CUtf.cpp bvsprintf.cpp
```

### `platform/shared/framework/src/`  (2)

```
CApp.cpp CAppExecutor.cpp
```

### `platform/shared/graphics/2d/src/`  (3)

```
CFont.cpp CGraphics2d_OGLES.cpp CTextParser.cpp
```

### `platform/shared/graphics/src/`  (28)

```
CBlit.cpp CBlitUtil.cpp CDIB.cpp CDisplayProgram.cpp CDisplayProgram_OGLES.cpp CGraphics.cpp CGraphicsAbstractionManager.cpp CGraphics_OGLES.cpp CGraphics_OGLES2.cpp CIndexBuffer.cpp CPNG.cpp CRasterizerState.cpp CRasterizerState_OGLES.cpp CRenderSurface.cpp CRenderSurfaceBuffer.cpp CRenderSurface_OGLES2_Texture_FBO.cpp CRenderSurface_OGLES_Targetable.cpp CRenderSurface_OGLES_Texture.cpp CRenderSurface_OGLES_Texture_FBO.cpp CRenderSurface_SW.cpp CShader.cpp CShaderProgram.cpp CShaderProgram_OGLES.cpp CShaderProgram_OGLES2.cpp CShader_OGLES.cpp CShader_OGLES2.cpp CVertex.cpp CVertexBuffer.cpp
```

### `platform/shared/ljpeg/src/`  (25)

```
jcomapi.c jdapimin.c jdapistd.c jdcoefct.c jdcolor.c jddctmgr.c jdhuff.c jdinput.c jdmainct.c jdmarker.c jdmaster.c jdmerge.c jdphuff.c jdpostct.c jdsample.c jerror.c jidctflt.c jidctfst.c jidctint.c jidctred.c jmemmgr.c jmemnobs.c jquant1.c jquant2.c jutils.c
```

### `platform/shared/lpng/src/`  (11)

```
png.c pngerror.c pngget.c pngmem.c pngread.c pngrio.c pngrtran.c pngrutil.c pngset.c pngstrlen.c pngtrans.c
```

### `platform/shared/math/src/`  (7)

```
CMath.cpp CMathFixed.cpp CMatrix2d.cpp CMatrix2dx.cpp CMatrix4d.cpp CMatrix4dh.cpp CVector3d.cpp
```

### `platform/shared/network/src/`  (3)

```
CHttpDataChunk.cpp CHttpTransport.cpp CWUtil.cpp
```

### `platform/shared/systems/src/`  (20)

```
CEvent.cpp CEventListener.cpp CExecutableRegistry.cpp CMessage.cpp CRegistry.cpp CRegistryElement.cpp CResource.cpp CResourceBigFile.cpp CResourceBinary.cpp CResourceDIB.cpp CResourceFactory.cpp CResourceFont.cpp CResourceKeyset.cpp CResourceManager.cpp CResourceMedia.cpp CResourcePalette.cpp CResourceRenderSurface.cpp CResourceShader.cpp CResourceShaderProgram.cpp CResourceStrWChar.cpp
```

### `platform/shared/zlib/src/`  (6)

```
adler32.c crc32.c inffast.c inflate.c inftrees.c zutil.c
```

### `src/`  (8)

```
CFontMgr.cpp CGameApp.cpp CGameProfiler.cpp COptionsMgr.cpp CResBank.cpp CSaveGameMgr.cpp CUtility.cpp drawSurface.cpp
```

### `src/NGClient/`  (2)

```
CFunctor.cpp CGunBrosFactory.cpp
```

### `src/PlayHaven/`  (2)

```
PHInterface.mm PHMgr.mm
```

### `src/adManager/`  (2)

```
AdMgr.mm AdMgrInterface.mm
```

### `src/cocoa/`  (4)

```
AppDelegate_mm.mm AppViewController.mm AppView_mm.mm AppleInterface.mm
```

### `src/gluMovie/`  (11)

```
embededMovie.cpp movie.cpp movieChapter.cpp movieEmptyRegion.cpp movieFill.cpp movieObject.cpp movieRegion.cpp movieSoundSet.cpp movieSprite.cpp movieText.cpp movieTiledSprite.cpp
```

### `src/gluScript/`  (11)

```
script.cpp scriptCode.cpp scriptCondition.cpp scriptEvent.cpp scriptFunction.cpp scriptInterpreter.cpp scriptResolver.cpp scriptResult.cpp scriptReturn.cpp scriptState.cpp scriptVariable.cpp
```

### `src/gunbros/`  (181)

```
AdColonyInterface.cpp CAchievementsMgr.cpp CEventLog.cpp CEventLog.mm CGKFriendRequestComposeViewController.cpp CGKFriendRequestComposeViewController.mm CMultiplayerMgr.cpp CPackageOfferMgr.cpp FeaturedAppMgr.cpp KillTracker.cpp Label.cpp LocalNotificationMgr.cpp LocalNotificationMgr.mm MessageComposerViewController.cpp MessageComposerViewController.mm NetworkParams.cpp PurchaseManager.mm StoreAutoPreview.cpp StoreItemOverride.cpp armor.cpp bgm.cpp brother.cpp brotherAI.cpp bullet.cpp camera.cpp challengeInfoOverlay.cpp challengeManager.cpp challengeProgressData.cpp collision.cpp collisionData.cpp contentTracker.cpp controlStick.cpp dailyBonusTracking.cpp debug.cpp dialogPopup.cpp effectLayer.cpp enemy.cpp enemySpawner.cpp engine.cpp flock.cpp friendData.cpp friendPowerManager.cpp friendsManager.cpp game.cpp gameAssetRef.cpp gameFlow.cpp gameObject.cpp gameObjectPack.cpp glTools.cpp gluMovie.cpp gun.cpp gunbros.cpp imagePool.cpp input.cpp inputPad.cpp inputPadMeter.cpp interpolator.cpp layerCamera.cpp layerCollision.cpp layerMovie.cpp layerObject.cpp layerPathLink.cpp layerPathMesh.cpp layerTile.cpp level.cpp levelIndicator.cpp levelObject.cpp levelObjectPool.cpp levelTag.cpp linkPathFinder.cpp mainScreen.cpp map.cpp menu.cpp menuAction.cpp menuChallengeOption.cpp menuChallenges.cpp menuDataProvider.cpp menuFriendOption.cpp menuFriendOptionGroup.cpp menuFriendPowerOption.cpp menuFriends.cpp menuGameResources.cpp menuGreeting.cpp menuIconOption.cpp menuIncentives.cpp menuInviteFriends.cpp menuList.cpp menuListOption.cpp menuLotteryPopup.cpp menuLotterySelection.cpp menuMesh.cpp menuMeshEnemy.cpp menuMeshOption.cpp menuMeshPlayer.cpp menuMidPopup.cpp menuMissionInfo.cpp menuMissionOption.cpp menuMissions.cpp menuMovieButton.cpp menuMovieControl.cpp menuMovieMultiplayerOverlay.cpp menuMovieQueuedOverlay.cpp menuMovieScrollBar.cpp menuNavigationBar.cpp menuOption.cpp menuOptionGroup.cpp menuPlayerSelect.cpp menuPopupPrompt.cpp menuPostGame.cpp menuPostGameOption.cpp menuSplash.cpp menuStack.cpp menuStore.cpp menuStoreOption.cpp menuStoreOptionGroup.cpp menuSystem.cpp menuTapjoyOption.cpp menuUpgradePopup.cpp mesh.cpp meshAnimationController.cpp meshCamera.cpp meshPathFinder.cpp mission.cpp missionHighScore.cpp missionObjective.cpp missionObjectivePrompt.cpp missionObjectiveStatus.cpp missionWaveStatus.cpp moveSet.cpp moveSetAnimController.cpp moveSetMesh.cpp moveSetMeshController.cpp movieOverlay.cpp mpMatch.cpp networkObject.cpp offerManager.cpp packetBuffer.cpp particle.cpp particleEffect.cpp particleEffectPlayer.cpp particleEmitter.cpp particleSystem.cpp pickup.cpp planet.cpp platform.cpp player.cpp playerConfiguration.cpp playerProgress.cpp playerStatistics.cpp powerUpSelector.cpp powerup.cpp prize.cpp prizeManager.cpp profileManager.cpp progression.cpp prop.cpp propertiesOverride.cpp purchases.cpp pushNotificationManager.cpp refinementManager.cpp remotePlayer.cpp renderQueue.cpp requirement.cpp resPackTOC.cpp resTOCManager.cpp resourceLoader.cpp scalarFloat.cpp serializer.cpp simpleStream.cpp soundEffectLoop.cpp soundQueue.cpp storeAggregator.cpp storeItem.cpp storeSpinMgr.cpp stunController.cpp targetingController.cpp textBox.cpp timerQueue.cpp tutorialManager.cpp utility.cpp weaponMastery.cpp
```

### `src/gunbros/cocoa/`  (2)

```
TapjoyInterface.mm TapjoyManager.mm
```

### `src/platformLocal/`  (3)

```
CBitmapFont.cpp CDrawUtil.cpp CResourceManager_v1.cpp
```

### `src/purchase/`  (4)

```
IAPInterface.mm PurchaseHandler.mm PurchaseHandlerDelegate.mm PurchaseObserver.mm
```

### `src/spriteGlu3/`  (3)

```
spriteGlu.cpp spriteIterator.cpp spritePlayer.cpp
```

### `tools/AdColony/`  (1)

```
AdColony_Facade.mm
```

### `tools/MessagingQueue/src/`  (8)

```
CNetAnalytics.cpp CNetMessageQueue.cpp CNetMessageServer.cpp CObjectMap.cpp CObjectMapArray.cpp CObjectMapObject.cpp CWStringBuffer.cpp JSONParser.cpp
```

### `tools/MessagingQueue/src/cocoa/`  (2)

```
CNetworkAvailability.mm CPlatformUtil.mm
```

### `tools/PlayHaven_v1.10.4/Cache/`  (3)

```
PHUrlPrefetchOperation.m SDCachedURLResponse.m SDURLCache.m
```

### `tools/PlayHaven_v1.10.4/src/`  (19)

```
NSObject+QueryComponents.m PHAPIRequest.m PHConstants.m PHContent.m PHContentView.m PHNetworkUtil.m PHNotificationBadgeRenderer.m PHNotificationRenderer.m PHNotificationView.m PHPublisherContentRequest.m PHPublisherIAPTrackingRequest.m PHPublisherMetadataRequest.m PHPublisherOpenRequest.m PHPublisherSubcontentRequest.m PHPurchase.m PHReward.m PHStringUtil.m PHURLLoader.m UIDevice+HardwareString.m
```

### `tools/TapjoyConnect_v8.1.10/`  (1)

```
TapjoyConnect.m
```

### `tools/TapjoyConnect_v8.1.10/Components/TJCAds/`  (2)

```
TJCAdRequestHandler.m TJCAdView.m
```

### `tools/TapjoyConnect_v8.1.10/Components/TJCCore/Utilities/`  (5)

```
TBXML.m TJCHardwareUtil.m TJCLog.m TJCNetReachability.m TJCUtil.m
```

### `tools/TapjoyConnect_v8.1.10/Components/TJCCore/Views/`  (5)

```
TJCCallsWrapper.m TJCLoadingView.m TJCUINavigationBarView.m TJCUIWebPageView.m TJCViewCommons.m
```

### `tools/TapjoyConnect_v8.1.10/Components/TJCCore/WebFetcher/`  (2)

```
TJCCoreFetcher.m TJCCoreFetcherHandler.m
```

### `tools/TapjoyConnect_v8.1.10/Components/TJCExtensions/`  (1)

```
OpenUDID.m
```

### `tools/TapjoyConnect_v8.1.10/Components/TJCFeaturedApp/`  (6)

```
TJCFeaturedAppDBManager.m TJCFeaturedAppManager.m TJCFeaturedAppModel.m TJCFeaturedAppRequestHandler.m TJCFeaturedAppView.m TJCFeaturedAppViewHandler.m
```

### `tools/TapjoyConnect_v8.1.10/Components/TJCOffers/`  (2)

```
TJCOffersViewHandler.m TJCOffersWebView.m
```

### `tools/TapjoyConnect_v8.1.10/Components/TJCUserAccount/`  (3)

```
TJCUserAccountManager.m TJCUserAccountModel.m TJCUserAccountRequestHandler.m
```

### `tools/TapjoyConnect_v8.1.10/Components/TJCVideoAds/`  (6)

```
TJCVideoLayer.m TJCVideoManager.m TJCVideoObject.m TJCVideoRequestHandler.m TJCVideoView.m TJCVideoViewHandler.m
```

### `tools/gServe/src/`  (28)

```
CGluSocialInterface.cpp CGluSocialManager.cpp CNGS.cpp CNGSAccountManager.cpp CNGSAttribute.cpp CNGSAttributeManager.cpp CNGSConst.cpp CNGSContentManager.cpp CNGSFactory.cpp CNGSFromServerMessageQ.cpp CNGSHeader.cpp CNGSLocalUser.cpp CNGSPushNotificationDetails.cpp CNGSRemoteUser.cpp CNGSRemoteUserList.cpp CNGSSKUBonus.cpp CNGSServerObject.cpp CNGSServerRequest.cpp CNGSSession.cpp CNGSSessionConfig.cpp CNGSSocialInterface.cpp CNGSSocialMessage.cpp CNGSTournament.cpp CNGSUIManager.cpp CNGSURLMgr.cpp CNGSUser.cpp CNGSUserCredentials.cpp CNGSUtil.cpp
```

### `tools/gServe/src/friends/`  (1)

```
CNGSFriendsManager.cpp
```

### `tools/gServe/src/gServe_platform/`  (3)

```
CFileUtil_gServe.cpp CHttpTransport_gServe.cpp CNetMessageQueue_gServe.cpp
```

### `tools/gServe/src/gServe_public/`  (1)

```
CNotificationHandler.cpp
```

### `tools/gServe/src/iphone/`  (18)

```
BundleInterface.mm CAskHowToProceedViewDialog.mm CErrorViewDialog.mm CFacebookInterface.mm CFacebookManager.mm CFacebookMessage.mm CGameCenterInterface.mm CGameCenterManager.mm CGenericButtonViewDialog.mm CGenericTextEntryViewDialog.mm CIdentityConfirmationViewDialog.mm CLoginViewDialog.mm CNGSView.mm CNGS_Platform.mm CNetworkUtil.mm CRegisterUserViewDialog.mm CUtil.mm Reachability.mm
```

### `tools/gServe/src/iphone/FBConnect/`  (4)

```
FBDialog.m FBLoginDialog.m FBRequest.m Facebook.m
```

### `tools/gServe/src/iphone/FBConnect/JSON/`  (6)

```
NSObject+SBJSON.m NSString+SBJSON.m SBJSON.m SBJsonBase.m SBJsonParser.m SBJsonWriter.m
```

### `tools/gServe/src/offers/`  (5)

```
CNGSDataIncentive.cpp CNGSDataOffer.cpp CNGSFriendInviteeList.cpp CNGSOfferManager.cpp CNGSOfferRequest.cpp
```

### `tools/gServe/src/storeOveride/`  (1)

```
CNGSJSONData.cpp
```

### `tools/glu_games_network/code/cocoa/src/`  (7)

```
CGluGamesNetwork.mm GGNWebViewController.m GluGamesNetworkController.mm GluVerticalCell.m GluVerticalTabBarController.m SubNavController.m TabViewController.m
```
