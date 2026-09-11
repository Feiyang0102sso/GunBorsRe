# 战役归档核查：左侧拾取区、LV2、babe 与 MAP6

本记录针对当前 `big/` 中 iOS 3.6.0 资源。MAP、LEVEL、Mission 编号分别核对，不把现存模型视为完整任务的证据。原资源与正式存档不修改。

## LV0 左侧拾取区：已修复命中路径

- MAP0 对象 70 引用 pack2 PROP32，原文件 `pack2_xga_0075_0x3f50.bin`。脚本初始血量为 0；受击后检查伤害属性 bit 0，满足时进入 destroyed，关闭角色和子弹碰撞组 1，并发送销毁事件。
- `CLayerCollision::TestCollisionSegment`（反编译 125350）枚举 PROP 有效子弹碰撞边，没有检查血量；`CBullet::CheckCollisionWithLevel`（61954）把命中传给 `CProp::Damage`。重建的 `MapPropWorld::Trace` 错误增加了血量过滤，导致这个事件永远无法送达。现在移除该过滤；范围伤害仍遵循 `CProp::CanCollide`（123423、114613）。格式已对照 `entries/prop_template.bt`、`collision_data.bt` 及 Flow 模板。
- bit 0 在旧版辅助 Flow 中名为 `CollisionAttribute.DestroyWall`，与当前 PROP 字节码检测一致。它不是“伤害足够大”或“所有爆炸”的同义词。当前 BIG 中 The Wombat（pack5 GUN54）引用 BULLET115，flags=0x41，具备此属性；完整依赖枚举由 `--campaign-content-check` 输出。
- LEVEL0 的对象销毁出口识别对象 70，切相机层 9、生成 tag 0 守卫（对象 87，pack1 ENEMY9）。左侧三个拾取物为对象 84–86，pack5 PICKUP0。它们不属于房间最终 ENEMY17 的完成条件，不要求先杀掉该敌人。
- `campaign-cache` 通过真实 Trace/ApplyHit 验证普通属性不破坏、bit 0 破坏，再按原碰撞边的法线穿过斜向门洞，逐帧记录真实拾取事件。检查设置局部起点、使用无敌，不代表从出生点全程人工通关。

## LV2：原关卡和当前敌人行为未接通，仍未修复

- pack2 LEVEL2 引用 MAP2，无 Mission 引用它；浏览器通过独立 LEVEL 入口启动。
- 原脚本 `pack2_xga_0010_0x14ac.flow.txt`：触发组 0 后切换碰撞层 10、相机层 8，进入 state 1 并生成 tag 0；等待 LEVEL 事件 1 后才能进入 state 2。击杀出口会启动 1.5 秒计时，但只有 state 2 处理该计时完成事件。
- MAP2 对象 59 引用 pack1 ENEMY18，位置 (666,198)、路径层 5；该敌人当前脚本 `pack1_xga_0040_0x48da.flow.txt` 的 SetPath 出口为空，也没有 native 46/56 的 LEVEL 通知。原 native 46（72537）直接发出 LEVEL 事件 1，重建已有此实现，但此资源没有调用它。
- 实测先真实触发，再等待 30 秒，仍为 state 1、存活敌人 1；通过受击/死亡流程击杀后等待 120 秒，仍 state 1、存活 0、未完成。这不是提前击杀打断入场的测试误差。
- 后续 state 2 的 native 41（按 tag 发送消息）也尚未实现。不能宣称本关脚本执行完整，更不能把“无存活敌人”强改为胜利来掩盖前面的资源行为缺口。
- 保留独立复现：`bin/Debug/GunBrosTests.exe --campaign-lava2-check --mute`，目前退出 1；不加入要求全绿的自动回归。未替换 ENEMY 引用、借用其他 Boss，亦未认定这些资源在所有旧版本中都是废案。

### 旧 Flow 补证：Rival 的入场分支

