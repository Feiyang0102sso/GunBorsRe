# 激光、弹体外观与商店筛选修复

## 范围、方案与验收

用户授权修复全部激光的断射/误阻挡、Kraken 大激光与细尾迹、默认步枪颜色及筛选后的切枪按钮。本阶段不改原 BIG、解包样本或真实存档。

按优先级验证：射线查询错误引入弹体半径；Sprite action 替换未执行；Ribbon native 遗漏；射程被误作计时器。第二项经实际像素复测排除，相关探索性改动已撤回；颜色的直接原因是第三项。原版消费和实际资源共同决定修复，不按武器 ID 改色或填特效参数。

- [x] 激光按 `CBullet::UpdateBeam` :61996 / `CLevel::RayCastNearest` :115580 查询，不扩大目标半径；核对松手、续射、实际命中。地图墙仍由原有线段查询处理，本轮未新增独立墙形状专项。
- [x] 默认步枪颜色来源定位：BULLET `00267585:45`，`pack5_xga_0049_0x4e9a.bin`，脚本偏移0x3B调用native12(30,20,10)，0x45调用native13(255,130,0,255)。基础精灵本来为蓝色；不是给蓝色图片乘RGB变黄，而是恢复独立橙黄Ribbon。Sprite action机制未在本轮修改。
- [x] `CBullet::SetRibbonTrail` :60520、native12/13 :61307 和 `TrailEffectHolder` :294891 恢复尾迹采样、颜色及消退。
- [x] native18 :61342 改为射程；默认3000来自 `CBullet::Bind` :63673。
- [x] `CMenuStore::RefreshCategoryContent` :179165 按分类显示切枪按钮，筛选后仍可点击、切槽和保存。
- [x] Release正式/研究构建，专项红绿验证、武器/商店/实战回归，记录边界。

BT：`entries/bullet_template.bt`、`entries/common.bt`、`flow_bytecode.bt`、`flow_native_sources.bt`、`sprite_global.bt`、`sprite_archetype.bt`、`ui_movie.bt`。均位于 `_Big_tool/binary template/big_assets/`。

## 修复前复现

`gun_bros_research.exe --mute --weapon-effects-check`，日志 `out/weapon-effects-before.log`，退出1：默认步枪上方弹体像素黄色0、蓝色826；六个激光/组合武器测试均撞到射线旁50单位的测试目标，共7项失败。Kraken 实际 BULLET `00267585:87` 半径345，Infinity `:85` 半径75，Tuning Fork `:92` 半径240，Retinator `:125` 半径149。半径来自BIG而非测试造数；原版射线查询不将其加入目标形状。

`--mute --store-card-check`，日志 `out/weapon-bug-store-before.log`，退出1：GUNS筛选后按钮进入隐藏phase1。旧回归错误地把筛选当作切换商店分类，现按用户实际步骤补断言。

原范围参数错误已由原代码确认，但本次已测激光的初始脚本未调用native18；不能把它说成本次全部误阻挡的直接根因。

第二个激光红态：`out/weapon-close-before.log`，退出1。正前方目标距枪口11单位、目标半径10，六组激光都出现“有弹体、零绘制quad”。现有代码仅在长度大于两端半长之和时画端点，而原版 `CBullet::Draw` :62998/:63026 无条件画端点；非正中段只跳过中段拼接。

## 实现与来源

- `CBullet::FunctionResolver`消费原BULLET脚本的native12/13，记录点数、宽度、采样间隔与RGBA；`WeaponEffects`在弹体更新后采样，销毁弹体时保留尾迹逐点消退。`TrailEffectHolder::Update` :294891 的严格计时边界和每tick至多一次采样保留。
- `CRibbonTrailEffect::Draw` :243677规定至少3点；构造函数 :243887、`CMeshLine::GetUpVector` :243326、`Update` :243392 和 `InsertVertex` :242742决定双条带、半宽、法向和透明边缘。宿主 `CQuadBatch::AddGradientQuad` 与shader恢复RGBA插值；纯色纹理只承载原脚本颜色，不是另一份资源表。
- Kraken导弹 `00267585:86`，样本 `pack5_xga_0090_0x5ddb.bin`：0x68 native12(30,10,60)，0x72 native13(255,130,0,255)。Gold Kraken导弹 `:124` / `pack5_xga_0128_0x711f.bin` 同样宽10、间隔60，颜色由其原脚本条件决定。默认步枪是宽20、间隔10，不能用Kraken参数代替。
- `WeaponEffects::Update`仅对beam向 `IProjectileWorld::Trace`传半径0；普通弹仍使用BIG半径。射程读 `CBullet::maximumBeamLength`。`Draw`始终画beam端点；中段长度非正时按原函数让末端使用源pivot。
- `GameFrontEnd::DrawStoreGunSwap`从判断 `shopFilter==0` 改成 `shopCategory==0`。原源码 :179165 判断商店分类成员mem700；筛选条件不是隐藏按钮的依据。回归在已拥有筛选下点击真实Movie按钮，经PLAYER Flow切槽并保存重载。

