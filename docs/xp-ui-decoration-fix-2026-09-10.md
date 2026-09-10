# 击杀浮字与 UI 外围装饰修复

## 范围与验收方案

用户要求恢复击杀 XP 浮字、Powerup 可用模式外框及 Airstrike 外围动画。直接修复原创建、组装与绘制链；不照截图另画边框，不增加资源替代表。

- [x] 复现三个缺失，并使真实击杀／像素回归失败。
- [x] 核对相关 BT、BIG 资源、原消费者及必要 ARM 指令。
- [x] 修正旋转精灵拼接、接回原文字效果、保留空袭选择器外框。
- [x] 检查浮字位置、渐隐、失效与重开；三种空袭六条使用路径及外框退场。
- [x] 检查公共绘制变换、实际商店页面与相关玩法回归，更新正式 EXE。

## 根因与原版依据

### Powerup：旋转精灵被多翻转一次

`GLU_MOVIE_DEATHMATCH_ONLY_POWERUPS`（core Movie144）本来包含三行 `Sprite 0:172`。每个边框有五个叶子，均成功加载：外框四片使用全局 map354–357、image274、变换4/5/6/7，圆灯使用map654、image503。不是资源缺失，也不是三行文字遮住边框。

`CSpritePlayer::Draw :59209` 先转换翻转枚举；`drawSurface :111818` 再转换并旋转。最终 bit2 是纹理坐标转置。原重建 `CQuadBatch::AddTransformedQuad` 额外反转 U，四片的上下边被翻到中间互相叠加，只剩横条。修正公共 UV 变换，并同步修正 `MovieRenderer` 平铺旋转图片的边缘裁切。

查阅 `sprite_global.bt`、`sprite_archetype.bt`、`ui_movie.bt`。没有改原图、坐标、Sprite 组合或动画帧。

### Airstrike：只画前景，遗漏持续存在的选择器 Movie

pack5 Movie2（1600ms）只包含人物、标题和流光。`CPowerup` native15 注册前景；`CPowerUpSelector::Draw :186536` 先绘自身主 Movie，再绘前景。`HideOnlyItems` 只隐藏商品，外框继续按章节2循环；native5 `HideSelector` 通过 `SetState(6) :185697` 播章节3，完成后才发 `OnSelectorHidden`。

重建在使用时关闭了整个选择器，只剩 `PowerupMoviePlayer` 的前景，因此外围机械件、光框、底板同时丢失。现由演出宿主持有原 core `GLU_MOVIE_POWERUP_MENU_NEW`（Movie131），从 BIG 读取章节2 `[100,899]` 和章节3 `[900,1000]`。原资源退场为100ms，替换该路径旧300ms回调。原商品隐藏／输入面板的其他回调仍是既有宿主实现，不声称其全部复刻完成。

查阅 `entries/powerup_template.bt`、`flow_bytecode.bt`、`flow_native_sources.bt` 和之前空袭记录中的三个真实 POWERUP Flow。仍由脚本选择使用路径、发伤害和恢复战斗；没有按商品ID补丁。

### XP：入账后漏掉文字效果调用

`CLevel::OnEnemyKilled :119758–119789` 格式化奖励值、投影敌人位置、选择font9，再调用 `CEffectLayer::AddTextEffect`。ARM `0x950AC` 明确将该次奖励传给字符串格式化。重建 `CombatScene::RewardEnemy` 只更新经验、库存及统计，没有对应效果。

`CLevel::Bind :121860` 从 BIG 读取 `IDS_HUD_EXPERIENCE_UP`／`IDS_HUD_POINTS_UP`；经验值仍来自 ENEMY 与原倍率。`AddTextEffect :66884` 最多20条；`TextEffect::Update :67086` 每秒上浮100屏幕单位，alpha每秒减0.5；`TextEffect::Draw` ARM `0x394D4–0x394F4` 以font9宽高居中。现创建时捕获屏幕位置，2秒移除；后续镜头移动不重新投影，暂停不推进，重开清除，尸体不会重复发浮字。

宿主适配边界：记录保存在 `CombatScene::ExperienceText`，数值交给 `SurvivalHud` 按原STR/font9绘制；没有声称移植了完整 `CEffectLayer`。Windows相机统一转换到既有1024×768 HUD逻辑画布。查阅 `entries/enemy_template.bt`、`bitmap_font.bt`。

## 验证记录

统一静音执行 `bin/x64/Release/gun_bros_research.exe --mute <参数>`。

- `--movie-check`：`out/decoration-border-red.log` 退出1，上／下边框像素0/0；修后 `out/decoration-border-green.log` 退出0，两者840/840，保留 `out/powerup-border-check.png`。
- `--powerup-play-check`：`out/xp-airstrike-red.log` 退出1，三次实际击杀浮字数量全部0，三种选择器空袭外围仅225个流光像素。
- 初次修后 `out/xp-airstrike-green.log` 退出0：三次击杀分别1/2/3条；三种空袭外围65010像素。六条路径的暂停、伤害、库存、恢复及取消检查仍通过。
- 最终 Release 构建：`MSBuild gun_bro_re.slnx /p:Configuration=Release /p:Platform=x64 /m /v:minimal /nologo`，`out/xp-decoration-final-build.log` 退出0。首次沙箱内构建遇到 MSBuild FileTracker 权限错误，沙箱外构建通过；不属于游戏代码错误。
- `out/xp-decoration-final-powerup.log` 退出0：实际死亡生成的浮字在0／1000／2000ms的RGB总亮度分别为861948／429729／0，1秒上浮100逻辑屏幕单位且alpha=0.5。镜头变更、零增量、尸体不重发、重开清空均通过；三个选择器空袭各观测7个退场采样，最后外框消失。保持三种伤害240／500／1600和单次扣库存。正式生存两波35击杀检查同时通过。
- `out/xp-decoration-final-movie.log` 退出0，175个Movie结构和现有字体／精灵图集回归；边框上下各840像素。
- `out/xp-decoration-final-weapons.log` 退出0，现有射线／弹体／电弧与武器效果回归。
- `out/xp-decoration-final-selector.log` 退出0，15项、14次购买、38次选择及原存档重载回归。
- `out/xp-decoration-final-store.log`：商店卡开闭、原文本和章节、购买、余额不足、预览、装备及存档重载的功能断言全部通过（failures=0）。已查看本轮 `out/store-card-powerups-open.png`，三行外围完整。程序随后进入既有210把武器参考图生成阶段，该额外研究循环主动结束，未声称完整图集自然跑完。
- `git -c core.safecrlf=false diff --check` 退出0。Release构建仍有既存的 `MapScene` C4244转换警告。

可视证据：`out/xp-text-0.png`、`xp-text-1000.png`、`xp-text-2000.png`；`out/airstrike-check-0-selector.png`、`airstrike-frame-closing.png`；`out/store-card-powerups-open.png`。浮字图来自隔离实际死亡检查，黑底用于量化渐隐；并非伪造游戏经验或替换原资源。

没有同场景 iOS 逐帧视频；按原资源与函数检查，不把本次修复等同全部UI与战斗细节均已1:1。
