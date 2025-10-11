# 一、角色框架：数据与行为分离的优点

***

[角色框架：数据与行为分离的优点](https://mp.weixin.qq.com/s/8FhAmgUjQ8egT_qggSoflA)

## 1.1 更准确的职责划分
- 传统OOP适合函数仅依赖对象自身数据的情况
- 游戏开发中，经常会出现一个行为关联多个模块数据（场景外的Player养成，场景内的World模拟），或关联多个实体的情况

```csharp
public class BoundingBoxUtils {
    public static bool Intersect(Cube lhs, BoundingBox rhs) {}
    public static bool Intersect(Cube lhs, Cube rhs) {}
    public static bool Intersect(Cube lhs, Sphere rhs) {}
}
```

## 1.2 利于数据驱动
- 传统OOP模块间耦合严重，难以拆分数据
- 新设计方案因数据集中，对于重度依赖配置的系统（如技能系统）更友好

```csharp
public class SkillTaskCfg {
    // 其它参数 ...
    private List<SkillVar> skillVars;
    private Task<Blackboard> task;
}
```

## 1.3 利于网络同步和数据回滚
- 数据集中会是数据类的层次和量都减少——同时标签类和策略模式会增加
	- 标签类能够简化类的层次结构，有利于构建组合架构，替代继承架构
- 数据集中化便于快照和状态恢复

```csharp
public class MoveComponent {
    private int type; // 标签：update方式
    private Vector3 velocity; // 矢量速度
    private Vector3 heading; // 航向
    private int curIndex; // 当前路径点
    private List<Vector3> waypoints; // 路径点
    // ...
}
```

## 1.4 利于面向数据（CPU）编程
- 释义：面向CPU缓存数据进行编程，它关注的是数据在内存的布局和CPU缓存行。
- 缺点：导致可读性和可维护性问题 —— 反常态编码。
- 游戏世界模拟依赖定时更新所有GameObject，大量迭代会因数据内存布局不连续而效率低下。
- 数据与行为分离可实现数据集中连续存储，提升迭代效率，但完全面向CPU编程（如ECS架构）需付出较大努力。

```csharp
public class MoveSystem {
	
	public void Update() {
    	for (int idx = 0, len = moveComponents.Length; idx < len; idx++) {    
        	moveMgr.Update(ref moveComponents[idx]);
    	}
    	for (int idx = 0, len = stateComponents.Length; idx < len; idx++) {
        	stateMgr.Update(ref stateComponents[idx]);
    	}
    	// ...
	}
}
```

## 1.5 利于热更新
- 将数据与行为分离；行为通过接口定义，热更新时替换行为的实现类即可。
- 不建议将有利于热更新作为重点关注项。

```csharp
// 队伍数据，单例 -- 也可以是静态类
public class TeamMgrData {
    private Dictionary<long, TeamMember> memberMap = new ();
    private Dictionary<long, Team> teamMap = new ();
}
// 队伍逻辑，无状态 -- 行为定义在接口中
public class TeamMgr : ITeamMgr {
    public int CreateTeam(CreateTeamRequest request) {}
}
```



# 二、角色框架：聊聊组件模式

***

[角色框架：聊聊组件模式](https://mp.weixin.qq.com/s/RcWmDupBBWf8nWNDxcePWg)

## 2.1 组件模式的定义
- 组件模式允许动态为对象添加组件来扩展对象的功能，而无需调整对象的类结构。

## 2.2 组件和模块的区别
- 组件模式侧重系统的可扩展性，组件通常有更好的独立性。
- 模块化侧重系统的功能拆分，模块侧重业务封装。
- 组件是可重用的模块，而模块不一定是组件——模块与整体的依赖还较高。

## 2.3 组件的管理方式

```csharp
// 放入List容器
public class GameObject {
    private List<Component> components; // 保持为添加顺序，可以有额外的缓存
    // 增删查接口
    public T GetComponent<T>() where T : Component {}
    public void AddComponent<T>(T comp) where T : Component {}
    public void RemoveComponent<T>(T comp) where T : Component {}
    // ...
}

// 平铺
public class GameObject {
    private MoveComponent moveComp;
    private StateComponent stateComp;
    private AIComponent aiComp;
    // 更多组件...
}
```

- List管理的优点：
  - 提供了统一的增删和查询接口
  - 更好的扩展性，可以支持编译时尚不存在的组件类型 -- 通用框架
  - 记录了添加顺序，可按照添加顺序迭代
  - 可以减少内存浪费
- List管理的缺点：
  - 组件的查询效率可能较低

## 2.4 GameObject的构造

- 传统方法：工厂模式
  - 缺点：硬编码、不灵活
  - 
```csharp
public class GameObjectFactory {
    // 由玩家数据创建
    public GameObject CreatePlayer(PlayerData data) {}
    // 由编辑器导出的配置创建Npc
    public GameObject CreateNpc(NpcJsonCfg jsonCfg) {}
    // 创建载具 
    public GameObject CreateVehicle(int cid) {}
    // 创建子弹
    public GameObject CreateBullet(int cid) {}    
    // ...
}
```

- 进阶方法：通过编辑器导出文件反序列化
  - 缺点：反序列化与硬编码的性能差距较大
  - 缺点：编辑器导出的配置文件变得更大
  - 缺点：要保证编辑器导出的数据符合规范，成本较大
- 建议方法：深拷贝原型对象代替反序列化
  - 释义：反序列化得到的GameObject作为原型，后续的GameObject通过深拷贝原型来创建
  - 深拷贝则可以避免反序列化影响效率的开销（存在复杂的上下文维护、输入解析逻辑）
  - 通过代码生成器生成所有用到的数据结构的深拷贝代码

```C++
public static GameObject DeepCopy(GameObject prototype) {}
``` 

## 组件的生命周期
- 正常情况下，组件的生命周期与GameObject相同
- 应尽可能避免在GameObject生命周期内动态增删组件
- 建议选择禁用组件，并释放对应的资源，但保留组件对象

## 组件之间通信
- 组件之间的通信与具体的业务相关，在框架层是用不到的
- 推荐使用事件系统，也可以直接缓存引用
- 可以通过接口减少对具体类型的依赖

```csharp
public class EventSystem : Component {} // 事件系统

public class SenserSystem : Componnet { // AI感知系统
    private MemoryComponent memoryComp; // AI记忆，启动前注入
}
```

## 2.5 组件的类型问题
- 推荐使用标记tag作为组件的类型判定

```csharp
public readonly struct ComponentType {
    public readonly Kind kind; // 类型，数据组件 or 行为组件    
    public readonly int type; // 类型，用户自定义类型
    public readonly int flags; // 用户标记
    public readonly long enableFuncs; // 组件支持的功能
    public readonly bool disallowMultiple; // 禁止重复挂载
    // 其它信息...
}
```

## 2.6 更通用的数据与行为分离
- 实体（Entity）由组件集成；实体可以是贫血模型，也可以是充血模型，由包含的组件决定
- 组件分为数据组件和行为组件
- 数据组件保持为简单数据对象，不包含业务逻辑；行为组件负责业务逻辑，不包含需要共享和同步的数据
- 数据组件的子类仍为数据组件，行为组件的子类仍为行为组件

```csharp
public class Entity {
  private List<IComponent> components;
}

public interface IComponent {} // 组件root接口
public interface IDataComponent : IComponent {} // 数据组件接口，非必需
public interface IBahaviorComponent : IComponent {} // 行为组件接口，非必需
```

## 2.7 MainComponent
- 充当与外界交互的门面
- 管理GameObject生命周期
- 处理组件依赖/冲突
- 定义组件的Update顺序



# 三、角色框架：场景内外玩法数据分离——变身玩法实现

***

[角色框架：场景内外玩法数据分离——变身玩法实现](https://mp.weixin.qq.com/s/95vcDZRQ4ZnkK3eOqEQ-uw)

## 3.1 变身玩法功能解析
- 场景内的玩法自由度很高，GameObject上的数据都是临时的
- 场景外的Player养成玩法，其关联的数据是需要持久化的

## 3.2 实现思路：数据分层
- 系统功能数据（Func）：如果是玩家，则由玩家的养成系统计算得出；如果是NPC等，则由配置表计算得出。
- 场景当前数据（Current）：非特殊玩法下，由系统功能数据计算得出；特殊玩法下，由场景内玩法数据计算得出。
- 系统功能数据被修改，是不会影响到场景内的玩法的，只有被应用为当前数据时才会产生运行时的影响。
- 由场景玩法决定系统功能缓存数据应用为当前数据的时机：既可能实时应用，也可能延迟应用，也可能忽略。
  - 延迟到当前帧尾或下一帧帧首
  - 延迟到副本特定阶段：比如当前战斗结束，进入等待区
  - 延迟到角色进入Idle状态：通常用于保证角色变身的正确性
- 需要时间系统定义事件接口，在玩家数据发生变化时，通知场景世界

```csharp
// 玩家技能信息变化，尝试刷新场景中的技能数据
public void onPlayerFuncSkillChanged(GameObject gobj, PlayerPO player) {}
```

## 3.3 数据收集器（Collector）
- 利用策略模式，实现一个数据收集组件，实现玩家在不同场景有不同的属性、模型、技能
- 数据收集器可以设计接口以处理不同的变身逻辑，甚至设计为更通用的接口以便复用在其他业务模块

```csharp
// 技能收集器
public interface SkillCollector {
    void collect(GameObject gobj, Int2ObjctMap<SkillData> result);
}
```

## 3.4 策略分层（玩法分层）
- 变身功能按玩法层级可以分为：
  - 由副本流程控制的变身：进入副本就变身也算；通常针对所有玩家，玩家还可以通过技能和Buff再次变身
  - 由Buff控制的变身：仅针对单个玩家，通常Buff之间互斥
  - Collector不仅在场景上存在，也需要在GameObject上存在。

```csharp
public class GameObjectSkillMgr {
    // 场景层收集器
    private SkillCollector collector = new DefaultCollector();
    // 刷新玩家当前技能
    public void refreshSkill(GameObject gobj) {
        // GameObject上的组件优先
        SkillCollector collector = gobj.getSkillComponent().getCollector();
        if (collector == null) {
            collector = this.collector;            
        }
        Int2ObjectMap<SkillData> result = new Int2ObjectMap<>();
        collector.collect(gobj, result);
        // 应用result到curSkillMap，需要尽可能保留既有SkillData对象
        // ...
    }
}
```



# 四、角色框架：GameObject类型实现

***

[角色框架：GameObject类型实现](https://mp.weixin.qq.com/s/qcgYJW0zkmaRdiPQgpAJYg)

## 4.1 继承&面向接口
- 用接口分类，描述对象支持的行为，再通过继承实现对象的行为
- 明显特征：程序的GameObject类型与策划的GameObject类型高度匹配
- 随着需求的复杂度增长，缺点越来越多：
  - 由于行为存在交集，需要定义大量的接口：包括原子接口（atomic）和混合(mixin)接口
  - GameObject的类型层次十分复杂，接口体系复杂，实现类体系也复杂
  - GameObject的子类中包含大量的方法（和字段）
  - 单继承下的代码复用性较差：不同类型存在重复实现接口的情况
  - 大量的类型测试：业务代码中充斥着大量的 instance_of / is / cast 等类型测试

```csharp
// GameObject定义
public interface IGameObject {}
public class GameObject : IGameObject {}

// Npc接口，约定Npc的公共行为
public interface INpc : IGameObject {}
// NPC抽象类，提供模板实现
public abstract class Npc : GameObject, INpc {}

// 宠物接口，约定动物的特定行为
public interface IPet {}
// 宠物实现类，实现宠物行为
public class Pet : Npc, IPet {}

// 细粒度接口：可移动的对象
public interface IMoveableGameObject : IGameObject {
    public void Move(Vector3 target, float speed);
}
```

## 4.2 模块化实现
- 将相关接口的实现封装为辅助对象，然后通过委托的方式复用代码。
- 所有的接口实现都应该封装为辅助对象，GameObject总是通过委托的方式提供功能。
  - 原因：为了保证灵活性和扩展性，新接口的实现都应该封装为辅助对象
  - 原因：为了统一编码风格，旧接口的实现也应该重构为辅助对象

```csharp
public abstract class Npc : GameObject, IMoveableGameObject {
    private GameObjectMoveHandler handler;
    // 行为委托给Handler
    public void Move(Vector3 target, float speed) => handler.Move(this, target, speed);
}
```

## 4.3 隐式组合 => 显式组合
- 提取并暴露模块，在GameObject中使用接口调用，以减少委托
- 为了让调用模块的方法就像是调用GameObject的方法一样，我们需要避免调用模块的方法时传入GameObject
  - 因此需要让模块持有GameObject的引用

```csharp
public class MoveModule {
    private GameObject gobj; // 实体引用
    //
    private int moveType;
    private Vector3 target;
    private float speed;
    //
    public void Move(Vector3 target, float speed) {}
}
```

## 4.4 组件模式
- 组件模式：动态为对象添加组件来扩展对象的功能，而无需调整对象的类结构。
- 为了让组件测试接口统一，以及减少内存开销，静态语言通常采用List和Dictionary来存储组件

```csharp
public class GameObject {
    private int id;
    private string name;
    private List<Component> components;
    // 组件相关接口
    public void AddComponent<T>(T comp);
    public T GetComponent<T>();
    public List<T> GetComponents<T>();
}
```

## 4.5 组件模式下的扩展：类型实现
- 使用类型数据区分GameObject的类型，比如枚举类型的主类型变量
- 完全的数据-行为分离的框架下，主类型涉及到对GameObject的快速筛选和额外的缓存管理，不建议使用Tag替换枚举实现主类型
- 使用标签Tag容器，可以定义对象的 **特征集** ，具有高度的灵活性
  - UE5的GameplayTag天然带有多级分层管理，更加灵活

```csharp
public class GameObject {
    public int cid;
    public GameObjectType type; // 主类型
	public List<int> tags; // 编辑器或配置表指定
}
```

## 4.6 类型组件
- 基于组件模式，将GameObject类型相关的数据封装为组件
  - 不同项目可以定义不同的TypeInfo，没有具体的类型依赖。
  - 类型组件可以独立继承扩展，虽然我们通常不这样做。
- GameObject上仍然保留cid属性，作为唯一标识，以便于Debug和开发

```csharp
public class GameObject {
    public long instId; // 实例id
    public int cid; // 配置id
}

public class GameObjectTypeInfo : Component {
    public GameObjectCfg cfg; // 配置对象，缓存字段
    public GameObjectType type; // 主类型，缓存字段
    public List<int> tags; // 类型标签
    public object extInfo; // 扩展数据
}
```