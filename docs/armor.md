# 盔甲研究与验收

2026-09-08：已接入原版盔甲脚本属性、角色装配和 Arena 战斗。

## 操作

```powershell
.\bin\x64\Release\gun_bros_re.exe --mute --armor 11 --weapon 47
.\bin\x64\Release\gun_bros_re.exe --mute --arena 0 --armor 4
.\bin\x64\Release\gun_bros_re.exe --mute --armor-check
.\bin\x64\Release\gun_bros_re.exe --mute --armor-render-check
```

菜单 17 为数据／脚本检查，18 为盔甲查看器，19 为全目录渲染检查；这些入口永久保留。
查看器左右键切换盔甲，替换对应槽位并保留其余槽；B 清除全部盔甲。1–7、N/M 换枪，F 射击，WASD 播放移动动作，鼠标拖动旋转。
换枪保留盔甲。Arena 可用 `--armor` 选择一件装备；原有操作不变。

## 原版依据与校正

- `CPlayerConfiguration::SetArmor`（反编译 171658）：模板首字节是装备槽，原解析中的 `m_flag4` 已更名为 `m_slot`。
- `CArmor::Bind`（176612）：224／225 字节是挂点，已更名为 `m_attachmentNode`；装备脚本 export 0 写入 class 13 的五个 int16 属性。
- `CBrother::Draw`（134780 起）：槽 0 替换腿部贴图；槽 1 替换躯干贴图并可同时绘制两处挂件；槽 2 绘制头部挂件。旧注释“一模型对应一位兄弟”不成立，已追加更正并保留原注释供追溯。
- `CBrother::Damage`（136667）：防御百分比相加后减伤，不加最大生命。
- `CBrother::GetDamageMultiplier`（136462）：攻击倍率为 `1 + 各槽攻击百分比之和 / 100`。
- `CBrother::OnMove`（137584）：速度使用同样的相加倍率规则。
- 属性 3／4 已核对为 XP／Xplodium 加成，并分别接入原击杀奖励路径；后续阶段结果见 `docs/overnight-progress.md`。

## 已验证

- 233 件模板：腿部 70，躯干 72，头部 91；第四槽没有资源条目。
- 全目录脚本、160 个模型引用和相应贴图解析通过，实例属性独立；详见 `out/armor-check.csv`。
- 三种武器配置 × 233 件盔甲，共 699 个挂点／渲染组合通过，GL 错误 0。
- 已核对截图：`out/armor-11-beam.png`、`out/armor-4-head.png`、`out/armor-7-legs.png`。
- Arena 固定三件套（目录 7、11、4）：10 点来袭伤害实扣 8.6；10 点基础攻击造成 10.5；100ms 水平移动 20.46。重置和换枪保留装备，卸下恢复基础倍率。
- Release 武器和 Arena 回归零失败；Release、Debug 均构建通过。

## 明确边界

单项查看器显示兄弟索引 0；正式双人场景已接兄弟 AI，在线好友角色选择尚未接入。研究目录保留包名／编号，正式商店使用原商品名、图标、价格和等级条件。
三种武器配置的遍历不等于全部 76 种配置的人工画面验收。
攻击加成在当前战斗世界结算命中时读取；若后续允许实战中换盔甲，需要进一步核对原版弹体出生时的倍率快照。
