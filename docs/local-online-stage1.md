# 本地模拟联网：第一阶段

后续修正：[BROS / BRO-OPS 本地内容恢复](social-local-content.md)。以下为空框架阶段的历史记录；缺少远端好友并不意味着默认兄弟、Bro Buffs 或挑战模板为空。当前版本已从 BIG 恢复这些内容，保留未实现的远端好友与邀请边界。

范围：使 `IsConnected` / `chc` 驱动本地服务适配，开放 BROS / BRO-OPS 原版菜单框架和分类、多人生存及 Versus 选择与可取消匹配请求；内购经过商品查询、模拟验证、按商品 ID 发货及存档。既有关闭开关时的离线内购演示保留。真实玩家匹配、同步对局、远端好友、邀请和挑战奖励不在本阶段。本实现为进程内模拟，不建立 HTTP/TCP 连接，也不验证 Apple 收据。

原版依据：

- `_prep/_IDA_OUT/gunbros_3.6.0_IOS.c`：`CMenuMovieMultiplayerOverlay::SetSelection` 250261，provider82 检查服务可用性，action22 切换模式；provider81 控制账户离线分支。
- `CMenuFriends::Init` 197410、`Bind` 197028、`TabButtonCallback` 195569：主 Movie 区域0为标题、1为三个分类按钮、2/3/4为好友内容。ARMv7 `MENU_FRIENDS` 0x402d70 的 +4 指向 `GLU_MOVIE_BROBUFF_MENU`。没有远端好友时远端列表为空，但仍应显示本地默认兄弟与 Buff 条件。
- `CGameCenterManager::findMultiplayerMatch:` 260057：使用 iOS `GKMatchmakerViewController`，两人匹配。此系统 UI 不在 BIG，Windows 等待面板是明确的宿主适配，使用已有原 Movie 弹窗布局。
- `CStoreAggregator::LaunchIAP` 158427、`AcquireIAP` 158280、`IAPTransactionCompletedCallback` 80879：从 STORE asset[0] 查商品 ID，完成回调按 ID 查商品后入账并保存。取消回调77501关闭等待。
- `_prep/_Big_tool/binary template/big_assets/ui_movie.bt`：原 Movie 章节、区域、关键帧；`entries/store_entry.bt`：商品字段、IAP ID与货币值。布局、文本、数量由 BIG 读取；本地服务响应时延属于宿主行为。

任务与验收：

- [x] 连接状态接入页面及多人模式，关闭开关时仍保留原离线判断。
- [x] 开启时 BROS / BRO-OPS 可进入并切换原分类。初版仅有框架，其内容空白属于实现缺失，后续已恢复本地兄弟、Bro Buffs 和挑战详情。
- [x] 多人模式能提交本地请求、等待、取消；正式菜单在多人选择下拦截战斗启动。模拟服务不会报告找到真人。
- [x] 内购商品查询和本地验证后只发货一次；断开不发货，按 BIG 商品 ID 解析，存档可重载。
- [x] 运行专项及受影响回归，构建 Debug/Release，记录结果。

2026-09-13 验证记录：

- 红灯：`pwsh -File tests/run.ps1 -Case offline-social`，真实模式点击输出 `connected mode selection blocked`，退出1。此前测试只要求保持离线。
- 绿灯：`pwsh -File tests/run.ps1 -Case offline-social,bank,planet-menu,progress`，4/4通过，退出0；受保护资源、真实账户和存档无变化。银行25个阶段交替启用模拟连接，覆盖真实按钮、等待、货币兑换及重载。
- `--social-check` 原永久入口扩充了在线测试：BROS / BRO-OPS 分类点击、在线转离线、模式选择、等待面板的取消按钮、断开取消、回调按商品ID定位、发货幂等、未知ID拒绝、存档重载。
- 截图位于 `tests/out/OriginalUI/offline-social/local-online/`，检查后纠正了标题锚点被误当作裁剪区域的问题。
- Debug / Release 的 `GunBrosRe.vcxproj` 均构建成功，退出0。使用 `/p:SkipAutoTests=true` 保留专项证据，默认 Debug 的 progress 检查已在上述4项中运行。Release 保留已有其他模块的 C4244 转换告警。
- 日志：`tests/local-online-red.log`、`tests/local-online-final-tests.log`、`tests/local-online-debug-build.log`、`tests/local-online-release-build.log`。

运行方式：启动 `bin/Debug/GunBrosRe.exe` 或 `bin/Release/GunBrosRe.exe`，输入 `chc` 切换，或在对应 EXE 旁的 `GunBrosRe.cfg` 中设 `IsConnected=1`。打开 BROS / BRO-OPS；PLAY 选择多人模式后选关进入本地等待，Cancel / Escape 返回；STORE 的货币商品按原四秒演示时延完成本地回调。
