# Pus Tank 死亡、爆裂与资源调用链核对

核对日期：2026-09-17。范围：当前 iOS 3.6.0 原始 BIG、对应解包样本、原反编译消费者和 BT；旧 `.flow` 只作版本比较。本记录不修改游戏实现或原件。文中的 `@0x...` 是解压后的单个 `.bin` 文件偏移；反编译行号不是原 C++ 文件行号。

## 结论与身份

用户所贴脚本确实对应本地旧文件 [`pusstank.flow`](<../_prep/flow scripts/enemy/pusstank.flow>)，包括错误的 `player.flow` 文件头。但它不完整代表当前 BIG 中的 Pus Tank：当前内嵌 Flow **存在爆裂与普通模型死亡两条分支**，旧文本没有资源依赖、爆裂判断、爆裂声音和粒子调用。因此不能从这份旧文本没有 `effect()` 推断当前原版没有爆裂效果。

ENEMY 是 type 5／Section 6。以下三个名字由 ENEMY 名字引用、包的类型基址以及实际字符串共同确认，未按图片外观命名：

| 名字 | pack1 ENEMY ordinal | 原文件 | 字节 | 名字局部序号 | 字符串 ID | 爆裂粒子 ordinal |
|---|---:|---|---:|---:|---:|---:|
| Pus Tank | 1 | [0023_0x33da](../_prep/big_360_out/pack1_xga/0xf4e02223/06_ENEMY/pack1_xga_0023_0x33da.bin) | 654 | 174 | 740 | 53 |
| Pus Tank EX | 2 | [0024_0x3555](../_prep/big_360_out/pack1_xga/0xf4e02223/06_ENEMY/pack1_xga_0024_0x3555.bin) | 710 | 179 | 745 | 55 |
| Pus Tank XL | 17 | [0039_0x4705](../_prep/big_360_out/pack1_xga/0xf4e02223/06_ENEMY/pack1_xga_0039_0x4705.bin) | 824 | 463 | 1029 | 54 |

三者名字引用在 `@0x1` 保存包 hash `0x00267581`，`@0x5` 保存 i32 序号。`___GAME_TOC_KEYSET` 对应物理 [0315](../_prep/big_360_out/pack1_xga/0x69e5d35c/pack1_xga_0315_0xe03152.bin)，类型 33 基址为 `0x21FF0236`；加上 174 得 `0x21FF02E4`，低位 ID 740。字符串聚合 [0000_0x17c8](../_prep/big_360_out/pack1_xga/0x69e4c505/pack1_xga_0000_0x17c8.bin) 的 `@0x1C57` 实际为 `04 00 00 00 "Pus Tank\0"`；EX、XL 分别在 `@0x1CA2`、`@0x3F35`。原 `CEnemy::Template::CreateNameString` 调用 `GetResId(type33)`（[原消费者 :73571](../_prep/_IDA_OUT/gunbros_3.6.0_IOS.c:73571)）；基址加载与相加见 [:129893](../_prep/_IDA_OUT/gunbros_3.6.0_IOS.c:129893)、[:78597](../_prep/_IDA_OUT/gunbros_3.6.0_IOS.c:78597)。

## ENEMY 模板内到底保存什么

[`enemy_template.bt`](<../_prep/_Big_tool/binary template/big_assets/enemy_template.bt>) 和原 [`CEnemy::Template::Init :67174`](../_prep/_IDA_OUT/gunbros_3.6.0_IOS.c:67174) 的读取顺序一致：debug 字节 → 名字引用 → **内嵌 CScript** → 模型 MoveSet → 子弹引用 → 奖励、尺寸等字段 → 碰撞数据。不是引用外部 `.flow` 文本。

普通模板首字节 debugScriptEnabled=0。原 `Template::Init` 只有该字节非零且 Debug::Enabled 时才走外部调试脚本覆盖（:67195–67199）；当前条目使用自身内嵌脚本。`Template::Load` 加载 MoveSet（:68895），`GetRequirements` 收集子弹与脚本资源依赖（:68910），然后 `Bind` 把脚本与模型动作控制器绑定到敌人实例。

