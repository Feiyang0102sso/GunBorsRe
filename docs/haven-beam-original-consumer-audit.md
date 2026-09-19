# Haven Boss 光束：原版消费者独立审计

日期：2026-09-19。范围：只读核查 iOS 3.6.0 的弹体绑定、敌我发射、Flow 回调与光束铺贴。未修改游戏实现或 BIG 原件。

本轮完整结论及新增的 iOS 3.0.0 → 3.1.0 单字节版本变化，见 [资源与版本调研](haven-boss-beam-investigation.md)。本文仅记录消费者核对，不能单独替代跨版本结论。

后续状态：用户确认 iOS 实机光束正常，并明确授权硬编码兼容修正。正式实现已在 `CBullet::Bind` 限定 `pack5/BULLET104` 使用运行时 0/1/2；这不改变本文的只读证据，也不证明原版实机使用同样的修正规则。完整运行差异仍未查明，验证与实现范围见上述调研文档。

## 结论

在本次核查的原版消费者链中，没有发现将 Boss 的主体动画从 1 隐式改为 0 的处理。原版 `Bind` 直接消费模板的动画字节，光束端点使用该编号加 1、加 2；敌人发射、脚本状态/动作回调和绘制均未提供这项修正。因此，“改为 0/1/2 能更连续”仍是替换资源选择的视觉候选，不能据此证明重建应修改编号，也不能据此断言原版实机就是断续的。

此结论只排除本节已核查的原因。纹理/精灵解析、边界、图集图像和运行资源版本是否与原实机一致，需结合另一份资源审计；本次没有原版实机运行结果。

## 证据与数据边界

- 主证据：`_prep/gunbros` 的 ARMv7 原始字节及 `_prep/_IDA_OUT/gunbros_3.6.0_IOS.c`。下文行号全部指该反编译文件，ARM 地址为运行虚拟地址。
- 核查模板：`_prep/_Big_tool/binary template/big_assets/entries/bullet_template.bt`、`entries/common.bt`、`flow_bytecode.bt`。模板是导航资料，关键结论回到原反编译和机器指令。
- Flow 清单：`_prep/out/binary-research/flow-disassembly/pack5_xga_0091_0x5e73.flow.txt` 与 `pack5_xga_0108_0x6743.flow.txt`。这两份为既有解码结果，本次通过当前 BIG 的 TOC/keyset 定位逻辑弹体、读取载荷并与解包样本逐字节对比，再核对其调用编号对应的原生实现；未重新执行完整 Flow 解码。
- 本次生成：`obj/haven-consumer-audit.py`、`obj/haven-consumer-audit-arm.txt`、`obj/haven-enemy-fire-arm.txt`。

## 绑定与动画选择

`CBullet::Template::Init`（L130584，0xA8A18）先调用 `CGameSpriteGluRef::Init` 读取引用，随后读取脚本、flags 和缩放等字段；本函数没有将动画编号减 1 或依据玩家/敌人身份重映射的分支。

`CBullet::Bind`（L63599，0x34918）先把模板 `+128` 的 flags 复制到弹体 `+28`，初始化主体 `CSpritePlayer`，将模板 `+11` action 复制到精灵 `+20`，然后以模板 `+12` 的字节直接调用 `SetAnimation`（L63647—63650）。当 flags 含 `0x100`，两端分别调用 `SetAnimation(animation + 1)` 和 `SetAnimation(animation + 2)`（L63657—63669）。

原 ARM 可直接证实：0x349C0 读取 `[r4,#0xc]`，0x349C4 调用 0x2E570；0x34A34—0x34A44 读取同字段、加 1，再调用；另一端同理加 2。可用 `python _prep/tools/disassemble_original.py CBullet4Bind --max-bytes 1024` 重现。

`CSpritePlayer::SetAnimation`（L58861，0x2E570）只在请求编号超出动画数量时夹到最后一个有效编号；否则按 `animations + 16 * animation` 选择序列并计算首帧边界。没有“编号 1 表示编号 0”的通用别名。

