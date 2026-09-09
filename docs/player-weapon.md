# 玩家与武器渲染

## 操作

主菜单第 15 项 `player weapon`，或命令行 `--player-weapon [目录索引]`。
GameView 使用相同的装备、动作与发射逻辑。

| 按键 | 功能 |
|---|---|
| 1 / 2 / 3 / 4 / 5 / 6 / 7 | pistol / rifle / shotgun / spread / heavy / special / laser |
| 8 / 9 | 预留 |
| N / M | 当前分类的上一把 / 下一把，首尾循环 |
| 鼠标左键 | GameView 按住开火；player weapon 拖动旋转 |
| F | player weapon 按住开火，松开停火 |
| WASD | GameView 移动；player weapon 播放行走动作 |
| 鼠标移动 | GameView 瞄准 |
| 滚轮 | player weapon 缩放 |
| 空格 / 句点 | 暂停 / 单步 |

E 保留油桶调试功能。窗口标题显示分类、类内序号及武器名。

## 数据关系

商店条目、武器本体、玩家动作分开解析，再由引用和装备脚本连接：

1. `CStoreItem`（Section 23）提供名称、价格、图标和枪械对象引用。组合礼包不覆盖单把武器的名称。
2. `CGun::Template`（Section 7）提供分类、武器 mesh/atlas、默认弹体、射击间隔、脚本、数值表与玩家动作覆盖表。
3. 装备时执行 `CGun::OnEquip` 的 export 0，native 6 将玩家的语义动作替换为枪械 moveset 中的动作。
4. `CBrother` 的状态机分别驱动上身与腿部；武器自身的 mesh 动画使用独立时钟。
5. 持枪字段 `0 / 1 / 2` 对应右手 / 左手 / 双手。玩家 mesh 的挂点分别为 4 / 2；双持绘制两份武器。

**武器分类不决定持枪姿态。** 手枪与长枪可能共用同一玩家 mesh，但使用不同帧范围。
旧查看器未执行装备脚本，又只绘制右手武器，因此默认手枪一直保持长枪动作。

本体 wire 顺序见 `CGun.h`：分类字节 → mesh / image / bullet 引用 → 射击间隔和其他标量 →
持枪字节 → `CScript` → 六组数值表 → `CMoveSetMesh`。其中仍未查明语义的标量保留原运行时偏移命名。
不能把截图中商店条目的固定 0xDD 长度套用到武器本体；本体包含变长脚本和 moveset。

核对依据为本地 `_IDA_OUT/gunbros_3.6.0_IOS.c`：
`CGun::Template::Init`（127712）、`CGun::OnEquip`（128529）、
`CGun::FunctionResolver`（128186）、`CBrother::Draw`（134780）、
`CBrother::SetMove`（138370）、`CStoreItem::Init`（159834）。

## 覆盖与边界

当前 3.6.0 资源共 76 个枪械条目，73 个具备发射所需数据。
`pack5 gun 60` 的默认弹体引用为空，脚本资源只有音效；`pack5 gun 61 / 62` 没有发射脚本。
这三个条目仍可查看模型，标题标为 `visual only`。

玩家姿态、左右手挂点、机械 mesh 动画、脚本射击节奏、发热叠色、枪声和循环音效已接入。
发射预览读取实际弹体精灵、mesh 与粒子资源。GameView 使用关卡脚本 native 6 选择的子弹碰撞层，
并加入 prop 的 bullet collision；玩家移动边界（例如熔岩岸边）不会自动阻挡普通子弹。
原版标志 0x20 的特殊弹体仍选择地形碰撞层。

弹体目前是可视运行层：已支持随弹体移动的粒子发射器；特殊闪电几何、Ribbon 尾迹、追踪与反弹轨迹尚未完整还原。
伤害、敌人死亡和弹体生成敌人也不在本次实现范围。
因此这里只完成装备与开火预览，不将 M5 战斗里程碑标为完成。

## 本次反馈修正

- 查看器与 GameView 共用世界单位，转台最后投影枪口、弹体与粒子，缩放和旋转不再改变模拟速度或散射角。
- rifle 4 / spread 1 的弹图使用图集 90 度打包标志；现在还原 UV 方向并交换宽高。
- 粒子字段为动画位掩码，`CParticle::Spawn` 调用 `Utility::RandomBit`，并非随机种子。
  The Kraken 的两个爆炸发射器选择动画 4 / 5，之前误画成同图集中的激光动画 0 / 1。
- 零发射间隔按 `CParticleEffectPlayer::UpdateEmitters`（131393）每次更新只发射一次，
  修复霰弹枪被错误按每毫秒补发而大量叠亮的问题；图集透明度与粒子淡出同时参与加色混合。
- 光束按完整动画帧的边界拼接，修正起止端、避免重叠，UV 使用半像素内缩避免采到图集分隔线。
- Pepper Grinder（pack5 gun 56，原 pistol 9）按用户核对归入 shotgun；
  pack5 gun 58 / 63 标注 `unused / unconfirmed`，保留原资源与模板持枪动作。
- 加特林过热后松开并立即重按，仍需等待脚本冷却结束；回归已验证不能绕过冷却。
- 慢速武器的扳机输入与当前攻击状态分开保存，按 `CBrother::OnShoot`（135928）和
  `UpdateNormal`（138357）处理脚本装填／冷却：普通弹体武器清除 ready 标志后退出攻击，
  仍按住扳机时等 ready 恢复再进入攻击；持续光束保留原版例外。
  修复霰弹枪、RPB 火箭在冷却期间重复攻击动作、枪声先于实际弹体和枪口火焰的问题。
