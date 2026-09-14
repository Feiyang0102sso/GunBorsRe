# Live 测试兄弟：好友与存档依据

研究日期：2026-09-14。范围：原 iOS 3.6.0 好友持久化、单人伙伴选择与 Windows 本地测试好友边界。本文是研究结果与实现建议，不表示 Live 全流程已完成复刻。

## 直接结论

**原版没有一个保存完整 bro 名单、名字、装备和等级的 `1006` 存档。** `1006` 只保存收到的好友经验礼物记录；当前选择的好友凭据另存为文件 `A`。好友名单、昵称及好友自身的装备和进度通过 NGS 好友对象、服务属性和运行时缓存取得。

原版单人模式确实把当前好友的配置和进度交给 `CGameFlow::ConfigureBrother`。因此同一个本地测试好友可以在好友列表被选择，再作为单人伙伴；这里有原版的配置绑定入口，不需要把好友记录塞入玩家自己的装备存档。

本地测试 bot 的身份与好友关系是 Windows 宿主新增数据，建议在账户目录使用明确命名、带版本的独立本地扩展文件保存；不要把它伪装成 `A` 的 NGS 凭据，也不要修改 `1006` 的含义。

## 文件与字节布局

以下偏移均相对于客户端负载；不是对象内存偏移，也不是所有存档的固定文件偏移。多字节整数按原 ARMv7 存档使用小端。

| 文件/记录 | 数据 | 原版依据 |
|---|---|---|
| `-1_1006`，DataStore 1006，版本 0 | 好友 XP 礼物环 | `CFriendDataManager::GetSaveSize :198759`、`LoadFromDisk :199518`、`SaveToDisk :199558` |
| `A`，不是编号 DataStore | 当前选中好友的 `CNGSUserCredentials` | `ACTIVE_CRED_FILENAME :27833`、`SaveToDisk :199586`、`FetchFriendsManagerInfo :200669` |
| `-1_1001`，DataStore 1001，版本 0 | **当前账户本人**的装备配置，不是所有好友配置 | `CPlayerConfiguration::LoadFromDisk :170602`、`SaveToDisk :170931` |
| `-1_1000`，DataStore 1000，版本 0 | 当前账户本人进度；包含好友赠送 XP 的窗口时间和累计量 | `PlayerProgressSnapshot` 模板、`AddBonusExperienceFromFriend :194244` |
| DataStore 1017 | 挑战槽中的好友通知/进度引用 | `CChallengeProgressData::LoadFromDisk :298792`；不构成完整好友名单 |

### 1006：XP 礼物环

| 负载偏移 | 字节 | 类型 | 含义 |
|---:|---:|---|---|
| 0 | 2 | u16 | 下一次礼物写入槽 `nextGiftWriteSlot` |
| 2 | 2 | u16 | 有效记录数 `giftRecordCount`，原逻辑上限 20 |
| `4 + 8 × i` | 4 | i32 | 赠送者 `friendClientId` |
| `8 + 8 × i` | 4 | u32 | `experienceGift` |

负载长度为 `4 + 8 × giftRecordCount`。`ProcessPlayerXPFromFriend :199797` 的 `:199823–199831` 写入客户端 ID、XP 数量并按 20 槽回绕；`CreatePlayerXPBonusString :200093` 按好友 ID 汇总。没有昵称、头像、武器、等级或时间戳字段。

模板：`_prep/_Big_tool/binary template/saves/save_payloads.bt` 中 `FriendDataPayload`，入口为 `GB_save_profile.bt`。

只读实物复核：`_prep/saves/-1_1006` 长 536 字节，version=0、wrapperMode=0、prefixSize=254，因此负载起点 `12 + 254 = 266`；4 字节负载为 `00 00 00 00`，两个 u16 都为 0。SHA256：`D3AECFE679635FB9486F074C15FD799DEFCB56BF06F01350C7A3C5C20524459E`。没有非空原版礼物环样本；非空布局依据读写代码，未声称已通过非空实物验证。本次未重新运行 CRC 算法，既有验证见 `_prep/docs/save-binary-templates.md`。

编号 DataStore 的当前封装为 `u32 version / u32 wrapperMode=0 / u32 prefixSize / prefix / payload / suffix / i32 owner / u32 CRC-32/BZIP2`；不可把样本的 266 硬编码为其他记录的负载起点。

### A：活动好友凭据

`CFriendDataManager::SaveToDisk :199558` 先写完 1006 负载；活动好友存在且不是默认兄弟时，另调用 `CNGSUser::SaveCredentials(activeFriend, "A")`。

`CNGSUserCredentials::writeToFile :288844` 将 `CreateObjectRepresentation` 经 `JSONParser::encodeValue` 编码，再用 `WriteJMUtf` 写出。因此 `A` **不是**与 1006 相同的固定字段二进制记录，也不是一个保存完整装备的 JSON 账户。具体 JMUtf 字节前缀、本批不存在的 `A` 实物和凭据所有键的精确语义，本研究未完成核对；不在此虚构字段顺序。

恢复选择的流程为：

