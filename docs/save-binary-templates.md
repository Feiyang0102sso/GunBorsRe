# 原版存档 Binary Template 索引

覆盖实际目录`saves/`的全部21个文件：18种编号DataStore、1000/1003各一份`.perfect`副本，以及独立的PDST。20个封装文件均通过原算法CRC校验；全部21个文件的SHA256与研究前一致。文件不是统一固定长度，536字节只是小负载的封装结果。

阅读入口：[GB_save_profile.bt](<../_Big_tool/binary template/saves/GB_save_profile.bt>)，在`saveStoreId`选择文件末尾编号。详细字段集中在[save_payloads.bt](<../_Big_tool/binary template/saves/save_payloads.bt>)；47项统计见[save_statistics.bt](<../_Big_tool/binary template/saves/save_statistics.bt>)。保留用户原来的[1000彩色模板](<../_Big_tool/binary template/saves/GB_save_1000.bt>)和[1003位图模板](<../_Big_tool/binary template/saves/GB_save_1003.bt>)，追加原代码补注，不删除旧观察。

源码行号均指`_IDA_OUT/gunbros_3.6.0_IOS.c`，不是原.cpp行号。模板末尾附原函数、ARMv7地址和符号恢复的原工程文件路径，例如`src/gunbros/playerProgress.cpp`、`profileManager.cpp`、`friendsManager.cpp`。

## 文件与原类

下表负载大小不含随机填充、版本、owner及CRC。偏移为当前样本负载在整个文件中的绝对位置，不可拿这个数字硬编码其他存档。

| 文件末尾ID | 原客户端/记录类 | 版本 | 文件字节 | 负载偏移/字节 | 内容 |
|---|---|---:|---:|---|---|
| 1000及`.perfect` | CPlayerProgress | 0 | 536 | 244 / 48 | 原料、双货币、经验等级、好友经验窗口、首次启动/内购/推送标记 |
| 1001 | CPlayerConfiguration | 0 | 536 | 208 / 120 | 两枪、两弹、四甲引用，当前枪槽、角色、熟练度及未明配置 |
| 1002 | CPurchases / PurchasedItem | 0 | 536 | 221 / 94 | count=9；每项10字节，引用键与1字节数量；2026-09-09按原符号纠正旧名CPurchasedItems |
| 1003及`.perfect` | CMissionWaveStatus / MissionWaveInfo | 0 | 3096 | 236 / 2624 | count=5；每项524字节，关卡进度与4096位完美波图 |
| 1004 | CMissionObjectiveStatus / MissionObjectiveInfo | 0 | 536 | 266 / 4 | 当前count=0；完成目标集合，每项只有8字节键 |
| 1005 | CKillTracker / KillTrackerWeapon | 0 | 536 | 238 / 60 | count=4；每项14字节，武器击杀数 |
| 1006 | CFriendDataManager | 0 | 536 | 266 / 4 | 礼物环形写槽、记录数；当前无记录，非完整好友资料 |
| 1007 | CTutorialManager | 0 | 536 | 257 / 22 | 22个教程已看标记 |
| 1008 | CRefinementManager | 1 | 536 | 98 / 340 | 时间检查点与12个精炼槽，每槽28字节 |
| 1009 | CDailyBonusTracking | 0 | 536 | 262 / 12 | 上次启动、连续访问累计秒数、上次领取的连续天序号 |
| 1010 | CPlayerStatistics | 0 | 536 | 172 / 192 | count=47，随后47个u32统计值 |
| 1011 | CPrizeManager | 0 | 536 | 266 / 4 | friendMax，好友奖品进度记录 |
| 1012 | COfferDataManager | 0 | 1560 | 254 / 1052 | 商业活动、启动/商店访问计数、店铺ID字符串、社交激励字段 |
| 1013 | CWeaponMastery / WeaponMasteryWeapon | 0 | 536 | 238 / 60 | count=4；每项14字节，武器熟练度经验 |
| 1014 | CContentTracker | 0 | 536 | 173 / 190 | 13包内容“已看过”位图，按四种内容类型分组 |
| 1016 | CMissionHighScore / MissionScoreInfo | 0 | 536 | 266 / 4 | 当前count=0；每项14字节，关卡最高分 |
| 1017 | CChallengeManager / CChallengeProgressData | 5 | 2584 | 235 / 2114 | 8槽挑战状态、8组好友通知及8组本地进度计数 |
| 1018 | CPackageOfferMgr / PackageOfferItem | 0 | 536 | 266 / 4 | 当前count=0；每项14字节，末尾数值的具体业务意义尚未证实 |
| PDST | CProfileManager状态数组 | 无 | 19 | 0 / 19 | 每个DataStore的同步/恢复状态；没有CRC封装 |

