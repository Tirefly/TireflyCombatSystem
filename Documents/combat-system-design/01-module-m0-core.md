# 01-module-m0-core.md — M0 内核设计

- 日期：2026-09-02
- 状态：设计 v1+（编译层模块名 **TcsCore**，R0 §9；依据已拍板决策 D0-1~D0-6、MD-1/MD-2；v1+ = M9 收尾轮折入 D0-6 诊断日志系统；v2 = PV 系列折入——FTcsParamValue 载体体系，2026-09-11）
- 职责一句话：**为战斗系统提供零战斗语义的运行时底座——池化与句柄寻址、事件总线、时钟泵、诊断。M0 不知道"战斗"是什么。**

## 1. 模块边界

- 消费者：M1–M8 全部模块。
- 依赖：仅 Engine 基础模块（Core、CoreUObject、GameplayTags）；**禁止 include 任何战斗域头文件**（铁律 1 的底）。
- 形态：纯逻辑结构体（池/堆/总线内核）+ 两个 `UTickableWorldSubsystem` 门面；可脱离 Actor 运行（Mass 兼容的前提）。

## 2. 类型词汇（对外）

### 2.1 句柄与池（D0-2、D0-4）
- `TCombatInstanceHandle<T>`：`uint32 Index + uint32 Generation`（64bit）。Index 直入池数组 O(1)；Generation 槽位复用时 +1；**悬空访问 = Development ensure 失败 / Shipping 返回无效**。
- 每实例类型独立别名（`FStateHandle = TCombatInstanceHandle<FStateTag>` 型式）——编译期防互串（铁律 4 的句柄版）。
- `TInstancePool<T>`：稠密数组 + freelist + 代际数组；`Allocate/Free/Resolve/IsValid/ForEach`；**单游戏线程断言**（`IsInGameThread()` ensure，越线即崩暴露架构违规）。
- M0 只提供**机制**；具体句柄类型（FStateHandle 等）由各域模块定义。
- **FTcsSourceHandle（归属来源标识）归本模块**（2026-09-02 D7 轮定案，M9 收尾轮措辞校正）：系统级通用**归属键**——修正器/瞬时流程/操作的"挂在谁的来源下"标识；只做归属，**不承载因果链**——因果链由对象图天然边承载（修正器→来源对象→施加者，每层对象自带 Source 字段，查询从明确起点沿对象逐级走；M2 对来源内容一无所知是硬约束）；uint64 原子递增永不复用（无代际位）+ Registry 活跃集（Allocate/Release/IsAlive）；级联撤销 = 按来源分组移除（RemoveBySource），不因句柄失效产生语义分支。临时修正器与级联撤销的地基（用户最满意设计）。
- **FTcsParamValue 归本模块**（D2-12 被 PV 系列取代，2026-09-11 定稿）：全插件统一"数值配置"载体 **`FTcsParamValue{ TInstancedStruct<FTcsParamValueSource> Source }`**（默认 Literal）——抽象基类 `FTcsParamValueSource`（USTRUCT）挂 **`virtual double Evaluate(const FTcsParamEvaluateContext&) const = 0`**（D3-7 v3 虚分派 StateTree 同构；**命名 Evaluate**——动词纪律：Resolve 仅句柄/Id→对象，禁泛化偷懒；**实现偏差**：UHT 为 USTRUCT 无条件生成 TCppStructOps 需可默认构造，纯虚不可编译——改用 StateTree 同款默认体 + `meta=(Hidden)`，plan1 已记"待追认"）；内置源 `FTcsParamSource_Literal{Value}`（原 Scalar.Literal 模式，命名用户拍板沿用 Literal）与 `FTcsParamSource_ParamRef{Key, Fallback}`（参数嵌套：链式允许、**编辑期 DAG 去重**——禁自引用/成环、限同域参数表、Fallback 必填）；解析上下文 `FTcsParamEvaluateContext`（Subject 句柄 + 参数表只读 + 级别位，零领域词汇；领域扩展 = 结构体继承 + checked cast，InstancedStruct 备选）；纯函数性纪律（同上下文同结果，随机/时间禁入——D0-1）。零战斗语义数据词汇，使用者 = 04 链字段/05 时段冷却/03 DurationTime/修正器 Operand（细则见 `2026-09-10-param-value-source-decision-points.md`）。**领域源与实体契约不进本模块**——等级表源/`ITcsEntityLevelProvider` 归 TcsState（评判轮修订，接口分离+域词汇下沉）。
- **PV 系列 2026-09-14 增补（用户拍板）**：①`FTcsParamValue` 补 **`Evaluate` 便利转发**（`Source.IsValid()` 兜底 0——消解调用点每次 `.Source.Get()` 的噪音）；②上下文补齐 **`Subject`（`FCombatEntityHandle`）与 `EffectiveLevel`（int32）**——落地时机 = **随 TcsState 等级源同批**（等级源/属性源是其唯一消费者；当前代码只有 `ParamTable`，属已记偏差）；③**`FCombatEntityHandle` 落本模块的边界让步记录**——它是 Core 唯一持有的战斗实体词汇（plan1"具体句柄类型由各域定义"口径的例外：上下文住 Core 且不得反向依赖域模块；02 §2.2a / 06 已把它当全插件通用实体键使用）；④**源的可枚举能力基类 `FTcsParamEnumerableSource`（PV-10）住本模块**——可选能力（`Enumerate` / `GetIndexForLevel`；**索引解析唯一真相在源、`Evaluate` 同源**），随 TcsState 等级源同批落地、**不进 plan1 Task 0**（零消费者不预建）；⑤**PV-8 校验矩阵扩为四联**：`源 × 上下文 × 可配约定（D5-18 v3）× 视图兼容（D5-17 v3：视图 `IsCompatible(Probe)`）`——M8 显式交付物。

