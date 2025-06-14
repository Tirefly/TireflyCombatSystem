# 属性模块

## 属性定义（Attribute Definition）

+ 属性的静态配置器，继承自`UPrimaryDataAsset`
+ 定义属性的名称、描述、范围、求值器、UI配置

### 属性范围（Attribute Range）

+ 限制属性的值范围的设置
+ 支持静态范围
+ 支持动态引用属性作为属性值的边界

## 属性实例（Attribute Instance）

+ 属性的运行时实例，是一个结构体
+ 包含属性的基础值、当前值、属性修改器列表

## 属性求值器（Attribute Evaluator）

+ 定义属性当前值的计算公式
+ 定义属性可使用的修改器类型

### 预设通用求值器（Generic Attribute Evaluator）

+ 预设的通用属性求值器
+ 支持多种属性修改器类型
+ 属性当前值计算公式：

$$
\text{CurrentValue} = 
\begin{cases}
    M_{\text{override}} & \text{if } \exists\, \text{valid Override modifier} \\[10pt]
    \dfrac{
        (\text{BaseValue} + \sum M_{\text{add}}) 
        \times \prod (1 + M_{\text{multAdd}}) 
        \times \prod M_{\text{multComp}}
    }{
        \max(\sum M_{\text{divide}}, 1)
    } & \text{otherwise}
\end{cases}
$$

## 属性修改器（Attribute Modifier）

+ 对属性进行修改的实例，是一个结构体
+ 包含修改器的类型、修改值、该修改器应用的时间戳



# 状态模块

## 状态定义（State Definition）

+ 定义状态的唯一标识、UI显示配置（名称、描述等）
+ 定义状态的类型标签（TagContainer）
+ 定义状态的功能标签（给角色添加的状态标签，如禁止移动、禁止施法、正在格挡）
+ 定义状态与其他状态的依赖关系、互斥关系

## 状态实例（State Instance）

+ 状态运行时实例，是一个UObject
+ 包含一个状态树实例的引用，运行时创建状态树实例处理状态逻辑
+ 会向状态树实例发送消息事件、能够修改状态树实例的参数
+ 包含状态运行时的数据
+ 能够监听事件
+ 能够运行各种Task状态

## 状态树（State Tree）

+ 状态机和行为树的结合增强版
+ 处理状态持续期间的各种逻辑（事件回调、属性修改等）
+ 作为状态的脚本，也能作为状态派生类型（技能、Buff效果、动作等）的脚本

## 状态树组件（State Tree Component）

+ 管理角色所有状态的转换逻辑
+ 状态转换的验证器
+ 使用状态树作为可视化的状态管理器
+ 包含状态的增删改查操作



# Buff效果模块

## Buff效果定义（Buff Definition）

+ 继承自`State Definition`
+ 定义Buff效果的持续时间、跳动执行
+ 定义Buff效果的等级设置
+ 定义Buff效果的属性修改器列表
+ 定义Buff效果的视觉效果配置
+ 定义Buff效果的最大叠层数、叠层方式（按照来源）、叠层后对属性修改器值、持续时间的影响
+ Buff效果只有一个职责：修改游戏角色的属性

## Buff效果实例（Buff Instance）

+ 继承自`State Instance`
+ 包含一个继承自父类的状态树实例的引用，运行时创建状态树实例处理Buff效果的逻辑
+ 会向状态树实例发送消息事件、能够修改状态树实例的参数

## Buff效果上下文（Buff Context）

+ 是一个结构体，用于描述Buff效果的运行时数据



# 技能模块

## 技能定义（Skill Definition）

+ 继承自`State Definition`
+ 定义技能的冷却时间
+ 定义技能的技能消耗（消耗资源类型，消耗值，或动态消耗方式等）
+ 定义技能的技能等级设置

## 技能实例（Skill Instance）

+ 继承自`State Instance`
+ 包含一个继承自父类的状态树实例的引用，运行时创建状态树实例处理Buff效果的逻辑
+ 会向状态树实例发送消息事件、能够修改状态树实例的参数

## 技能上下文（Skill Context）

+ 是一个结构体，用于描述技能的运行时数据



# 动作模块

+ 先暂定可能直接使用状态树的Task来作为动作逻辑的执行脚本
