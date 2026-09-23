## 1. 实施

- [x] 1.1 新 `Public/Trigger/TcsTriggerPayloadReader.h` + `Private/Trigger/TcsTriggerPayloadReader.cpp`——载荷读取器注册表 + 自注册宏对 + `FTcsTriggerPayloadInfo`
- [x] 1.2 新 `Public/Trigger/TcsTriggerEvaluator.h` + `Private/Trigger/TcsTriggerEvaluator.cpp`——`UTcsTriggerEvaluator : UTcsEventHandler`（总线适配）+ 四道门求值
- [x] 1.3 改 `Public/TcsEffectSubsystem.h`——登记表（值语义 `TArray` + 空闲链表 + 代际计数）、登记/摘除/点灯 API、订阅计数表、求值器 `UPROPERTY` 持有
- [x] 1.4 改 `Private/TcsEffectSubsystem.cpp`——上述实现 + `Deinitialize` 全量退订 + 求值器声明 `friend`
- [x] 1.5 编译验证：UBT Development Editor **零警告零错误**

> **实施中的两处追加（超出原任务书，均为实施期复核发现）**：
> - **登记表拆为独立纯逻辑类** `FTcsTriggerRegistry`（`Public|Private/Trigger/TcsTriggerRegistry.h/.cpp` + `_Query.cpp`）——理由：`TcsEffectSubsystem.cpp` 实施前已 289 行，本批再塞登记表会越过"单 `.cpp` ≤300 行"的仓规。形态照 `UTcsEventBusSubsystem` 持 `FTcsEventBus` 的既有分工（内核住纯 C++ 类，UObject 门面只做反射面与生命周期）。
> - **门面 `AddReferencedObjects` 补触发登记表**（见 2.6——plan3 的判据是错的，实施时纠正）。

## 2. 验证（禁 TDD 纪律：编译 + 定向人工检查）

- [x] 2.1 UBT Development Editor 编译零警告零错误（首次因 `PassesConditions` 的 const 限定失配失败一次——取随机值会推进流，故该函数 MUST 非 const；改后通过）
- [x] 2.2 **补跑 Shipping**——`Result: Succeeded`（零警告零错误）
- [x] 2.3 依赖面自检：`grep "^#include"` 零领域模块（TcsEffect 只 include 自身 + TcsCore/TcsAttribute + 引擎）
- [x] 2.4 **订阅计数配对自检**（静态读码）：`EnsureSubscription` 首行 `TagSubscriptions.Contains` 复用；`DropSubscriptionIfUnused` 首行 `HasRowForTag` 提前返回；`Reset` 遍历全量退订
- [x] 2.5 代际校验自检（静态读码）：`UnregisterRow` 先 `IsValidIndex` 再比 `RowGenerations[...] != Handle.Inner.Generation` → 拒
- [x] 2.6 **GC 补引用（实施中追加，非原任务书项）**：复核发现 `plan3` 注记"值语义 `TArray` → 无需 ARO 覆写"的**判据是错的**——是否需要 ARO 与值/指针语义无关，只取决于容器是否 GC 可见；`FTcsTriggerRegistry` 是门面的非 `UPROPERTY` 成员，而行内 `FInstancedStruct` 内层可放对象引用（T-8 同类缺口）。已补 `FTcsTriggerRegistry::AddReferencedObjects` + 门面转发，并回写规格场景

## 3. 规格

- [x] 3.1 `openspec validate add-effect-trigger-registry --strict` 通过
- [x] 3.2 提案归档（`effect-trigger`：MODIFIED × 1 + ADDED × 3）
- [x] 3.3 规格库自检 `openspec validate --specs --strict` 全绿（**23 passed, 0 failed**）

## 4. 未覆盖（如实记）

- **无运行期实证**：本批交付**有调用方的机制**（登记表 + 求值器），但**验收路径在 Task 4**（端到端"破甲"修改器）——本批自身仍是静态检查（编译 + 读码），因为触发行要真跑起来需要 Task 3 的 `ModifyFlow` 原语（否则触发了也没有能改黑板的步骤）。**故 2.4/2.5/2.6 是读码结论，不是实测结论**。
- **载荷读取器无属主登记**：注册表落地但 TcsDamage 的收集事件读取器随 Task 3 登记——故本批 `ClassificationTags` 在真实流程事件上**仍是空集**（`HasAllTags` 条件此时恒不过，这是规格明文交付的语义）。
- **反射注册入口未做**：`Register` 仍是纯 C++ 面（`TFunction` 不可反射）——共享欠账，同批解决（提案"非目标"）。
- **编辑器 picker 检查未做**：需开编辑器（MCP 操作边界纪律：先与用户确认）。
