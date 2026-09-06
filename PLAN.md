# Gun Bros 重写计划

用 C++ 在 Windows 上重写 Glu Mobile 的自研引擎，**运行时直接读原版 `.big`**，让游戏跑起来。

参考资料：
- [`_IDA_OUT/gunbros_3.6.0_IOS.c`](_IDA_OUT/gunbros_3.6.0_IOS.c) — 反编译产物，**唯一真相来源**
- [`_IDA_OUT/source_tree.md`](_IDA_OUT/source_tree.md) — 还原出的原始源码树 + P0~P3 优先级
- [`_Big_tool/`](_Big_tool/) — `.big` 格式逆向笔记，**仅供参考，不保证正确**（本地文件，不入库）
- [`_IDA_OUT/CVector.inl`](_IDA_OUT/CVector.inl) — 误打包进 IPA 的一手引擎源码

---

## 一、目标与非目标

**目标**：行为对得上原版，能玩。

**非目标**（明确排除，避免中途摇摆）：

- 不做二进制级等价复刻，不追求逐函数一致
- 不解包资源，不做资源转换管线
- 不做联网、多人、广告、内购、成就、社交
- 不支持 hvga，只跑 xga
- 暂不跨平台，先 Windows 跑通

---

## 二、技术栈

| 项 | 选择 |
|---|---|
| 语言 | C++17 |
| 窗口 / 输入 / 音频 | SDL3 |
| 渲染 | OpenGL 3.3 Core，加载器自己写（~30 个函数，`engine/platform/GLLoader.h`） |
| 图像 | PNG 编解码自己写（`engine/CPNG.cpp`），不用 stb_image |
| 解压 | zlib（`inflate` 给资源和 PNG，`deflate` 只给截图） |
| 构建 | Visual Studio / MSBuild（`gun_bro_re.slnx`，工具集 v145），只支持 x64 |
| 第三方库 | 只有 zlib（源码内置）和 SDL3（官方预编译包），不用包管理器 |

**glad 和 stb_image 最终都没用**（M2 时决定）。原因是成本账反了：我们只会用到几十个 GL
函数，glad 生成的是 4000+ 行；而全部 1534 张 PNG 只有 3 种变体（8-bit，非隔行，
colortype 6/2/3），自己写解码器约 180 行，且出问题能直接调。引入外部代码要省的那
260 行，抵不上多出来的 4300 行怀疑对象。

**SDL3 用官方预编译 VC 包**（`src/third_party/SDL3/`，include + x64 的 lib/dll），
而不是源码内置——源码内置意味着往 vcxproj 里塞几千个文件，或者被迫引入 CMake，
两条路都和"简洁"背道而驰。

### 关键决策与理由

**为什么 C++ 而不是 C#** — 决定性因素是**能否推迟理解**。开发模式是对着 IDA 伪代码逐函数啃，而伪代码里全是 `*(_DWORD *)(v30 + 544)` 这种指针算术。C++ 里可以原样粘贴、先跑起来、之后再查清 offset 544 是什么字段；C# 里必须先搞懂对象布局才能写出第一行能编译的代码。这个成本要乘以 3 万个函数。次要理由：`CVector.inl` 是 C++ 模板源码；引擎对象模型是 `CClass` + 虚表 + 多继承 thunk，C++ 原生映射。

**为什么直接用 VS 工程而不是 CMake** — 只跑 Windows、只有一个可执行目标、第三方库全部源码内置。CMake 的三个卖点（跨平台、依赖发现、多生成器）在这里一条都用不上，留着只是在 IDE 和编译器中间多垫一层间接。工程交给 VS 自己维护，加文件右键就完事，不用记 `add_executable` 的语法。代价是日后要上 Linux 得重新配一套——`_IDA_OUT` 里的东西还够啃一年，那天很远。

**为什么第三方库源码内置而不是 vcpkg** — 依赖只有 zlib 一个，而且只用 `uncompress` 一个函数。为它装一整套包管理器、维护 manifest、在每台机器上先跑 bootstrap，成本远大于把 15 个 .c 文件拖进工程。等 SDL3 进来时再重新评估。

> M2 时已重新评估：SDL3 用官方预编译包，只有 include/ + lib/x64/ 两样东西，
> 结论不变——仍然不需要包管理器。这批二进制入库，见第三节版本控制。

