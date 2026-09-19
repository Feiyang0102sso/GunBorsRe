# Haven Boss 光束断裂复查（2026-09-19）

## 调研结论

用户已明确确认 **iOS 实机效果正常**，并于 2026-09-19 授权必要时硬编码修复。因此撤回“版本资源变化足以解释 iOS 原版表现”的推断：下述字节差异和重建版复现仍然成立，但原版与重建的完整运行差异尚未解释。本次修正是限定资源的兼容措施，不是修复原版资源错误。

已找到此前缺失的版本证据：**iOS 3.0.0 → 3.1.0 将 Haven Boss 的 `pack5/BULLET104` 主体动画字节由 0 改为 1，3.1.0–3.6.0 均保留此值。** 两份 235 字节弹体载荷只有文件内偏移 `0x06` 这一字节不同；脚本、伤害、标志及其他字段完全相同。

同一原型 139 的六个动画，沿全局 sprite map 解引用后，帧、时间、位置、尺寸、变换及混合方式全部一致；图集 19 个裁切矩形及原 PNG 也完全一致。因此这次变化改变的是动画选择，没有配套改变图像含义。

当前重建重复绘制动画 1 时产生串珠，是已复现的直接原因。依据已核查的 iOS 3.6.0 消费者链，没有找到将 1 隐式转换回 0 的处理；这仅说明当前核查尚未解释用户确认正常的 iOS 效果，不能把静态核对冒充实机验证。

## 玩家为何正常

| 对象 | 逻辑弹体 | 原型 | 按已核查 Bind 推导的主体 / 源端 / 末端 | 当前资源缩放 |
| --- | --- | --- | --- | --- |
| Infinity Laser | pack5/BULLET85 | 0 | 0 / 1 / 2 | 0.5 |
| 玩家 Kraken 的光束 | pack5/BULLET87 | 139 | 0 / 1 / 2 | 3.0 |
| Haven Boss（iOS 2.3.0–3.0.0） | pack5/BULLET104 | 139 | 0 / 1 / 2 | 1.0 |
| Haven Boss（iOS 3.1.0–3.6.0） | pack5/BULLET104 | 139 | 1 / 2 / 3 | 1.0 |

玩家与 Boss 共用 `CBullet::Bind` 和 `CBullet::Draw`。原型 139 的动画 0 每帧是 `128×150`、Y 偏移 `-150` 的光束主体；动画 1 每帧是 `128×72`、Y 偏移 `0`，它正是 Kraken 通过 `base+1` 绘制的源端亮斑。修复前的重建版 Boss 把动画 1 当主体重复铺贴，所以源端亮斑沿射线反复出现。动画 2 是 `128×65`、Y 偏移 `-65` 的末端；动画 3 另被 Kraken 的导弹 BULLET86 使用，并非该连续光束三件套的下一段。这是资源选择差异，不是玩家与 Boss 同时绘制时相互覆盖。

两种光束 flags 都为 `0x100`，也都由原 Flow 创建相同参数的电弧。细电弧和重复亮斑同时存在，不能用“缺了电弧”解释串珠。正确逻辑映射与原 ARM 审计见 [原版消费者审计](haven-beam-original-consumer-audit.md)。

## 逐版本一手资源核对

使用各版本自己的 `packTOC_xga.dat` → 包内 TOC → `___GAME_TOC_KEYSET` → BULLET 段基址 → 逻辑序号 104 → BIG 稀疏映射 → 解压载荷，未按文件名猜逻辑编号。

| 本地原 IPA 提取版本 | BULLET104 动画 | 载荷长度 | 相对 3.0.0 |
| --- | --- | --- | --- |
| 2.0.0 / 2.1.0 / 2.2.0 | 不存在该序号 | — | BULLET 数分别为 85 / 90 / 98 |
| 2.2.1 | 未核对 | — | 原样本 TOC 为零字节，不能猜其映射 |
| 2.3.0 / 2.4.0 / 3.0.0 | 0 | 235 | 载荷完全相同 |
| 3.1.0 / 3.2.0 / 3.3.0 / 3.4.0 / 3.5.0 / 3.6.0 | 1 | 235 | 仅偏移 0x06：00 → 01 |

所有此次读取的 12 份 iOS `pack5` 均重新计算 SHA-256，与对应提取 manifest 相符。另从原始 `Gun Bros 3.6.0.ipa` 直接读取 `Payload/gunbros.app/pack5_xga.big`，与运行目录 `big/pack5_xga.big` 逐字节相等；因此不是本地运行资源后来被修改造成。旧官方 PC 样本的 BULLET104 与 iOS 2.3.0–3.0.0 的 235 字节也完全一致；它只作交叉证据，不覆盖当前 iOS 版本。

