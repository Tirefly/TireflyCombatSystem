## MODIFIED Requirements

### Requirement: 流程模板与登记表

`TcsDamage` MUST 以**数据模板**承载瞬时流程（D7-5 流程管线宿主化：阶段构成本身是项目知识，插件不预设）：

- `FTcsFlowTemplate{ FGameplayTag TemplateId; TArray<FInstancedStruct> Steps; }`（USTRUCT；**2026-09-22 改造：`TemplateId` 类型 `FName` → `FGameplayTag`**）——有序步骤数组，元素形状同 `FEffectStep`（步骤无公共基类，类型合法性由执行器注册表在执行期判定）；
- `UTcsDamageSubsystem`（世界级子系统门面）MUST 提供登记表：`RegisterTemplate` / `UnregisterTemplate` / `FindTemplate`，**键 = `TemplateId`**；拒绝面（ensure + false）：`TemplateId` **无效**（`!TemplateId.IsValid()`）、同 id 重复登记（不得静默覆写）；`FindTemplate` 未登记返回 nullptr（**不 ensure**——正常查询路径）；
- `RunTemplate` 遇未登记模板 MUST 拒绝：Error 日志 + 不执行（执行期配置错误，不 ensure）；
- 登记表 MUST 以**地址稳定**方式持有模板（`TMap<FGameplayTag, TUniquePtr<…>>` 或同效手段；**2026-09-22 改造：键类型改 tag**）——解释器在一次执行期间持模板引用；
- 登记表 MUST 同时做到**对 GC 可见**（2026-09-23 补，修台账 T-8）：模板步骤可携带 `UPROPERTY` 对象引用（如 `FTcsFlowDelegate::Delegate` 这类 `TScriptInterface<ITcsDamageFlowDelegate>`），而**裸 C++ 容器不经 GC 的 `RefLink`** —— 子系统 MUST 覆写 `AddReferencedObjects` 并逐模板调 `FReferenceCollector::AddPropertyReferencesWithStructARO`，否则那些引用会被静默回收（步骤取到空引用，表现为"公式不生效"而非崩溃）。

**地址稳定与 GC 可见是两件正交的事**：前者防 `TMap` 扩容搬移导致解释器持有的 C++ 引用悬空；后者防对象引用被 GC 回收。二者 MUST 同时满足。

**模板 id 的来源（2026-09-22）**：流程模板是**项目知识**（D7-5"阶段构成本身是项目知识"），故 `TemplateId` 属**项目词汇**——由项目 `Config/DefaultGameplayTags.ini` 声明（`Tcs.Flow.Template.<Id>`）。**例外**：插件自登记的官方默认模板 `Default` 属**框架词汇**，由插件原生声明（`Tag_Tcs_Flow_Template_Default`）。

#### Scenario: 登记后可按 id 查到

- **WHEN** `RegisterTemplate` 一条 `TemplateId = Tcs.Flow.Template.Flow_Test` 的模板后 `FindTemplate`（同一 tag）
- **THEN** 返回该模板，步骤数组与登记内容一致

#### Scenario: 重复登记被拒

- **WHEN** 对同一 `TemplateId` 再次 `RegisterTemplate`
- **THEN** 返回 false、原模板不被覆写、留 ensure 提示与日志

#### Scenario: 未登记模板被拒绝执行

- **WHEN** `RunTemplate` 一个未登记的 `TemplateId`（有效 tag 但未登记）
- **THEN** 不执行任何步骤 + Error 日志（不崩溃、不 ensure）

#### Scenario: 模板步骤里的对象引用对 GC 可见

- **WHEN** 登记一条含对象引用的模板（步骤带 `TScriptInterface<ITcsDamageFlowDelegate>` 委托字段），且该对象**无其他强引用**，随后触发 GC
- **THEN** GC 后该对象仍存活（未被回收）；模板步骤取到的委托仍有效