**为什么 GL 3.3 Core 而不是 bgfx / SDL_GPU** — 原引擎的渲染能力集小到极点（纯色 / 纯纹理 / 纹理×顶点色，一个 mvp 矩阵，alpha blend + alpha test + 深度测试，无纹理压缩、无光照、无后处理），任何现代 API 都绰绰有余，选型依据只能是**翻译成本**。GL 3.3 与 ES2.0 是同一心智模型，`CGraphics_OGLES2` 的每个方法基本一一对应，原 shader 改几个关键字就能用。多一层框架抽象就多一个怀疑对象，会干扰"对着反编译逐行验证"。

---

## 三、目录结构

```
gun_bro_re/
├── gun_bro_re.slnx            解决方案（VS 新版 XML 格式）
├── gun_bro_re.vcxproj         唯一的工程，源文件列表都在这
├── gun_bro_re.vcxproj.filters 解决方案资源管理器里的目录树
├── .gitignore
├── PLAN.md
├── src/
│   ├── gun_bros_re/
│   │   ├── main.cpp      ← 只做参数解析和里程碑分发
│   │   ├── engine/       ← platform/shared 保留的 ~95 个
│   │   │   └── platform/ ← SDL3 窗口 / GL 上下文 / GL 加载器
│   │   │                   （原 platform/shared/cocoa 的角色）
│   │   ├── gun_bros/     ← src/gunbros 保留的 ~160 个
│   │   ├── sprite_glu/   ← src/spriteGlu3 的 3 个，与 gunbros 平级的独立子系统
│   │   ├── milestones/   ← 每个里程碑一个验收台，main 保持薄
│   │   └── shaders/      ← 7 个，从反编译里的 GLSL 原文改写为 GL 3.3
│   └── third_party/      ← zlib（源码）/ SDL3（预编译）
│
├── big/                  26 个 .big + packTOC（运行时直接读，不解包）
├── png/  mp3/  glu_logo/ 散资源
├── gunbros               原 ARMv6/v7 FAT 二进制
├── _IDA_OUT/  _Big_tool/ 逆向资料（CVector.inl 在 _IDA_OUT/ 下）
└── _junk/                iOS 打包残留，暂不删

bin/  obj/                构建输出，按 $(Platform)\$(Configuration) 分开，不入库
```

**资源路径**：工程注入 `ASSET_ROOT` 宏，开发阶段用绝对路径。不搞资源打包和路径搜索。