`Bind` 中看似未知的虚调用 `vtable + 156`（L63656）也已经排除：原件虚表 0x402864 的目标是 0x35598，即 `CBullet::SetForceDraw(bool)`，只写弹体 `+460`（L64036）。它不是动画/flags 初始化钩子。

## 玩家与敌人发射

`CEnemy::FireBullet`（L71190，0x3E494）的顺序为：从池中取弹体 → 计算炮口位置和方向 → `CBullet::Bind` → `CBullet::Configure` → 设置伤害倍率 → `CBullet::Fire` → 加入关卡。ARM 0x3E580、0x3E5C0、0x3E608、0x3E61C 依次证实这些调用。伤害读取的虚调用 `+72` 已由原虚表证实是 `CBullet::GetDamage`（0x30194），不会改变动画。

玩家 `CGun::FireBullet`（L128076，0xA3CF8）同样调用 `Bind`（L128124）和 `Configure`（L128147），随后通过持有者调用发射；`CBrother::FireBullet`（L136422，0xB09A0）修改伤害后调用同一 `CBullet::Fire`（L136454）。

`Configure`（L62251，0x32D50）写入枪实例、位置、速度和命中/轨迹参数，玩家路径额外计算熟练度伤害；`Fire`（L62193，0x32C34）写入层、拥有者、阵营、碰撞查询结果。两者没有修改主体动画或原型，也没有“敌人光束使用另一种铺贴器”的分支。

## Flow 与继承虚调用

**校正记录：初稿误把解包物理文件号当作 BULLET 逻辑序号，导致 Flow 部分引用了其他弹体；本节已撤回初稿数据并用 BIG TOC/keyset 重新定位。** 正确映射为：

| 逻辑弹体 | 带类型 handle | BIG 逻辑资源 ID | BIG 物理序号 | BIG 偏移 | 解包文件 |
| --- | --- | --- | --- | --- | --- |
| BULLET87 | 0x0300015A | 346 | 91 | 0x5E73 | pack5_xga_0091_0x5e73.bin |
| BULLET104 | 0x0300016B | 363 | 108 | 0x6743 | pack5_xga_0108_0x6743.bin |

复核脚本 `obj/haven-logical-beam-map.py` 直接从当前 BIG 读取，不依赖文件名推测；两项载荷均与表中样本逐字节相等。SHA256 分别为 `0d47f18cccb083d8d22a47fd1e26473d25a52cec5eb3e00d51b5c49dbed2c955`、`9f39e6c7c2e20d45bc18dd8dfcf9a91bbd4f3eba8057b5c33887820c5ca1cb97`。

`CBullet::OnSpawn`（L62389，0x32FAC）调用脚本导出 0。真正的两份脚本均将导出 0 映射到内部函数 1，再调用初始化函数 0、写 `CBullet.variable[1] = 1000`、进入状态 0，且 state sequenceCount 均为 0。

| 内容 | BULLET87 | BULLET104 |
| --- | --- | --- |
| local[0] 寿命毫秒 | 1000 | 2000 |
| local[1] / CBullet.variable[1] | 1000 | 1000 |
| local[3] 效果槽 | 0 | 0 |
| native 0x090F | (5,2,400,8,10) | (5,2,400,8,10) |
| native 0x0910 | (200,80,255,255) | (200,80,255,255) |
| 显式 native 0x0909 | 无 | (5) |
| 进入状态 | 0x090E(local[0],7)，生成效果槽0 | 相同 |

对应 `CBullet::FunctionResolver`（L61101，0x317C4）：0x090E 设置原始毫秒延时及到期内部函数索引（L61313，写 +436/+440）；0x090F 调用 `SetLightning`（L61318，原实现 L60480 / 0x3051C），创建附加电弧并传入参数；0x0909 设置 Z-order group（L61290，写 +448）；0x0901 生成效果；到期函数 7 调用 0x0905 移除。`VariableResolver`（L60550）slot 1 返回弹体 `+32`，不是 flags `+28` 或主体动画；两个弹体在此写入相同值。