注册证据：`CGunBros::Init`，反编译行80407–80472。**1015无注册对象，也无本批文件**，但PDST数组仍保留该槽。当前1003和1003.perfect的完整SHA256相同；1000两份不同，不能仅凭文件名假设每份`.perfect`都有额外结构。

## 封装和校验

读取器`CProfileManager::LoadFromDisk`位于202880，写出器`SaveToDisk`位于203163。当前格式按小端顺序为：

```
u32 clientVersion
u32 wrapperMode = 0
u32 prefixSize
byte prefix[prefixSize]
客户端专属负载
byte suffix[由负载与封装长度推导]
i32 ownerClientId
u32 crc32Bzip2
```

设客户端真实负载为`P`，原代码使用`A = P + 512 - P % 512`，文件总长`A + 24`；即使P整除512也会再增加512。前缀长度为`(A >> 1) - (P >> 1)`。随机的是填充内容，不是这个长度；`GetRand(0x7FFF)`按u32写入，残余字节补0。前后缀不承担真正的字段存储，也不是负载的加密算法。

必须按客户端结构确定负载终点。`文件长度 - 24 - 2*prefixSize`只能得到向下取偶数后的P，奇数负载会少1，不能把它当万能解析公式。当前样本虽多为偶数，这个边界由写出公式决定。

CRC范围是整个文件去掉最后4字节，含owner与随机填充。算法为CRC-32/BZIP2：poly=`0x04C11DB7`、init=`0xFFFFFFFF`、输入/输出不反射、xorout=`0xFFFFFFFF`；结果以小端存储。普通`zlib.crc32`算法不同。owner取`CNGSUser::GetClientID`；样本全部-1只证明没有有效绑定ID，不能据此推导服务器现状。

旧格式第二个u32不为0时，它本身是prefix长度，不再读第三个u32。原读取器也不走当前owner/CRC验证分支。此分支有源码证据但没有本批样本，不能声称已经过实物验证。

## 两种引用不能混用

1001装备槽保存原ARM内存的8字节`GameObjectRef`：`packHash4 + cachedPackIndex2 + ordinal1 + alignment1`。槽位本身决定对象类型；载入后通过hash协调当前包索引。

集合存档1002/1003/1004/1005/1013/1016/1018先把原内存中的2字节包索引换成4字节hash，再写剩余记录，形成8字节键：`hash4 + ordinal1 + objectType1 + syncState1 + alignment1`。例如1003的`objectType=7`指LEVEL，不能因为显示的是星球名就当成PLANET(type13)。`WriteSavedData`证据见81309及84992。

每条集合记录的syncState：0表示新改动；本地WriteSavedData把0改1；服务器成功通知置2。它与PDST不是同一套枚举。14字节记录在键后还有2字节对齐，再跟u32数值；数量字段不总是u32，1002的quantity只有1字节。

## 重要字段的消费证据