`ProjectDir` 形如 `E:\...\gun_bro_re\`，先把反斜杠换成正斜杠、去掉结尾斜杠，
才能直接拼进 C++ 字符串字面量（反斜杠在里面是转义符）：

```xml
<PropertyGroup Label="UserMacros">
  <AssetRoot>$(ProjectDir.Replace('\','/').TrimEnd('/'))</AssetRoot>
</PropertyGroup>

<ItemDefinitionGroup>
  <ClCompile>
    <PreprocessorDefinitions>ASSET_ROOT="$(AssetRoot)";%(PreprocessorDefinitions)</PreprocessorDefinitions>
  </ClCompile>
</ItemDefinitionGroup>
```

**版本控制**：原版资源（`big/ png/ mp3/ glu_logo/ gunbros _junk/`）是 Glu Mobile 的版权素材，
体积也有 220M，一律不入库。逆向资料里 **`_IDA_OUT/` 入库，`_Big_tool/` 不入库**——
后者是本地笔记和一次性脚本，结论一旦确认就应该搬进代码注释或 PLAN，
留在库外反而少一处需要同步的真相。

> M1 对 `.big` 格式的校正记录写在 `_Big_tool/refs.md` 和 `tocs.md` 里，只存在于本地。
> 关键结论（handle 位域、聚合寻址、包哈希算法）已经落到
> `CBigFileReader.h` 和 `CAggregateResource.h` 的注释里，不依赖那两份笔记。

**第三方二进制**：`src/third_party/SDL3/lib/x64/` 的 `.lib` 和 `.dll` **入库**，
否则 clone 下来链接不了。`.gitignore` 里为此开了例外——`x64/` 和 `*.lib` 两条规则
本来会吞掉它们，而 git 不允许在父目录被排除的情况下恢复其中的文件，
所以要先恢复目录再恢复文件。

---

## 四、里程碑

每个里程碑都有**能看到的验收结果**，做不出来不算完成。

### M0 — 骨架

VS 工程 + SDL3 窗口 + glad 加载 + 主循环。

> **验收**：能开关的黑窗口，稳定 60fps，`glGetString(GL_VERSION)` 打出 3.3。

> 实测：`GL 3.3.0 NVIDIA 546.92`，开 vsync 后稳定 165fps（锁在显示器刷新率）。
> M0 与 M2 合并实现——单独的黑窗口没有信息量。

### M1 — 读得到资源 ★第一块地基

照反编译实现容器解析：

| 文件 | 原位置 |
|---|---|
| `CBigFileReader` | `platform/shared/components/` |
| `CResourceBigFile` | `platform/shared/systems/` |
| `resPackTOC` / `resTOCManager` | `src/gunbros/` |

链路：`packTOC_xga.dat`（**大端**）→ `FGIB` 头 → table1 游程映射 → table2 偏移表 → 资源块头 → zlib 解压。

注意：文件名是 `CResTOCManager::Init` 里逐段拼的（`"p"` + … + `"_"` + artset + `"."`），artset 写死 `"xga"`，`pack0_core` 有特判。

> **验收**：命令行列出 `pack0_core_xga.big` 全部资源的 `id / groupHash / 大小 / 压缩标志`，随机抽 20 个解压后校验魔数（PNG / RIFF）。与 `_Big_tool` 输出交叉比对，**不一致以反编译为准**并记录差异。

### M2 — 看得到贴图

PNG 解码 + 纹理上传 + 7 个 shader 移植 + 最简 2D 批处理（dynamic VBO 每帧重填，先不做架构）。

shader 移植改动：`attribute`→`in`、`varying`→`out`/`in`、`gl_FragColor`→自定义输出、`precision` 限定符去掉、`glAlphaFunc` 用 `discard` 模拟。

**必须原样照抄**：UV 缩放 `0.0002441406255`（= 1/4096）。贴图坐标在美术数据里是整数存的，改了全部错位。

> **验收**：窗口里显示一张从 `.big` 取出的 PNG，尺寸和颜色正确。

> 实测：三种 colortype 各验一张——`pack0_core` 313（512x512 RGB，角色贴图集）、
> 316（136x114 RGBA，武器图标，alpha 混合正确）、`pack1` 467（512x512 调色板，敌人贴图集）。
> UV 缩放常量随之验证：错了图会整体错位。
> `--screenshot <file.png>` 存下首帧后退出，里程碑不需要人盯着也能验收。

### M3 — 画得出地图

`TILESET`(Section 25) + `TILELAYER`(Section 24) + `CLayerTile` 拼图 + 相机。

照抄两个细节：`tileId == 255` 跳过不画；图层比画布小时**行列取模环绕平铺**。图块尺寸恒 256×256，绘制边长取 `tiles[0].w`。

> **验收**：完整显示一关静态地图，鼠标可拖动查看，图块无错位。

> 实测：22 张地图里 20 张正常画出，图块无错位。默认缩放到适应窗口（能一眼看完整关），
> 鼠标拖动平移、滚轮缩放。`--maps` 列出含地图的包，`--map <pack> <n>` 选图。
> 环绕平铺在 pack2 map 4 上验证：12x5 的岩浆层铺满 13x5 画布。
>
> **pack7 的 map 0~4 画出来是空的，这不是缺陷**——它们的图块层数据本身全是 255。
> map 1 和 map 2 已完整解析所有图层仍是 0 个图块，可以确证。这几关的视觉内容全在 PROP 里，
> pack7 有 69 个 PROP 模板，是所有包里最多的。
>
> **缩小查看时图块边缘有淡接缝**，是无 mipmap 的缩小走样，不是拼接错位——
> 1:1 查看时干净。原版同样没有 mipmap，所以保持一致，没有加。

#### M3.1 — 地图上的装饰物

`CLayerObject` + `CProp::Template` + `spriteGlu3` 三件套，把 PROP 画到地形上。

先补齐了 `CMap` 的图层解析：借 [`_Big_tool/binary template/big_assets/maps/map.bt`](_Big_tool/binary%20template/big_assets/maps/map.bt)
（对 83 张地图验证过）拿到 PathLink / PathMesh 的布局，与反编译逐字段核对后写进
`CMap.cpp`。之前遇到路径图层就停，藏在它们后面的对象层根本读不到——补齐后
props 从 1624 涨到 1653。现在每张地图的所有图层都读到资源末字节，剩余 0 字节。

寻址链五级：对象层 → PROP 段基址 + localIndex → `CGameSpriteGluRef`(packHash +
archetype + action + anim) → archetype 资源 → anim → frame → sprite → spriteMap
→ 包级 imageSlot 表 → archetype 的 rect 表 → 图集页 PNG。

> **验收**：完整地图上出现石堆、管道、传送门、岩浆池。

> 实测：22 张地图共 **1653 个 PROP，0 个解析失败，0 个精灵部件画不出**。
> **pack7 的 5 张"空图"现在全都有内容了**，证实了上面的判断。
>
> 查看器把全部 22 张图排成**一条扁平列表**：左右键逐张走，走到一个包的末尾直接进下一个包；
> 上下键整包跳。`--map` 只决定从哪张开始，不再是切包的唯一手段。
> `--maps` 会把这条列表连同全局编号一起列出来。`T` / `P` 开关地形与装饰物。
>
> 顺带修了一个**潜伏的跨包寻址 bug**：图块集和图集原先拿"地图所在的包"去解析，
> 把引用自带的 packHash 丢掉了。照 `CGunBros::GetGameObject` 的做法，
> packHash 是地址的一部分——**先选包，再加段基址**。实测这 69 个引用目前碰巧都指向自己的包，
> 所以画面没错；但 PROP 已经有 5 个跨到 pack1，同样的写法迟早会踩。
> 现在所有引用统一走 `ReadSectionResource(packHash, section, ordinal)`。
>
> 三个细节照抄了原版：
> - **三个绘制槽**。`CProp` 有主 / 前景 / 背景三个 `CSpritePlayer`，261 个模板里
>   有 85 个**只填背景槽**——只画主槽会静默丢掉三分之一的布景。
> - **绘制顺序**。`CRenderQueue::Draw` 是按 `(zOrderGroup, y)` 排一次序、
>   然后整队走三遍（全体背景 → 全体主 → 全体前景），不是每个物件画完自己的三槽。
>   一个物件的前景要压在**下一个**物件的主精灵上。
> - **两级倒序**。`SetFrame` / `NextSprite` 从最后一个部件往前走，所以帧部件和
>   精灵部件都是倒着画的。顺着画的话叠放关系会整体翻过来。
>
> `CQuadBatch` 因此改了两处：分组从"按纹理合并"改成"按纹理游程切分"
> （合并会把后提交的四边形提前画，地面就盖到石头上了），以及每组带自己的混合模式
> ——spriteMap 的第三个字节是混合标志，bit6/bit7 是加法混合，不做的话
> 所有辉光贴图都带一圈黑框。
>
> **未做**：ENEMY（3D mesh）、PARTICLEEFFECT（粒子系统）、PLAYER（不可见标记），
> 合计占物件总数的 7%。sprite 替换表和图元填充矩形也没做——
> 这几个包的替换组数全是 0，图元只有 pack7 有 1 个。
> 动画不播放，每个槽固定取第 0 帧（原版取随机帧，静态查看器要可复现）。

#### M3.2 — 地图活起来

`CSpritePlayer` + 主循环时间步，把 M3.1 画出来的静止布景播起来。

动画数据在 M3.1 就已经解析进 `CSpriteGluArchetype` 了（`AnimationStep` 的帧号与时长），
只是每个槽固定取第 0 帧。这一步补的是那个缺席的时钟。

> **验收**：地图上的岩浆、辉光、传送门动起来，图块与叠放关系不变。

> 实测：22 张图 **1653 个 PROP、0 个未解析、0 个部件画不出**，与 M3.1 逐项一致。
> 其中 **138 个 PROP 会动**（模板里至少一个槽多于一步），pack9 map 0 最多，有 31 个。
> 只有 pack7 map 2 一张图完全静止。
>
> `--advance <ms>` 先把动画时钟推进指定毫秒再画第一帧，这样 `--screenshot` 也能验收动画：
> 同一张图取几个时刻截图对比即可。像素差随时间增长（pack9 map 0 相对 t=0：
> 120ms 差 2156 像素，200ms 差 4304，400ms 差 4422），且差异只落在辉光上，
> 地形与其余布景一动不动。查看器另加 `空格` 暂停、`.` 单步 80ms。
>
> **`AnimationStep` 的字段顺序原本就是对的**。`LoadArcheType`（:57552）里线上先帧号后时长、
> 内存里反过来存成 `{时长, 帧号}`——和 sprite map 那个坑同型，但 M3.1 按线上顺序读的，
> 没踩到。`kDurationUnitMs = 10` 也和原版那个 `10 *` 对得上。
>
> 三个细节照抄了原版：
> - **时间结转**。`AdvanceFrame` 进入新一步时不是把时钟重置成整段时长，而是
>   `max(新时长 + 上一步的欠账, 新时长 / 2)`。欠账让慢帧不会拉长动画，
>   下限让一个极慢帧不会无限跳步。帧率无关性就是从这来的。
> - **只有主槽随机起始帧**。`CProp::Bind`（:124916）对主槽调 `Random(0, 步数-1)`，
>   前景槽和背景槽都从第 0 步开始。不照抄的话一片岩浆会整齐划一地脉动。
>   这里用按绘制序的确定性哈希代替真随机，`--screenshot` 才还能两次跑出同一张图。
> - **单步动画回卷到自己时不动时钟**。原版在那条路径上直接返回，
>   这正是"静止布景保持静止"的实现方式，而不是靠额外判断。
>
> **绘制顺序没有跟着动画变**。排序键 `(zOrderGroup, y)` 只跟 prop 的位置有关，
> 而动画只换贴图不换位置——所以队列仍然只在加载时排一次。zOrderGroup 也仍按第 0 步判定，
> 与 M3.1 逐字节一致。`CRenderQueue` 因此**没有**抽出来：现在没有会移动的实体，
> 抽出来只是多一层没有职责的间接。等 M4a 有了玩家和敌人再说。
>
> 几何改成每帧重建。实测最大的一张图也只有 190 个图块 quad + 342 个精灵 quad，
> 对一个本来就为"每帧重填"设计的 dynamic VBO 来说无关痛痒，所以没有把图块拆出去缓存。

#### M3.3 — 背景流动起来

岩浆、海水、星空的流动，是**图块层整层漂移**，不是换帧。`CLayerTile::Update`（:126770）
维护一对偏移：`offset = frac(offset + speed × dt)`，`SetSpeed`（:126803）把参数乘 −0.05。

> **验收**：有滚动的地图上，背景层持续漂移，地形与装饰物不受影响。

> 实测：22 张图里 **6 张有滚动背景**，其余 16 张原版就是静止的。1653 个 PROP、0 未解析，
> 与 M3.2 逐项一致。
>
> | 地图 | 滚动层与速度（图块/秒） |
> |---|---|
> | pack12 map 0 | 层0 `(0, −0.075)`，层1 `(+0.06, +0.035)` — 双层视差 |
> | pack2 map 1 | 层0 `(−0.025, 0)` |
> | pack2 map 2 | 层0 `(−0.05, 0)` |
> | pack2 map 5 | 层0 `(0, −0.05)` |
> | pack2 map 7 | 层0 `(−0.06, −0.065)` |
> | pack9 map 0 | 层0 `(−0.15, −0.15)` — 最快，对角漂移 |
>
> **速度不在地图数据里**。它由关卡脚本在运行时设置：脚本调 `CLevel::FunctionResolver`
> 的 42 号函数（:117823），后者转调 `CLayerTile::SetSpeed`。原生函数 id 是 16 位，
> 高字节选类（5 = CLevel）、低字节选函数，由 `ScriptResolver::ResolveFunction`（:107633）分派。
>
> 于是这里**没有实现解释器**，而是按字节签名扫描出这些调用——见
> [`LevelScriptScan.h`](src/gun_bros_re/gun_bros/LevelScriptScan.h)。
> 调用是固定的十字节形状（`00 2A 05 03` + 三个 uint16），且这批档案里三个参数全是字面量
> （bit15 标记字面量，`CScriptInterpreter::CallFunction` :107496），所以数值是**读出来的，不是编的**。
> 代价是扫描判不出可达性：写在条件分支里的滚动会被当成无条件。这批档案里没有这种情况
> （命中全在关卡开头的初始化段），但扫描本身证明不了。**M4a 做出 `CScript` 后这个文件应该删掉。**
>
> **偏移只留小数、丢掉整数部分**，所以漂移永远在一个图块内循环。这只有在"整层平移一格看不出区别"时
> 才无缝——也就是均质填充的岩浆、海水、星空，恰好就是唯一被滚动的那几层。
> 结论：**滚动完全不需要移动格子索引，只要平移绘制位置**，环绕平铺（M3 就做了）负责其余部分。
>
> 漂移会在让开的那一侧空出一条不到一格的缝，所以那一侧要多画一列（或一行）。
> 只补**漂移方向对侧**那一边：四边都补的话，整张图会多出一圈可见的边框。
> 这一格在原版里永远看不到（相机不会走出地图），但本查看器是整图适配的，所以它露在地图外沿。

#### M3.4 — 脚本跑起来

照反编译实现 `CScript` + `CScriptInterpreter`，执行档案里的字节码，
把 M3.3 那个字节签名扫描换掉。

**从 M4a 里拆出来单列**，因为它的验收不需要玩家实体——滚动速度改由执行得出，
在现有地图查看器里就能验完。M4a 因此瘦回出生点 / 输入 / 相机 / 碰撞。

##### 内核做完，resolver 只填最小集

| 部分 | 内容 | 伪代码 | 这一步 |
|---|---|---|---|
| **内核** | `CScript::Load` + 8 个语句的 `Execute`/`Skip` + `CScriptState` + `CScriptInterpreter` | ~2300 行（:105677–:108010） | 全做 |
| **Resolver** | 12 个类的 `FunctionResolver` / `VariableResolver` | ~2800 行，约 230 个函数 | 只做 `CLevel` 最小集 |

**Resolver 不是地基，是接口，必须跟着各子系统增量长。** `CLevel::FunctionResolver`（:117365）
有 82 个 case，绝大多数要调 `CEnemy` / `CBrother` / `CResourceLoader` / `Mission`——都是 M4、M5 的东西，
现在硬做只能写 82 个空壳。这一步只填 42 号（设置图块层滚动速度）和 8 个 `CLevel` 变量
（`CLevel::VariableResolver` :114371，很浅，全是对象里的 int16 字段），
其余调用打一行 log 记下 id 和参数就返回——log 攒出来的就是 M4 / M5 该补哪些 case。

##### 格式已解出

七张 uint8 计数前缀的表（`CScript::Load` :106317）：导出函数索引 ×2 / 资源引用 /
数据块 / 变量初值 / 状态 / 全局函数体。`CScriptCode` 是 `[uint8 字节长][uint8 语句数][语句…]`，
长度前缀让 `Skip` 成为 O(1)——条件分支的未命中臂就是这么跳过去的。

八个语句 opcode（`CScriptCode::Execute` :106801）：
`0` 函数调用、`1` 切状态、`2` 变量运算（9 种，含"右值取函数返回值"）、
`3` 事件（`Execute` 时跳过，只在 `Evaluate` 时匹配）、`4` 条件（带 else-if 链，
比较符 11 种，`6` 是无条件 else）、`5` 返回值。

操作数四种寻址（`GetData` :107478）——**这正是 M3.3 只敢认字面量的原因**：
bit15 字面量、高字节非 0 走 `ResolveVariable`、低字节 <0xFA 是脚本局部变量、
≥0xFA 是函数实参寄存器。

`CScriptState` 是**带继承的状态机**：36 字节里有帧序列、若干带 id 的导出函数、
进入块、退出块，外加一个 `parent` 字节（255 为无）。`GetSequence` / `Evaluate` / `OnExit`
查不到就沿 parent 链往上爬。

> **验收**：全部脚本资源**解析到末字节剩余 0**；干跑 6 张滚动地图的关卡脚本，
> 42 号函数拿到的数值与 M3.3 那张表**逐项相同**；`LevelScriptScan` 删除——
> 但要等这条交叉验证通过之后再删，它现在是回归基线。

> 实测：**23 个关卡模板全部解析到末字节，剩余 0 字节，0 个读越界**，其中 19 个带脚本——
> 与 M3.3 量出的密度分毫不差。`--levels` 现在解析整个模板而不是嗅探字节，这条就是
> 格式读对了的证明。
>
> 6 张滚动地图的 7 个滚动层，数值与 M3.3 那张表**逐项相同**。交叉验证跑完 22 张图
> **0 处不一致**，`LevelScriptScan` 随即删除。
>
> | 地图 | 脚本传的参数 | ×(−0.05) 之后 |
> |---|---|---|
> | pack12 map 0 | 层0 `(0, 1.5)`，层1 `(−1.199, −0.699)` | `(0, −0.075)`，`(+0.060, +0.035)` |
> | pack2 map 1 | 层0 `(0.5, 0)` | `(−0.025, 0)` |
> | pack2 map 2 | 层0 `(1.0, 0)` | `(−0.05, 0)` |
> | pack2 map 5 | 层0 `(0, 1.0)` | `(0, −0.05)` |
> | pack2 map 7 | 层0 `(1.199, 1.297)` | `(−0.060, −0.065)` |
> | pack9 map 0 | 层0 `(3.0, 3.0)` | `(−0.15, −0.15)` |
>
> **那个 255 字节上限没有成为问题**，因为编译器根本不生成长代码块：最大的脚本
> （pack12 level 0）有 **122 个状态、45 个函数**，逻辑是横着铺开的，不是纵向堆在一个块里。
> 上百状态的都是波次关，一波一个状态；其余关卡只有 1~11 个。
>
> **未实现的调用逐条记了下来，这就是 M4 / M5 的清单。** `CLevel` 的 54 号被调用 **122 次**，
> 遥遥领先，其后是 55 号（36 次）、3 号（27）、14 号（23）、6/4/2/1 号（各 19）。
> 跨类调用也有，主要落在 spawner。
>
> **文件与原始源码树一一对应**：`src/gluScript/` 的 11 个文件，按第五节的命名映射
> （小写驼峰 → `C` + 首字母大写）落成 `glu_script/` 的 11 组头 + 实现。
>
> 一处新增：`CScriptResolver.h` 里的 `IScriptObject`。原版的 host 是裸 `void *`，
> 但 `SetState`（:107323）和 `Refresh`（:107271）要调它虚表的前三项——**声明那张虚表的类
> 没能从符号表还原出来**，所以按用途起了名字。它放在 `scriptResolver` 里而不是单开一个文件，
> 因为那本来就是 gluScript 与十二个游戏类之间的那一层。
>
> `CLayerTile::SetSpeed` 收回了原版的 ×(−0.05)。M3.3 时那个因子折在调用方，理由是
> "读脚本的人已经要做一次缩放"；现在真 resolver 到位了，那个理由不成立了。

##### 顺带：地图不再画到画布边缘

`CLayerCamera`（`layerCamera.cpp`，:127663）补上了——之前只是跳过。它是一对世界像素矩形，
**地图能被看到的范围**：图块层会一直铺到画布边缘（滚动层还会环绕填充），而相机走不到那里，
原版永远不会揭穿。整图适配的查看器会，所以现在按这个矩形裁剪（`glScissor`），
适配缩放也按它算。

`CLevel` 的 **1 号函数 `setCameraLayer` 一并实现了**——参数索引的是**整个图层栈**，
不是相机层的序号（`CMap::SetCameraLayer` :92289 拿它直接索引全部图层的数组）。
22 张图里有脚本的那些全部命中，0 个落空，这本身就验证了索引语义。

> 查看器裁剪用的是**所有相机矩形的并集**，不是脚本开场选的那一个。多数地图有 2~4 个相机区域，
> 关卡推进时切换：pack2 map 0 开场那个只有 572×669，而整张图 2304×1792。
> 并集才是"游戏里能看到的一切"——22 张图**每一张的并集都小于画布**，
> 所以外沿照样砍掉，只是不会把查看器裁到关卡开场的一角。日志两个都打。

### M4 — 动得起来

**这一关拆成两半，因为角色不是精灵。** `CBrother::Template::Init`（:134571）读的是
`CMoveSetMesh`，`CBrother::Draw` 走 `CMeshCamera::DrawHeirarchy`；模板里那个
`CGameSpriteGluRef` 只在 `DrawBackground` 里用，是影子。`CGun` 和 `CEnemy` 同样是
`CMoveSetMesh`。`CMoveSet`（精灵动作集）全项目只有 `CProp` 用——也就是说
M3.2 已经把精灵动画这条线走完了，角色那条线绕不开 Section 31。

先做地基再补表现：mesh 做完了角色也还是不能动，而时间步、相机、碰撞是 M5 也要踩的。

#### M4a — 关卡与实体骨架

`CLevel`(Section 8) + 出生点 + `input` + 相机跟随 + 碰撞层。
`CLevel::Template::Init`（:114779）很浅：地图的 `GameObjectRef` + `CScript` + 3 个 uint16，
其中 `CScript` 那一段 M3.4 已经解析并跑起来了。
出生点是对象层的 type 15（`PlacedObjectType::Player`），M3.1 已经解析到，只是当成
不可见标记跳过了。碰撞层同理——有解析器，但只跳过不保留。
`CLevel::FunctionResolver` 那 82 个 case，按关卡实际用到的补，不照 id 顺序铺。

玩家先用占位：真解析 `CBrother::Template`，画它的影子精灵加一个朝向标记。
这一步也该把 `CCamera` / `CRenderQueue` 从 `M3Map.cpp` 里提炼出来——到这时才有
会移动的实体，队列才真的需要每帧重排。

> **验收**：一个东西在真关卡里按真碰撞跑，相机跟随。

#### M4b — 真角色

Section 31 mesh + `CMoveSetMesh` + `CMeshCamera`。需要给渲染层加顶点/索引缓冲和深度测试，
是独立的一块。

> **验收**：角色站在地图上，键盘/手柄能移动，播放行走动画。

### M5 — 打得起来

`bullet` + `gun` + `enemy` + `enemySpawner` + `collision` + `particle`。

> **验收**：能开枪、命中、敌人死亡、有粒子特效。**到这里游戏算"能玩"。**

之后排期：音频 → 菜单（40 个文件）→ 进度存档 → 3D mesh。

---

## 五、工作方法

1. **保留原类名和文件名**。`CBrother`、`CLayerTile`、`bullet.cpp` 一个不改。符号未剥离是最大红利，名字对得上才能随时反查伪代码。命名映射：`bullet.cpp` → `CBullet`，小写驼峰 → `C` + 首字母大写。
2. **先编译通过，再重构**。允许 `*(int*)(this + 544)` 先留着，加 `// TODO: offset 544 = ?`，跑起来后再命名。
3. **二进制解析层写测试**。M1 每个结构体解析都要有断言，这是后面一切的地基。
4. **冲突时以反编译为准**。`_Big_tool` 的笔记是参考，发现差异就记回它的 md 里。

---

## 六、已知风险

| 风险 | 影响 | 应对 |
|---|---|---|
| ~~**体系外资源寻址未解**~~：已解。所谓"不在 Section 表内"的资源住在**聚合资源**里，handle bit29 标记，走 `CAggregateResource` | 已消除 | M1 实测：5659 个条目解析 5594，41 个空引用，21 个未解（仅两个 name key，疑为引擎元数据） |
| **`gluScript` 脚本系统**（11 文件 + `CScriptInterpreter`） | **绕不开，且比预想的重**；风险已从"格式未知"转成"resolver 面积大" | 密度量出来了（`--levels`）：**23 个关卡里 19 个带脚本**。13 个模板类型有 `CScript` 字段，且脚本不只做剧情——连背景滚动这种纯表现的事都走它（M3.3）。字节码格式**已全部解出**。**内核已在 M3.4 做掉**，`LevelScriptScan` 已删。剩下的面积在 resolver：12 个类约 230 个函数，只能跟着 M4 / M5 各子系统增量长，按关卡实际用到的补 |
| ~~**`spriteGlu3`** 只有 3 个文件但可能是关键~~：已解。`CGameSpriteGluRef` 走 packHash + archetype，不带资源 ID | 已消除 | M3.1 实测：22 张图 1653 个 PROP 全部画出，0 失败。特效贴图那批"体系外资源"正是走这条路 |
| **3D mesh 格式未完全解出**（Section 31） | 挡住玩家、敌人、枪——三者都是 `CMoveSetMesh` | 隔离成 M4b。M4a 先用占位跑通关卡骨架，不让表现问题挡住玩法逻辑 |
| ~~**寻路图层格式未解**（`CLayerPathLink` type 5 / `CLayerPathMesh` type 6）~~：已解，见 `_Big_tool` 的 `map.bt` | 已消除 | M3.1 补齐了两者的解析（只跳过不保留）。之前遇到它们就停，藏在后面的对象层读不到 |

---

## 七、进度

- [x] M0 骨架 —— 2026-09-05 完成
- [x] M1 读得到资源 —— 2026-09-05 完成
- [x] M2 看得到贴图 —— 2026-09-05 完成
- [x] M3 画得出地图 —— 2026-09-05 完成
- [x] M3.1 地图上的装饰物 —— 2026-09-05 完成
- [x] M3.2 地图活起来 —— 2026-09-05 完成
- [x] M3.3 背景流动起来 —— 2026-09-06 完成
- [x] M3.4 脚本跑起来 —— 2026-09-06 完成
- [ ] M4 动得起来
- [ ] M5 打得起来
