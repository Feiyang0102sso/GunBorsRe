# UI 页面背景来源核对

> 本文保留一手证据与追加核对；主线程已据此实施背景归属、清屏色和共享游标修复，验收见 [UI 重组结果](ui-optimization-result.md)。

日期：2026-09-19。范围仅为 `ZMenuSurface::Begin` 统一绘制 Movie47@1600 与指定八类菜单的背景绑定；只读核对，不修改实现，不运行构建。

## 结论

应删除宿主对页面统一绘制 Movie47 的规则。在本次八类中，只有 **CMenuMissionInfo** 明确持有并绘制该 Movie，应由该菜单自行绘制，沿用星图当前播放时间。其余七类没有此背景调用；设置、炼油有各自的共享背景，其他页面绘制自己的主 Movie。不能因删除47后看见透明区域，就补一层未经原版证实的颜色或图片。

以下行号均指 `_prep/_IDA_OUT/gunbros_3.6.0_IOS.c`，不是恢复的原 `.cpp` 行号。Movie帧、内嵌对象和时长仍由 BIG 读取，格式依据 `_prep/_Big_tool/binary template/big_assets/ui_movie.bt`；本文列出的原生菜单描述符只是绑定关系，不是手写资源数据源。

| 原类 | 原背景/绘制顺序与函数证据 | 时钟证据 | 对统一47的处理 |
|---|---|---|---|
| CMenuList | `Init:140552` 从描述符+12绑定主 Movie（140603）；描述符+8为共享背景索引，12表示无背景（140640–140647）。`Draw:140362` 先绘制this+236背景，再this+232及控件。MENU_OPTIONS 实物索引9为 `GLU_MOVIE_BG_OPTIONS`，主资源为 `GLU_MOVIE_LIST_MENU`。 | `Update:140380` 在140397–140400推进可选背景，输入为帧delta；主Movie及控件有自身可见性门槛。 | 移除47，保留BG_OPTIONS自己的播放状态。 |
| CMenuFriends | `Init:197410` 在197446–197447由描述符+4绑定主Movie到this+12，+8/+12等是弹层/内容Movie（197493–197505）；未获取共享星图Movie。`Draw:196367` 在196388绘制主Movie，再按页面状态绘内容或弹层。 | `Update:196440` 的196487、196552按状态推进主Movie，196553推进内容Movie；196603推进弹层，均接收delta。 | 移除47。本类Draw没有FillScreen或clearColor。 |
| CMenuChallenges | `Init:237003` 在237036–237037由描述符+4绑定主Movie；237071–237072绑定弹层。`Draw:236173` 在236183绘主Movie，236188按状态绘弹层；未获取共享星图Movie。 | `Update:236194` 在236263推进主Movie(delta)，236219推进弹层(delta)；236264的另一内容Movie使用2×delta，不应共用固定游标。 | 移除47。本类Draw没有FillScreen或clearColor。 |
| CMenuPostGame | `Init:166022` 在166082–166083由描述符+4绑定主Movie到this+12，模式选择不同描述符。`Draw:165451` 仅在可见标志this+872成立时绘制该主Movie。 | `Update:165459` 在状态this+836不为4时，于165473推进主Movie(delta)。 | 移除47。本类Draw没有FillScreen或clearColor。 |
| CMenuGreeting | `Draw:208341` 在208343明确调用 `Utility::FillScreen(0,…)`；`DrawOverlay:208315` 在208326按可见状态绘制主Movie。`Init:208608` 在208626–208627从描述符+4绑定主Movie。底色与Overlay属于两阶段绘制。 | `Update:208347` 在208358按可见标志推进主Movie(delta)。 | 移除47，保留已证实的FillScreen背景及主Movie覆盖层。 |
| CMenuPlayerSelect | `Init:195247` 在195270–195271由描述符+4绑定主Movie到this+12。`Draw:195134` 只按可见标志绘制此主Movie。 | `Update:195142` 在195152按可见状态推进主Movie(delta)。 | 移除47。本类Draw没有FillScreen或clearColor。 |
| CMenuMissionInfo | `Init:189581` 在189710明确调用 `GetMovie(4)`，存入this+104并居中。`Draw:189166` 在189174先画该背景，189175再画主Movie。shared index4就是 `GLU_MOVIE_MAP_PARALAX_COPY`，BIG内类型序号47。 | `Update:189191` 在189211更新主Movie，在189219更新另一个Movie；不更新this+104背景。该背景与星图共享实例；`CMenuMission::Update` 在162646按delta×速度推进同一Movie。 | 应在本页显式画47，使用保留的 `state.starMap.starTime`，不固定1600，也不跟随本页main时钟推进。 |
| CMenuGameResources | `Init:172652` 在172704绑定主Movie；172809–172814读取描述符+8选共享背景，12表示无背景。`Draw:172363` 先画this+16背景，再按状态绘制主Movie。MENU_GAME_RESOURCES 实物索引10为 `GLU_MOVIE_EXPLODIUM_BG`，主资源为 `GLU_MOVIE_EXPLODIUM`。 | `Update:173264` 在173301–173303直接推进背景(delta)。 | 移除47，保留EXPLODIUM_BG自己的播放状态。 |

