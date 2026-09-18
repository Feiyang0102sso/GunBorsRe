# 地图职责归位

## 范围与授权

2026-09-18 用户授权：全部地图相关代码拆分放入 `gameplay/map/`，尽量对应原版并消除 Z 文件；Viewer 专属适配集中保留一个 Z 实现文件。开始时间 16:03 UTC，最迟 21:03 UTC 收尾。原始 BIG、解包样本和存档只读。

## 方案与验收

1. 将 CMap、地图图层、相机、TileSet、CProp 移入 map；共享碰撞算法和通用模型投影仍保留其共享职责，更新引用。
2. 删除 ZLoadedMap 包装：解析地图和场景资源由同一个 CMap 管理。资源缓存作为明确的宿主实现细节，不能伪称恢复了原内存布局。
3. 删除 ZPlacedProp 的脚本/动画双持有；CProp 持有实例位置、脚本和三个播放器，模板与不可变绘制资源按引用共享。
4. 道具调度归 CLevel 的分文件实现；移除只有一个实现的 ZPropWorld 接口。保留现有脚本关卡与战斗关卡连接方式，不在本轮改写整个生存循环。
5. 绘制按 CMap / CLayerTile / CProp / CRenderQueue 的原职责拆分。资源解包与 Windows 提交属于适配实现，注明依据。
6. Viewer 通过实际 CProp 脚本选择研究状态，删除手写动画、碰撞和转场资源表；仅保留研究对象/状态选择与浏览操作。现有研究和测试入口继续可用。
7. 每阶段构建并按影响验证；最终 Debug/Release 三产物与地图、道具、遮挡、寻路、关卡和生存相关检查。测试均 --mute，保留异常原样本诊断。

## 证据入口

- `_prep/_IDA_OUT/source_tree.md`：N_SO/N_FUN 原文件归属。
- MAP / TILESET：`maps/map.bt`、`maps/tileset.bt`；CMap::Init 92498、Bind 92365 附近、Draw 92130 起；CLayerTile 126759 起。
- PROP：`entries/prop_template.bt`、Sprite 与 Flow 模板；CProp::Bind 124863、GetZOrderGroup 123406、Draw 123506 起、GetBounds 123561、Update 123609、FunctionResolver 124467 起。
- 资源：CGunBros::GetGameObject 78497；CGameObjectPack、TileSet::Load 130243 附近；加载行为逐函数核对。
- 绘制队列：CRenderQueue::Compare 145029、Draw 145123；CMeshCamera::DrawHeirarchy 99263。
- 道具调度：CLayerObject::OnStart 126250、CLevel::TransformObjectElapseMS 114280；CProp native 7/10/16/17 的伤害与效果分派。

## 任务清单

- [x] 核对证据、拆分存储与依赖。
- [x] 地图目录归位，CMap 统一资源所有权。
- [x] CProp 统一实例与动画，关卡道具调度归位。
- [x] 绘制、图层、资源与粒子职责拆分。
- [x] Viewer 单文件适配与 Flow 预览。
- [x] Debug/Release 构建及相关流程验证。
- [x] 更新映射、结果与未验证边界。

## 实施与验证结果

`gameplay/map/` 共 46 个源码文件；唯一 Z 文件为 `ZMapViewer.h`。原九个 `ZMap*` 文件和 `ZPropWorld.h` 已移除，不保留转发头或旧包装别名。未修改工程数量，仍由现有单工程通配收集到三种产物。

完成时间：2026-09-18 17:02 UTC，约 59 分钟，未超过 5 小时上限。

