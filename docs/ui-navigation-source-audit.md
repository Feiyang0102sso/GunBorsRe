# UI 导航提交与地图路由核对

> 研究快照：本文中的“当前实现”和旧路径对应调查时点。相关修复已实施，最终归属、仍未恢复的部分及验收以 [UI 重组结果](ui-optimization-result.md) 为准。

日期：2026-09-19。仅研究指定的 CMenuStack/CMenuSystem/ZMenuSession 和 kPlanetPacks/kPlanetMaps；未修改实现/测试，未构建。原行号指 `_prep/_IDA_OUT/gunbros_3.6.0_IOS.c`。当前文件正由主线程迁移，以下按函数而非临时行号描述。

## 一、CMenuStack 最小准确生命周期

### 原版证据

| 原函数 | 明确行为 |
|---|---|
| SetMenu 147654 | 先释放上一个 pending；创建目标菜单并 Init/恢复选择参数，记录为 this+108 的 pending。this+104 的 active 存在且要求动画时调用旧页 OnExit；若调用者明确不要退出动画则释放 active。不会直接把 pending 变成 active。 |
| PushMenu 147695 | 保存旧活动菜单 GetSelectedIndex 到历史项，再加深栈并调用 SetMenu；历史不仅是页号，还包含配置、选择状态和参数。 |
| PopMenu 147711 | 从上一历史项取配置、选择状态和参数交给 SetMenu；不是仅把当前页面整数减回去。 |
| Hide 147876 | 调旧页 OnExit，设置隐藏待完成标志125；等待后续 Update。 |
| Update 147889 | 更新 popup并协调输入；在活动页已加载后更新活动页，再检查其 IsBusy。忙则保留旧 active。空闲且hide请求时释放active并清hide标志；空闲且pending存在时，先旧页 CleanUp(next配置)、再释放旧页、提升pending为active并调用 LoadMenu。无active且有pending可直接提升并LoadMenu。 |
| LoadMenu 148246 | 活动页未加载则提交资源Load；加载器完成后 BindMenu。资源未就绪不应开放目标页面输入。 |
| BindMenu 147785 | 根据 IsLoaded 分支刷新已加载菜单，或 Bind 并在栈有焦点时 OnShow；不是单纯设置一个bound布尔值。 |
| IsBusy 148107 / IsLoading 148120 | 忙状态委托活动页；加载中另行表达，pending存在但active为空也算loading。 |
| Draw 147959 | 只绘制active页面。不能因请求在绘制途中出现就绘制pending页面。 |

运行时偏移104/108/124/125/126仅用于解释反编译，不能当作磁盘格式。

### 当前实现的准确差距

`system/CMenuStack.h` 的 Push/Pop 是立即修改page的调用者历史容器；`CMenuSystem::Navigate` 提前清历史、重置目标页面状态并调用Push。它还不是原CMenuStack生命周期的完整恢复。

`host/ZMenuSession.cpp` 在一个帧中有多段独立 `if (state.stack.page == ...)`。若前一页的Draw中Navigate立即改变page，后面的条件可能继续画目标页；header、popup、WIPE也可能观察到与帧初不同的page。商店业务和页面准备状态不宜在这种途中变化下执行。

现有欢迎页自行延迟退出：请求设置greetingExitRequested，Draw完成奖励提交并倒放，达到0再Navigate。炼油也保留refineryExitPending并等header准备完成。这些是已有行为，应接入统一pending出口，不能直接删除后改成瞬间切页。

### 推荐最小状态与提交时机

保留activePage和已有历史，增加一项pending请求，至少携带目标、Push/Pop/Root种类、恢复参数及是否已经启动退出。pending记录须足够完成原有返回状态；当前optionsReturnFocus/Scroll可暂保留，之后再统一历史项。

1. **请求阶段**：Navigate/Back只记录请求。对旧active调用一次OnExit；进入退出阶段后阻止新的页面导航输入，但继续更新其Movie、模型及效果。请求时不要清除旧页仍使用的状态、history或target绑定标志。
2. **退出阶段**：继续画旧active，按页面自己的IsBusy等待。不能统一写成固定等待N毫秒，更不能只等全局WIPE结束来代替所有页面退出。
3. **提交阶段**：旧页更新之后且IsBusy为假、当前资源/模态条件允许时，集中执行旧页清理、history变更、目标active提升、Load/Bind/OnShow与目标状态复位。每次pending只能提交一次。
4. **目标就绪阶段**：加载完成后开放输入；同帧已有的释放事件不得落到新页面。

理想顺序是`输入请求 → 旧页Update → pending提交 → 新active Draw`，与原Update后Draw一致。当前Update与Draw混合，最小安全迁移可以先用**统一帧边界**：本帧旧页完成更新/绘制并记下退出完成；下一帧Begin前提交，再冻结本帧page快照用于分派。这个一帧边界是宿主过渡实现，不能声称与原版完全等时。不要在独立页面Draw内部变更active。

针对现有ShowGameMenu：