普通 Pus Tank 的 Flow 位于 `@0x9..0x20C`，模型 MoveSet 从 `@0x20D` 开始。Flow 容器定义见 [`entries/common.bt`](<../_prep/_Big_tool/binary template/big_assets/entries/common.bt>)，指令规则见 [`flow_bytecode.bt`](<../_prep/_Big_tool/binary template/big_assets/flow_bytecode.bt>)。函数名、局部变量原名没有存入这段字节码；下文的可读名称是按原生消费者解释的说明，不冒充恢复出的源码。

该样本首字节 `debugScriptEnabled=0`。原 `Template::Init :67195～67199` 只有这个字节非零且 `Debug::Enabled` 为真才调用 `LoadDebugScript("e")`，因此这里不能假设旧外部 `.flow` 会覆盖内嵌脚本。

普通 Pus Tank 的依赖表：

| 槽 | 包 | 类型 | 类型内序号 | 依赖表偏移 | 用途／调用 |
|---:|---|---:|---:|---|---|
| 0 | pack1 | 11／PARTICLEEFFECT | 53 | `0x17` hash，`0x1B` type，`0x1C` ordinal | 爆裂 `native 0x0708(0)` |
| 1 | pack1 | 11／PARTICLEEFFECT | 73 | `0x20`、`0x24`、`0x25` | 普通死亡 `native 0x071F(0,0,1)` 的第三参数 |
| 2 | pack1 | 21／SOUNDEFFECT | 20 | `0x29`、`0x2D`、`0x2E` | 爆裂声音 `native 0x0709(2)` |
| 3 | pack5 | 11／PARTICLEEFFECT | 106 | `0x32`、`0x36`、`0x37` | 命中特殊状态的附着效果；不是死亡爆裂图块 |

这里的 `0` 是脚本资源槽，`53` 是 pack1 粒子类型内序号，`0109` 是解包物理文件序号；三者不可互换。脚本资源经原 [`GetResource :107222`](../_prep/_IDA_OUT/gunbros_3.6.0_IOS.c:107222) 解为运行时包索引和类型内序号。

声音槽 2 对应 [SOUNDEFFECT ordinal20／0170_0x98ae.bin](../_prep/big_360_out/pack1_xga/0xf4e02223/22_SOUNDEFFECT/pack1_xga_0170_0x98ae.bin)，其中再引用 pack1 WAV ordinal52。声音事件与碎片 PNG 是两条不同依赖。

## 从出生、受击到死亡分支

1. 原 `CEnemy::Bind` 把模板的 CScript 绑定到实例解释器（[:73381](../_prep/_IDA_OUT/gunbros_3.6.0_IOS.c:73381)，`SetScript` 在 :73473）；`Spawn` 调用 export 0（[:73237](../_prep/_IDA_OUT/gunbros_3.6.0_IOS.c:73237)）。实际导出表 `@0x11 = 00 02 01 03`，所以 export 0→function 0、export 1→function 2、export 2→function 1、export 3→function 3。
2. 普通 Pus Tank function 0 设置移动速度 65、调用设置生命 native `0x0732(50)`，进入 state 1。生命值还乘 `CLevel::GetHealthMultiplier`，50 是脚本基础值，不能直接当最终每关血量。跟随距离随机区间为 170～250。
3. 碰撞路径保存本次命中伤害、方向、flags 和 critical 值，再发出 `enemy.shot`（事件 `0x0702`）。原普通命中写 `mem+992 = CBullet+28`、`mem+988 = CBullet+452`，见 [:71530](../_prep/_IDA_OUT/gunbros_3.6.0_IOS.c:71530)；范围伤害也写相同字段，见 [:68010](../_prep/_IDA_OUT/gunbros_3.6.0_IOS.c:68010)。
4. 内嵌父状态 state 0 的受击处理在 `@0x56` 调 `native 0x0731(0)`，把命中 flags 的 bit 0 存入 local[2]。原 native 0x31 的实现就是 `(mem+992 & (1 << 参数)) != 0`（[:72544](../_prep/_IDA_OUT/gunbros_3.6.0_IOS.c:72544)）。此字段的作用已由分支证明；没有在本记录中虚构完整 flags 枚举名。
5. 受击脚本还处理 flags bit 2／3 对应的附着、定时伤害／状态分支。通常路径在 `@0xD5` 调 `native 0x070D()` → `ApplyCollision` → `ResolveCollision` → `Damage`；状态分支可经 native 0x35 延后调用 `Damage`。不能把所有受击都简写成一次立即扣血。对应实际字节清单见 [普通 Pus Tank Flow](../_prep/out/binary-research/flow-disassembly/pack1_xga_0023_0x33da.flow.txt)。
6. `CEnemy::Damage` 把生命扣至最低 0，生命归零时调用 **export 1**，之后通知 `CLevel::OnEnemyKilled`（[:71552](../_prep/_IDA_OUT/gunbros_3.6.0_IOS.c:71552)）。`CallExportFunction` 按映射取出 function 2（[:107394](../_prep/_IDA_OUT/gunbros_3.6.0_IOS.c:107394)）。

