# 本地 Live 实现与验收

## 2026-09-14 再次反馈修正

范围：纠正进关壁纸及其 Movie 区域、仅 Live 构造新 AI、减少逐帧方向抖动并补枪械移动倍率、原默认兄弟名字、BROS 三项滚动及 Focus/UnFocus 高光、关闭 fake connection 后恢复默认选择。

原证据见 `live-loading-correction-research.md` 和 `bros-scroll-animation-research.md`。旧阶段把 `0x4031B0 MENU_BOOT_LOAD` 误用于关卡；实际 `SetMenu(18)` 因指针表 `+4` 对应 `0x4031F0 MENU_GAME_LOAD`，Movie24 的背景从 header 下方开始。按用户要求 Live 用 KEYSET22–25、Deathmatch 用26，保留与 iOS 原混池分支的差异记录。

复现：`--local-live-check --mute` 在 `obj/live-correction-red.log` 返回1，记录 `disconnect-default=0`；`obj/live-correction-policy-red.log` 返回1，静止目标5秒内急转10次；`obj/live-correction-scroll-red.log` 返回1，四项名单 `position=0`。修改后初轮专项退出0，分别恢复默认选择、急转0次、名单滚动74单位、原单人策略；远距离救援仍到达70单位并救活。最终验收在末尾追加。

以下七项反馈段落与其主图结论为上一阶段历史，当前加载实现以上述纠正为准。

## 2026-09-14 七项反馈修正

用户授权：多 bot 配置与 BRO BOOST、多人加载、屏外队友定位、救援效果、双方波间输入与倒计时、远距离救援、双方商店全局暂停。原证据见 `live-hud-followup-research.md` 和 `local-roster-boost-research.md`；截图只作问题线索。

本阶段任务：公共账户名单加载／轮换与奖励加成 → 多人会话暂停及 HUD 修正 → bot 导航拐点修正 → 实际地图、资源画面、存档重载和受影响回归。

已确认旧阶段两项研究结论错误，现予纠正：多人 Splash descriptor 指定 `IDB_SPLASH_MAIN_MP`，不能直接全屏展示 KEYSET 的红底轮播图；波间 15 秒仅暂停 LEVEL 脚本，双方仍可移动。`Next wave in ` 是文本前缀，数字由原回调追加。

救援失败已用真实地图的绕障碍路径复现：60 秒后仍倒地、相距约 744 单位。根因为 bot 在导航拐点前按 70 单位救援距离停步，且用 100 单位探测误判推进方向。修正后到达 70 单位救援位置并完成原复活。救援范围、持续粒子、起身和屏外定位仍由通用多人模块处理。

本阶段验收以末尾追加的最新结果为准，以下是历史记录。

## 已确认范围

按用户要求恢复原 GameType 2 的合作流程。Windows 仅用测试 AI 替换匹配到的真人输入；关卡、角色、枪械、伤害和道具继续执行 BIG 模板及 Flow。测试 AI 独立放在 `BroAIDeathmatch`，不改变默认单人兄弟的策略。用户已授权本阶段实现。

## 任务与验收

- 匹配：原菜单弹窗完成后延迟 1.5 秒连接 LOCAL BOT；真实菜单循环返回关卡，不停留在等待界面。
- 战斗：两人独立伤害、死亡、击杀、双枪助攻、经验与矿；救援保留原半径、积累进度和复活 Flow。受伤、拒绝伤害和队友击杀不能串账。
- 展示：读取多人加载 Movie/图像、每波双列统计、15 秒波间等待、最终多人卡片和重开按钮；以原字段和非零测试数值截图核对。
- 商店：750 毫秒请求延迟、首请求者占有、10 秒上限；远端浏览期间本地不能操作商品。死亡道具按模板 afterDeath 字段筛选，执行原复活出口。
- 好友：稳定测试身份、本地独立原格式账户、好友选择跨启动保留；单人选择该好友时读取其配置。原 1006/A 文件不得伪装成完整好友档案。
- AI 与作弊：独立移动避敌、救援、频繁换枪、库存内用道具和逛店；`brow/brok/bror/bros/brop` 仅测试 AI 生效，后两项仅多人。

