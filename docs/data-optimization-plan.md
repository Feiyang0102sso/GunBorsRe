# data 职责归位方案

日期：2026-09-19。用户已批准整体优化：按原版职责组织，尽可能删除 Z 包装；不把自建类型改成原版名字冒充复刻。

## 依据与范围

- `engine/resources` 负责 BIG、TOC、解压与 handle 读取。游戏侧 `data/objects` 负责类型、引用及对象持有。原 `CGunBros::GetGameObject/InitGameObject`（78471、78497）委托 `CGameObjectPack`（129033、129772），后者保存对象并避免重复初始化。
- 模板格式沿用 `big_keyset.bt`、`object_counts.bt`、`entries/{gun_template,armor_template,powerup_template,store_entry,planet_entry,mission_entry}.bt`；字段解析不改造成资源替代表。
- 商品聚合／报价归原 `CStoreAggregator`，覆盖归 `CStoreItemOverride`；枪的 Flow/弹体推测不得作为正式商店隐藏依据。
- `Planet/Mission` 保存资源及引用，菜单展示归菜单；研究探针和报告归 tests，Viewer 标签归 Viewer。
- 原 `CGunBros::Init` 80407 注册存档客户端；`CProfileManager::LoadFromDisk/SaveToDisk` 202880/203163 处理封装。对照 `saves/GB_save_profile.bt`、`save_payloads.bt`。Windows 文件替换和只读来源保护保留明确宿主身份。
- 保留现有注释、研究入口、旧 BIG 支持及原件只读保护。工作区已有大量未提交改动，在当前内容上增量修改。

## 阶段与任务

1. 包归位及研究依赖清理：data 下 objects/store/mission/profile；模型加载到 graphics；测试探针和目录标签归调用者。核对 include 和构建。
2. 游戏对象访问与目录拆除：统一文本读取，原对象接管单条解析和目录持有；去掉重复转发，删除装备、商品、任务 Z Catalog。验收多版本 BIG、装备／商品／任务解析及真实菜单。
3. 存档职责拆分：封装与调度归 CProfileManager，各客户端读写按已确认职责归位；宿主文件操作独立。合并研究导入和正式加载，删除按包名／脚本状态数猜星球的旧路径。验收原样本、未知字节、校验、回读、库存和进度。
4. 全面核对：Debug/Release 三产物及有影响的装备、商店、任务、存档、战斗专项；更新逐文件映射、剩余适配和验证边界。

## 验收标准

- data 根目录无游离源码，旧路径和旧包装引用清零，不保留转发壳。
- engine 不依赖游戏业务包；游戏对象复用不跨越其所有者寿命。
- 商品、任务、进度仍以 BIG/原存档为事实来源；未知数据不静默造值。
- 原存档读取和未修改记录保留、修改后保存重载通过；测试原件和 BIG 不变。
- 自动运行显式静音；本阶段按影响检查，不重复全量截图基线。

## 执行结果

四个阶段已完成。data 归入四包，原 23 个 Z 文件中 21 个拆除、2 个迁至 graphics；Debug／Release 的 Game、Viewer、Tests 构建通过，最终专项分别 17/17、8/8 通过，受保护输入变化为 0。逐文件归属、原版证据和保留边界见 [整体优化结果](data-optimization-result.md)；未完整恢复的客户端注册和远程同步没有计入完成范围。