普通 Pus Tank 的 function 2（长度字节在 `@0x183`）可读为：

```text
取消已有脚本定时调用                         @0x185 native 0x0704()
如果本次命中 critical 非零：                 @0x189 variable[19]（mem+988）
    local[2] = 1                             @0x191

如果 local[2] 非零：                         @0x198
    播放资源槽 2 的声音                       @0x1A0 native 0x0709(2)
    在敌人位置生成资源槽 0 的粒子              @0x1A6 native 0x0708(0)
    标记敌人完成／可移除                      @0x1AC native 0x0706()
否则：
    在 linked slot 0、node 0 挂资源槽 1        @0x1B8 native 0x071F(0,0,1)
    进入 state 4                             @0x1C2
```

critical 来源不是仅凭变量槽号猜测：原变量解析 case 19 返回 `mem+988`（[:69058](../_prep/_IDA_OUT/gunbros_3.6.0_IOS.c:69058)），碰撞从子弹 `+452` 拷入；同一个子弹字段在 `Damage` 中用于 `OnEnemyCriticalHit` 通知（:71570 附近）。普通 Pus Tank 和 EX 都有这条 critical 强制爆裂分支。XL 的 function 2 仅检查其 local[2]，**没有**这条 variable[19] 判断；不能把普通型条件复制到 XL。

## 爆裂分支为什么不等模型死亡动画

原 [`ResolveFunctionLocally :71692`](../_prep/_IDA_OUT/gunbros_3.6.0_IOS.c:71692) 的三个分支明确分工：

- case 8：先 `GetResource`，再 `CGunBros::GetGameObject(type11)`，最后 `CParticleSystem::AddEffect`，位置为敌人当前 `mem+828/+832`，不把这个独立实例挂回敌人。
- case 9：解析 SOUNDEFFECT 模板，交 `CSoundQueue::PlaySound`。
- case 6：仅将 `mem+931` 置 1；`CEnemy::IsDone` 读取此标记。不是在函数里销毁地图粒子。

因此爆裂分支是“声音 + 地图粒子 + 标记敌人结束”，并不进入 state 4 的模型死亡动作。粒子模板 53 对应 [pack1_xga_0109_0x7b92.bin](../_prep/big_360_out/pack1_xga/0xf4e02223/12_PARTICLEEFFECT/pack1_xga_0109_0x7b92.bin)。

## 普通死亡仍然包含粒子与模型动画

普通分支调用 `StartLinkedEffect(slot0,node0,resource1)`，资源 1 指向 pack1 粒子 73；原 [`StartLinkedEffect :70563`](../_prep/_IDA_OUT/gunbros_3.6.0_IOS.c:70563) 从地图 `CParticleSystem` 领取播放器，记录在敌人的 linked slot，并设角色锚点。它和上面的独立爆裂粒子不是同一个持有方式。

