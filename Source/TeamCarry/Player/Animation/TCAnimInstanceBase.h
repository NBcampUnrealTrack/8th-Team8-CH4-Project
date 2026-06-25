// TCAnimInstanceBase.h

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "TCAnimInstanceBase.generated.h"


UCLASS()
class TEAMCARRY_API UTCAnimInstanceBase : public UAnimInstance
{
	GENERATED_BODY()
	
public:
	// 매 프레임 애니메이션 상태 갱신
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	// 캐릭터 이동 속도
	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	float Speed;

	// 캐릭터가 공중에 떠 있는지 여부
	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	bool bIsFalling;

};