- 发射声音有三条来源：枪械脚本、弹体脚本、玩家动作帧。补齐 `CMoveSetMesh::GetSound`（123129）
  和 `CBrother::UpdateAnimation`（137451）所用的动作帧声音，引用的是直接 WAV 索引，不能当作 SoundEffect 模板。
  手枪、霰弹、RPB11 / RPB35 / RPB88 及本次列出的 special 已通过站立无碰撞发射检查。
  之前只看枪械脚本便认为 RPB 系列没有发射声，结论不完整，现已纠正。
- 喷火器的无限尾迹发射器在两次发射之间保持存活，跟随弹体沿前方持续生成火焰；
  子弹移除后停止生成，已有火焰按各自寿命消退。
- 按 `CLevel::UpdateNormal`（121334）让新生弹体在当帧更新，按 `CBullet::Update`（63502）恢复弹体动画时钟。
  `CBullet::GetZOrder`（60280）规定玩家附近 100 单位内的弹体先于玩家绘制，避免加特林弹迹覆盖枪身。
- 弹体 mesh 按 `CBullet::Draw`（62782）补乘与玩家一致的相机缩放系数，修正 Cataclysm X4 导弹偏大。
- GameView 固定使用 572×429 世界单位的参考视野，各地图只限制相机中心，不再按地图尺寸改变人物大小。
  这是以当前默认首图为参照的查看器设置，不宣称是原版常量。

视觉修正通过截图检查；尚未与原游戏逐帧录像对齐，因此不宣称全部特殊武器的特效已经完全复刻。

## 代码边界

`gun_bros/CGun`、`CBullet`、`CBrother` 保留依据反编译还原的模板与脚本行为。
`gun_bros/WeaponEffects` 是新增的游戏层整合适配类，接合弹体模拟、粒子发射与武器声音，原版没有这个同名类。
原版 `src/gunbros/` 包含 `bullet.cpp`、`particleEffect.cpp`、`particleSystem.cpp`、`soundQueue.cpp`，
见 `_IDA_OUT/source_tree.md`。这些逻辑属于游戏层；通用 mesh/quad 绘制和 WAV 播放由 `engine` 后端负责。
`runtime/PlayerModel`、`WeaponCatalog` 和 `PackTables` 保留查看器与 GameView 共用的装配及资源访问适配。
`milestones` 中保留窗口入口、资源调查和回归检查；现有地图查看器仍在其中，本次没有重构整个地图系统。

## 回归检查

```powershell
.\bin\x64\Release\gun_bros_re.exe --weapon-check
.\bin\x64\Release\gun_bros_re.exe --player-weapon 0 --fire --advance 800 --screenshot out/pistol.png
.\bin\x64\Release\gun_bros_re.exe --gameview --weapon 47 --fire --advance 800 --screenshot out/laser.png
```

`--weapon-check` 逐一加载全部条目的实际模型，验证默认手枪使用装备动作覆盖和不同的双手挂点，
执行站立、行走、边走边射、站立射击、停火、再次射击及清理流程，并检查 GL 错误。
同时验证七个分类键、N/M 首尾循环，以及 E、8、9 不切换武器，
并覆盖粒子动画掩码、霰弹枪零间隔发射数量、两把喷火器持续尾迹、加特林冷却、前后绘制分层与分类修正。
无声武器另做站立、无碰撞的 160 毫秒发射检查，只有 WAV 解码且 SDL 成功排入播放队列才计数；
霰弹 1–4 和 RPB 火箭另检查持续按住 3.2 秒时攻击声不领先弹体、冷却后自动续射，以及松开后 1.6 秒内不再续射。
不会由脚步或之后的爆炸声音替代发射声。这验证播放调用路径，不等于已经进行耳听或原版音量比对。
这覆盖资源与运行逻辑，不替代逐把与原游戏录像进行视觉比对。

## 2026-09-09 原资源目录与UI模型更正

- 原武器分类现由对应STORE条目取得，删除pack5 ordinal56分类特判，以及58/63的unused判定。没有STORE引用只说明当前目录未出售，不能直接定性为废案；以上历史分类描述据此更正。
- 商店模型使用CBrother::SpawnForUI export9、UpdateUI及BIG PLAYER/GUN动作覆盖。CMesh按原IsFrameUsedInMoves保留帧，包围盒来自原首个保留帧；DrawUI按当前躯干Z范围和原区域高度计算比例，整页投影不再用区域裁切武器。
- CMenuMeshPlayer action92只发OnSwapGun；原PLAYER Flow控制收枪和native3切槽。两枪资源保持存活，同一兄弟解释器不重建；切槽时保留旧躯干动作，下一序列才读取新枪覆盖。双向三次交换、实际原装备、活动槽保存重载均已验证。
- UpdateUI直接将动作速度乘毫秒后截断，区别于普通MoveSet控制器的四舍五入；原腿部仍同步前一时刻躯干相位。UI动作帧音效尚未完整接入声音队列。
- 已移除小视口裁切和经验比例，不代表所有姿态已解释。实际原存档大枪仍存在遮脸观感，尚无依据支持额外旋转或缩小，继续保留原数值核对。
- 地图主体迁移runtime/MapScene、敌人模型迁移runtime/EnemyModel，milestones保留薄研究入口；正式GUI不依赖研究程序和自动驾驶器。现有播放器、装备、武器研究入口永久保留。
