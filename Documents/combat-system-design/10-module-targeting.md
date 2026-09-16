# 10-module-targeting.md — TcsTargeting 目标选择层设计（v2 定稿）

- 日期：2026-09-02
- 状态：**v2 定稿**——策略模式取代枚举模式（用户拍板：枚举太具体、不像模组词汇；目标语义 100% 宿主本体论）；修订记录见文末
- 职责一句话：**目标选择的策略契约与默认实现——抽象接口 + 少量默认策略 + SelectTargets 步骤执行器；只选目标，不执行效果、不渲染指示器。**

## 1. 模块边界

- 消费者：M4（SelectTargets 步骤+执行器经注册表分派）、M5（技能链编排经链步骤间接消费）、宿主（策略实现/注入接口实现/指示器渲染）。
- 依赖：TcsCore（句柄/总线）、TcsEffect（FEffectStep 基类型 + EStepResult + `UE_DEFINE_EFFECT_STEP_EXECUTOR` 自注册宏；**ICombatEntityQuery 注入接口**——机制层定义，宿主实现；实现名 ITcsEntityQuery 见 plan2）。**不依赖 TcsState/TcsAttribute/TcsDamage/TcsSkill**——单位遍历/存活/阵营判定经注入接口（具名审计 MEM-20260902-13）。
- R3：六模块之一（计划二）——**策略接口 + Self/EventTarget 两默认实现 + SelectTargets 执行器**；RadiusArea/FTargetingShape 后置（用户拍板：竖切剧本单体选择已覆盖，无消费者不预设）。

## 2. 类型词汇（对外）

### 2.1 选择器策略（策略模式取代枚举模式，用户拍板）
- `FTcsTargetSelectorStrategy`（USTRUCT 反射基类，**纯虚 `Resolve(Context, 注入查询, OutTargets)`**——解析目标集写入 Context.Targets。**载体（D3-7 v3）**：USTRUCT 基类 + C++ 虚函数分派（StateTree 同构），Def/步骤以 `TInstancedStruct<FTcsTargetSelectorStrategy>` 成员持有；BP 策略扩展通道放弃（R0 §9"蓝图不承诺"承责），宿主扩展 = C++ 新 struct 子类）。
- **默认实现（框架提供）**：
  - `FTcsSelSelf`——Context.Caster；
  - `FTcsSelEventTarget`——Context 事件载荷目标。
- **后置项**：RadiusArea（范围选择）与 FTargetingShape（形状单一来源）整体后置——竖切剧本（单体选择）无消费者；形态（裸半径 vs 形状参数化）待真实需求出现时再定（走查样例 04 §9 例二为预演形态）。TagQuery/Instigator 同批后置。
- **宿主扩展 = 新策略 struct 子类**（策略模式核心收益）：准星指向、链接目标等宿主本体论语义零框架改动；配置面 = `TInstancedStruct` 内嵌编辑（StructUtilsEditor 开箱：类型 picker + 内嵌展开 + TArray 专用 details，非 TSubclassOf 下拉）。
- **与 FEntrySelector 的边界（D4-4 纠正记录）**：`FEntrySelector` 是 M5 域——选择技能参数修正器作用到哪些已学条目；不是目标选择器。两个词汇不混用。

### 2.2 过滤器策略（存活/敌对语义 = 宿主本体论）
- `FTcsTargetFilterStrategy`（USTRUCT 反射基类，**纯虚 `Pass(Candidate, Context)`**——候选通过判定；载体同 D3-7 v3）。
- **框架零默认 Filter**：怎么算存活、怎么算敌人是宿主语义（原枚举 TTF_Alive/TTF_Hostile 是框架假装认识宿主语义再转手委托——v2 改为直接暴露契约）；R3 竖切由测试装置/宿主实现（内部可调 ICombatEntityQuery.IsAlive / IRelationResolver.IsHostile）。
- 组合语义：SelectTargets 步骤持 Filter 策略实例数组，**AND 全过**。

### 2.3 注入接口（宿主实现，M6 适配）
- `ICombatEntityQuery`（UINTerface，TcsEffect 定义——机制层对宿主能力的契约）：实体遍历（稳定序全量句柄）/ GetLocation / IsAlive。实现适配：军官组件 / Mass 桶（M6）——内部可转发中央注册表（M3 拥有；命名统一待办，R3 执行期定）。
- `IRelationResolver`：阵营关系判定——IsHostile / IsFriendly(来源, 目标)。
- 传递方式：经 FEffectContext 携带的注入引用访问（链上下文注入——运行时协作不产生编译边）。

## 3. 入口服务与关键机制

