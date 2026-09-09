# 血条水平偏移修复（2026-09-09）

用户截图中血条偏在敌人左侧。根因是 `CombatScene::EnemyHealthBars` 在世界坐标中先减去血条的屏幕像素半宽，之后 `MapScene` 再做镜头投影，导致居中偏移也被镜头放大。上一轮尺寸与显隐检查没有覆盖最终屏幕矩形的位置。

修复保持 BIG 模型包围盒和原脚本显隐作为来源；复核 `enemy_template.bt`、`CEnemy::GetBoundsInternal/GetBounds`（iOS 反编译 67314/67485）、`CLevel::DrawEnemyHealthBars`（120454）及本项目 `EnemyMatrix/BuildEnemyGameMatrix`。不添加按敌人 ID 或截图调整的偏移量。

- `CombatScene` 输出世界坐标的顶部中心锚点，尺寸仍为屏幕像素。
- `ProjectEnemyHealthBars` 先投影锚点，再在屏幕坐标中减去半宽，得到 HUD 左上角。
- 保留原模型包围盒顶部及原生 20 单位边距；本轮修复的是水平居中误差，没有人为移动竖直锚点。

永久研究入口 72 / `--combat-feedback-check --mute` 增加真实 pack1 ENEMY 0 的对齐检查。以原单部件模型的包围盒中心独立计算预期屏幕位置，比较最终矩形中心；覆盖 1024×768、1600×1200、1920×1440，以及镜头倍率 0.5、1、2.6，共 9 组。

| 验证 | 结果 |
|---|---|
| 修复前 | 检查退出 1，最大水平中心误差 96 像素；`out/healthbar-alignment-before.log` |
| 修复后 | 检查退出 0，9 组最大误差 0.000 像素；`out/healthbar-alignment-after.log` |
| 原脚本显隐、群杀音效及此前战斗专项 | 同一入口全部通过 |
| Release 构建 | 退出 0；`out/healthbar-alignment-build.log`，正式与研究 EXE 均更新 |
| 图像核对 | [修复截图](../out/healthbar-alignment.png)，敌人血条已水平居中 |

正式 EXE 构建时间：2026-09-09 17:34:55（本机）。本轮不修改资源读取器、BIG 或存档；`git diff --check` 通过。