## 证据与边界

原函数、BT 和二进制补证分别见 `live-combat-research.md`、`live-presentation-research.md`、`live-friend-save-research.md`。AI 决策与本地账户目录属于 Windows 适配，不声称来自原 iOS AI。没有服务器，不能验证真实网络延迟、断线恢复和跨设备协议互通。Death Match 仍是独立后续模式，不能把合作统计冒充其规则。

## 验证记录

使用 `GunBrosTests.exe --local-live-check --mute --fixtures <只读样本目录> --test-output <测试输出目录>`，保留 `obj/live-phase*-build.log`、`obj/live-phase*-test.log` 与 `tests/out/live-phase*` 的截图。

阶段 5 已验证真实菜单匹配返回、好友重载、商店请求互斥/时间边界、远端输入隔离、波结算时间及双列结算绘制。救援回归暴露测试传送位置越过地图可走边界，阶段 6 使用真实移动模拟选择合法位置继续验证。最终结果以本文件后续追加的实际退出码为准。

### 最终实现与专项验证

`local-live` 最新专项退出 0，实际覆盖：菜单匹配完成并返回关卡、两波推进、双方距离救援、双方倒地、复活道具完整 Movie/Flow 与库存消耗、重开、两枪分别贡献助攻、击杀奖励归属、真人身份的作弊隔离、单人选择测试好友、商店只读与计时边界、多人 REMATCH 按钮输入。BOKOR 已按原 MissionType 2 接入并运行 8 秒刷怪；其波间流程不套用普通生存的 15 秒面板。

Debug 与 Release 主游戏构建退出 0。构建命令均使用 `GunBrosRe.vcxproj /p:Configuration=<Debug或Release> /p:Platform=x64 /p:SkipAutoTests=true /m`；日志为 `obj/live-final-debug-build.log`、`obj/live-final-release-build.log`。测试程序使用最新 Debug 构建，日志为 `obj/live-final-tests-build.log`。

已目视核对 `tests/out/Core/local-live/` 中多人商店、波结算、最终结算及 BOKOR 截图。最终列表区域直接来自 Movie 119 的 region 4（132,478,760,276），保留原位置。复活条使用原 CBrother 的 100×100 边界及视口缩放，好友头像使用其独立账户中的兄弟索引。

上述结果是所列流程的验证，不代表全部原生函数、触摸滚动细节和网络异常已经逐项完成 1:1 验收；真实服务器及 Death Match 不在已接通范围内。

最终定向回归命令：

```powershell
pwsh -NoProfile -File tests/run.ps1 -Case progress,local-live,debug-input,powerup-selector,powerup-play,brother,player-death,postgame-menu,postgame-presentation,offline-social -NoBuild -TimeoutSeconds 600
```

结果：10/10 通过，各项退出码及总退出码均为 0；`protected-changes=0`，原 BIG、样本存档、真实账户和宿主配置无变化。运行器自动给全部测试传入 `--mute`。汇总为 `tests/out/results.json`，每项日志位于对应检查的 `logs/`，总日志为 `obj/live-final-validation.log`。`git -c core.safecrlf=false diff --check` 通过。未重跑无关的全量截图基线。

### 七项反馈最终验收（2026-09-14）

定向回归在以上十项基础上增加 `daily-bonus,refinery-menu`，12/12 通过，总退出码 0，`protected-changes=0`。日志 `obj/live-followup-validation.log`，汇总 `tests/out/results.json`。首次配置迁移又补充了等级内经验保留修正；随后重新构建测试程序并运行完整 `--local-live-check --mute`，退出 0，日志 `obj/live-followup-final-local-live.log`。

