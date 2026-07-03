// TCSessionFlow.cpp

#include "Network/Session/TCSessionFlow.h"
#include "Network/Session/TCGameInstance.h"
#include "Network/Session/TCLobbyGameState.h"
#include "Network/Net/TCNetStatics.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

void UTCSessionFlow::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// DefaultGame.ini [/Script/TeamCarry.TCSessionFlow] 의 맵 경로 오버라이드를 인스턴스에 반영.
	LoadConfig();
	BindGameInstanceEvents();
	UE_LOG(LogTCNet, Log, TEXT("UTCSessionFlow Initialized."));
}

void UTCSessionFlow::Deinitialize()
{
	UnbindGameInstanceEvents();
	Super::Deinitialize();
}

UTCGameInstance* UTCSessionFlow::GetTCGameInstance() const
{
	return Cast<UTCGameInstance>(GetGameInstance());
}

void UTCSessionFlow::BindGameInstanceEvents()
{
	if (UTCGameInstance* GI = GetTCGameInstance())
	{
		GI->OnCreateSessionComplete.AddUniqueDynamic(this, &UTCSessionFlow::HandleCreateSessionComplete);
		GI->OnFindSessionsComplete.AddUniqueDynamic(this, &UTCSessionFlow::HandleFindSessionsComplete);
		GI->OnJoinSessionComplete.AddUniqueDynamic(this, &UTCSessionFlow::HandleJoinSessionComplete);
	}
	else
	{
		// GameInstanceClass 가 UTCGameInstance 가 아니면 세션 API 자체가 없다(EDITOR-TASKS §1).
		UE_LOG(LogTCNet, Warning, TEXT("UTCSessionFlow: GameInstance 가 UTCGameInstance 가 아님 — 세션 바인딩 불가"));
	}
}

void UTCSessionFlow::UnbindGameInstanceEvents()
{
	if (UTCGameInstance* GI = GetTCGameInstance())
	{
		GI->OnCreateSessionComplete.RemoveDynamic(this, &UTCSessionFlow::HandleCreateSessionComplete);
		GI->OnFindSessionsComplete.RemoveDynamic(this, &UTCSessionFlow::HandleFindSessionsComplete);
		GI->OnJoinSessionComplete.RemoveDynamic(this, &UTCSessionFlow::HandleJoinSessionComplete);
	}
}

void UTCSessionFlow::SetPhase(ETCSessionPhase Phase, const FString& Message)
{
	UE_LOG(LogTCNet, Log, TEXT("[SessionFlow] Phase=%d %s"), static_cast<int32>(Phase), *Message);
	OnSessionPhaseChanged.Broadcast(Phase, Message);
}

bool UTCSessionFlow::IsHost() const
{
	if (const UWorld* World = GetWorld())
	{
		const ENetMode Mode = World->GetNetMode();
		return Mode == NM_ListenServer || Mode == NM_Standalone || Mode == NM_DedicatedServer;
	}
	return false;
}

// ── 세이브 선택 ──
void UTCSessionFlow::SetSaveSelection(const FString& InSlotName, bool bInContinue)
{
	SelectedSlotName = InSlotName;
	bContinueMode = bInContinue;
	UE_LOG(LogTCNet, Log, TEXT("[SessionFlow] SaveSelection: slot='%s' continue=%d"), *InSlotName, bInContinue);
}

// ── 호스트 의도 ──
void UTCSessionFlow::HostCreateRoom()
{
	UTCGameInstance* GI = GetTCGameInstance();
	if (!GI)
	{
		SetPhase(ETCSessionPhase::Failed, TEXT("GameInstance 가 UTCGameInstance 가 아님"));
		return;
	}
	SetPhase(ETCSessionPhase::Creating, TEXT("방 생성 중..."));
	// 성공 시 HostSteamSession 내부 콜백이 LobbyMap 으로 ?listen ServerTravel 한다.
	GI->HostSteamSession(LobbyMapPath, MaxPlayers, /*bLAN=*/false);
}

void UTCSessionFlow::HostStartGame()
{
	if (!IsHost())
	{
		UE_LOG(LogTCNet, Warning, TEXT("[SessionFlow] HostStartGame: 호스트 아님 — 무시"));
		return;
	}
	// 새 게임 → 튜토리얼, 이어하기 → 스테이지 선택.
	const FString& NextMap = bContinueMode ? StageSelectMapPath : TutorialMapPath;
	HostServerTravel(NextMap);
}

void UTCSessionFlow::HostTravelToStage(const FString& StageMapPath)
{
	if (!IsHost())
	{
		UE_LOG(LogTCNet, Warning, TEXT("[SessionFlow] HostTravelToStage: 호스트 아님 — 무시"));
		return;
	}
	HostServerTravel(StageMapPath);
}

void UTCSessionFlow::HostReturnToLobby()
{
	if (!IsHost())
	{
		return;
	}
	HostServerTravel(LobbyMapPath);
}

void UTCSessionFlow::HostServerTravel(const FString& MapPath)
{
	UWorld* World = GetWorld();
	if (!World || MapPath.IsEmpty())
	{
		SetPhase(ETCSessionPhase::Failed, TEXT("ServerTravel 대상 맵/월드 없음"));
		return;
	}
	UE_LOG(LogTCNet, Log, TEXT("[SessionFlow] ServerTravel → %s"), *MapPath);
	// 이미 리슨서버이므로 ?listen 재지정 불필요. 클라는 자동 추종.
	World->ServerTravel(MapPath);
}

