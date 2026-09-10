# 地图建筑与角色遮挡修复

## 阶段方案与验收

用户报告玩家从大型建筑上侧经过时像站在屋顶，下侧正常。本次修复绘制链，不修改碰撞体积、地图位置或图片数据。

- [x] 新增真实 BIG 建筑与玩家模型像素复现；修前 `--map-occlusion-check --mute` 退出1，三个地图建筑后方仍露出1112–1311个应被遮住的像素。
- [x] 核对原地图／PROP模板及排序、绘制消费者。
- [x] 将建筑、玩家、兄弟与敌人放回同一主层排序，保留背景／前景独立遍历。
- [x] 验证四图前后遮挡、透明区域、实际战斗及已有特效；更新Release。

## 已确认依据

`maps/map.bt` 的 ObjectInstance 按原 `InitializeObjects :126576` 读取有符号世界x/y；`entries/prop_template.bt` 区分主精灵、背景／前景动作和实体／弹体碰撞。没有新的磁盘字段或资源表。

`CRenderQueue::Compare :145029` 先比较组，再比较整数Z顺序；`CProp::GetZOrder :123396`、`CEnemy::GetZOrder :67233`、`CBrother::GetZOrder :134176` 都取实体世界Y，向零截断。三者有主层时组号都是3。`CProp::GetZOrderGroup :123406` 对仅背景／仅前景分别返回0／6。

`CLevel::Draw :120614–120632` 将地图、实体、粒子加入同一队列；`CRenderQueue::Draw :145123` 排序后依次遍历所有对象的背景、主层、前景。`CProp::Draw/DrawBackground/DrawForeground :123542–123558` 分别使用三个独立SpritePlayer。

重建 `BuildGeometry` 虽给建筑之间排序，却把建筑的三个层全画完后才调用 `DrawModels`，生存模式又随后画兄弟和动态敌人。角色从未参与建筑排序，因而始终盖在建筑上。这解释了上侧错误而下侧碰巧正常；不是本次复现点的碰撞数据缺失。

## 验证记录

实现位于 `src/gun_bros_re/runtime/MapScene.cpp`：`BuildGeometry` 生成地面及背景；`DrawMapObjects` 按组和整数Y交错提交主精灵与模型，再画前景。模型前先提交待绘制的2D批次，每个模型独立清深度，保留模型内部深度测试，避免前一个模型的深度值破坏后续对象的排序。`CMeshCamera::DrawHeirarchy :99263` 调用渲染接口清理标志4；Windows使用 `GL_DEPTH_BUFFER_BIT` 承接。角色矩阵、缩放、眩晕偏移均沿用原实现，未改碰撞和移动。

地图查看器、正式生存的玩家／兄弟／动态敌人使用同一函数。相同整数Y使用宿主稳定顺序；原 `qsort` 对相等元素的先后未定义，不声称该极端情况逐像素一致。

所有运行统一 `--mute`。命令前缀为 `bin/x64/Release/gun_bros_research.exe`：

| 命令 | 结果与日志 |
|---|---|
| `--map-occlusion-check` | 退出0；`out/map-occlusion-final.log`。四图八种前后位置与独立按层合成参考的显著RGB差异均为0；旧“角色最后画”顺序会额外露出2190像素；四图前方玩家分别可见1143／1359／1108／1114像素。 |
| `--survival-check --brother --map pack2 7` | 退出0；`out/map-occlusion-survival-pack2.log`，两波35击杀。 |
| `--survival-check --brother --map pack7 6` | 退出0；`out/map-occlusion-survival-pack7.log`。 |
| `--survival-check --brother --map pack9 0` | 退出0；`out/map-occlusion-survival-pack9.log`。 |
| `--survival-check --brother --map pack12 0` | 退出0；`out/map-occlusion-survival-pack12.log`。 |
| `--boss-check` | 退出0；`out/map-occlusion-boss.log`，四图Boss检查无失败。 |
| `--powerup-play-check` | 退出0；`out/map-occlusion-powerup.log`，包含实际击杀浮字、空袭、库存及恢复战斗检查。 |

Release构建命令 `MSBuild gun_bro_re.slnx /p:Configuration=Release /p:Platform=x64 /m /v:minimal /nologo` 退出0，日志 `out/map-occlusion-final-build.log`。仍有既存C4244转换警告。`git -c core.safecrlf=false diff --check` 通过。

永久保留研究菜单 **79** / `--map-occlusion-check`。截图 `out/map-occlusion-pack2-0.png` 显示岩石遮住上侧玩家；`out/map-occlusion-pack2-1.png` 显示玩家处于岩石前方。`pack7` 的建筑开口中玩家保持可见，证明没有用建筑矩形粗暴覆盖；`pack9`／`pack12` 同时覆盖部分遮挡。测试选择真实内部障碍物并独立开启正常alpha混合，原始BIG与存档只读。

边界：本次恢复地图物件与角色的共用主层排序。武器特效及地图粒子的既有分组宿主仍保留，未声称完整移植原 `CRenderQueue` 全部对象类型、阴影通道或任意同Y顺序；也未把这次回归当作所有地图碰撞均已验证。