### 2.2 事件总线（裁决 2a + D0-3 + BP/CS 动态监听层）
- `FCombatEventBus`：Tag 路由 + 订阅表（`TMultiMap<FGameplayTag, FEventSubscription>`）+ 共享 Handler UClass（事件类型 → Handler CDO 执行；struct 上不自绑 delegate）。
- **两条通道**：立即派发（同步）与帧末队列（泵统一 flush）；订阅退订走 `FEventSubscriptionHandle`（句柄配对清理，复用 2.1 机制）。
- 载荷规约：核心词汇级事件 = 具体 FStruct（数量少、结构稳）；域扩展载荷 = `FInstancedStruct` 扩展位；**核心不 include 域扩展头**。
- **BP/CS 动态监听层（M9 收尾轮拍板 A'，Lyra GMS 形态）**：router 内泛化动态多播 `(EventTag, FInstancedStruct)`——Publish 同步喂（立即通道）/flush 时喂（帧末通道）；`UTcsAsyncAction_ListenForCombatEvent{Tag 过滤, PayloadType(UScriptStruct*) 类型匹配过滤, MatchType(精确/部分)}` 提供"绑定即过滤"的 BP/CS 订阅入口（Lyra 先例：FInstancedStruct 作动态委托参数已 shipped；用户线索：FInstancedStruct 支持反射与网络同步，载荷较大——总线不复制，网络面未来讨论）。K2 强类型引脚节点 = 未来项（M8 轮）；类型化委托 = 未来增量（哪个事件用得痛再单独加）。C++ Handler CDO 路径原样不动。验证项两条（不阻塞 R3）：BP InstancedStruct 节点面版本覆盖、CS 读 FInstancedStruct 载荷实测。

### 2.3 时钟与到期堆（D0-1、D0-5、D3-6）
- `UCombatClockSubsystem : UTickableWorldSubsystem`：PrePhysics 前把 ScaledDt 泵给订阅者与到期堆。
- `FCombatClock`：唯一"取时间"入口（帧号 + 累计 ScaledDt + DeltaSeconds）；`ITimeSource` 可注入（默认 `World DeltaSeconds × TimeDilation`；回合制宿主注入"回合即时间"）。
- `FExpiryHeap`：按到期时间排序的最小堆；惰性删除 + 代际校验；**帧成本 = 到期项数**。服务 M3 到期、M4 定时步骤、M5 冷却。
- 确定性纪律（D0-1）在此落地：禁 wall-clock（时钟封装内不出现 `FDateTime::UtcNow` / `FPlatformTime`——review 检查点）；遍历稳定序；随机流显式注入。

### 2.4 诊断（D0-6 最终收缩：日志归 UE 原生，无自建设施）
- **日志 = UE 原生分类制**：每模块声明自有分类（`DECLARE_LOG_CATEGORY_EXTERN(LogTcsCore, Log, All)`，命名规范 `LogTcs<模块名>`；**声明独立成通道文件** `Public/<模块名>LogChannel.h` + `Private/<模块名>LogChannel.cpp`——使用日志 include LogChannel 头，不 include Module.h）——verbosity 全套、运行期 `log <分类>` 控制台可调、查看器按分类过滤，零自建代码（用户最终拍板：统一注册入口/门面/级别均无必要——注册表无消费者、自建转发反丢分类过滤、面板清单可后补）。
- **屏显 = 测试装置/宿主职责**：插件模块不做屏显调用（R3 验收信号由测试装置直调 AddOnScreenDebugMessage）；showdebug 面板 = 未来接口位（届时日志路由随面板回归）。
- 统计 CVar（池占用/堆深度，随对应设施落地）；`UTcsDeveloperSettings` 基类。不做诊断 UI（M8 的事）。

## 3. 入口服务

- `UCombatClockSubsystem`、`UCombatEventBusSubsystem`（各司其职，不做上帝单例）。
- `FCombatCore::Get(World)`：轻量 facade，一次解析双子系统指针。

## 4. 网络姿态落点（NET-1/2）

- 池与总线预留**镜像模式接口位**：镜像侧的 Allocate/写入只接受复制操作流重放；本期只留接口不实现。
- 无跨 World 静态状态；子系统按 World 分离（PIE 安全）。

## 5. 非目标

不做网络实现、不做 ECS 框架化、不做反射工具箱、不做多线程、不含任何战斗词汇、不做诊断 UI。

## 6. 依据

- 拍板：D0-1~D0-6（2026-09-02 问答框；D0-6 三轮演化定稿——注册+级别门面 → 砍自建级别（UE 原生 Verbosity）→ **撤销设施**（统一注册入口无消费者，用户最终拍板）：日志归 UE 原生分类制（`LogTcs<模块名>` 命名规范）、屏显归测试装置/宿主）；裁决 2a/2b；**PV 系列（2026-09-10/11：FTcsParamValue{TInstancedStruct} 取代 D2-12 FTcsParamScalar、Evaluate 命名、Literal/ParamRef 内置；等级表源与实体等级接口归 TcsState——评判轮修订）**；**PV-1 增补 + PV-10（2026-09-14：Evaluate 便利转发、上下文补 `Subject`/`EffectiveLevel`、实体身份句柄边界让步记录、`FTcsParamEnumerableSource` 可枚举能力基类住本模块）**。
- 证据：AbilityKit core 层取证（池/BuffRuntime 对象池/中央管理器形态）；TCS 13 报告（诊断与时间语义缺失的教训）；ZZ:68（TCS 零 ScaledDt 实证）。

## 7. 验收钩子

竖切剧本（`2026-09-02-r3-vertical-slice-script.md`）第 3（flush 时序）、4（句柄悬空 ensure）、7（ScaledDt 冻结）项落在 M0。
