# 第 50 波间歇卡顿：寻路查询缓存

## 复现与原因

用户截图：pack2 / map7，第 1 轮第 50 波，35 个敌人时约 19 FPS，32 个敌人时约 163 FPS。慢帧截图坐标为 (454.8, 688.7)，兄弟仍存活。

在该坐标保留原刷怪脚本、保持两位兄弟存活后，正常补算循环捕获到一次 50.825 ms 的更新，其中寻路查询 45.243 ms、24 次查询、每次针对 186 节点图，一帧补算 3 次。这一帧没有新敌人出生。证据：`obj/path-cache-evidence/spawn-realtime-before/performance-frames.csv`，frame 1196。

当前 `ILayerPath::FindNext` 每次调用都分配搜索数组并重新执行 Dijkstra。敌人和兄弟反复询问相同节点之间的下一步，目标位置及导航阶段比敌人总数更能解释耗时变化。一次慢更新又可能触发下一帧多次补算。这里确认了主要热点，不代表所有卡顿都来自寻路。

## 原版依据与实现边界

- `_prep/_Big_tool/binary template/big_assets/maps/map.bt:656`：PathMesh 原节点、邻接和中心位置来源，保持 BIG 数据读取不变。
- `_prep/_IDA_OUT/gunbros_3.6.0_IOS.c:167955`：`CLayerPathMesh::CalculateDistanceMap` 在目标导航格改变时才重建距离表。
- 同文件 `:170441`：`CFlock::RefreshDistanceMaps` 管理玩家、兄弟等目标的距离表。
- 同文件 `:145509`：`CLevelObjectPool::GetEnemy` 确认原版另有敌人实例池；它与本次发现的寻路重复计算是两件事。

本次在 `ILayerPath` 缓存 `(起点节点, 终点节点) → 下一节点`，包括不可达结果。首次查询仍调用未改动的搜索算法，保留相同长度路线的选择顺序。图重载、单节点锁定、批量锁定及传播锁定均使缓存失效。

这是宿主对现有查询接口的优化，不宣称完整复刻了原版 CFlock 距离表布局。没有添加资源表、降低 AI 更新频率或改变敌人数。

## 性能与行为对照

修正原性能入口遗漏固定随机种子的问题后，同一程序通过 `--uncached-paths` 切换旧搜索和缓存搜索。所有自动运行均显式 `--mute`。

| 1,200 个固定更新步 | 旧搜索 | 查询缓存 |
|---|---:|---:|
| CPU 帧耗时 P50 | 7.102 ms | 4.150 ms |
| CPU 帧耗时 P95 | 13.975 ms | 6.480 ms |
| 寻路查询耗时 P95 | 7.8994 ms | 0.0054 ms |
| 最高存活敌人数 | 35 | 35 |

1,200 帧中记录的存活数、出生数、兄弟血量、玩家坐标、寻路调用次数和更新步数逐帧一致；两次均击杀 1 个敌人。这不等同于验证了所有内部状态或逐像素一致。

正常补算模式均完成 1,200 次逻辑更新：CPU 帧耗时 P99 从 22.994 ms 降到 10.025 ms，P95 从 12.681 ms 降到 7.226 ms。该模式使用真实墙钟，两次渲染帧数分别为 3,241 和 4,158，不用逐帧索引比较游戏状态。

CPU 帧耗时包含更新、几何、场景和 HUD，排除 Present；不能直接当成用户机器实际显示的 FPS。首次 HUD 渲染仍有约 70 ms 的加载峰值，未计作寻路修复结果。有声播放开销未在静音测试中验证。

## 复测命令

```powershell
pwsh -File tests/run.ps1 -NoBuild -Case path-cache
pwsh -File tests/run.ps1 -NoBuild -Case spawn-performance
pwsh -File tests/run.ps1 -NoBuild -Case spawn-performance-realtime

# A/B baseline; --uncached-paths is development-only.
.\bin\Debug\GunBrosTests.exe --mute --spawn-performance-check --uncached-paths --test-output E:\coding_projects\c_projects\gun_bro_re\obj\path-baseline
```

`tests/run.ps1` 会清理 `tests/out`，本次性能 CSV 和日志已另存 `obj/path-cache-evidence`。缓存回归在修复前退出 1（198 次预期命中实际为 0），修复后退出 0；覆盖重复可达/不可达查询、等长路线、锁定/解锁/传播、边界及同尺寸图重载。

## 回归边界

关卡脚本与缓存专项检查通过。四个星球的第 499—500 波均运行到第 500 波结束、无无效出生，但综合测试均退出 1：`bullet=93` 的眩晕到期断言失败，时长为预期 750 ms，检查结束时仍处于眩晕状态；该独立武器场景没有绑定地图。

名称纠正：`pack5 POWERUP:14 Shock G.` 引用 `BULLET:93`，失败的是电雷；`POWERUP:15 Freeze G.` 引用 `BULLET:94`，冰雷的 2000 ms 时长与到期检查均通过。依据 `_prep/out/powerup-check.txt:16-17` 的原资源解析及本次 impact 日志。此前对话误称“冰冻手雷”，应以此为准。日志中的 failures 是累计值，冰雷行显示 1 并不意味着冰雷新增了失败。

pack2 关闭缓存后同样退出 1、出现相同断言；开启和关闭缓存均为 638 次出生、622 次击杀、结束时敌人数 0。此失败与本次寻路优化无关，未更改武器行为或降低断言要求。四图完整日志位于 `obj/path-cache-evidence/final-pack*/logs/stdout.log`，对照位于 `obj/path-cache-evidence/final-pack2-uncached.log`。各次脚本检查的受保护文件变化均为 0。

最终 `GunBrosRe.vcxproj` 的 Debug、Release 构建均退出 0，输出为 `bin/Debug/GunBrosRe.exe` 和 `bin/Release/GunBrosRe.exe`；构建日志保留在 `obj/path-cache-game-*-build.log`。未运行全量截图基线。
