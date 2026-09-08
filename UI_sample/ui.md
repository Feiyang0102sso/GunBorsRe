## 概览

有关网络的部分

可以在cfg中设置IsConnected = 0/1 来测试不同状态

某些一定需要联网并要求登录之类的就不管他 逗人/pvp 我一个人进去看看图也行

同时可以设置 DebugMode  = 0/1 来显示 debug 信息 debug信息非常重要 能记的都可以记录在屏幕上

cheatcode 只要你认为有助于测试都可以加上
所有都已ch开头 后面想些什么随你



## 教程

当没有存档时第一次进入游戏会进入一个教程 选择兄弟 大致是教你如何切枪 送你步枪 然后捡手雷炸大怪 大怪在被手雷攻击前是无敌的 同时有一部分存档用来记录相关的教程信息 是否完成等等

![image-20260908132540840](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\image-20260908132540840.png)

![image-20260908132606793](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\image-20260908132606793.png)

![image-20260908132824077](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\image-20260908132824077.png)

对了 目前兄弟逻辑还是有点问题 他装备默认装备 目前的模型渲染怪怪的，不知道哪儿有问题 而且他的出生点和玩家叠在一起了

## 进入游戏

loading时右下角会出现一个跑动小人的动画

只要是加载都会跑这个小人动画 不管是从哪儿切换到哪儿

![image-20260908121940283](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\image-20260908121940283.png)

loading完后

显示签到奖励 这玩意要求联网 可以暂时把他调成不用联网使用本机时间

新增作弊码 cht (表示cheattime) 来快速获取下一次奖励 

我不知道5天之后会发生什么

同时增加chm快速获得5,000金币和5,00绿币

右侧显示当前exp经验和等级登记上限200 没有三位数时前面补0

到200级条子就不动了会一直是空的

那个帧数是pc版debug用的 可以加上

当前版本帧数似乎不高 老是卡

那一栏图标有一个一个个跳出来的动画

![image-20260908122023802](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\image-20260908122023802.png)

bro ops / brotherhood 的按钮可以点击 然后跳转到对应界面

## BROS

要求联网 pc 进去长这样

正式版现在进不去 会显示断网

![image-20260908122659311](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\image-20260908122659311.png)

![image-20260908122712492](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\image-20260908122712492.png)

![image-20260908122726517](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\image-20260908122726517.png)

点击下面的invite friend

会弹出一个界面 pc版这个界面不知道咋关掉 还是那句话 pc版bug一堆

![image-20260908122753069](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\image-20260908122753069.png)

网络相关的现在不考虑 但可以把他们的功能做出来

![2026_09_08_12_41_IMG_0798](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\2026_09_08_12_41_IMG_0798.PNG)

为了测试他们都让他们能点击或者准备对应的作弊码

## BRO OPS

recuit/request目前都是空的我也不知道ios版会发生什么

依旧要网络

![2026_09_08_12_41_IMG_0799](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\2026_09_08_12_41_IMG_0799.PNG)

![image-20260908123006185](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\image-20260908123006185.png)

## STORE

GUNS/ARMOR/POWER UPS/BANK bank按钮为绿色

玩家在右侧 可以点击蓝色的按钮切换武器 可以绑定快捷键Q

玩家默认嘴里有一个香烟 不知道当前版本漏了没有 我记得可能是没有的

装备的/已购买的会有额外mark

### GUNS

![image-20260908123144279](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\image-20260908123144279.png)

![image-20260908123322474](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\image-20260908123322474.png)

右下角filter可以打开筛选 支持选择多个进行同时筛选

武器条目

可以进行预览

![image-20260908123424607](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\image-20260908123424607.png)

最前面有换钱和一个特价礼包

我的意见是把所有都列出来

![2026_09_08_12_42_IMG_0800](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\2026_09_08_12_42_IMG_0800.PNG)

### ARMOR

同理

![2026_09_08_12_42_IMG_0802](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\2026_09_08_12_42_IMG_0802.PNG)

![2026_09_08_12_42_IMG_0801](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\2026_09_08_12_42_IMG_0801.PNG)

### POWER UPS

点进去可以显示在哪个模式可用

