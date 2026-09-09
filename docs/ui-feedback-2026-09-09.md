# 商店与启动反馈修复

本阶段按用户最新要求优先处理三项；不扩展此前八小时任务。

## 方案与验收

1. 一次性礼包：遵循 `CStoreAggregator::CanItemBeAcquired`（155172）、`AcquireItem`（158044）和 `CPackageOfferMgr::AddItem`（396370）。原 STORE 引用作为购买键，写入原存档 1018 的 CollectionValue32；键存在表示买过，不能把 value=0 误解为未购买。仅 GUNS 显示未购买的礼包。验收连续购买、余额、原生存档重载和分类。
2. 商店遮挡：分别验证原 Movie 基准、Sprite 边界和绘制/裁剪顺序。`CMenuStoreOption::CornerCallback`（180730）原样绘制数量 Sprite，不先假设需要缩图或整体下移。确认原因后修复并保留截图。
3. 启动：视频结束后显示原 `png/Default-Landscape.png`，等待全屏点击或确认键，再进入原选人/签到流程。新增闪烁提示由本次用户明确授权，使用 BIG font11；直接指定页面的截图入口保持可用。

## 任务与证据

- [x] 礼包失败回归：原资源 pack3 STORE 91 连买两次均 Purchased，第二次扣款；`out/ui-feedback-2026-09-09/package-red.log`，退出码 1。
- [x] 实现原购买记录与列表规则，验证重新加载。
- [x] 定位商店遮挡并按原区域/绘制流程修复。
- [x] 恢复 Splash 等待和 font11 闪烁提示。
- [x] Release／Debug构建、专项流程测试和截图核对。

## 结果与定位

### 数量角标

BIG解析结果：STORE_MENU内容区域 y=253，STORE_SCROLL输入区域 y=262，首列 y=264，数量Sprite包围盒顶部 y=256。旧代码以输入区域裁剪，首排缺6个逻辑像素；`clip-red.log`退出1。`CMenuStore::ItemCallback`（178878）与`CMovieRegion::Draw`（109978）均未按该输入区域裁剪回调内容。现在仅保留横向裁剪，纵向由窗口边界限制，两排商品完整显示。没有整体下移页面、缩小图标或复制资源布局。中途试用内容区域纵向裁剪也会截第二排底部，截图核对发现后已移除。

### 一次性礼包

`CStoreItem::resource`保存加载器取得的STORE引用，对应原运行时+356／+358，并非新增磁盘字段。`CProfileManager::AcquireItem`仅在实际购买成功后记录，赠送分支不记购买。`NativeProfile`读写1018的14字节CollectionValue32，保留已有value与padding，新记录value=0；按键存在判断已购。普通清单排除singlePurchase，GUNS仅插入未购买礼包；展开状态购买后也关闭详情。旧`.dat`研究格式升至11以保存相同状态，兼容1–10；正式游戏仍使用原存档文件。

原源1018没有历史购买记录时，不能根据已拥有礼包装备倒推是否买过，因此不自动补写旧BUG期间的购买记录。之后成功购买会正确记录。

### Splash

正式默认入口改为page14：视频结束→原`png/Default-Landscape.png`→等待点击→原选人／签到。提示字形来自BIG font11；文字、底部居中位置及600ms亮／600ms灭是本次用户授权新增的宿主显示规则。B／E等导航键不会绕过等待，Enter／空格可继续。指定页面截图入口仍直接进入目标页。

## 验证

所有命令带`--mute`，原件只读，测试写`out/`隔离账户。

| 验证 | 结果与日志 |
|---|---|
| Release、Debug GUI及研究工具构建 | 退出0；`release-final.log`、`debug-build.log` |
| `--progress-check` | 退出0；`progress-final.log`：第二次Owned且余额不变 |
| `--native-profile-check` | 退出0；`native-final.log`：1018重载、零value已购、拒绝二次扣款；其他原记录回归通过 |
| `--ui-feedback-check` | 退出0；`ui-final.log`：4个Splash阶段、7个商店阶段，真实卡片点击／购买／重载；POWER UPS和ARMOR同一位置不是礼包 |
| 正常GUI启动 | `normal-boot.log`确认`intro completed`；computer-use实看Splash仍等待，B键不跳转，点击左上空白进入原签到，再关闭测试窗口 |
| Splash亮／灭像素比较 | 退出0；`splash-frame-check.json`：1600×1200图像仅(396,1056)–(1210,1114)提示区域变化，其余像素完全相同 |
| `--store-template-check` | 退出0；`store-regression.log`，原筛选／卡片／模型及76款武器截图回归通过 |
| `--game-menu-check` | 修正旧测试定位后退出0；`menu-final.log`：连续道具购买／重载、预览、展开、筛选、导航、设置和交易等回归通过 |

旧`--game-menu-check`最初仍点击第二排固定坐标，移除POWER UPS礼包占位后买到了Defense Boost，故Speed Boost断言失败。这是旧测试布局假设失效：已改为从BIG展示顺序、STORE_SCROLL列、SHOP_BOX区域和原按钮宽度取得目标与点击位置，同时从原商品对象列表和价格计算验收数量／余额。

上述日志位于`out/ui-feedback-2026-09-09/`。`splash-0.png`／`splash-1.png`为提示亮／灭，`splash-2.png`／`splash-3.png`为老／新账户后续入口；`store-0.png`为完整两排道具，`store-3.png`为GUNS礼包被选中，`store-4.png`和`store-5.png`为购买后及重载状态。所有账户均是隔离副本，不覆盖实际`userdata/saves/`。
