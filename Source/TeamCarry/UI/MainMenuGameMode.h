// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MainMenuGameMode.generated.h"

/**
 * AMainMenuGameMode - 메인 메뉴 레벨용 게임 모드.
 *
 * 폰을 스폰하지 않으며(DefaultPawnClass = nullptr), UI 생성 권한을 가진
 * 범용 컨트롤러 AGameUIPlayerController 를 기본 컨트롤러로 사용한다.
 */
UCLASS()
class TEAMCARRY_API AMainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMainMenuGameMode();
};