// ── 클라이언트 의도 ──
void UTCSessionFlow::JoinRoomByCode(const FString& RoomCode)
{
	UTCGameInstance* GI = GetTCGameInstance();
	if (!GI)
	{
		SetPhase(ETCSessionPhase::Failed, TEXT("GameInstance 가 UTCGameInstance 가 아님"));
		return;
	}
	// 코드 정규화(공백 제거 + 대문자) — 호스트 광고값과 동일 규칙으로 매칭.
	PendingJoinCode = RoomCode.TrimStartAndEnd().ToUpper();
	if (PendingJoinCode.IsEmpty())
	{
		SetPhase(ETCSessionPhase::Failed, TEXT("방 코드를 입력하세요"));
		return;
	}
	bWantsJoinAfterSearch = true;
	SetPhase(ETCSessionPhase::Searching, TEXT("세션 검색 중..."));
	GI->FindSteamSessions(/*bLAN=*/false, /*MaxSearchResults=*/20);
}

FString UTCSessionFlow::GetRoomCode() const
{
	// 1) 호스트: GameInstance 가 직접 코드를 보유.
	const UTCGameInstance* GI = GetTCGameInstance();
	if (GI && !GI->GetHostRoomCode().IsEmpty())
	{
		return GI->GetHostRoomCode();
	}

	// 2) 클라이언트: 호스트가 GameState 에 복제해준 코드로 폴백.
	//    (HostRoomCode 는 호스트 프로세스에만 있어 클라 UI 가 "오프라인"으로 뜨던 문제 해결)
	if (const UWorld* World = GetWorld())
	{
		if (const ATCLobbyGameState* LobbyGS = World->GetGameState<ATCLobbyGameState>())
		{
			return LobbyGS->GetRoomCode();
		}
	}
	return FString();
}

// --- UI 테스트용 ---
void UTCSessionFlow::HostReturnToStageSelect()
{
	// 방장(호스트)이 아니면 실행을 무시합니다. (싱글 플레이는 IsHost()가 true를 반환하므로 정상 실행됨)
	if (!IsHost())
	{
		UE_LOG(LogTCNet, Warning, TEXT("[SessionFlow] HostReturnToStageSelect: 호스트 아님 — 무시"));
		return;
	}

	// 내부 창고(protected)에 있는 맵 경로를 찾아 알아서 이동을 지시합니다.
	HostServerTravel(StageSelectMapPath);
}

// ── 공용 ──
void UTCSessionFlow::LeaveToTitle()
{
	if (UTCGameInstance* GI = GetTCGameInstance())
	{
		GI->DestroySteamSession();
	}
	SetPhase(ETCSessionPhase::Idle, TEXT("타이틀로 복귀"));
	// 세션 파기는 비동기지만, 타이틀 복귀는 로컬 맵 오픈으로 즉시 진행.
	if (!TitleMapPath.IsEmpty())
	{
		UGameplayStatics::OpenLevel(this, FName(*TitleMapPath));
	}
}

// ── UTCGameInstance 콜백 ──
void UTCSessionFlow::HandleCreateSessionComplete(bool bSuccess)
{
	if (bSuccess)
	{
		// 실제 트래블은 HostSteamSession 콜백이 수행 → 여기선 단계 통지만.
		SetPhase(ETCSessionPhase::Hosting, TEXT("방 생성 완료 — 로비 진입"));
	}
	else
	{
		SetPhase(ETCSessionPhase::Failed, TEXT("방 생성 실패"));
	}
}

void UTCSessionFlow::HandleFindSessionsComplete(bool bSuccess, int32 NumFound)
{
	if (!bWantsJoinAfterSearch)
	{
		return; // 단순 검색(목록 갱신 등)은 무시.
	}
	bWantsJoinAfterSearch = false;

	UTCGameInstance* GI = GetTCGameInstance();
	if (!bSuccess || NumFound <= 0 || !GI)
	{
		SetPhase(ETCSessionPhase::Failed, TEXT("접속 가능한 방이 없습니다"));
		return;
	}

	// 입력 코드와 광고된 방 코드가 일치하는 세션을 찾는다.
	int32 JoinIndex = INDEX_NONE;
	for (int32 i = 0; i < NumFound; ++i)
	{
		const FString FoundCode = GI->GetFoundSessionCode(i).TrimStartAndEnd().ToUpper();
		if (!FoundCode.IsEmpty() && FoundCode == PendingJoinCode)
		{
			JoinIndex = i;
			break;
		}
	}

	if (JoinIndex == INDEX_NONE)
	{
		SetPhase(ETCSessionPhase::Failed, FString::Printf(TEXT("코드 '%s' 와 일치하는 방이 없습니다"), *PendingJoinCode));
		return;
	}

	SetPhase(ETCSessionPhase::Joining, TEXT("방 접속 중..."));
	GI->JoinFoundSession(JoinIndex);
}

void UTCSessionFlow::HandleJoinSessionComplete(bool bSuccess)
{
	if (bSuccess)
	{
		// ClientTravel 은 JoinFoundSession 콜백이 수행 → 단계 통지만.
		SetPhase(ETCSessionPhase::Joined, TEXT("방 접속 완료"));
	}
	else
	{
		SetPhase(ETCSessionPhase::Failed, TEXT("방 접속 실패"));
	}
}