- **1000货币不是32位高位垃圾。** 原`AddCommonCurrency :193424`有64位累加，`BeginRefinement :178519`对64位原料扣除。用户模板仍保留low/high拆分观察，但通用模板按u64表示，避免低32位符号扩展。
- **1000好友经验窗口。** `UpdateFriendXPBonusTimer :194707`按网络秒比较86400；`AddBonusExperienceFromFriend :194264`根据PLAYERPROGRESSION限制奖励累计量。原代码194718把时间差写回时间字段，该可疑行为保留记录，不能在研究时悄悄改成当前时间。
- **1001当前枪槽。** 原mem+76由`CBrother::Bind :135820/:135847`选择第一或第二个CGun并OnEquip，含义为0/1槽号；不是资源ordinal。mem+96..127仍有未确定用途的配置值，注释保留清零初始化证据。
- **1001左右道具。** 2026-09-10 追踪确认 mem+128/129（payload+116/117）为左右 powerup ordinal。`CPowerUpSelector::OptionEquip :184626` 写入；`Bind :187378` 在 FF 时按 `CPowerup::IsButtonDefault :188759`（export4）选择原默认道具。NativeProfile 已接入读写及重载回归，模板保留旧观察并追加纠正。
- **1003进度和完美波分开。** `GetWaveProgress :192480`返回record+6；`AddWaves :192853`只提升进度。`WasWavePerfected :192507`读`bits[index>>3] & (1<<(index&7))`；4096是位图容量，不等于正式玩法轮数。maxPerfectWaveIndex初始0，因此0不独立证明第一波完美；位图才是每波证据。
- **1006不是完整好友列表。** `ProcessPlayerXPFromFriend :199823`写clientId，199825写赠送XP，199826按20槽回绕，199831限制有效条数。`CreatePlayerXPBonusString :200139`按好友ID累加用于展示。单独选中好友凭据另存ACTIVE_CRED_FILENAME，不嵌入1006。
- **1007教程枚举。** `SetTutorialHasSeen :210736`对索引15直接返回，索引14处理WeaponTeaserPrize。22个字节的槽名没有全部恢复，不把顺序对应成任意22段文字。
- **1008离线精炼。** 12槽各保存state、IEEE754效率、剩余毫秒、开始秒数、总毫秒、u64原料量。`Commit :178471`与`LoadFromServer :176889`可交叉核对单位。状态0锁定、1空闲、2运行、3可领；这不是BIG中16.16定点的磁盘格式。
- **1009连续签到。** `CalculateBonus :209285`按间隔小于172800秒决定是否延续；consecutive累计的是秒，连续天数=`consecutive/86400+1`。`CommitBonus :209606`保存已领天序号，奖励项按`(day-1)%周期长度`取值，不能解释成三个时间戳。
- **1010统计不是所有SESSION都代表当前局。** 击杀/波数/完美波的SESSION槽使用`SetStatGreater :75776/:75936/:76078`保存历史最大值。GreenShield槽为单次盾期间挡下伤害的最大值，证据136774、223859、224630。未找到时间/射击槽的直接累加处，单位和更新方式明确保留未知。
- **1010还区分累计量、当前量和标记。** BRO_BUFFS由`CFriendPowerManager::CalculateAggregates :234495`写当前达标条目数；LOTTERY_A/B/C由`CMenuLotteryPopup::SetState :398048`只与1取最大值，并非累计抽奖次数。JUNGLE_WAVES_CLEARED保存指定关卡最大进度，AUTOAIM_USES的实际累加处在StartAutoFire137234。每项具体依据见统计模板。
- **1011存在原代码ID不一致。** 本地注册1011，但`CPrizeManager::SaveToServer :209927`传1007；记录此差异，不把1007的本地负载改认成奖品。
- **1012字符串是4字节字符。** `SaveToServer :222773`与`CStrWChar`消费路径支持256个32位字符，整段1024字节；不能按Windows的UTF-16把后面字段提前512字节。
- **1014按包的内容提示。** `ObjectHasBeenSeen :225689`映射0盔甲、1枪、2星球、3强化。每项位图按资源ordinal低位优先；运行时另外还有“已可用”位图，当前本地序列化未把它一并写出。
- **1017是v5挑战布局。** `LoadFromDisk :298792`遇版本<=4直接Reset，不能套v5解释老数据。每个20字节本地计数包含击杀、清波位图、完美波数据、起始等级u16、起始好友数u16、已用强化位图；后两个u16不是一个未知u32。好友记录含进度和请求方向标志，末字节按对齐保留。

## PDST及未验证范围

[GB_save_PDST.bt](<../_Big_tool/binary template/saves/GB_save_PDST.bt>)对应`CProfileManager::SaveStatus :203033`原this+200的19字节数组，第i项为1000+i。Initialize203909先置4，再尝试读文件。当前样本全4，同时其他编号确有有效负载，所以全4不等于“没有存档”。

已证实转换：本地SaveToDisk203283可置1，但原状态4会被保留；服务器保存成功回调203137可置2；服务器加载回调203301也可能在203357置2，受协调标志限制。未注册客户端GetDataStoreStatus201211返回2。尚未证明其他数值的完整语义，不借用集合记录的0/1/2强行解释。

有读取器但缺非空实物记录的分支包括1004、1006礼物项、1016、1018。本次验证了当前空集合边界；非空布局来自原写出/读取代码，并未伪造一份“通过”的原版样本。1018末尾value、1001配置尾部、部分教程枚举及统计消费者仍待进一步追踪。所有原始存档保持只读，没有做回写游戏验证。

逐字段真实数值、偏移、CRC结果和SHA256见[save-catalog.json](../out/binary-research/save-catalog.json)。复核命令：`D:\Python312\python.exe src/tools/catalog_save_records.py`。模板用于研究阅读，本轮没有运行010 Editor。

## 2026-09-09：应用选项p补证

原COptionsMgr把CRC与32字节对象片段独立写入p，不在1000–1018内。见[options.bt](<../_Big_tool/binary template/saves/options.bt>)。原saves样本未包含该文件，模板与实现依据原Read/Write/Reset；已验证构造默认、SFX/MUSIC、三态AutoBro、通知、未知字节及CRC。Windows运行账户在独立存档目录创建p，原saves仍只读。挑战推送仍属于1000负载+46。
