# 刷怪位置按原版离屏规则（2026-09-09）

用户反馈：敌人会直接刷在玩家脸上，原版是在看不见的地方刷。本轮按原版规则重做刷怪点选取。行号均指 `_IDA_OUT/gunbros_3.6.0_IOS.c`。

## 原版规则

`CEnemySpawner::GetSpawnPoint` :146098 按“是否指定了节点表”二选一：

| 情况 | 行为 |
|---|---|
| 脚本调用过 DisableAllNodes + EnableNode | `GetSpawnPointSpecific` :146576：在列出的节点里均匀随机，不做其他判断 |
| 默认（全部节点可用） | `GetSpawnPointOffScreen` :146112 |

离屏路径把两样东西交给 `CLayerPathLink::GetSpawnLocation` :166819：

- **参考点**：`GetSpawnSource` :147224 取关卡里玩家对象的 `GetLocation`，即玩家当前位置；
- **过滤器**：`SetupSpawnFilter` :147283 取 `CCamera::GetBounds`，四边各外扩 10 单位（x-10, y-10, w+20, h+20）；第二个矩形置 (-1,-1,0,0) 关闭，那是分屏联机用的另一名玩家视野。`COffscreenSpawnLocationFilter::AcceptSpawnLocation` :147156 对落在矩形内的节点返回否。

`GetSpawnLocation` 遍历全部节点，跳过锁定的（node+16）和被过滤器否掉的，其余按到玩家的**平方距离**送进 `DistanceList::AddEntry` :167261。该列表容量 5、升序保留，也就是**只留下离玩家最近的五个离屏节点**；最后 `Utility::Random(0, count-1)` 在这五个里随机取一个。

一个节点都不合格是正常结果：原版这一拍不刷，下一拍再试。

## 改动

本移植原先是“从上次序号开始循环找第一个可用节点”，代码里也写着 free-node scoring 尚未研究。这个规则完全不看玩家位置和镜头，于是节点恰好在玩家旁边时就会当脸刷。

现改为 `SurvivalSession::ChooseSpawnNode`，逐条对应上表：节点表模式均匀随机；默认模式用镜头矩形外扩 10 过滤、按平方距离保留最近五个、再随机取一。随机流用关卡脚本那条（原版整个 `Utility::Random` 共用一个 `CRandGen` 单例，本移植是每宿主一条流，用关卡这条最接近）。选不出节点时 `SpawnEnemy` 返回 false，由既有的刷怪规则下一拍重试。

`UpdateCamera` 顺带记下当前镜头矩形，供本规则和弹体剔除共用。

## 验证

`--survival-check` 新增一行证据：本波所有规则刷怪里离玩家最近的一次距离，以及落在镜头矩形内的次数（后者非零即判失败）。

| 地图 | 结果 |
|---|---|
| pack2 7 | wave=3 kills=42 invalid=0；closest-spawn=117.6 on-screen-spawns=0 |
| pack7 6 | wave=3 kills=42 invalid=0；closest-spawn=286.6 on-screen-spawns=0 |
| pack9 0 | wave=3 kills=42 invalid=0；closest-spawn=244.1 on-screen-spawns=0 |
| pack12 0 | wave=3 kills=42 invalid=0；closest-spawn=189.7 on-screen-spawns=0 |

四张图都能正常打完三波，没有一次刷在镜头内。closest-spawn 小于半个视野是镜头贴地图边界被 `CCamera::UpdatePosition` 夹住、玩家不在正中时的正常情况。

回归（均退出 0）：`--brother-check`、`--combat-feedback-check`、`--level-flow-check`、`--horde-check 0`、`--campaign-check pack2 10`、`--tutorial-check`。

## 验证边界

- 只改了“节点怎么选”，刷怪速率、上限、规则调度都没动。
- 若某张图的镜头矩形能盖住全部节点，本规则会一直刷不出来——这与原版一致，但本移植的桌面视野尺寸并非 iOS 原值，缩放若拉得过大就会更容易触发。目前四张正式图未出现。
- 玩家对象的参考点用的是 `CombatScene::playerX/Y`，未包含 AI 兄弟；原版单人也只取玩家对象。