- `previousPage`应保留提交前active，以便跨分支转场准确记录；提交之后再确定本帧唯一的绘制页。
- header目标、页面分派、截图trace和WIPE源/目标要观察同一次提交；避免已经改page但仍以旧画面采集目标帧。
- `masteryFrame` 当前临时修改stack.page来画底页，建议改成局部renderPage，不要触碰真正active，也不能把临时底页变化当作导航请求。
- 欢迎页倒放到0改为报告退出完成，奖励仍只提交一次；提交器使用原pending目标和root参数，不让欢迎页再次Navigate覆盖历史语义。
- 炼油现有header入口完成门槛仍有单独依据，保留在页面退出/转场协调逻辑中；不要把它套到所有页面。

### 页面退出不能简化成一个通用布尔值

- `CMenuStore::OnExit:179351` 倒放STORE_MENU chapter1、隐藏滚动控制和全部卡片、倒放/停止筛选章节、隐藏换枪按钮、隐藏角色模型。
- `CMenuStore::IsBusy:179280` 检查邀请/奖励子窗、滚动Movie控制、筛选栏状态与玩家模型。仅检查卡片closing或取消按钮不等价。
- `CMenuGreeting::OnExit:208272` 设chapter1并倒放、触发action95奖励、隐藏按钮；`IsBusy:207863` 对加载/可见标志和Movie章/时间作判定。当前等待倒放到0已有源码依据。

这批可先为已恢复页面接准确OnExit/IsBusy；未知页面明确保留适配标记，不编造退出时长。

### 栈内切页与分支WIPE是两层

原`CMenuSystem::SetBranch:96614` 有当前分支318、待切分支319、导航条和system状态门槛；旧分支UnFocus、目标PrepForFocus，并将WIPE时间归零。`Update:96926–96966` 更新旧/新分支，在目标不loading时推进WIPE，完成后才将319提交到318并Focus目标。

因此不能用一份立即page/history切换加“画一个WIPE”宣称完整还原原分支系统。当前宿主可继续用MenuBranchPage识别哪些导航需要WIPE，但应说明这是保留原时序约束的适配；同分支pending退出和跨分支WIPE分别完成。

### 最小验收

- Navigate后旧active不立即变化；OnExit只发生一次，busy期间Draw仍是旧页。
- 退出完成后只提交一次；新页输入须等资源就绪，原点击不穿透。
- 欢迎奖励不因重复pending提交；Back恢复选中项和滚动；Root只在成功提交时清历史。
- 单帧页面trace只有一个active；modal底页绘制不改变active。
- 跨分支WIPE仍从0开始，冷加载耗时不使首帧跳过；同分支不额外制造WIPE。

## 二、kPlanetPacks / kPlanetMaps 的实际范围

定义在`ui/host/ZMenuTypes.h`：kPlanetPacks为pack2/7/9/12/11/1，kPlanetMaps为7/6/0/0/0。全src/tests检索只发现`ZGameFrontEnd.cpp`正式循环末尾的两次读取。

它们只用于`profile.nativeArchive == nullptr`的显式legacy `.dat`研究profile分支。前端默认和`--original-profile`经LoadProfile读取原编号存档；此路径不使用这两个数组。不能把这两项描述为当前正式native游戏的地图来源。

choice5教程和choice4 BOKOR在上述分支前已经单独处理并continue，因此不能仅凭两个数组长度不同就断言这里已经发生越界。显式.dat路径中的星图/任务页又受到nativeArchive检查，不据本次静态研究推断该历史回退在所有UI入口都可达或全部不可达。

正式native路径当前已经：

`nativeArchive.survivalLevels[choice] → ReadSectionResource(Level) → CLevel::Template::Init → mapRef → GetPackName(hash)/localIndex`。

`survivalLevels`在`data/ZProfileStorage.cpp:380–416`由BIG派生：按所有PLANET遍历mission列表，筛Mission.type=1，按Planet.mapSlot排序，为每个零售星球保留唯一level。重复且不一致level会失败，不造数。这里choice是剔除教程/BOKOR后的四项序号，**不是直接原mapSlot**。

### 可替代方式

可以让legacy研究入口也调用同一资源派生链，无需伪造nativeArchive或转写存档：

1. 抽取/复用现有“PLANET→Mission.type1→按mapSlot排序→唯一Level引用”的只读查询。
2. 以既有choice解析Level，再读取CLevel::Template.mapRef；正常native路径可继续使用已核对的survivalLevels，也可与共享查询核对一致性。
3. 删除两个数组及通用ZMenuTypes中的地图事实。若要保留旧研究地图差异，改为调用者显式指定研究资源引用并明确研究入口；不要静默继续假定某个planetIndex对应硬编码pack/map。

不能直接用`LoadPlanetCatalog()[choice]`第一条mission替换：该目录含教程/BOKOR，排序及过滤与四项零售生存列表不同。需要Mission.type1和mapSlot规则。

原证据：`Planet::Init:169935`读取missions；`CMenuMission::Bind:162721`按mem+80 mapSlot布置星图；`Mission::Init:164402`读取level；`CLevel::Template::Init:114770`首先读取map引用。已阅读`entries/planet_entry.bt`、`mission_entry.bt`、`level_template.bt`。这些原资源已经携带所需关系，完全不需要把数组换成JSON。

验收：对四个零售choice比较共享资源查询与nativeArchive现有level/map一致；显示未知或有多个不同生存level时报告原引用；legacy研究入口不再读硬编码数组；保留已有独立研究入口。此次没有执行这些验收。