## 共享索引与原始字节复核

`CMenuSystem::GetMovie:96280` 将索引映射到从this+1324开始、每项204字节的共享Movie数组。`CMenuSystem::Init` 的绑定如下：

| shared index | 运行时成员 | 资源 | 原行号 |
|---|---|---|---|
| 4 | this+2140 | GLU_MOVIE_MAP_PARALAX_COPY | 97423–97424 |
| 9 | this+3160 | GLU_MOVIE_BG_OPTIONS | 97433–97434 |
| 10 | this+3364 | GLU_MOVIE_EXPLODIUM_BG | 97435–97436 |

反编译器在 CMenuList/CMenuGameResources 的虚调用处省略了GetMovie第二参数，不能把省略理解成索引0。已直接读取 `_prep/gunbros` ARMv7 Mach-O符号及映射字节：

| 描述符符号 | ARMv7 VA | 整个gunbros文件偏移 | 起始32位原值 |
|---|---|---|---|
| `__ZL12MENU_OPTIONS` | 0x402e50 | 0xb90e50 | `[0,193,9,3740289,3,2,1,1]` |
| `__ZL19MENU_GAME_RESOURCES` | 0x402d50 | 0xb90d50 | `[6,3740465,10,3740485,3740508,157,0,0]` |

OPTIONS+12的指针0x391281指向 `GLU_MOVIE_LIST_MENU`；GAME_RESOURCES+4的指针0x391331指向 `GLU_MOVIE_EXPLODIUM`。这两个描述符的+8分别为9和10，与消费者及共享数组一致。原件SHA256为 `987d78475fc2261c066884f91dccde18a4c3d2f5d8fe403e701f84687178a6cd`。读取使用 `extract_menu_statics.read_image()` 的只读段映射及Mach-O LC_SYMTAB符号，未执行该脚本生成入口。

## 验证边界

- `CMenuSystem::Draw:96761` 与 `CMenuStack::Draw:147959` 没有“普通页面统一画47”的规则。共享加载不等于所有页面均应绘制。
- Friends、Challenges、PostGame、PlayerSelect 本类不清屏、不画额外47；共用帧清屏由CGameApp提供。后续补核已确认明确黑色清屏，见下节，现有宿主蓝色不对应原版。
- MissionInfo沿用星图时钟的依据是同一共享Movie实例及本页不推进该实例；`starMap.starTime`是宿主对应状态，并非原符号。它须在切页时保留，不能被本页初始化清零。
- 推荐验证：八页逐一记录背景资源与时间；MissionInfo进入前后47时间相同；停留MissionInfo时只推进其自身动画；设置与炼油不叠加47；Greeting保留独立底色/Overlay顺序。无需为了本次背景修复扩大到其他功能。

## 补核：MissionInfo 不继承星球缩略图回调

结论：正常从星图退出、提交任务详情页后，**MissionInfo绘制无星球回调的Movie4是正确结果**。原版确实共享Movie实例，但星球回调在旧页清理时明确清除；不能仅凭共享实例推断缩略图也须继续显示。原符号叫 `CMenuMission::PlanetCallback`，不是PlanetImageCallback。