- `CMap` 持有嵌套资源存储；移动地图时资源整体转移，缓存、纹理、道具和 Flow 宿主地址保持稳定。
- `CProp` 持有实例位置、三播放器、脚本与碰撞；原 `ZPlacedProp` 的重复动画层、人工状态表和运行时指针包装已删除。`CLevel::Props` 负责启动层、消息、更新、受击及原生动作分派。
- `CMap::Load` 只加载地图资源。正式流程由选定 `CLevel` 绑定脚本；遍历匹配 LEVEL 的历史研究行为只保留在 `LoadPreviewMap`。
- 瓦片绘制归 `CLayerTile`，道具绘制和包围范围归 `CProp`，混合排序归 `CRenderQueue`。背景、主体和前景均使用组号／Y 排序，避免绑定后再移动道具实例。地图实现不再包含会话聚合头 `CMapInternal.h`。
- Viewer 浏览、研究对象选择、状态切换、炮台预览及放置敌人的研究显示统一到 `ZMapViewer.h`。交互状态切换后执行实际 Flow，粒子与声音从产生的资源动作取得，不再维护资源替代表。Viewer 的逐帧标记不进入 CProp。
- `ZMapScene.h` 的会话参数归 `CGame::Launch`／`CGameSession.h`；键盘策略回到输入文件。共享碰撞算法、特效类型与引擎渲染保留原所属目录。
- 图层记录成为所属类的嵌套类型；`CLayerMovie` 读取并保留原引用和有符号位置。新增测试验证字段宽度、符号和截断拒绝。

### 构建与回归

Debug、Release 的 Game 构建均递归成功生成 Game／Viewer／Tests，共六个产物，退出码 0。命令使用 `/p:GbProduct=Game /p:Configuration=<Debug或Release> /p:Platform=x64 /p:SkipAutoTests=true`；对应检查单独运行。最终构建日志：`obj/map-migration/verified-Debug-build.log`、`verified-Release-build.log`。

| 验证 | 结果 | 证据 |
|---|---|---|
| Debug 地图与相关游戏流程 | 23/23 完成，22 通过、1 个预期原数据异常；退出码 0 | `obj/map-migration/final-Debug-tests.log`、`Debug-results/summary.json` |
| Release 核心地图与终轮 | 12/12 通过；退出码 0 | `obj/map-migration/final-Release-tests.log`、`Release-results/summary.json` |
| 全地图生产加载与资源寿命 | 22 张地图、1,653 个道具、12 组共享模板实例独立性；零失败 | 两种配置的 `Core/map-resources/logs/stdout.log` |
| Movie 图层读取边界 | 引用、有符号坐标、截断拒绝均通过 | `Core/level-flow/logs/stdout.log` 中 `map-movie-check` |
| 原件完整性 | 两轮 protected-changes 均为 0 | 两种配置的 `summary.json` 与 protected 快照 |
| 图像核对 | 掩体碎屑／烟雾、树木与角色前后遮挡正常 | Debug 的 `Core/cover-scale/no-dock.png`、`Core/map-occlusion/map-occlusion-pack12-0.png` |
| Viewer 冒烟 | 全部通过，退出码 0；地图等六类入口、静帧、菜单返回、配置和无效参数拒绝正常 | `obj/map-migration/viewer-smoke.log`、`tests/out/viewer-smoke/` |

Debug 检查集合：`map-resources,map-turret,cover-scale,map-occlusion,prop-combat,props,level-flow,path-cache,mines,actor-feedback,combat-feedback,boss,deathmatch-feedback,brother,player-death,profile-play,big-version,viewer-controls,progress,final-pack2,final-pack7,final-pack9,final-pack12`。Release 集合：`map-resources,map-turret,cover-scale,map-occlusion,prop-combat,level-flow,path-cache,combat-feedback,final-pack2,final-pack7,final-pack9,final-pack12`。

统一执行：`pwsh -NoProfile -File tests/run.ps1 -Configuration <配置> -Case <集合> -NoBuild`，脚本显式传入 `--mute`，存档和资源原件只读。没有运行全量截图基线。

Viewer 单独执行 `pwsh -NoProfile -File tests/viewer-smoke.ps1`，脚本显式传入 `--mute`；已核对 `map.png`。最后仅移动原注释到对应函数旁，并再次增量构建六个产物；功能回归之后未修改执行逻辑。`git diff --check` 通过，旧地图路径与包装类型引用扫描无残留。

