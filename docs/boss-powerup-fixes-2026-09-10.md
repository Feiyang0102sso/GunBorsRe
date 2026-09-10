# Boss 技能与道具修复（2026-09-10）

## 范围与验收

用户授权修复 Boss 技能异常、核查 REV10 生命与破甲秒杀、左右道具闪烁、装备持久化及紧急购买拖动。阶段任务：核对 BIG/BT/原消费者；复现；实现确定偏差；通过实际资源回归；记录原版行为和验证边界。原版资源和实物存档只读，测试使用 out 副本，全部传入 `--mute`。

## Boss 光束

- pack7 机械 Boss 为 `pack6 ENEMY:8`，Flow `pack6_xga_0014_0x1316.bin` 的状态 12/13 发射 `pack5 BULLET:104`，分别经 native69 的节点方向与 native19 的显式方向。
- 原 `CEnemy::FireBullet :71190` 调用 `CBullet::Configure :62250` 时没有跟随回调。`CBullet::UpdateBeam :61996` 只在 mem+416 有回调时更新发射点及方向。旧实现却逐帧将所有敌人光束绑定回模型节点，覆盖了显式方向。
- 修复后只有枪发射的光束跟随枪口；敌人光束保留发射时的位置和方向。专项将模型节点移动到 (300,400)、转向73°，敌人光束仍保持 (50,300)、0°。修前检查退出1，修后退出0。
- `CBullet.native15` 原先为空，现接回 `CLightningArc`：原 `SetLightning :60480` 参数缩放、`GenerateArc :243259` 递归、`CMeshLine` mode2 边顶点、20ms 插值、`CBullet::Draw :63049` 分段长度适配。参数均来自弹体 Flow，未按 Boss ID 编造效果。
- `pack5 BULLET:104` 的 Sprite 引用确实是 archetype139/animation1，绿色点状底图来自原资源；新增白色电弧叠在原底图上。原 iOS `FunctionResolver :61101` 没有 native16 分支，因此未将遗留颜色调用擅自实现为紫色。Windows 随机序列不保证与 iOS rand 逐帧一致，几何算法对应原代码。
- BT：`entries/bullet_template.bt`、`flow_native_sources.bt`。实现：`CBullet`、`CLightningArc`、`WeaponEffects`。

## Boss 血量与手雷：核实后保留原版

四图真实 `stboss` 路径检查波25/50/250/451/500（程序索引24/49/249/450/499）。pack2/7/12 血量为600/1500/7500/1000/15000，倍率为1/1/5/10/10；pack9 为1800/4500/10500/1200/18000，倍率为3/3/7/12/12。REV倍率没有丢失。

普通枪弹的裸甲4倍受伤不能套到手雷上。实际普通雷四次爆炸：前三次拆甲而不扣血；pack2/7/9 第四次扣100（测试装备攻击倍率1）。pack12 Boss 的原 Flow 在 `pack5_xga_0151_0x7d96.bin @0xACD` 执行 SetHealth(1)，再提交爆炸，REV10 会先变成10血再死亡。其15,000血被一雷清空可以复现，但不是血量倍率实现错误。已纠正旧研究概括，未增加人工血量或绕过该原版分支。

另一个确定的伤害偏差已修复：`WeaponEffects::Cue` 给爆炸多乘了枪械熟练度/暴击倍率。原 `CBullet::Configure :62250` 将它应用于弹体直接伤害，`FunctionResolver :61169/:61242/:61379` 的爆炸使用脚本实参和 Level/装备倍率，未重复乘熟练度。满熟练度两款 Kraken 的真实弹体86/124，原爆炸参数5曾被乘到85。已移除爆炸的额外倍率，直接命中仍保留熟练度加成。普通手雷不走 CGun 熟练度，这条修复不会改变 pack12 的原版规则。

## 道具显示、保存和拖动

1. **一直闪烁**：左右控制 Movie `[0,500,700]` 的章节2是退出/隐藏动画，旧循环选错章节。按 `CInputPad::Load :88166` 使用章节1的待机范围。连续采样覆盖完整旧循环，修前11帧变化，修后0。
2. **装备不保存**：旧路径只改运行期左右引用，进入战斗又写死默认值。按 `CPowerUpSelector::OptionEquip :184626` / `Bind :187378` 恢复配置 ordinal，写入 DataStore1001 payload116/117（mem128/129），装备后立即保存；重进按保存引用加载，未选择值FF按原 export4解析默认道具。NativeProfile 保留所有其他原始字节；旧研究文本存档升为v12，旧版本默认FF。
3. **一拖到底**：旧路径把鼠标像素直接当商品数量。现按原 `GLU_MOVIE_POWER_UP_LAYOUT` Region2/3的中心间距换算（当前资源128逻辑像素），拖12像素移动0.09375个位置，松手保留小数位置。列表商品在释放时才提交点击，移动超过点击容差后不触发购买/装备。

实现涉及 `CPlayerConfiguration`、`NativeProfile`、`CProfileManager`、`PowerupScene`、`SurvivalHud` 与 `MapScene`。存档 BT 保留旧注释并补充字段消费证据。

## 验证记录

构建命令：`MSBuild gun_bro_re.slnx /p:Configuration=Release /p:Platform=x64 /m /v:minimal /nologo`。检查命令均为 `bin/x64/Release/gun_bros_research.exe --mute <参数>`。

- `out/boss-anchor-red.log`：旧光束被改为(300,400)/73°，退出1。
- `out/boss-lightning-green.log`：方向保持、真实104弹体电弧56个四边形、现有武器检查通过，退出0；截图 `out/boss-beam-pack5-104.png`。
- `out/powerup-idle-red.log` / `powerup-idle-green.log`：11帧变化→0，退出1→0。
- `out/powerup-scroll-red.log` / `powerup-scroll-green.log`：拖动单位修正，退出1→0。
- `out/splash-mastery-red.log`：满熟练度两款 Kraken 共150次爆炸，出现3次非原值，最大85，退出1。修后 `out/final-weapon-effects-check.log` 两者各75次全部为5，退出0；同次包含枪口跟随、近端光束、敌人固定锚点及电弧检查。
- `out/final-powerup-selector-check.log`：12像素移动0.094项，拖动释放无误点击，位置2.281在等待2秒后保持不变；15商品、14购买、38个选择、原存档重载通过，退出0。
- `out/final-original-hud-check.log`：连续采样变化0帧，原HUD区域/动画检查退出0。
- `out/final-native-profile-check.log`：DataStore1001两个槽位及其他原始字节保存重载检查退出0。
- `out/final-native-profile-play-check.log`：正式存档驱动的生存/Horde流程与保存重载检查退出0。
- `out/final-boss-check.log`：四地图、20个额外波次、REV10首尾波四次实际手雷、入场镜头与死亡回普通波，退出0。
- `out/final-powerup-play-check.log`：真实使用/命中/库存/装备切换回归；`powerup-equip-check` 左14右5经原DataStore写入并建立新PowerupScene后保持，研究存档v12同步通过，退出0。
- 最终双EXE Release构建 `out/boss-powerup-final-build.log` 退出0；`git diff --check` 退出0。工程仍有既存C4244转换警告，未将构建通过描述为无警告。

边界：没有 iOS 同场景逐帧视频对照；不把单张电弧截图当作所有 Boss 技能已逐招对照。未改变原版 pack12 的破甲后普通雷致死规则。