随后 state 4 的动作序列 `@0x14D = 04` 指向 **MoveSet move 4**。普通 Pus Tank 的这一动作定义仍在同一个 ENEMY `.bin`：

| 字段 | 文件偏移 | 实际值 |
|---|---|---|
| 模型包 | `0x20D` | pack1 |
| mesh ordinal | `0x212` | 1 |
| texture ordinal | `0x213` | 1 |
| move4 起止模型关键帧 | `0x258`、`0x25A` | 43～52 |
| move4 播放倍率 | `0x25D` | 65536，即 16.16 的 1.0 |
| move4 声音事件 | `0x266`、`0x268` | 模型帧 43，WAV ordinal 5 |

state 4 先把角度模式置 3，再设置碰撞冲击方向；`anim.sequence` 事件 `0x0301` 到达时调用 `native0x0706()`（`@0x162`）。原 `CEnemy::OnMoveChanged` 调 `CMoveSetMeshController::SetMove`（[:68811](../_prep/_IDA_OUT/gunbros_3.6.0_IOS.c:68811)）；`CEnemy::Update` 先更新模型控制器，再 `CScriptInterpreter::Refresh`（:67870～67876）。`Refresh` 在动作循环完成且到序列末项时评估 `0x0301`（[:107271](../_prep/_IDA_OUT/gunbros_3.6.0_IOS.c:107271)）。不是固定等待一个人为延时。

敌人 `OnRemove` 对已持有的附着粒子调用 `StopSpawning` 并清空 linked slots（[:67697](../_prep/_IDA_OUT/gunbros_3.6.0_IOS.c:67697)），不是立即清掉所有残余粒子。

## 为什么旧脚本里看不出来

| 项目 | 用户提供的旧 `pusstank.flow` | 当前 pack1 ENEMY1 内嵌 Flow |
|---|---|---|
| 出生速度 | 50 | 65 |
| 生命设置参数 | 60 | 50，再由原生乘关卡倍率 |
| 跟随随机距离 | 100～250 | 170～250 |
| 受击处理 | 只调用 applyCollision | 记录 flags、特殊命中状态／定时伤害处理，再分流 |
| OnKilled | 直接 goto killed | 取消计时、检查爆裂条件、播放对应声音和粒子或进入模型死亡 |
| 普通死亡附着粒子 | 无调用 | linked resource1 → pack1 粒子 73 |
| 内嵌资源依赖 | 无 | 4 条依赖（表见上） |

旧文本的死亡动作 state 主体与当前 state 4 相近，但 **进入该状态之前的死亡分流已经不同**。不能只查 `state killed`，更不能因它写着 `moves.die` 就认定所有视觉内容都藏在该动作里面。

## 验证及边界

- 执行 `D:/Python312/python.exe obj/verify_pustank_enemy.py`，退出码 0；日志 [`obj/pustank-enemy-verify.log`](../obj/pustank-enemy-verify.log)。三份 ENEMY 从实际样本重新解析至 EOF，并逐字段与现有索引核对一致；名字字符串实际字节已复核。该只读脚本没有调用索引工具的生成入口。
- 已同时查 `enemy_template.bt`、`entries/common.bt`、`flow_bytecode.bt`、原 `Template::Init / Bind / Spawn / Damage / ResolveFunctionLocally / StartLinkedEffect / OnRemove` 和原脚本解释器。
- `section-catalog.json` 中 ENEMY 尾部有历史误名（如 `health`／`speed` 实际为奖励字段）；本文不采用这些误名来解释生命与速度，二者以 Flow 赋值及原消费者为准。
- 原始内嵌 Flow 不保留局部变量与内部函数原名。本记录确认当前死亡与资源链，未宣称恢复出完整可重新编译的原 `.flow` 文本，也未证明旧文本具体属于哪个发布版本。
- flags bit 0 的分支作用及 critical 字段来源已明确；所有上游武器如何设置该 bit、联网同步的全部情况不在本次逐项核对范围。未为 flags bit 2／3 擅自补全官方枚举名字。
- 本次是静态资源与消费者研究，没有执行游戏截图、行为回归或更改运行逻辑；不能将字节与分支核对等同于完整运行时复现验证。

