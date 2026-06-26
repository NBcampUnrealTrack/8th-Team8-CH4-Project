// Fill out your copyright notice in the Description page of Project Settings.

#include "TeamCarry/UI/MainMenuGameMode.h"
#include "TeamCarry/UI/GameUIPlayerController.h"

AMainMenuGameMode::AMainMenuGameMode()
{
	// 메뉴 화면: 조작할 폰을 스폰하지 않는다.
	DefaultPawnClass = nullptr;

	// UI 생성 권한을 가진 범용 컨트롤러를 기본값으로 지정.
	PlayerControllerClass = AGameUIPlayerController::StaticClass();
}
