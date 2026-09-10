# 地图黑边（tile 接缝）修复与加载指示器复核

## 范围

用户报告两处渲染问题：splash 加载小人的边框特别粗；地图上某些位置有明显黑边、像被裁剪过。本轮只改宿主渲染层，不动 BIG、解包样本和存档。

- [x] 地图黑边：定位为图集采样越界，`CQuadBatch` 统一按纹素中心取 UV。
- [x] 加载小人：逐像素核对渲染结果与原图集，确认没有偏差，记录证据与结论。

## 一、地图黑边

### 复现与定位

`gun_bros_research.exe --gameview --map pack2 7 --mute --screenshot out/render-fix/game7.png`。
该地图 `tile set 1: 2 atlases, 18 tiles, draw size 256`，图集 0 是 1024x1024，即 16 张 256x256 的 tile **紧挨着**排布，中间没有留边。相机 `zoom=2.7972`，一格 tile 占 716 屏幕像素，因此 tile 边界落在 x=78 / 794 / 1510、y=596。

用脚本统计"比左右邻列都暗"的列（每 7 行采样一次）：

| 位置 | 修复前平均压暗 | 修复后平均压暗 |
|---|---|---|
| x=78 | 15.13 | -0.11 |
| x=793 | 1.30 | -1.08 |
| x=1509 | 0.30 | 0.35 |
| y=596 | 4.79 | -0.06 |

峰值列正好是 tile 边界，且压暗幅度取决于图集里相邻 tile 的明暗——这就是黑边"像裁剪"的原因：它并不是缺像素，而是**采到了图集里隔壁那张 tile 的边缘像素**。

### 原因

`CTexture::Create` 用 `GL_LINEAR`。GL_LINEAR 取采样点周围 2x2 纹素做插值，而 `CQuadBatch::AddQuad` 之前把 UV 直接放在矩形**边界**上：

```
u0 = source.x / width          // 边界，不是纹素中心
u1 = (source.x + source.width) / width
```

四边的采样点正好压在两张 tile 的交界处，各取一半，于是把邻居的边缘混了进来。图集里 tile 之间没有 padding，所以每条 tile 边界都会出现一条 1 像素的异色线；放大 2.8 倍后就是屏幕上那条明显的黑线。

`AddTransformedQuad`（精灵、字体、特效走这条路）早就做了半纹素内缩，只有 `AddQuad`（tile 层、prop、整图 blit）没有——所以问题只出现在地图上。

### 修复

`src/gun_bros_re/engine/CQuadBatch.cpp` 抽出 `TexelCentreRect()`，两条加 quad 的路径共用：UV 各边内缩半个纹素，落在最外圈纹素的**中心**上，双线性插值的 2x2 邻域就全部留在矩形内部。翻转也一并在这里处理，因为它只是两端对调。

代价是贴图内容被压缩了 1/256（0.4%），肉眼不可见；换来的是任何图集矩形都不会再吃到邻居。

### 验证

| 检查 | 结果 |
|---|---|
| MSBuild `gun_bro_re.slnx /p:Configuration=Release /p:Platform=x64` | 退出 0，正式与研究 EXE 均更新 |
| `--gameview --map pack2 7 --screenshot` | tile 边界压暗归零，见上表 |
| `--play --mute --screenshot` | 实战视角无接缝，`out/render-fix/play-after.png` |
| `--loading-wipe-check --mute` | failures=0 |
| `--store-card-check --mute` | failures=0 |
| `--original-hud-check --mute` | failures=0 |

对比图：`out/render-fix/seam-compare.png`（左修复前、右修复后，对比度放大 3 倍）。

## 二、加载小人的蓝色描边

**结论：渲染没有问题，那圈蓝边就是原图。** 不做改动。

核对过程：

1. 临时在 `MovieRenderer::DrawSprite` 打印 archetype 0 / animation 124（原版 `CMenuSystem::Bind` :97115 就是 `SetAnimation(0x7C)`）展开出的 quad。8 个动画步各**只有一个** quad，无叠加、无重复绘制：例如 `src=19,731 113x129 off=-59,-87 flip=10 blend=0`（Alpha 混合）。
2. 该 quad 来自 `pack0_core` 图集 PNG 0338（1024x1024）。把这块矩形从解包样本里裁出来、按窗口缩放 1.5625 倍，与实际截图同位置逐像素比对：整块平均差 8.5/255，穿过发光带的扫描线两边逐像素在 ±10 以内（见 `out/render-fix/compare5.png`）。差值来自半像素对齐，不是混合方式或 alpha 处理。
3. 位置与缩放也照原版：`CMenuSystem::Bind` :97142 用 `GetBounds` 得到 118x142，取 `x = 屏宽 - 宽`、`y = 屏高 - 高 + 高/4`，`CSpritePlayer::Draw` 不传缩放。我们的 `OriginalLoadingSplash::Draw` 与之一致。

也就是说：原资源画的就是一个**黑色剪影 + 蓝色外发光**，发光带本身占 118 像素宽小人里的 7 像素左右。窗口是 1600x1200、虚拟画布 1024x768，整个 UI 连同这圈发光一起被放大 1.5625 倍并做线性插值，所以看起来比在 iPad 原生 1024x768 上粗、也更糊——这是窗口尺寸带来的整体放大，不是这个精灵单独出的问题。

## 验证边界

- tile 接缝以 pack2 map7 实测，修复在 `CQuadBatch` 公共路径上，对所有图集生效；未逐张地图复测。
- 加载小人只核对了 archetype 0 / animation 124 的静态帧与原图集像素，没有对比真机逐帧动画时序。
