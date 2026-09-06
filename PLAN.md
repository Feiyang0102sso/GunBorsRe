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

### M4 — 动得起来

资源模板系统（`gameObject` / `gameObjectPack` / 各 `Template::Init`）+ `brother` + `moveSet` + 精灵动画 + `input`。

> **验收**：角色站在地图上，键盘/手柄能移动，播放行走动画，相机跟随。

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
| **`gluScript` 脚本系统**（11 文件 + `CScriptInterpreter`） | 若逻辑大量走脚本则绕不开 | M4 前先探调用密度 |
| ~~**`spriteGlu3`** 只有 3 个文件但可能是关键~~：已解。`CGameSpriteGluRef` 走 packHash + archetype，不带资源 ID | 已消除 | M3.1 实测：22 张图 1653 个 PROP 全部画出，0 失败。特效贴图那批"体系外资源"正是走这条路 |
| **3D mesh 格式未完全解出**（Section 31） | 影响角色模型 | 推到 M5 之后，先用精灵占位 |
| ~~**寻路图层格式未解**（`CLayerPathLink` type 5 / `CLayerPathMesh` type 6）~~：已解，见 `_Big_tool` 的 `map.bt` | 已消除 | M3.1 补齐了两者的解析（只跳过不保留）。之前遇到它们就停，藏在后面的对象层读不到 |

---

## 七、进度

- [x] M0 骨架 —— 2026-09-05 完成
- [x] M1 读得到资源 —— 2026-09-05 完成
- [x] M2 看得到贴图 —— 2026-09-05 完成
- [x] M3 画得出地图 —— 2026-09-05 完成
- [x] M3.1 地图上的装饰物 —— 2026-09-05 完成
- [ ] M4 动得起来
- [ ] M5 打得起来
