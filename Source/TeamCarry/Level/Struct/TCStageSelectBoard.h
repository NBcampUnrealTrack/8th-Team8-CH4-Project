// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/Interface/TCInteractable.h"
#include "TCStageSelectBoard.generated.h"

class UStaticMeshComponent;
class UWidgetComponent;
class ATCPlayerCharacter;
class UW_StageBoardScreen;

/**
 * ATCStageSelectBoard - 로비 게시판 액터(명세 4장-5, 6장-9).
 *
 * 기존 ITCInteractable/UGrabComponent 상호작용 파이프라인(가구·문과 동일 경로, ATCMapInteractable과
 * 동일 패턴)을 그대로 재사용한다. GrabComponent::ScanBestTarget()의 박스 트레이스가 자동으로 이
 * 액터를 대상 후보로 인식하므로 별도 트리거 볼륨이 필요 없다.
 *
 * 방장 판정은 두 지점에서 각각 다른 방식으로 이뤄진다:
 *  - CanInteract_Implementation (서버 권위, ServerTryInteract_Implementation 경유): 요청한 폰이
 *    서버 시점에서 "로컬 컨트롤"인지로 판별한다(리슨 서버 = 호스트 자신의 폰만 로컬 컨트롤).
 *  - OnFocus/OnUnfocus (각 클라이언트 로컬 호출): UTCSessionFlow::IsHost() 로 "이 클라이언트가
 *    호스트 프로세스인지"를 판별해, 방장 클라이언트에서만 상호작용 프롬프트를 노출한다.
 */
UCLASS()
class TEAMCARRY_API ATCStageSelectBoard : public AActor, public ITCInteractable
{
	GENERATED_BODY()

public:
	ATCStageSelectBoard();

	// ITCInteractable
	virtual bool CanInteract_Implementation(ATCPlayerCharacter* Player) override;
	virtual void OnFocus_Implementation() override;
	virtual void OnUnfocus_Implementation() override;
	virtual void OnInteract_Implementation(ATCPlayerCharacter* Player) override;

	// S_Lobby 의 Btn_StageSelect 가 방장 캐릭터의 텔레포트 목적지로 사용한다(명세 4장-3).
	UFUNCTION(BlueprintPure, Category = "StageSelectBoard")
	FVector GetTeleportLocation() const;

	UFUNCTION(BlueprintPure, Category = "StageSelectBoard")
	FRotator GetTeleportRotation() const;

	// ATCPlayerController::Input_BoardListUp/Down이 게시판 클릭 모드 중 키보드 탐색을 전달할 대상을
	// 찾을 때 사용한다(ClientEnterBoardInteractionMode(this)로 저장해 둔 보드에서 역참조).
	UFUNCTION(BlueprintPure, Category = "StageSelectBoard")
	UW_StageBoardScreen* GetBoardScreenWidget() const;

protected:
	UPROPERTY(VisibleAnywhere, Category = "StageSelectBoard")
	TObjectPtr<UStaticMeshComponent> BoardMesh;

	// 방장이 보드를 마주보고 서는 위치/방향(텔레포트 목적지).
	UPROPERTY(VisibleAnywhere, Category = "StageSelectBoard")
	TObjectPtr<USceneComponent> TeleportAnchor;

	// 게시판 월드 스크린(W_StageBoardScreen, 명세 3장·4장-5). 실제 위젯 클래스는 블루프린트 자식에서 지정.
	UPROPERTY(VisibleAnywhere, Category = "StageSelectBoard")
	TObjectPtr<UWidgetComponent> BoardScreen;
};