1. `FetchFriendsManagerInfo :200669` 先选默认兄弟并重置活动凭据。
2. 若 `A` 存在，调用 `CNGSUserCredentials::readFromFile`。
3. `CNGSFactory::getRemoteUserByCredentials` 解析远端用户，且必须满足 `CNGSRemoteUser::GetIsFriendOfLocalUser` 才采用。
4. `FriendsManagerInfoLoad :200740` 等待资料就绪；资料无效时回到默认兄弟。
5. `ValidateActiveFriend :199934` 在已加载远端好友列表中重新寻找凭据；不再存在则恢复默认。

因此仅向磁盘写一个自编的 `A` 不能建立原版认可的完整好友，也不能代替好友数据服务。

另外存在 `LoadFromDisk(CResourceLoader*) :199425` 的非 DataStore 重载，使用 `DATAMODS_FILENAME[0]`，当前反编译全局值显示为 `"p"`，读取的也是同一礼物环。该历史入口的调用条件与当前 `COptionsMgr` 也使用 `p` 的关系尚未证实；不能据此改写当前选项文件。

### 1001：单个账户配置

原代码整块写出 `CPlayerConfiguration` 的 `mem+12` 起 120 字节。每个装备引用为 `u32 packHash / u16 cachedPackIndex / u8 ordinal / u8 alignment`；槽位决定类型。

| 负载偏移 | 字节 | 内容 |
|---:|---:|---|
| 0 | 16 | 两把枪的引用 |
| 16 | 16 | 两个弹种引用 |
| 32 | 32 | 四个盔甲槽引用 |
| 64 | 1 | 当前枪槽 0/1 |
| 65 | 1 | 角色选择 |
| 66 | 2 | 对齐 |
| 68 | 8 | 两个 u32 枪熟练度 |
| 76 | 8 | 两个 u32 活动熟练度 |
| 84 | 32 | 用途未明配置值，保留原样 |
| 116 | 2 | 左右 powerup ordinal；FF 的含义由默认道具选择逻辑处理 |
| 118 | 2 | 尾部对齐 |

装备属性、售价和默认配置仍需从 BIG 及原 Reset 消费流程取得；存档只保存状态与原资源引用。

## 好友缓存与单人绑定

- `CFriendDataManager::FetchFriendListData :200549` 从本地用户的 `CNGSRemoteUserList` 取得好友对象，按批次取资料；列表缓存并非 1006。`GetFriendCount :198813` 从该列表取得数量。
- `CFriendData::handleResponseLoadFromServer :205275` 从服务返回属性表装载 `CFriendData mem+220` 的 `CPlayerConfiguration` 和 `mem+132` 的 `CPlayerProgress`。`IsCached :205310` 判断资料时间加 120000 毫秒是否仍在有效期内，这是运行时资料缓存判断，不能称其已写在 1006。
- `GetFriendAvatarConfig :198839` / `GetFriendAvatarProgress :198819` 在索引为 -1 时使用活动好友对象。
- `SetActiveFriend :200362` 保存对象指针并复制其凭据；空条目恢复默认兄弟。
- `CGunBros::InitMission :78858` 在 `GetGameType()==1` 时取活动好友配置/进度，调用 `CGameFlow::ConfigureBrother :77430`。后者复制配置负载前 118 字节和对应进度数据，明确支持使用好友的独立装备与等级。
- `InitDefaultBrother :200797` 使用原 `IDS_FRIEND_DEFAULT_BRO1/2` 文本并复制传入配置；默认兄弟不是从 1006 构造出的好友记录。
- 反编译中可找到 `CFriendsManager` 的类注册符号，但本文实际追踪的好友容器消费者为 `CNGSRemoteUserList` 和 `CFriendDataManager`，不凭类名假设另一个完整磁盘列表格式。

## 当前重建代码与建议接入

研究时当前代码：`NativeProfile.cpp :138` 只验证/保留 1006 的环形记录负载；`NativeProfileArchive` 保留原文件/负载以及未知字段，没有完整好友资料结构。`MenuInternal.h` 的 `selectedLocalFriend` 仅属会话菜单状态；`OriginalSocialContent.cpp` 可选 LOCAL BOT，`GameFrontEnd.cpp` 将选择传给单人 `SurvivalLaunch.localBot`，重启选择尚未持久化。Live 当前另用试玩档副本，不能等同已恢复原版账户结算。

建议的最小实现边界：

1. Windows 本地好友记录只新增版本、稳定本地身份、显示名、活动选择及其独立账户目录引用；名称明确标为本地测试身份。避免使用 NGS client ID 冒充远端好友。
2. bot 独立装备/库存/货币/进度复用原 NativeProfile 序列化与 BIG 引用；不要在好友扩展文件复制武器/盔甲属性表。初建数据须明确来源于原新账户默认或明确的测试账户副本。
3. 菜单与单人伙伴绑定读取同一个本地好友对象；Live 匹配提供器也返回这一身份。bro 作弊码通过显式本地测试 bot 身份判断生效，不能仅检查“第二个角色”或多人角色就作用于真人。
4. bot 自己的商店与道具消费使用其自己的账户和交易路径；玩家档只记录玩家结果，不能互相扣费或共享可变库存。
5. 保存/重载验证覆盖活动好友不丢失、双方账户互不污染、1006 与未知原字段保持不变。原 `_prep/saves` 只读。

以上为实现建议，具体扩展文件名及 API 尚未在本研究中实施。当前证据不支持宣称“原版完整好友名单离线落在某个编号存档”，也不支持把本地机器人身份称为原版 NGS 协议数据。