进一步核对 3.0.0 / 3.1.0 / 3.6.0 的 `pack6/ENEMY8`：三份载荷均为 4627 字节且完全一致，SHA-256 为 `7259a9e3c930997f55c85a8c1141bcd9cf73a95cd3bb0871eaaa84bb09e1d123`。其脚本依赖 15 在载荷偏移 `0x9E / 0xA2 / 0xA3` 分别指定 pack5 hash、对象类型 3（Bullet）与逻辑序号 104，排除了同编号在这次版本变化中被另一种敌人/攻击替代的解释。证据保存于 `obj/haven-boss-version-consumer.log`。

用于反编译的 `_prep/gunbros` 与 `_prep/versions/3.6.0/gunbros` 散列相同：`987d78475fc2261c066884f91dccde18a4c3d2f5d8fe403e701f84687178a6cd`；本轮未把旧版主程序当作 3.6.0 消费者。

关键原字节：

```text
资源：pack5 / BULLET104；handle 0x0300016B；逻辑 ID 363；物理 ID 108
前 7 字节 = packHash（4） / archetype（1） / action（1） / animation（1）
iOS 3.0.0：85 75 26 00 8B 00 00
iOS 3.1.0：85 75 26 00 8B 00 01
iOS 3.6.0：85 75 26 00 8B 00 01
```

可复核的散列：

- BULLET104，iOS 2.3.0–3.0.0：`ff926fa57526af7a575e14752c1649d7a655675bf5c878c99ed7558d17835745`。
- BULLET104，iOS 3.1.0–3.6.0：`9f39e6c7c2e20d45bc18dd8dfcf9a91bbd4f3eba8057b5c33887820c5ca1cb97`。
- 原型 139 图集 PNG，已比对 iOS 3.0.0、3.1.0、3.6.0 及旧 PC：`9ab2b50523a928b57926bd2baa459b1f19eb7e4dfeaff900f6f4aabdc1a127b5`。
- 运行及原 IPA 的 3.6.0 pack5：`af04757872a53c1ff9ba0ce7d846cd4047e5d3a44a3354eccbbb63897eb8da82`。

来源目录为 `_prep/versions/<版本>/big/`，提取 manifest 在其父目录。3.0.0 / 3.1.0 / 3.6.0 的 BULLET104 BIG 块偏移分别为 `0x5FE8` / `0x6279` / `0x6743`；这些不是载荷内偏移。原始归档与所有样本均只读。

## 复现与证据

- 使用实际 `pack5/BULLET104`，调用正式 `CBullet::DrawProjectile`；光段之间的列绿色亮度最低约为峰值的 3%，截图出现重复亮斑及细线连接，与用户报告一致。
- 复现命令：`bin/Debug/GunBrosTests.exe --mute --weapon-effects-check --test-output E:/coding_projects/c_projects/gun_bro_re/obj/haven-beam-before`。原专项退出 0，因为已有用例只记录低谷，不将连续性作为通过条件；另外对测量值执行低于 20% 即失败的诊断断言，退出 1。20% 是此次视觉验收指标，不是原资源参数。
- 原图：`obj/haven-beam-before/boss-beam-pack5-104.png`；日志：`obj/haven-beam-before.log`。
- `entries/bullet_template.bt`、原 `CBullet::Bind`（反编译 63647–63669）及 ARM 原指令均确认：原型 139，action 0，主体 1、源端 2、末端 3。不能将主体改为 0 后称为按原引用复刻。
- `sprite_global.bt`、`sprite_archetype.bt`、`CSpritePlayer::SetAnimation`（58861）、`CalculateBoundsForFrame`（58513 附近）、`CBullet::Draw`（62965–63038）用于核对绑定、帧边界和铺贴。`pack5` GLOBAL 的 `substitutionGroupCount` 在文件偏移 6675 为 0，本例不能由 action 替换组解释。
- 原 `CBullet::OnStateChanged` 的 ARM 符号地址 `0x30150` 只有 `bx lr`；进入无动画序列的状态不会在此把主体改成 0。原始字节、BIG 和存档均未修改。

## 兼容修正

仅在测试场景中将这束弹体的动画组合改为 0/1/2，仍使用同一份 BIG 图集、正式绘制路径和原电弧，生成 `obj/haven-beam-before/haven-beam-candidate-not-original.png`。对比结果为连续绿色光束。它只证明候选效果可行，不能证明 iOS 原版使用这一组合。

