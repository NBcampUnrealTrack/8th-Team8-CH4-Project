// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TCFeedbackOverride.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UTCFeedbackOverride : public UInterface
{
	GENERATED_BODY()
};

/**
 * 피드백 자동 재생에 액터가 개입할 수 있는 '선택적' 인터페이스.
 * 구현하지 않으면 기본 동작(피드백 서브시스템의 자동 부착 + 기본 에셋 재생)이 적용되므로
 * 기존 클래스는 아무 수정 없이 그대로 동작한다. 커스텀이 필요한 액터(C++/BP)만 구현하면 된다.
 */
class TEAMCARRY_API ITCFeedbackOverride
{
	GENERATED_BODY()

public:
	// false 반환 시 이 액터에는 피드백 컴포넌트를 자동 부착/재생하지 않는다
	UFUNCTION(BlueprintNativeEvent, Category = "Feedback")
	bool ShouldAutoFeedback();
	virtual bool ShouldAutoFeedback_Implementation() { return true; }

	// 이펙트 스폰 위치 커스텀 (기본: 액터 위치)
	UFUNCTION(BlueprintNativeEvent, Category = "Feedback")
	FVector GetFeedbackLocation();
	virtual FVector GetFeedbackLocation_Implementation();
};