![2026_09_08_12_42_IMG_0803](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\2026_09_08_12_42_IMG_0803.PNG)

![2026_09_08_12_42_IMG_0804](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\2026_09_08_12_42_IMG_0804.PNG)

![2026_09_08_12_42_IMG_0805](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\2026_09_08_12_42_IMG_0805.PNG)

### BANK

![image-20260908123643039](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\image-20260908123643039.png)

由于是内购 会弹出 please wait 应该还原这个 让他跑3-5秒然后到账

![image-20260908123734293](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\image-20260908123734293.png)

![2026_09_08_12_43_IMG_0806](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\2026_09_08_12_43_IMG_0806.PNG)

## REFINERY

![2026_09_08_12_43_IMG_0807](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\2026_09_08_12_43_IMG_0807.PNG)

![2026_09_08_12_43_IMG_0808](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\2026_09_08_12_43_IMG_0808.PNG)

除了第一个其他都要联网

## SETTING

一个滚动菜单 可以向下滑动

![2026_09_08_12_44_IMG_0809](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\2026_09_08_12_44_IMG_0809.PNG)

## GAMES

PC版没有 IOS版有 点不进去 不知道为什么

## PLAY

### 选择模式

live和vs 需要联网

选择模式后模式按钮会去右下角

点他可以再次选择模式

![2026_09_08_12_44_IMG_0810](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\2026_09_08_12_44_IMG_0810.PNG)

![2026_09_08_12_44_IMG_0811](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\2026_09_08_12_44_IMG_0811.PNG)

![2026_09_08_13_04_IMG_0826](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\2026_09_08_13_04_IMG_0826.PNG)

### 星图

主地图 可以拖动 星球会随之变化

包括间距之类的

![2026_09_08_12_44_IMG_0812](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\2026_09_08_12_44_IMG_0812.PNG)

### 标准星球

选关界面长这样

显示rev然后点进去再显示 wave

完美的表现会被打成金色

![2026_09_08_12_45_IMG_0813](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\2026_09_08_12_45_IMG_0813.PNG)

![2026_09_08_12_45_IMG_0814](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\2026_09_08_12_45_IMG_0814.PNG)

### 僵尸星球

10个 horade 进去没有wave直接play

![2026_09_08_12_45_IMG_0815](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\2026_09_08_12_45_IMG_0815.PNG)

![2026_09_08_12_45_IMG_0816](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\2026_09_08_12_45_IMG_0816.PNG)

## 游玩

![2026_09_08_12_46_IMG_0817](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\2026_09_08_12_46_IMG_0817.PNG)

红色按钮可以购买装备

左右轮盘移动 考虑到整体画面，目前可以保留轮盘但是把所有功能都交给wsad和鼠标左键。

蓝色按钮切枪 绑定Q

![2026_09_08_12_47_IMG_0818](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\2026_09_08_12_47_IMG_0818.PNG)

可以直接选择装备到装备栏复活或者其他一些可以直接使用

![2026_09_08_12_58_IMG_0824](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\2026_09_08_12_58_IMG_0824.PNG)

游戏内菜单

同样是滚动菜单

![2026_09_08_12_47_IMG_0819](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\2026_09_08_12_47_IMG_0819.PNG)

完美的表现不会显示具体给了多少只会显示+10% 目前显示具体信息很管用 可以当作debug信息

![2026_09_08_13_07_IMG_0827](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\2026_09_08_13_07_IMG_0827.PNG)

## 结算

死亡或投降后会优先弹出武器熟练度 如果没满的话

![2026_09_08_12_47_IMG_0820](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\2026_09_08_12_47_IMG_0820.PNG)

x掉之后显示结算信息

![2026_09_08_12_47_IMG_0821](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\2026_09_08_12_47_IMG_0821.PNG)

右边casualties可以显示击杀信息

![2026_09_08_12_55_IMG_0822](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\2026_09_08_12_55_IMG_0822.PNG)

然后去精炼矿石，在完成精炼之前上方菜单都不会出现然后再一次弹出

![2026_09_08_12_57_IMG_0823](E:\coding_projects\c_projects\gun_bro_re\UI_sample\assets\2026_09_08_12_57_IMG_0823.PNG)