1. **绑定位置**：`CMenuMission::Init:163245–163246` 取得共享Movie；163253–163269对region1至末尾绑定 `PlanetCallback`，回调上下文为CMenuMission自身。region0是视口，不绑定星球。
2. **退出阶段仍保留回调以完成淡出**：`OnExit:162349` 的162353建立固定点插值 `0x10000→0`、时长 `0x15e=350ms`，162354停止星图时间推进速度；162363写入待转换状态6（this+44），并不是当场把alpha清零。`Update:162681–162685` 提交待转换状态，状态6在162693推进插值，162694–162695完成后进入state8。
3. **星球回调的可见性门槛**：`PlanetCallback:161457` 在161478要求loaded标记this+36成立，161482要求state(this+40)不为8。state0/6使用this+152插值（161487–161513）；普通星球按该值缩放及淡出，选中星球alpha为 `min(2×value,0x10000)`。因此350ms退出完成时所有星球alpha为0，并且state8直接跳过整个星球绘制。OnExit调用瞬间尚未归零。
4. **提交新页前清回调**：`CMenuStack::Update:147889` 先更新旧页（147908），待IsBusy返回false（147909–147910）后，147928–147930调用旧页CleanUp，再析构旧页并提升pending（147931–147938）。`CMenuMission::IsBusy:161015` 对state8返回false（161021–161026）。`CMenuMission::CleanUp:162984` 在163126–163129明确调用 `CMovie::ClearUserRegionCallbacks(sharedMovie)`，只清本页指针，未销毁共享Movie或重置其播放时间；163154清loaded标志。
5. **Hide路径同样不会留下悬空回调**：`CMenuStack::Hide:147876` 对活动页调用OnExit并设销毁标志；Update的147912–147920在退出完成后析构。`CMenuMission::~CMenuMission` 的163509–163510调用CleanUp，因此也会清回调。本次符号中未见独立的CMenuMission::Hide或Unbind；实际清理者是CleanUp/析构。
6. **详情页仅重新取共享背景**：`CMenuMissionInfo::Init:189710–189715` 取Movie4并居中，不重新安装星球回调；`Draw:189174` 绘背景，Update不推进背景。因此宿主 `DrawNamed("GLU_MOVIE_MAP_PARALAX_COPY", state.starMap.starTime)` 不传 `ZStarPlanetCallbacks` 符合新页已提交后的原绘制结果。

建议断言：退出请求发生当帧不立即清零星球alpha；退出期间旧页继续绘制星球并在350ms内淡出；到state8/提交详情页时星球不可见；详情页不产生星球region回调，背景时间仍为星图停止时的游标。宿主若尚未实现原350ms退出插值，应记录为退出动画待恢复，不能用给详情页继续传星球回调来补偿。

## 补核：菜单正常渲染帧明确清为不透明黑色

**已找到完整消费链，撤销之前“默认清屏色未知”的边界。** 原程序在游戏应用每个正常渲染帧显式设置 `(0,0,0,1)` 并清除颜色缓冲，然后才进入菜单绘制。这不是依据OpenGL默认值推断。

### 应用帧入口与菜单调用

- `CGameApp::HandleRender:54596`：54603–54604检查应用渲染标志；54606–54607检查Tapjoy offers未打开。下述结论限于经过该正常渲染入口的帧，不扩展到暂停或外部广告覆盖期间。
- 54609取得 `ICGraphics::GetInstance(0)`；54611–54616调用图形虚表+72，实参位型为 `0,0,0,1065353216`，最后一个即IEEE754的1.0。此槽已由原二进制核实为四分量SetClearColor。
- 54617调用虚表+48的 `ClearBuffers(458752)`，即原引擎标志 `0x70000`，包含颜色/深度/模板三个清除位。
- 54618–54628准备显示程序与栅格状态；54629调用 `CGunBros::Draw`；其78885–78896设置UI绘制颜色并进入 `CMenuSystem::Draw`。普通菜单状态跳过CGame::Draw，仍已执行上述帧清屏。
- 54630结束显示程序，54631调用SwapBuffers。此次证据链已经覆盖真正调用菜单的应用帧入口，无须依赖MainScreen尺寸辅助函数推断颜色。

### 设置、初始化与执行

