# Why

LAC 的 C# selector 探针依赖 UnrealSharp 尚未上游化的 `out` 容器参数 glue 修复，致使两仓自身的提交不能独立复现。现有 TCS 声明的 `TArray&` 被 UHT 识别为纯输出参数，而选择器契约实际是填充一个传入容器。

# What Changes

- TCS `ResolveTargets` 为目标数组添加 `UPARAM(ref)`，维持 C++ 函数签名和转发行为，让反射声明表达输入输出语义。
- LAC C# 探针改为 `ref IList<FTcsCombatEntityHandle>`，向传入列表按顺序追加目标。
- 编译并检查 UHT/UnrealSharp 生成的 `ref` 形态；PIE 行为与 GC 回归由用户在编辑器内验证。

# Impact

- 涉及 TCS 的公开反射接口和现有 Blueprint/C# 实现签名；生成绑定和 Blueprint 节点的参数形态会变化。
- 不修改 UnrealSharp 仓库，不覆写旧 E2E 日志；旧证据仅证明修改前的形态。
- 如 `ref` 容器编译或回写失败，保留失败证据并退回设计讨论，不宣称替代方案已成立。