## Sprite、图集与粒子参数的完整连接

以下是对实际解包字节重新解析，并与原 BIG 比较后的结果；没有按图片外观推断引用关系。所有动画、发射器、资源序号均从 0 开始。

### 爆裂资源文件

| 环节 | 资源 | 定位 |
|---|---|---|
| 粒子模板 | [pack1_xga_0109_0x7b92.bin](../_prep/big_360_out/pack1_xga/0xf4e02223/12_PARTICLEEFFECT/pack1_xga_0109_0x7b92.bin) | type 11 / Section 12 / ordinal 53；910 字节 |
| Sprite 共享映射 | [pack1_xga_0514_0x12658fc.bin](../_prep/big_360_out/pack1_xga/0xf4e02223/pack1_xga_0514_0x12658fc.bin) | `SPRITEGLU__BINARY_GLOBAL`；3035 字节 |
| Sprite 原型 | [pack1_xga_0579_0x1267b90.bin](../_prep/big_360_out/pack1_xga/0xf4e02223/pack1_xga_0579_0x1267b90.bin) | `SPRITEGLU__BINARY_ARCHETYPE_000 + 64`；187 字节 |
| 裁切表 | [pack1_xga_0697_0x126a5f5.bin](../_prep/big_360_out/pack1_xga/0xf4e02223/pack1_xga_0697_0x126a5f5.bin) | `BASE_TEXTURE_MAP + 64`；823 字节 |
| 纹理页数量表 | [pack1_xga_0751_0x126b69e.bin](../_prep/big_360_out/pack1_xga/0xf4e02223/pack1_xga_0751_0x126b69e.bin) | `TEXTURE_MAP_GLOBAL`；120 字节 |

粒子模板 `+0x00` 的 Sprite 包 hash 为 `0x00267581`（pack1），`+0x04` 发射器数为 6。六个发射器起点为 `0x005/0x087/0x122/0x1BD/0x258/0x2F3`，原型字段均为 64。紧随原型字节的 u32 动画 mask 依次是 `0x20/0x10/0x01/0x04/0x08/0x20`，因此选择动画 `5/4/0/2/3/5`。

原型文件 `+0x58` 的 animationCount=6，每条动画只有一个 step，分别引用 frame 0–5；每步磁盘时长为 10，原读取器乘 10 后为 100 ms。这是六条单帧动画，并非一条有六帧的爆炸动画；100 ms 也不是粒子寿命。原型 action 0 的 substitutionGroup=255，没有额外替换组。

### 六幅图块与截图四张 PNG

`TEXTURE_MAP_GLOBAL` 中原型 64 前共有 109 页；其自身有 4 页。通过 `BASE_TEXTURE_PAGE_0 + 109 + page` 定位到截图中的 PNG。完整路径在下表文件链接中。