### 既有失败与未验证边界

1. `props` 保留原先预期退出码 1：`pack9 PROP 43` 无法完整解析，其余 264 个模板解析、脚本回调检查通过。没有修改原资源、猜字段或新增假数据。
2. `campaign-doors` 在本轮改动前的 Release 和本轮 Debug 中均失败：门状态由 0 进入 1 后，玩家同样停在 `(1951.9,2641.9)`，目标 Y 为 2782。该既有战役入口问题没有在地图拆分中掩盖或修改敌人逻辑。证据：`baseline-doors.log`、`stage9-door-stdout.log`；最后一次正式回归不将它记作通过。
3. `combat-feedback` 原夹具同时生成 24 只怪，超过此前已对齐的默认活动上限 20；改动前 Release 也有相同 4 项生成失败。夹具现在按 `GetEnemyLimit()/kinds` 分配数量，保留两类怪物及声音去重断言；生产上限未变，两种配置现均通过。
4. 原始地图没有 Movie 图层样本。其读取依据源码与 BT，绘制仍未实现，遇到该层会明确记录日志；不将保留数据声称为恢复完整 Movie 地图播放。
5. `Resources`、`Props`、嵌套记录与 GL 缓存是桌面内部实现，不宣称恢复原二进制内存布局。原章节时间、Sprite、碰撞、粒子和声音仍来自 BIG。

## 原包装注释存档

以下保留被归并结构的原注释；其中重复播放器和预设状态描述已由本轮实现替代。

```cpp
/** One animation slot of a prop template, expanded step by step. */
// One quad list per animation step, so playback is a subscript rather than
// a walk back down the sprite tree. A template's steps run to a couple of
// dozen at most, and expanding them all costs a fraction of the archive
// read that got us the template in the first place.
// The same steps' durations, which is all a CSpritePlayer needs.
/** The three sprite layers and collision rule used by one prop state. */
/**
 * One prop template's three sprite slots, every step already expanded.
 *
 * Shared between instances: a map places a hundred props from a couple of
 * dozen templates, and expanding the same animation a hundred times would mean
 * a hundred archive reads for nothing. Instances differ only in where they
 * stand and how far into the animation they are, and both of those live on
 * PlacedProp.
 */
// What the walk could not draw, kept so the load can report a total
// rather than a line per template.
/** One prop standing on the map, each of its three slots playing its own. */
```

```cpp
    // Judged on the first step alone, which is what M3.1 sorted on before
    // anything animated. Playback moves what a slot draws but never whether it
    // draws, so keeping the test on step 0 keeps the draw order fixed for the
    // life of the map -- and lets the queue stay sorted once, at load.
    // That fixed order is now only the prop/animation table; the mixed actor
    // and prop main pass is sorted again each frame by DrawMapObjects.
    bool backgroundDraws = SlotDrawsAtStart(out.background);
    bool mainDraws = SlotDrawsAtStart(out.main);
    bool foregroundDraws = SlotDrawsAtStart(out.foreground);
    if (out.interactiveKind != ZInteractivePropKind::None) {
        backgroundDraws = SlotDrawsAtStart(out.states[0].background);
        mainDraws = SlotDrawsAtStart(out.states[0].main);
        foregroundDraws = SlotDrawsAtStart(out.states[0].foreground);
    }

    if (mainDraws) {
        out.zOrderGroup = kZGroupNormal;
    } else if (backgroundDraws) {
        out.zOrderGroup = kZGroupBackgroundOnly;
    } else if (foregroundDraws) {
        out.zOrderGroup = kZGroupForegroundOnly;
    } else {
        out.zOrderGroup = kZGroupNormal;
    }

```

此固定初始排序改为 CProp 当前动画查询与逐帧队列排序。

## 原内部聚合头注释存档

原注释保留于此；描述旧 M3 阶段、跳过图层或重复播放器的段落是历史记录，当前职责以上文及代码为准。

