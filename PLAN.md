# Gun Bros 重写计划

用 C++ 在 Windows 上重写 Glu Mobile 的自研引擎，**运行时直接读原版 `.big`**，让游戏跑起来。

参考资料：
- [`_IDA_OUT/gunbros_3.6.0_IOS.c`](_IDA_OUT/gunbros_3.6.0_IOS.c) — 反编译产物，**唯一真相来源**
- [`_IDA_OUT/source_tree.md`](_IDA_OUT/source_tree.md) — 还原出的原始源码树 + P0~P3 优先级
- [`_Big_tool/`](_Big_tool/) — `.big` 格式逆向笔记，**仅供参考，不保证正确**
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
| 渲染 | OpenGL 3.3 Core + glad |
| 图像 | stb_image |
| 解压 | zlib（只需 `inflate`） |
| 构建 | Visual Studio / MSBuild（`gun_bro_re.slnx`，工具集 v145） |
| 第三方库 | 源码内置于 `src/third_party/`，不用包管理器 |

### 关键决策与理由

**为什么 C++ 而不是 C#** — 决定性因素是**能否推迟理解**。开发模式是对着 IDA 伪代码逐函数啃，而伪代码里全是 `*(_DWORD *)(v30 + 544)` 这种指针算术。C++ 里可以原样粘贴、先跑起来、之后再查清 offset 544 是什么字段；C# 里必须先搞懂对象布局才能写出第一行能编译的代码。这个成本要乘以 3 万个函数。次要理由：`CVector.inl` 是 C++ 模板源码；引擎对象模型是 `CClass` + 虚表 + 多继承 thunk，C++ 原生映射。

**为什么直接用 VS 工程而不是 CMake** — 只跑 Windows、只有一个可执行目标、第三方库全部源码内置。CMake 的三个卖点（跨平台、依赖发现、多生成器）在这里一条都用不上，留着只是在 IDE 和编译器中间多垫一层间接。工程交给 VS 自己维护，加文件右键就完事，不用记 `add_executable` 的语法。代价是日后要上 Linux 得重新配一套——`_IDA_OUT` 里的东西还够啃一年，那天很远。

**为什么第三方库源码内置而不是 vcpkg** — 依赖只有 zlib 一个，而且只用 `uncompress` 一个函数。为它装一整套包管理器、维护 manifest、在每台机器上先跑 bootstrap，成本远大于把 15 个 .c 文件拖进工程。等 SDL3 进来时再重新评估。

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
│   │   ├── engine/       ← platform/shared 保留的 ~95 个
│   │   │   └── platform/ ← SDL3 + GL 上下文（原 platform/shared/cocoa 的角色）
│   │   ├── gun_bros/     ← src/gunbros 保留的 ~160 个
│   │   └── shaders/      ← 还原出的 7 个，改写为 GL 3.3
│   └── third_party/      ← zlib / glad / stb_image（与项目平级，不混进自己代码）
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
体积也有 220M，一律不入库；逆向资料 `_IDA_OUT/ _Big_tool/` 入库。

---

## 四、里程碑

每个里程碑都有**能看到的验收结果**，做不出来不算完成。

### M0 — 骨架

VS 工程 + SDL3 窗口 + glad 加载 + 主循环。

> **验收**：能开关的黑窗口，稳定 60fps，`glGetString(GL_VERSION)` 打出 3.3。

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

### M3 — 画得出地图

`TILESET`(Section 25) + `TILELAYER`(Section 24) + `CLayerTile` 拼图 + 相机。

照抄两个细节：`tileId == 255` 跳过不画；图层比画布小时**行列取模环绕平铺**。图块尺寸恒 256×256，绘制边长取 `tiles[0].w`。

> **验收**：完整显示一关静态地图，鼠标可拖动查看，图块无错位。

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
| **体系外资源寻址未解**：pack1 有 442 个、pack5 有 684 个资源不在 Section 表内 | M3/M4 可能取不到特效贴图和音效 | 撞上再攻，先用内容嗅探兜底 |
| **`gluScript` 脚本系统**（11 文件 + `CScriptInterpreter`） | 若逻辑大量走脚本则绕不开 | M4 前先探调用密度 |
| **`spriteGlu3`** 只有 3 个文件但可能是关键 | `CGameSpriteGluRef` 是装饰物/特效的寻址入口 | M3 时一并读 |
| **3D mesh 格式未完全解出**（Section 31） | 影响角色模型 | 推到 M5 之后，先用精灵占位 |

---

## 七、进度

- [ ] M0 骨架
- [ ] M1 读得到资源
- [ ] M2 看得到贴图
- [ ] M3 画得出地图
- [ ] M4 动得起来
- [ ] M5 打得起来
