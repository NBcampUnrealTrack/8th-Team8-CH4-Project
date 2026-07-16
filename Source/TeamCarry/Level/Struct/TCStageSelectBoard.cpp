// Fill out your copyright notice in the Description page of Project Settings.

#include "Level/Struct/TCStageSelectBoard.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Player/Character/TCPlayerCharacter.h"
#include "Player/PlayerController/TCPlayerController.h"
#include "Network/Session/TCSessionFlow.h"
#include "TeamCarry/UI/MockUIController.h"
#include "TeamCarry/UI/W_StageBoardScreen.h"
#include "GameFramework/Controller.h"
#include "Engine/GameInstance.h"

ATCStageSelectBoard::ATCStageSelectBoard()
{
	BoardMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoardMesh"));
	SetRootComponent(BoardMesh);
	BoardMesh->SetCollisionProfileName(TEXT("BlockAll"));

	TeleportAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("TeleportAnchor"));
	TeleportAnchor->SetupAttachment(BoardMesh);
	// 보드 앞쪽에 서서 보드를 마주보는 기본 오프셋. 실제 보드 메시 크기에 맞춰 에디터에서 조정한다.
	TeleportAnchor->SetRelativeLocation(FVector(150.f, 0.f, 0.f));
	TeleportAnchor->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));

	BoardScreen = CreateDefaultSubobject<UWidgetComponent>(TEXT("BoardScreen"));
	BoardScreen->SetupAttachment(BoardMesh);
	BoardScreen->SetWidgetSpace(EWidgetSpace::World);
}

bool ATCStageSelectBoard::CanInteract_Implementation(ATCPlayerCharacter* Player)
{
	// 서버 권위 재검증(명세 6장-8): 클라 UI 노출 여부와 무관하게, 요청한 폰이 실제로
	// 이 서버 프로세스에서 로컬 컨트롤인지(=리슨 서버의 호스트 자신)로 최종 판별한다.
	// IsLocallyControlled() 는 AController 가 아닌 APawn 의 멤버 함수다.
	return Player && Player->IsLocallyControlled();
}

void ATCStageSelectBoard::OnFocus_Implementation()
{
	// 아웃라인 하이라이트(ATCMapInteractable/InteractableDoor와 동일 패턴, CustomDepth-Stencil
	// PP 아웃라인 — 스텐실==1 기준). SetRenderCustomDepth는 로컬 렌더 플래그라 리플리케이트되지
	// 않으므로, 다른 클라이언트 화면에는 영향을 주지 않는다 — IsHost() 게이팅 밖에 두어 방장이
	// 아니어도(어차피 실제 상호작용은 CanInteract에서 서버가 막지만) "상호작용 가능한 오브젝트"라는
	// 시각 정보는 동일하게 받도록 한다.
	// BoardMesh(루트)가 아니라 액터에 붙은 모든 StaticMeshComponent에 적용한다 — 배치된
	// BP_StageSelectBoard 인스턴스는 BoardMesh 자체엔 메시가 없고, 실제 보이는 게시판 메시는
	// 그 자식으로 붙은 별도 컴포넌트(예: Cube)이기 때문에 BoardMesh만 대상으로 하면 아무 것도
	// 렌더링되지 않아 아웃라인이 안 보였다.
	TArray<UStaticMeshComponent*> MeshComponents;
	GetComponents<UStaticMeshComponent>(MeshComponents);
	for (UStaticMeshComponent* MeshComp : MeshComponents)
	{
		if (MeshComp)
		{
			MeshComp->SetCustomDepthStencilValue(1);
			MeshComp->SetRenderCustomDepth(true);
		}
	}

	// 클라이언트 로컬 호출(명세 4장-5): 방장 클라이언트에서만 상호작용 프롬프트를 노출한다.
	// 비방장 클라이언트는 이 함수 자체는 호출되어도(자기 자신의 폰이 근접했으므로) IsHost() 가
	// false 이므로 아무 것도 Broadcast 하지 않아 프롬프트가 뜨지 않는다(완전 숨김).
	if (const UTCSessionFlow* Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTCSessionFlow>() : nullptr)
	{
		if (Flow->IsHost())
		{
			if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
			{
				// 실제 상호작용 키는 IA_Interact(=좌클릭, IMC_PlayerCharacter 기준)이다 — "E"가 아니다.
				MockController->OnInteractTargetChanged.Broadcast(this, TEXT("좌클릭 - 스테이지 선택"));
			}
		}
	}
}

void ATCStageSelectBoard::OnUnfocus_Implementation()
{
	TArray<UStaticMeshComponent*> MeshComponents;
	GetComponents<UStaticMeshComponent>(MeshComponents);
	for (UStaticMeshComponent* MeshComp : MeshComponents)
	{
		if (MeshComp)
		{
			MeshComp->SetRenderCustomDepth(false);
		}
	}

	if (const UTCSessionFlow* Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTCSessionFlow>() : nullptr)
	{
		if (Flow->IsHost())
		{
			if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
			{
				MockController->OnInteractTargetChanged.Broadcast(nullptr, FString());
			}
		}
	}
}

void ATCStageSelectBoard::OnInteract_Implementation(ATCPlayerCharacter* Player)
{
	// GrabComponent::ServerTryInteract_Implementation 을 거쳐 서버에서 호출됨(InteractableDoor와 동일 패턴).
	// CanInteract 를 통과했다는 것은 Player 가 이 서버 프로세스의 로컬 컨트롤(=호스트 자신)이라는
	// 뜻이다. 화면 오버레이(O_StageSelect)를 여는 대신, 게시판의 BoardScreen(월드 스페이스 위젯)에
	// 실제 선택 UI를 렌더링하고, 호스트의 커서/WidgetInteractionComponent로 그것을 클릭하게 한다
	// (v3 내부 개정 — 게시판 UI 디자인 확정). Client RPC이므로 리슨 서버 여부와 무관하게 항상 올바른
	// 클라이언트로 라우팅된다.
	UE_LOG(LogTemp, Warning, TEXT("[StageSelectBoard] OnInteract 진입: HasAuthority=%d, Player=%s"),
		HasAuthority(), Player ? *Player->GetName() : TEXT("null"));
	if (!HasAuthority())
	{
		return;
	}
	if (ATCPlayerController* PC = Player ? Cast<ATCPlayerController>(Player->GetController()) : nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[StageSelectBoard] ClientEnterBoardInteractionMode 호출"));
		PC->ClientEnterBoardInteractionMode(this);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[StageSelectBoard] PC 캐스트 실패"));
	}
}

UW_StageBoardScreen* ATCStageSelectBoard::GetBoardScreenWidget() const
{
	return BoardScreen ? Cast<UW_StageBoardScreen>(BoardScreen->GetUserWidgetObject()) : nullptr;
}

FVector ATCStageSelectBoard::GetTeleportLocation() const
{
	return TeleportAnchor ? TeleportAnchor->GetComponentLocation() : GetActorLocation();
}

FRotator ATCStageSelectBoard::GetTeleportRotation() const
{
	return TeleportAnchor ? TeleportAnchor->GetComponentRotation() : GetActorRotation();
}