专项实际验证十好友配置、持久化轮换和单人选择、经验／货币保留、每日金币与精炼产出加成、双方暂停及波间移动、离屏定位、两种救援持续效果、真实地图远距离绕障碍救援，并保留原助攻／道具／死亡／商店验证。原版主图、BRO BOOST 解锁、`NEXT WAVE IN 5`、定位头像及绿／红橙救援粒子已目视核对。最终截图保留在 `obj/live-followup-evidence/final/`，不依赖下一次清理 `tests/out/`。

Debug、Release 主游戏和最终测试程序构建均退出 0，日志分别为 `obj/live-followup-debug-build.log`、`obj/live-followup-release-build.log`、`obj/live-followup-tests-final-build.log`。主游戏保留已有 26 项类型转换警告，未出现构建错误；最终测试增量构建零警告。`git -c core.safecrlf=false diff --check` 通过。

已从 Debug／Release 各自账户的副本生成并落盘 `bin/<Configuration>/saves/local-bots.cfg`，默认各一名 LOCAL BOT，保留各自实际装备。Debug 现有 bot 仅新增配置指纹，Release 尚无 bot 的账户新增独立好友存档。落盘后逐文件 SHA-256 核对，所有原先存在的账户文件均未改变。配置格式、循环规则、BRO BOOST 档位及修改语义见 `local-bots-config.md`。

### 再次反馈最终验收（2026-09-14）

用户补充断网场景为进入战斗前输入 `chc`。已在菜单连接刷新和单人启动参数两处校正：清除活动好友及待匹配指针，持久化默认选择；离线冷启动同样处理。原默认名字由 BIG `IDS_FRIEND_DEFAULT_BRO1/2` 取得，修复先前默认装备搭配了旧好友名称的引用错误。自定义配置中的名字保持用户设置。

命令 `pwsh -NoProfile -File tests/run.ps1 -Case progress,local-live,brother,player-death,offline-social,debug-input,loading-wipe,postgame-presentation,powerup-play -NoBuild -TimeoutSeconds 600`：9/9 通过，各项及总退出码0，`protected-changes=0`，日志 `obj/live-correction-validation.log`，汇总 `tests/out/results.json`。均显式静音。最终 Live 专项记录断网回退成功、默认名 Francis Gun、高光像素变化、四条名单下滚并点击最后一项、静止目标5秒急转从10次降至0次、Solo 仍为原策略、绕障碍救援成功。人工暂停检查曾在活敌仍存在时注入波间画面，与之后正常波次计数互相干扰；现将正常波次验证独立重开，未为此更改正式关卡逻辑。

Debug／Release 主游戏构建退出0，保留已有26项转换警告；日志 `obj/live-correction-debug-build.log`、`obj/live-correction-release-build.log`。测试程序最新构建退出0，日志 `obj/live-correction-tests-build.log`。`git -c core.safecrlf=false diff --check` 通过。本轮无需修改账户配置或存档。

加载四张合作壁纸和独立 DM 壁纸、原三项列表、选中最后一项的高光已实际绘制；截图复制至 `obj/live-correction-evidence/`。Deathmatch 壁纸分池已验证，但完整 Deathmatch 对战仍未接通；此处不把图片恢复当作对战模式完成。

### 当前激活 bot 匹配（2026-09-14）

用户要求 Live 直接使用 BROS 当前激活的 bot，覆盖此前按配置轮换的要求。这是 Windows 本地匹配策略，不修改原 BIG 或原版战斗规则。任务：移除轮换游标读写；让普通 Live 与 BOKOR 共用活动好友选择；更新配置说明及现有名单回归。验收覆盖重复匹配、切换激活好友、重启持久化和未激活 bot 时使用首位配置的边界；REMATCH 继续保留本局队友。

验收完成：local-live 专项通过（退出 0，44 秒，protected-changes=0）；日志确认 active-bot-match=1、repeat=12、reload=1、selection-change=1、default=1。Debug／Release 游戏和 Debug 测试程序均构建成功，零警告零错误。构建及测试日志位于 obj/active-bot-*.log。