| 环节 | 原函数与行号 | 确认内容 |
|---|---|---|
| 图形初始化 | `CGraphics_OGLES::Initialize:383221`，383541–383546 | 显式SetClearColor(0,0,0,1)、SetClearColorMask(1,1,1,1)、clearDepth=1、clearStencil=0，并关闭清屏裁剪；黑色也有初始化依据。 |
| 颜色写入 | 四分量 `SetClearColor:384205`，384216–384220；数组重载384225–384235 | RGBA存入图形变量表的Color项，不是立即调用GL。 |
| 清屏请求 | `ClearBuffers:384100`，384121–384134 | 保存请求标志；未使用延迟指令时384124直接调用InstrClearBuffers，否则记录同一清屏指令状态。 |
| 清屏颜色消费 | `InstrClearBuffers:386893`，387175–387216 | 检查原引擎颜色位0x10000，取Color四分量，设置GL_COLOR_BUFFER_BIT(0x4000)；状态改变或未初始化时实际调用glClearColor。相同颜色可命中状态缓存，但仍清颜色缓冲。 |
| 写掩码与其他缓冲 | 387230–387257、387259–387296、387319 | 读取ColorMask并设置glColorMask；0x20000转GL_DEPTH_BUFFER_BIT(0x100)，0x40000转GL_STENCIL_BUFFER_BIT(0x400)。 |
| 最终清除 | 387335–387355 | 请求非零时调用 `glClear(v34)`。因此0x70000实际产生含颜色位的清除，不能解释为只清深度或“不清色”。 |

ARMv7原件字节复核：虚表 `off_42D878`（文件偏移0xbbb878）与OGLES2虚表 `off_42D958`（0xbbb958）的+48均为0x284b74（ClearBuffers:384100），+52均为0x284c48（SwapBuffers:384140），+72均为0x284eac（四分量SetClearColor:384205），+76均为0x284f1c（数组重载:384225）。这消除了HandleRender匿名虚调用的槽位歧义。原件SHA256同上。

实现建议只涉及已问的清屏色：宿主菜单帧使用 `glClearColor(0,0,0,1)` 有明确原版依据；当前 `(0.063,0.114,0.176,1)` 应移除。原引擎同时请求模板清除，但Windows目标是否配置模板缓冲属于宿主适配，此处不据此要求扩大渲染改动。

## 补核：共享背景9/10的时钟生命周期

普通页面Bind/OnShow/CleanUp **不重置背景9/10的时间**。二者时间属于CMenuSystem的共享Movie实例，页面只在Update被调用时推进；页面离开后暂停，重新取同一实例时续播。由每类持久保存的 `backgroundElapsed` 对应此行为是可行的，但必须随整个菜单系统Reset归零；不可使用全局绝对时间，也不可每次Bind归零。

| 操作 | CMenuList背景9 | CMenuGameResources背景10 |
|---|---|---|
| Init | 140640–140654只取得背景this+236并居中，没有SetTime/ResetPlayback。 | 172807–172823取得this+16、Refresh并居中，没有SetTime/ResetPlayback。 |
| Bind | 140448–140489未操作背景；140476的Refresh对象为this+452，不是this+236。 | 172835起的Bind没有重置背景；172909刷新的是主Movie this+12。 |
| OnShow | 140353 SetTime(0)的对象为this+232主Movie，140354设置主Movie循环章节；不改变this+236。 | 173448–173450操作主Movie this+12的章节/播放标志；不改变this+16。 |
| OnExit/Reset | 140329–140336退出控制器和主Movie，Reset140439–140444重置控制器/可见性；未动共享背景。 | OnExit171917、Reset172980处理主Movie和计量器；Reset中的ResetPlayback属于计量器内部Movie，不是this+16。 |
| CleanUp | 140546只将this+236清空，不销毁或重置共享Movie。 | 172593只将this+16清空，不销毁或重置共享Movie。 |
| Update | 140398–140400：有背景就 `CMovie::Update(background,delta)`，独立于主Movie可见性分支。 | 173301–173303：有背景就 `CMovie::Update(background,delta)`。 |

`CMovie::Refresh:108538` 的108569–108572读取当前this+160/164时间传给对象刷新，并未将时间设零，因此炼油Init调用Refresh不能作为“重新进入从0播放”的依据。

真正清零发生在 `CMenuSystem::Reset:96989`：97005对this+3160（背景9）、97006对this+3364（背景10）调用ResetPlayback。该函数108307–108316明确将当前时间this+160与前一时间this+164写0，同时重置章节状态。

宿主若暂将Update合在Draw函数中，应确保每个实际更新帧只累计一次delta；额外重绘或绘制模态底页本身不应让背景多走一次。普通切页保留值有证据，完整系统重置仍须归零。页面退出动画期间只要旧页仍被Update，其背景也继续推进，不能在提出切页请求时就提前冻结。
