## ADDED Requirements

### Requirement: 状态 Def 值约定白名单校验

状态 Def 资产的 `IsDataValid`（`WITH_EDITOR`）MUST 追加一条**错误**：`Params` 的某行配了**非 `VCF_None`** 的 `ValueConvention`，而该行数值来源 `Base.Source` 的 `AllowsValueConvention()` 为假（引用类源与域侧换算源的书写值不是结果，约定作用其上无定义）——每条错误 MUST 挂在出问题的配置元素上。

**判据 MUST 走虚分派**（读该行数值来源的能力位），MUST NOT 建立"源类型 × 可配约定"的中心名单或 `switch`——宿主自定义源自行声明能力，校验面零公共代码改动。

**运行期不拦**：快照构建对不允许约定的行只按"不转换"处理（该行的书写值即规范值），不 ensure、不拒绝施加——静默错配在**作者期**被报出。

#### Scenario: 引用类源配了约定列被报错

- **WHEN** 某行数值来源为引用类源（能力位为假）而行上配 `ValueConvention = VCF_Percent`
- **THEN** `IsDataValid` 报出一条错误并挂在该行上

#### Scenario: 允许约定的源配约定列合法

- **WHEN** 某行数值来源为字面量源而行上配 `ValueConvention = VCF_Percent`
- **THEN** 不报错（能力位为真，约定在快照写入点生效）

#### Scenario: 未覆写能力位的宿主源自带缺省真值

- **WHEN** 某行使用一个未覆写 `AllowsValueConvention` 的源（默认返回真）并配了约定列
- **THEN** 不报错——能力位缺省即可配，宿主源无需任何公共代码改动