```cpp
/**
 * @file ZMapWorldInternal.h
 * @brief M3 milestone harness: a whole level, terrain and scenery, on screen.
 *
 * Two chains meet here. The terrain one:
 *
 *   ___GAME_TOC_KEYSET -> 33 section bases          (CGameObjectPack)
 *   TILELAYER base + n -> map resource              (CMap)
 *   map.tileSetRef     -> TILESET base + local      (TileSet)
 *   tileset.images[i]  -> PNG base + assetId        (CTexture)
 *   layer cells        -> quads                     (CQuadBatch)
 *
 * and the scenery one, which is where the rocks, pipes and portals live:
 *
 *   object layer       -> placed objects            (CLayerObject)
 *   PROP base + local  -> prop template             (CProp::Template)
 *   template.sprite    -> pack + archetype          (CGameSpriteGluRef)
 *   archetype          -> frames and atlas pages    (CSpriteGluArchetype)
 *   frame              -> quads                     (CSpriteIterator)
 *
 * Objects other than props are read but not drawn: enemies are 3D meshes,
 * particle effects need the particle system, and player tags are invisible.
 * Between them they are seven per cent of what a map places.
 *
 * `K` puts a marker on every one of them anyway. A spawn point has no art, so
 * the only way to tell one that is in the right place from one that is merely
 * plausible is to draw the coordinate the data gives next to the terrain it
 * belongs to. That is what M4a needs before it can put anything anywhere.
 */
// Internal map state and resource lifetimes shared by loading, rendering, and game sessions.
/**
 * The section bases and SpriteGlu tables of one pack, built on first use.
 *
 * Keyed by pack index rather than assumed, because references cross packs:
 * pack2's maps borrow five props from pack1.
 * Returns null when the pack cannot be addressed at all.
 */
/**
 * Read the resource a (pack hash, section, ordinal) triple names.
 *
 * Every reference in the data is one of these, and the pack hash is part of
 * the address -- CGunBros::GetGameObject picks the pack first and only then
 * adds the section base. Resolving an ordinal against the pack that happened
 * to hold the reference works right up until something points elsewhere, and
 * props already do.
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:78497
 */
/**
 * Run the scripts of whichever levels use this map.
 *
 * Scroll speeds are not in the map. A level script sets them when the level
 * starts -- CLevel::Init binds the script, binds the map, then calls export 0
 * (:120988) -- so getting them means running that script, which is what this
 * does. Levels are walked in section order and the ones naming this map are
 * bound and started; a map with no level, or a level that asks for no
 * scrolling, leaves every layer at rest. That is sixteen of the twenty-two.
 *
 * Every level template is parsed, not just the matching ones, and each is
 * checked for leftover bytes -- the whole-archive check that the script format
 * is read correctly.
 *
 * The CLevel is local because nothing outlives the call yet: the script runs
 * once, at load, and what it changes it changes in the map. M4a gives levels
 * a lifetime.
 */
/**
 * Load a map, its tile set and the atlases the tile set names.
 *
 * @param mapPackIndex Which pack the map itself lives in. Everything it
 *                     references is addressed by its own pack hash, not by
 *                     this one.
 */
/**
 * Expand every step of one animation into the quads that step draws.
 *
 * An unused slot -- animation 255, which is how a prop says it has no
 * foreground or no main sprite -- comes back empty, and so does one whose
 * animation is out of range. Neither is an error: most templates fill one slot
 * of the three, and a player pointed at an empty slot simply never ticks.
 */
/**
 * Whether any of a template's slots has more than one step to play.
 *
 * Most scenery is a single step and stands perfectly still, which is correct
 * and also indistinguishable from a broken clock. Counting the ones that can
 * move is what tells the two apart without staring at the window.
 */
/** Whether a slot draws anything on its first step. */
/** Whether a PROP reference names one of the two PvP barricade orientations. */
/** Build the known visual states named by the three original prop scripts. */
/**
 * Expand one prop template into the quads its three slots draw.
 *
 * This is CProp::Bind's job: it resolves the sprite reference to an archetype
 * and points three CSpritePlayers at three animations of it. The z-order group
 * falls out of which of the three ended up with an animation, exactly as
 * CProp::GetZOrderGroup computes it.
 */
/**
 * A repeatable stand-in for the Utility::Random in CProp::Bind.
 *
 * Bind starts the main slot on a random step so a field of identical rocks
 * does not pulse in unison. A real random would cost --screenshot its one
 * useful property, that two runs produce the same image, so this hashes the
 * prop's place in the draw order instead: scattered between neighbours, and
 * the same on every run.
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:124916
 *
 * The multiplier is Knuth's, and the shift drops the low bits, which move too
 * regularly between consecutive ordinals to scatter anything.
 */
/** Background slot selected by a prop's current cover state. */
/**
 * Point one placed prop's three players at their slots, as CProp::Bind does.
 *
 * All three loop forwards, which is CSpritePlayer's constructed state and
 * which Bind never changes. Only the main slot starts part-way in; the other
 * two begin at step 0, so a prop's foreground and background stay in step with
 * each other however its body is phased.
 */
/** Human-readable state for the keyboard diagnostic. */
/** Advance the shared cover state, wrapping hidden back to intact. */
/** Apply one state to every destructible cover on the current map. */
/** Apply one visual state to every prop of a scripted interactive kind. */
/** Resolve SoundEffect -> WAV, cache it, then play one batch transition cue. */
/** Parse and expand one original particle template the first time it is used. */
/** Queue one cached effect at a prop's world position. */
/** Original script resource indices and z groups for one state transition. */
/** Start each matching prop's script-authored particle cues. */
/** Advance emitters and their live particles. */
/** Select the looping sprite step belonging to a particle's current age. */
/** Emit live particle sprites in the requested z-order interval. */
/** The original disables both collision shapes in the destroyed state. */
/** Order props the way CRenderQueue does: by group, then down the screen. */
/**
 * Turn the map's object layers into drawable props.
 *
 * Objects of other types are counted and left alone. Nothing here fails the
 * load: a prop whose template or sprite will not resolve is dropped and
 * reported, because one bad rock should not cost the whole level.
 */
/** Move every prop's three players on by one frame's worth of time. */
/** Move every drifting tile layer on by one frame's worth of time. */
/**
 * The quads a slot draws at its player's current step.
 *
 * An empty slot -- an unused animation, or one that expanded to nothing --
 * lands on the out-of-range path and draws nothing, which is why the players
 * of empty slots never need a special case anywhere else.
 */
/** Emit one prop's slot, positioned at the prop and offset by each quad. */
/**
 * Turn the tile layers and the props into quads.
 *
 * Tiles walk the canvas rather than each layer's own extent, so a layer
 * smaller than the canvas repeats -- CLayerTile::GetCell does the wrapping.
 * Layers are emitted bottom first, matching CMap::DrawBackground's order.
 *
 * Then the props, in the order CRenderQueue::Draw produces: the queue is
 * sorted once and walked three times over, background slot for everything
 * first, then main, then foreground. That is why this cannot just draw each
 * prop's three slots together -- a prop's foreground has to land on top of the
 * NEXT prop's main sprite, not just its own.
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:145235
 *
 * This runs every frame now, because a prop's quads change as it animates. The
 * tiles do not, and could in principle be kept -- but the largest map in these
 * archives is 190 tile quads and 342 prop ones, so the whole rebuild is a few
 * hundred quads into a buffer designed to be refilled every frame. Splitting
 * the batch in two to save half of nothing would cost a second buffer and the
 * code that decides which half is stale.
 */
/**
 * Where a spawn marker goes, and how big.
 *
 * The size is the viewer's, not the data's: a spawn point is a single
 * coordinate, and it has to be big enough to find against a 256-pixel tile.
 */
// Depth kept by the map's projection. Terrain and props are flat at z = 0;
// this only has to be deep enough for the tallest placed model, and the
// biggest one in the archives is a couple of hundred world units.
/** Collect one outline per object of the given type, centred on its own x,y. */
/**
 * Assemble exactly the collision shapes CLayerCollision tests for a player.
 *
 * The level script selects one map collision layer. Static props then add
 * their template-local geometry at their placed position. Keeping this as one
 * scene lets the existing resolver choose the nearest edge across both kinds
 * instead of resolving each prop in an arbitrary order.
 */
/** Collect the exact collision scene used by player movement. */
/**
 * The camera scale a level snaps to. Reference: :120747, where CLevel does
 * `CCamera::SnapScale(camera, 0.8)` right after binding the map.
 *
 * The real camera moves it afterwards; until there is a real camera this is
 * the number the game starts every level with.
 */
/** Convert world anchors and native pixel sizes to the HUD's logical canvas. */
/** "3 player spawns, 41 enemy spawns" -- what the K overlay should show. */
/**
 * Load a model for every enemy the object layer places.
 *
 * **These are leftovers, not how the shipped game works.** Every shipped map
 * is survival: enemies arrive from off screen and close in, spawned by the
 * level rather than placed on it. The object layer's enemies are what was left
 * of a campaign mode, which is why most maps have none and the ones that do
 * cannot be checked against anything -- except pack9's two, which are turrets
 * and stand where they are placed.
 */
/** Move every placed enemy's animation on. */
/**
 * Stand a player on every spawn point the object layer names.
 *
 * The default model and nothing else: no weapon, no armour. Which gun a
 * player carries is a loadout question and the loadout is not read yet, so
 * putting one in his hand here would be inventing data.
 */
/** Swap equipment only after every referenced asset has loaded. */
/**
 * Draw every model the object layer places -- enemies and players alike.
 *
 * Depth on, and the depth buffer cleared first: the terrain and props are 2D
 * and drawn in order, so they neither read nor write depth, but a model is
 * solid and needs it against itself.
 */
/** CRenderQueue::Compare :145029: group, then integer world Y. */
/** Shared main/foreground passes for the game and permanent map viewers.
 * CRenderQueue::Draw :145123 puts actors and scenery in the SAME main pass.
 * The caller has already drawn tiles and every prop's background slot.
 */
/** How many objects of one type a map places. */
/** One map, wherever it lives. */
/**
 * Every map in every pack, as one flat list.
 *
 * Flat rather than grouped because a pack boundary is not something the viewer
 * should make anyone think about -- the packs are a packaging detail, and a map
 * reaches across them for its props anyway. Walking the list runs off the end
 * of one pack straight into the next.
 */
/** Slot of the first map of the pack `slot` belongs to. */
/**
 * Slot of the first map of the next pack, wrapping round the end.
 *
 * Left and right already cross pack boundaries one map at a time; this is the
 * shortcut for skipping a whole pack, which matters when pack2 alone holds
 * nine maps.
 */
/** Slot of the first map of the previous pack, wrapping round the start. */
/** The camera state the viewer manipulates. */
/**
 * The rectangle worth looking at, in world pixels.
 *
 * The map's camera layer when it has one, and the whole canvas when it does
 * not. Two of the twenty-two maps declare no camera layer.
 */
/** Zoom out far enough to see the whole viewed region, and centre it. */
/** Centre the fixed GameView camera on the controlled player. */
/** Keep the default stage's framing across maps; bounds only limit panning.
 * This viewer setting is deliberately independent of stage dimensions.
 */
/**
 * Where the camera bounds land on the window, as a scissor rectangle.
 *
 * A map's tile layers run past the rectangle the game is ever allowed to show:
 * a lava or starfield layer wraps and keeps filling to the edge of the canvas,
 * and the terrain layer above it stops short. The engine never reveals that
 * because the camera stops at these bounds. This viewer fits whole maps on
 * screen, so it has to clip instead.
 *
 * @param scissor Filled with x, y, width, height in window pixels, GL's
 *                bottom-left origin.
 * @return false when the map declares no camera layer; nothing is clipped then.
 */
/** Open the archives and pick out one pack, already bound. */
```