- **SelectTargets 步骤 + 执行器**：`UE_DEFINE_EFFECT_STEP_EXECUTOR` 静态自注册（D4-14）；执行 = 策略 Resolve → Filter 数组 AND 过滤 → 结果写 `FEffectContext.Targets`（供后续战斗步骤消费）。
- **目标集 = 链上显式数据流**：SelectTargets 产出 Context.Targets → 下游步骤消费；再次 SelectTargets 显式改写（顺序链天然支持"多效果各有目标"——每段前插 SelectTargets）。
- **Context 默认目标初始化（D4-4 v2 改口配套）**：Context 创建时 Targets 默认 = 事件载荷目标——灼烧 tick 这类单步链 `[Damage]` 零 SelectTargets 步骤直接消费；需要不同目标集时才显式插 SelectTargets。
- **确定性纪律（D0-1）**：遍历序 = 注册表稳定序（Index 升序），过滤保序——同输入同输出，客户端重算一致。
- **挂起语义**：SelectTargets 是即时步骤（不挂起）；策略/过滤产出的句柄经代际校验，悬空跳过。

## 4. 网络姿态落点（NET-1/2）

- 仅权威侧执行（04 NET：链执行权威侧全量，客户端镜像只跑 Cue 类步骤）；选择结果**不复制**——客户端表现由复制的操作流与事件驱动，归宿主。

## 5. 非目标

不做指示器/瞄准渲染（宿主/LAC，D4-15）；不做锁定/软锁辅助（玩家输入域归宿主）；不做空间加速结构（未来增量）；不做目标评分/权重排序（未见需求）；不认识具体效果与伤害（纯选择，域不透明）；不做投射物/区域实体（D4-16 移除）；**不做枚举式内置模式集**（v2 策略化——目标语义全宿主化，框架只立契约与少量默认实现）。

## 6. 依据

- 拍板：D4-4（数据化+EntrySelector 纠正）、D4-14（注册制分派）、D4-15（模块成立）、D4-16（Spawn 移除）；**策略化三拍板（2026-09-02 审阅轮 5 后讨论）**——①Selector/Filter 策略模式取代枚举（用户："枚举太具体、不像模组该有的样子"；Filter"需要接口让宿主实现，不然没法确认怎么算存活怎么算敌人"）；②解法 B——TcsDamage 不内嵌 selector、读 Context.Targets，Context 默认目标=事件目标（编译集 {Core,Attribute,Effect} 保持，TcsDamage→TcsTargeting 边消失）；③RadiusArea/FTargetingShape 后置（竖切无消费者）。
- 证据：04 §2.3 / §9 例二（预演形态）/ §10 伪代码 D；TCS 09 核验（净新增）；R0 §9 模块表；竖切剧本 v2（单体选择，RadiusArea 无消费者实证）。
- 载体修订（D3-7 v3，2026-09-10 用户拍板）：策略载体 EditInlineNew Instanced UObject → FInstancedStruct/TInstancedStruct——依据 = UE5.8 StructUtils 已迁入 CoreUObject（编辑器开箱 picker/内嵌编辑/TArray details）+ StateTree"USTRUCT 策略基类虚函数 + FInstancedStruct 存储"shipped 先例 + 与 D4-14"数据+分派"心智收敛 + 网络开箱（原生 NetSerialize + Iris FInstancedStructNetSerializer）+ 旧 TCS"TSubclassOf 选类+FInstancedStruct 传配置"双形态不回归。

## 7. 修订记录

- v1（2026-09-02）：初版折入（R3 补文档轮）——枚举模式形态（3+2 收窄）。
- v2（2026-09-02，审阅轮 5 后讨论）：**策略模式重写**——用户三拍板（策略化/解法 B/RadiusArea 后置）；新增 Context 默认目标初始化；D4-4"战斗步骤内嵌"改口。
- v3（2026-09-10，D3-7 v3）：**策略载体切换**——EditInlineNew Instanced UObject → USTRUCT 反射基类 + C++ 虚函数分派 + `TInstancedStruct<Base>` 持有（可行性调研后用户拍板；类型名 UTcs→FTcs）；BP 策略扩展通道明确放弃（R0"蓝图不承诺"承责）；编辑器校验钩子由 Def 资产 IsDataValid 承担。

## 8. 验收钩子

计划二竖切：测试链 `WaitDelay → SelectTargets(单体策略) → Damage`——策略 Resolve → Context.Targets → Damage 消费；人工检查：策略内嵌编辑可用、过滤 AND 生效、单步链（无 SelectTargets）经 Context 默认目标直接命中、同帧重复选择结果一致（确定性）。
