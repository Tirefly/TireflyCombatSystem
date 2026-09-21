// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "Handle/TcsCombatEntityHandle.h"
#include "Host/TcsEntityQuery.h"

#include "TcsPieEntityQuery.generated.h"



class UTcsCombatEntityComponent;
class AActor;



/**
 * 实体查询的 PIE 实现（**宿主侧唯一"句柄 ↔ Actor"映射点**）。
 *
 * **文件与类名注意**：TcsEffect 的契约头是 `Host/TcsEntityQuery.h`（`ITcsEntityQuery` + 其 U 类
 * `UTcsEntityQuery`）。本模块的实现**两者都必须改名**：类名若用 `UTcsEntityQuery` 会与契约的 UINTERFACE
 * U 类重名；**文件名若用 `TcsEntityQuery.h` 则 UHT 直接报错**（2026-09-21 编译实证：
 * `Two headers with the same name is not allowed`——UHT 要求全项目头文件名唯一，与模块无关）。
 * 故本文件 = `TcsPieEntityQuery.h`、类 = `UTcsPieEntityQuery`。
 *
 * 职责（`ITcsEntityQuery` 三支能力，全部以**句柄**为参数——机制层不认识 Actor）：
 * - `EnumerateEntities`：遍历带 `UTcsCombatEntityComponent` 的 Actor 并吐句柄（**稳定序 = 注册序**）；
 * - `GetLocation` / `IsAlive`：经本类持有的映射解析（组件注册时登记、注销时移除）。
 *
 * **映射归属纪律**：句柄由属性门面（`UTcsAttributeSubsystem::RegisterUnit`）发号，本类只记
 * "句柄 → Actor"的对应关系。机制层（TcsEffect/TcsTargeting/TcsDamage）MUST NOT 依赖本映射；
 * 内容资产 MUST NOT 存句柄（运行期发号，跨会话/跨机不同——要指定实体用"稳定标识 + 运行期宿主解析"）。
 *
 * 存活判定（宿主本体论）：本实现取"组件仍注册"——框架不认识"死亡"；真实项目应覆写/替换为
 * 项目自己的存活规则（如血量归零、Actor 已销毁）。
 */
UCLASS()
class TCSINTEGRATION_API UTcsPieEntityQuery : public UObject, public ITcsEntityQuery
{
	GENERATED_BODY()

// 映射维护（供组件调用）
#pragma region Mapping

public:
	/**
	 * 登记"句柄 → Actor"映射（组件注册成功后调用）。
	 * 重复登记同一句柄按后者覆盖（组件注销后重新注册会拿到新句柄，旧句柄由调用方负责移除）。
	 *
	 * @param Handle 实体句柄。
	 * @param Actor 持有该实体的 Actor。
	 */
	void RegisterEntity(FTcsCombatEntityHandle Handle, AActor* Actor);

	/**
	 * 移除映射（组件注销时调用）。句柄未知时静默返回（注销路径的重复调用是正常时序）。
	 *
	 * @param Handle 实体句柄。
	 */
	void UnregisterEntity(FTcsCombatEntityHandle Handle);

	/**
	 * 映射是否为空（宿主清理时序自检用）。
	 *
	 * @return 返回是否无任何登记实体。
	 */
	bool IsEmpty() const
	{
		return EntityToActor.Num() == 0;
	}

#pragma endregion


// ITcsEntityQuery
#pragma region Query

public:
	// 遍历全部已登记实体并吐句柄（稳定序 = 登记序——同输入同输出）
	virtual void EnumerateEntities(TFunctionRef<void(FTcsCombatEntityHandle)> Visitor) override;

	// 取实体定位（Actor 已失效时返回 false）
	virtual bool GetLocation(FTcsCombatEntityHandle Entity, FVector& OutLocation) override;

	// 存活判定（宿主本体论：本实现 = "组件仍注册且 Actor 有效"）
	virtual bool IsAlive(FTcsCombatEntityHandle Entity) override;

#pragma endregion


// 映射数据
#pragma region Data

private:
	// 句柄 → Actor 映射（TMap 迭代序不稳定，故另存登记序数组保证遍历稳定序）
	TMap<FTcsCombatEntityHandle, TWeakObjectPtr<AActor>> EntityToActor;

	// 登记序（EnumerateEntities 的稳定序来源；注销时移除，保持"登记序"语义）
	TArray<FTcsCombatEntityHandle> RegistrationOrder;

#pragma endregion
};