- `--enemy 18 --screenshot obj/level2-enemy18.png --mute` 从当前 BIG 渲染 ENEMY18，确认是持枪人形角色，不是 Skull Tank。其躯干、腿、枪三部分装配与战斗状态对应辅助旧稿 `boss/rival_0.flow`。
- 旧稿有 24 个状态，前四个为 `entrance_hit_handler`、`entrance`、`entrance_laugh`、`entrance_pause`；其后的 20 个战斗状态与当前编译脚本的结构对应。当前脚本仅保留 20 个状态，`OnPathSet` 为空；旧稿的该出口则进入 `entrance`。旧稿初始血量 300、当前 110，说明不能把整份旧稿当作当前版本直接覆盖。
- 旧入场流程为：配置腿部跑步动作、面向移动方向、速度 70、前往相邻路径节点；到达后配置腿部待机并播放躯干大笑序列；动画序列完成调用 `registerMovementDone()`（native 46），向 LEVEL 发送事件 1，再切回常规战斗。玩家不需要额外交互。
- `prop/lava-spike.flow` 只处理 `Open -> grow -> hold`，生长动画结束后保持地刺并设置底部图层。当前 pack2 PROP44 字节码保留这一流程。LEVEL 的 state 2 使用 native 41 对 tag 1 发送 Open，原 native 41（117805）遍历活动关卡对象比较 tag，并发送消息。
- 恢复应将旧入场分支作为明确标注版本来源的战役兼容工作，核对当前 MoveSet 中对应动作及原 `gotoRandomConnectedNode` 消费者，再补齐 LEVEL 按 tag 发消息。不能直接把当前 ENEMY18 全局换成旧脚本、改成 Skull Tank，或仅强发完成事件跳过入场。本轮只补充证据与恢复方案，未实施该兼容行为。

## babe：当前归档确实是 1 点基础血量，并允许双方命中

- pack1 ENEMY14（`pack1_xga_0036_0x44bf.bin`）初始化调用 native 50(1)，设置阵营变量 16 为 2。
- 原 native 50（72551）将基础值乘 `CLevel::GetHealthMultiplier`，LV4 没有设置敌人血量倍率，当前实际为 1。
- 原 `CEnemy::CanCollide`（67243）分别排除阵营 0 的敌方弹体与阵营 1 的玩家弹体；阵营 2 不受这两项排除。当前实现没有额外给 babe 添加友伤或降低血量。
- 此结论只适用于这份归档里的 NPC，不外推其他版本的救援设计；本次不修改其属性。

## MAP6：有场景和出生点，缺少关联 LEVEL

- 遍历全部包的 23 个 LEVEL，没有一个引用 pack2 MAP6；全部 Mission 关联同样没有补出这条链。
- MAP6 有 PLAYER，预放对象除 PLAYER 外都是 PROP；依赖表也未列 ENEMY。不是因为缺玩家而灰显，也不能仅凭其他位置已有 Boss 模型认定它必然是一场完整 Boss 战。
- pack2 LEVEL6 实际引用 MAP7，LEVEL7 引用 MAP8，LEVEL8 再次引用 MAP7。数字相同不构成关联。
- 地图自身的场景数据存在，但目前没有找到驱动它的 LEVEL／任务脚本。保持不可调用标记，不手写一套任务或套用其他关卡来伪装恢复。

## 验证证据

- 修复前 `--campaign-cache-check --mute`：退出 1，`wall hp=0 traced=0`，见 `obj/campaign-cache-before.log`。
- 修复后同命令：退出 0，`collected=3 finished=0 failures=0`，见 `obj/campaign-cache-after.log`。
- LV2 复现：退出 1，见 `obj/campaign-lava2-before.log`。
- 全 BIG 引用核查：退出 0，见 `obj/campaign-content-all.log`。
- Debug／Release 主程序构建记录：`obj/campaign-cache-debug-build.log`、`obj/campaign-cache-release-build.log`。
- 相关自动回归 7/7 通过：`campaign-cache`、`campaign-targets`、`campaign-doors`、`campaign-progression`、`campaign-rescue`、`campaign-portal`、`final-pack2`，保护文件变化为 0。记录为 `obj/campaign-cache-validation.log`，逐项结果为 `tests/out/results.json`；这不包括上面明确失败的 LV2 复现。