获得用户明确授权后，在 `src/gun_bros_re/gameplay/weapon/CBullet.cpp` 的 `Bind` 中加入修正：仅匹配 `pack5/BULLET104`、pack5 原型 139/action 0/animation 1 且含 beam 标志的弹体，将运行时主体设为 0，随后按原绑定流程生成源端 1、末端 2。原模板仍保留动画 1，原 BIG、伤害、碰撞、计时及电弧脚本不变。其余弹体沿用原绑定流程，不增加按精灵边界猜测动画的通用规则。这个组合与已核对旧版资源相符，但不宣称还原了 iOS 3.6.0 实机的完整机制。

## 复核命令与产物

- `python obj/ZHavenBeamResourceAudit.py`：退出 0，直接读当前与旧 PC 的 BIG，输出引用、帧结构和逐资源散列，结果为 `obj/haven-beam-resource-audit.json`。
- `python obj/ZHavenBeamVersionAudit.py`：退出 0，按各自 TOC 读取上述 iOS 版本，结果为 `obj/haven-beam-ios-versions.json`。
- `obj/haven-beam-transition-resources.json`、`obj/haven-beam-transition-comparison.log`：3.0.0、3.1.0、3.6.0 原型 139 的独立展开结果及归一化比较；去除跨版重排的全局映射编号后全部六个动画相等，图集矩形相等，BULLET104 唯一差异为偏移 6。
- `python obj/haven-logical-beam-map.py` 与 `python obj/haven-consumer-audit.py`：退出 0，复核正确逻辑弹体及原 ARM 消费者。消费者审计初稿混淆物理序号的部分已经撤回并修正，不能引用旧的 Flow 推断。

调研阶段的临时对比绘制代码已撤回，保留截图和实验 patch 于 `obj/`。随后按用户授权实施上述正式兼容修正；原始 BIG 与存档始终未修改。

## 验收与结果

验收覆盖原模板仍保留动画 1、兼容后的有效显示组合、连续性亮度检查、真实 Haven Boss 发射，以及玩家激光原有动画绑定。先只修改测试并构建 Debug Tests，再执行 `--mute --weapon-effects-check --test-output E:/coding_projects/c_projects/gun_bro_re/obj/haven-beam-red`，退出 1：绑定与连续性两项失败，低谷为峰值的 3%。日志为 `obj/haven-beam-red.log`。修复后的验证结果见后续记录。

修复后完成以下验证：

| 检查 | 结果 | 日志 |
| --- | --- | --- |
| Debug Game 构建（含 Viewer/Tests）及工程默认检查 | 退出 0；progress、big-version、viewer-controls、viewer-smoke 通过 | `obj/haven-beam-debug-build.log` |
| Release Game 构建（含 Viewer/Tests） | 退出 0 | `obj/haven-beam-release-build.log` |
| Debug / Release 武器特效 | 均退出 0，failures=0；主体 0/源端 1/末端 2，模板仍为 1；亮度低谷 44% | `obj/haven-beam-fixed-debug.log`、`obj/haven-beam-fixed-release.log` |
| Debug / Release 真实 Boss 流程 | 均退出 0；Haven 发射观察成功，failures=0；四张 Boss 地图回归通过 | `obj/haven-boss-fixed-debug.log`、`obj/haven-boss-fixed-release.log` |

专项命令模式：`bin/<Debug或Release>/GunBrosTests.exe --mute --weapon-effects-check --test-output <上述对应日志同名的绝对目录>`；真实 Boss 专项将 `--weapon-effects-check` 换为 `--boss-check`，目录使用 `haven-boss-fixed-debug` / `haven-boss-fixed-release`。Debug 默认检查也显式静音。

已查看正式绘制截图 `obj/haven-beam-fixed-debug/boss-beam-pack5-104.png` 与真实发射截图 `obj/haven-boss-fixed-debug/haven-boss-live-beam.png`：横向、斜向光束均连续。真实攻击日志保留 `bullet=00267585:104 sprite=139/1 body=0 caps=1/2 group=5 length=953.3`。玩家 Infinity Laser、Kraken、Gold Kraken 等专项通过，并新增实际玩家光束绑定必须与模板一致的断言。

`bin/Debug/GunBrosRe.exe` 与 `bin/Release/GunBrosRe.exe` 均已更新。修正完成；尚未解释的 iOS 完整运行差异仍作为研究边界保留，不影响此次经授权的兼容修正交付。