**0x0910 在当前原程序中没有颜色处理。** 反编译未列 case 16；本次回到 ARM 跳表确认：基址 0x31834，slot 16 位于 0x31874，值为 0x670，目标为 0x31EA4 的统一返回出口。故不能仅根据四个参数把它实现成设颜色或修改精灵的函数。这一事实可用 `obj/haven-resolver-arm.txt` 复核。

附加电弧是两份资源共同要求的行为，并非 Boss 独有；上述脚本仍然没有切换主体动画，也不会把动画 1 改为 0。不过初稿漏列电弧，不能继续把原版光束描述为只有精灵三段；真实外观还包含 `Draw` 后续的 `CLightningArc` 分支（L63040 起）。

`CScriptInterpreter::SetState`（L107323，0x81280）确实可以调用宿主 OnStateChanged、OnMoveChanged。因此不能仅凭 `Bind` 就排除状态机修改。然而原件 ARM 进一步确认：

| 原符号 | 地址 | 指令 |
| --- | --- | --- |
| CBullet::OnStateChanged 及其 thunk | 0x30150 / 0x3014C | bx lr |
| CBullet::OnMoveChanged 及其 thunk | 0x30158 / 0x30154 | bx lr |

这些小函数未出现在现有反编译正文，但保留在原符号和机器指令中；均为空实现，没有漏掉动画修改。

## Draw 的真实铺贴

`CBullet::Draw`（L62489，0x33130）的精灵光束分支（L62957—63039）使用当前主体帧边界，按长度方向重复绘制；flags `0x200` 控制是否绘制两端，`0x400` 跳过整组精灵段。没有按弹体逻辑 ID 或敌我阵营选择主体动画。

本次重新从有效代码块 0x33DFC 开始反汇编，避免从函数头线性扫描遇到 0x336AC 附近内嵌常量而提前终止的问题。关键指令如下：

- 0x33E40 调用主体 `GetBounds`；0x33E4C 读取弹体 `+224`，即主体精灵的 cached bounds.height。
- 0x33EA8 计算光束长度除以 spriteScale，截断为局部长度。
- 0x33F28、0x33F30 读取两端的 height；未设置 `0x200` 时从局部长度减去两端高度和的一半。
- 0x33F8C—0x33FA0 计算 `floor(bodyLength / bodyHeight) + 1`；0x33FAC—0x33FB8 将长度方向缩放为 `bodyLength / (segmentCount * bodyHeight)`。
- 0x33FCC 读取之前 `GetBounds` 得到的 height，0x33FD8 将其乘以 0、-1、-2……，0x33FE8 调用主体 `CSpritePlayer::Draw`。

因此原实现确实保证各个**边界矩形**按长度铺满，但不能保证矩形中的非透明像素本身没有间隔。这一区别解释了为何仅验证“段数和步长正确”不足以证明视觉连续；它不证明哪种外观才是原版最终效果。

## 未解决问题与后续边界

1. 未取得同一 iOS/BIG 版本的原版实机截图或帧捕获，不能认定原版视觉外观。
2. 本次排除的仅为绑定、敌我发射差异、已用 Flow native、空状态/动作回调和铺贴数量/步长；图集转换与完整图像装配仍需资源审计。
3. 没有为此修改 src/tests，也没有把动画 1 改为 0。原件和已有样本只读。

复现本次机器指令审计：`python obj/haven-consumer-audit.py`；退出码 0。敌人链：`python _prep/tools/disassemble_original.py CEnemy10FireBullet --max-bytes 500`；退出码 0。项目根目录不存在 `.venv`，故本次使用现有 Python，Capstone 由既有只读工具从 `_prep/out/research-python` 加载。