| 动画 | SpriteMap → imageSlot → imageIndex → rectangle | PNG | 裁切 x,y,w,h | 粒子 53 使用者 |
|---:|---|---|---|---|
| 0 | 48 → 45 → 45 → 0 | [0428](../_prep/big_360_out/pack1_xga/0xb7178678/pack1_xga_0428_0xffac22.png) | 0,0,48,38 | 发射器 2 |
| 1 | 49 → 46 → 46 → 1 | [0429](../_prep/big_360_out/pack1_xga/0xb7178678/pack1_xga_0429_0xffd7c3.png) | 79,0,43,55 | 未引用 |
| 2 | 50 → 47 → 47 → 2 | [0428](../_prep/big_360_out/pack1_xga/0xb7178678/pack1_xga_0428_0xffac22.png) | 49,0,58,45 | 发射器 3 |
| 3 | 51 → 48 → 48 → 3 | [0429](../_prep/big_360_out/pack1_xga/0xb7178678/pack1_xga_0429_0xffd7c3.png) | 0,0,78,60 | 发射器 4 |
| 4 | 52 → 49 → 49 → 4 | [0430](../_prep/big_360_out/pack1_xga/0xb7178678/pack1_xga_0430_0x1000dcf.png) | 0,0,51,52 | 发射器 1 |
| 5 | 17 → 15 → 15 → 5 | [0431](../_prep/big_360_out/pack1_xga/0xb7178678/pack1_xga_0431_0x1002235.png) | 0,0,134,86 | 发射器 0、5 |

因此截图四张 PNG 确实都被普通 Pus Tank 的爆裂资源引用；但第二张右侧的图块（动画 1）不在粒子 53 的选择集合中，不能声称这张图上的每个部件都会在此次爆裂中出现。粒子 57（`pack1_xga_0113_0x7f85.bin`）也引用原型 64 的动画 5，说明图块存在复用；本记录不根据复用关系猜测其他敌人的名称。

### 动起来和淡出的来源

参数均来自粒子 53，表中十进制数是 float 原值的可读舍入；原始值、偏移和字段名保留在 `obj/pustank-assets-evidence.json`。

| 发射器 | 动画 | 出生中心 x,y | 径向角度范围（度） | 径向速度原值 | Y 加速度 | 缩放通道 | alpha 通道 |
|---:|---:|---|---|---|---:|---|---|
| 0 | 5 | 0,-20 | -360～360 | 0 | 0 | 0～700 ms：0.7→1.8 | 400～700 ms：1→0 |
| 1 | 4 | 0,-40 | -50～50 | -200～-100 | 150 | 0～700 ms：0.5→0.8 | 500～700 ms：1→0 |
| 2 | 0 | 0,-20 | -100～-90 | -100 | 150 | 0～700 ms：0.6→0.9 | 500～700 ms：1→0 |
| 3 | 2 | 0,-20 | -360～360 | 100 | 150 | 0～500 ms：0.5→0.8 | 500～700 ms：1→0 |
| 4 | 3 | 0,-10 | 90 | -100 | 150 | 0～500 ms：0.6→0.9 | 500～800 ms：1→0 |
| 5 | 5 | 0,-20 | -360～360 | 0 | 0 | 0～700 ms：0.2→1.6 | 400～700 ms：1→0 |

所有发射器的出生 pattern=2，但半径和厚度均为 0，因此实际出生在各自指定中心。速度 mode=1（径向）；负速度是原始有效值，不应擅自取绝对值。原 `CParticleSpawnVelocityRadial::GetVelocity` 使用 `sin(angle)*speed` 与 `cos(angle)*speed`；原 `CParticle::Update` 按秒积分加速度及位移，并应用播放器角度。

发射器 1–4 还包含旋转插值；发射器 5 的旋转通道固定为 -60 度。两层动画 5 图案依靠缩放和 alpha 形成扩散、淡出，部件图依靠粒子位置、旋转、alpha 变化形成飞散。这里不存在预先烘焙的多帧“爆血电影”。

这些 alpha 段的结束时间只表示淡出段结束，不是统一回收时刻。原 `CParticle::IsDone`（反编译 133278）检查所有通道；本资源仍有 1500/2000 ms 的旋转段。零发射间隔的处理和池是否有空位也会影响实际生成，不能从“6 个发射器”直接推导所有情况下都显示 6 个粒子。

### 原消费者与模板依据