正式路径不含按武器ID补色、补半径或补尾迹参数的分支。研究入口75 / `--weapon-effects-check`永久保留，`test-muted.ps1 -Phase Core`已加入该检查。

## 回归结果

命令从项目根执行。研究程序为 `bin/x64/Release/gun_bros_research.exe`，所有运行传 `--mute`，存档检查使用各检查在out内建立的隔离账户。

| 检查 | 命令参数 | 结果与日志 |
|---|---|---|
| 正式/研究Release | MSBuild `gun_bro_re.slnx /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo` | 退出0，`out/weapon-bug-final-build.log` |
| 激光/尾迹专项 | `--mute --weapon-effects-check` | 退出0，7把武器、6组beam；`out/weapon-fix-weapon-effects-check.log` |
| 全武器 | `--mute --weapon-check` | 退出0，76模板、3个纯视觉条目；`out/weapon-fix-weapon-check.log` |
| 敌人与碰撞契约 | `--mute --arena-check` | 退出0，78条目、76脚本、unknown_calls=0；`out/weapon-fix-arena-check.log` |
| 存档续玩 | `--mute --profile-play-check` | 退出0，从第2波续玩到第4波，XP101、XPlo90；`out/weapon-fix-profile-play-check.log` |
| Kraken/Haven | `--mute --survival-check --map pack7 6 --weapon 48 --check-waves 2` | 退出0，2波、35生成/35击杀；`out/weapon-fix-kraken-haven.log` |
| Infinity/首星球 | `--mute --survival-check --map pack2 7 --weapon 47 --check-waves 2` | 退出0，2波、35生成/35击杀；`out/weapon-fix-infinity-survival.log` |
| 商店完整卡片 | `--mute --store-card-check` | 退出0，772模板、筛选后真实按钮/PLAYER Flow/活动槽保存重载均通过；`out/weapon-fix-store-card-check.log` |
| 永久研究菜单 | `'75'`通过标准输入传给 `--mute --research` | 退出0，实际进入专项并全部通过；`out/weapon-effects-research-menu-check.log` |

菜单接入复核修正了与原入口73的编号重复，并将合法编号上限74更新为75；原73/74分派保留。首次菜单启动命令遗漏 `--research`、第二次因旧上限回到默认菜单，均停止，未执行购买或装备操作；这两次启动不计入隔离存档检查。

专项持续射击6秒，六组beam全部 `off-axis-beam-contacts=0`，Infinity逐帧连续性断言 `missing-held-frames=0`；其余武器验证光束确实发射、误阻挡和近距离绘制，Kraken保留原预热/间隔，不以无限连续光束验收。正前方目标仍将光束截到90单位，贴近目标截到1单位时绘制quad仍非零；松手后弹体与Ribbon均清空。另直接验证native18只改射程、不改寿命。步枪真实OpenGL截图中橙黄像素2383、蓝色229，黄色尾迹已经恢复；最初红态像素数使用未开启混合的夹具，只用于记录“无黄色”，不将其蓝色计数与最终值作为严格同条件比率比较。

目视截图：`out/weapon-effects-5-1008.png`（默认步枪）、`out/weapon-effects-48-1008.png`（Kraken细尾迹）、`out/weapon-effects-close-47.png`（近距离Infinity）；Haven自动实战截图为 `out/survival-check-pack7.png`，其中Kraken导弹弯曲橙色细尾迹可见。实战检查自动保存此固定文件名，传入的截图参数不改变该检查输出路径。

## 验证边界

本轮修复的是激光共用碰撞/绘制和所有调用native12/13的弹体共用尾迹链，不宣称全部武器逐帧对应iOS。七把专项覆盖默认步枪、Infinity、Tuning Fork、Retinator、两把Kraken及目录中的另一把激光模板；76把检查验证资源、动作和发射流程。原native15/16相关LightningArc几何仍未完整恢复，未以Ribbon替代。自动实战使用现有驾驶器，不等于人工难度体验或全波次验收。
