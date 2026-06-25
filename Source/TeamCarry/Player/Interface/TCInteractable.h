// TCInteractable.cpp

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TCInteractable.generated.h"


UINTERFACE(MinimalAPI)
class UTCInteractable : public UInterface
{
	GENERATED_BODY()
};


class TEAMCARRY_API ITCInteractable
{
	GENERATED_BODY()

public:
	// 상호작용이 가능한 상태인지 확인
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool CanInteract(class ATCPlayerCharacter* Player);

	// 대상을 바라볼 때 외곽선 하이라이트 활성화
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void OnFocus();

	// 대상을 바라볼 때 외곽선 하이라이트 비활성화
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void OnUnfocus();

	// 실제 상호작용(잡기/던지기)을 실행하는 함수
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void OnInteract(class ATCPlayerCharacter* Player);
};