- [粒子 BT](<../_prep/_Big_tool/binary template/big_assets/entries/particle_effect.bt>)：包 hash、发射器、动画位集合、出生区域、速度及通道布局。
- [Sprite 原型 BT](<../_prep/_Big_tool/binary template/big_assets/sprite_archetype.bt>)、[共享映射 BT](<../_prep/_Big_tool/binary template/big_assets/sprite_global.bt>)、[裁切 BT](<../_prep/_Big_tool/binary template/big_assets/sprite_texture_map.bt>)、[纹理页 BT](<../_prep/_Big_tool/binary template/big_assets/sprite_texture_pages.bt>)。
- `_prep/_IDA_OUT/gunbros_3.6.0_IOS.c`：`CParticleEffect::Init` 130974，`CParticleEmitter::Init` 131811，`Load` 132026（加载指定 Sprite 原型，action=0），`CParticle::Spawn` 133114/133137（动画 mask 选位），`Update` 133703/133795（运动与按年龄选帧），`Draw` 133466/133682（变换、alpha、SpriteIterator 和 SpritePlayer）。
- 同一反编译：`CSpriteGlu::LoadTexturePack` 57348、`LoadArcheType` 57552、`LoadTexturePackData` 57721、`Init` 57762。裁切由原矩形及纹理坐标实现，无需生成新的 PNG 小文件。

## 本轮只读验证

- `D:\Python312\python.exe obj/pustank-assets-research.py`：退出码 0。直接解析原 Sprite 共享表、原型、裁切表、页表及三个候选粒子文件，全部精确消费到文件末尾；沿原 TOC 键和页累计数重新计算 PNG 引用。日志 `obj/pustank-assets-research.log`，字段证据 `obj/pustank-assets-evidence.json`。
- `D:\Python312\python.exe obj/pustank-big-verify.py`：退出码 0。按 `big/pack1_xga.big` 原 table2 定位并解压 22 个相关文件，与现有解包样本逐字节相同。日志 `obj/pustank-big-verify.log` 含文件长度与 SHA256。覆盖三份 ENEMY、四份粒子、四张爆裂 PNG、四份 Sprite 配套文件、声音模板及两份 WAV、普通死亡模型与纹理、名字 keyset 和字符串聚合。
- 原 BIG、解包文件、旧 `.flow` 均保持只读；没有修改游戏实现，没有运行游戏或用截图证明行为。本轮结论是资源／字节码／原生消费者的静态证据链，不冒充当前重建运行效果的回归结果。


## 普通死亡与声音的配套文件

- 普通死亡附着粒子：pack1 type11 ordinal73 → [0129_0x8a73.bin](../_prep/big_360_out/pack1_xga/0xf4e02223/12_PARTICLEEFFECT/pack1_xga_0129_0x8a73.bin)，315 字节；两发射器均引用 pack1 Sprite 原型 7、动画 6。这和截图的原型 64 是两套资源。
- 模型：pack1 MODEL ordinal1 → [0274_0x7aa8f9.bin](../_prep/big_360_out/pack1_xga/0xf4e02223/31_MODEL/pack1_xga_0274_0x7aa8f9.bin)；模型纹理：pack1 PNG ordinal1 → [0187_0x1ce02.png](../_prep/big_360_out/pack1_xga/0xb7178678/29_PNG/pack1_xga_0187_0x1ce02.png)。普通死亡使用该模型的 MoveSet move4（43～52）。
- move4 帧43的声音：pack1 WAV ordinal5 → [0219_0x42b249.wav](../_prep/big_360_out/pack1_xga/0xfd8a7754/30_WAV/pack1_xga_0219_0x42b249.wav)。
- 爆裂声音：脚本 resource2 → pack1 SOUNDEFFECT ordinal20 → [0170_0x98ae.bin](../_prep/big_360_out/pack1_xga/0xf4e02223/22_SOUNDEFFECT/pack1_xga_0170_0x98ae.bin)；其 8 字节指向 pack1 WAV ordinal52 → [0266_0x74c04e.wav](../_prep/big_360_out/pack1_xga/0xfd8a7754/30_WAV/pack1_xga_0266_0x74c04e.wav)。原消费者为 `ResolveFunctionLocally` case9（:71967–71976）。本轮没有自动播放声音。
