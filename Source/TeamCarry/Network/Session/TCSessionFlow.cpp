// TCSessionFlow.cpp

#include "Network/Session/TCSessionFlow.h"
#include "Network/Session/TCGameInstance.h"
#include "Network/Session/TCLobbyGameState.h"
#include "Network/Net/TCNetStatics.h"
#include "Core/TeamCarryGameState.h"
#include "Core/TCSaveGame.h"
#include "Player/PlayerController/TCPlayerController.h"
#include "TeamCarry/UI/S_Loading.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectGlobals.h"
#include "MoviePlayer.h"

UTCSessionFlow::UTCSessionFlow()
{
	// MoviePlayer 로딩 화면 위젯 클래스(hard travel 보강용, 2장·4장-9). 자산이 아직 없으면
	// FClassFinder가 실패해 nullptr로 남고, HandlePreLoadMap()이 경고 로그만 남긴 채 넘어간다
	// (MockUIController::LoadingWidgetClass와 동일한 폴백 패턴).
	// (2026-07-19) 전용 WBP_MovieLoadingScreen 대신 기존 WBP_S_Loading을 그대로 재사용한다 —
	// hard travel이든 seamless travel이든 사용자에게 보이는 로딩 화면 디자인이 항상 동일해야
	// 한다는 요청 반영. 내부적으로는 여전히 MoviePlayer(별도 렌더 경로)가 hard travel 구간의
	// 게임 스레드 블로킹을 덮고, WBP_S_Loading은 그 위에 표시되는 "겉모습"만 담당한다.
	static ConstructorHelpers::FClassFinder<UUserWidget> MovieLoadingWidgetFinder(
		TEXT("/Game/Developers/MinkiCho/Blueprint/UI/WBP_S_Loading"));
	if (MovieLoadingWidgetFinder.Succeeded())
	{
		MovieLoadingWidgetClass = MovieLoadingWidgetFinder.Class;
	}
}

void UTCSessionFlow::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// DefaultGame.ini [/Script/TeamCarry.TCSessionFlow] 의 맵 경로 오버라이드를 인스턴스에 반영.
	LoadConfig();

	// DT_Stages 가 맵 경로의 정본(2026-07-09 통합) — RowName "Title"/"Lobby"/"Tutorial" 행이
	// 있으면 ini 값을 덮어쓴다 (StageId=0 예약행, 스테이지 목록에는 노출되지 않음).
	// 행이 없거나 DT 미설정이면 기존 ini/기본값 폴백 그대로 동작한다.
	if (UDataTable* Table = StageDataTable.LoadSynchronous())
	{
		auto OverrideFromRow = [Table](const TCHAR* RowName, FString& InOutPath)
		{
			if (const FStageInfo* Row = Table->FindRow<FStageInfo>(RowName, TEXT("SessionFlow.MapPaths"), false))
			{
				if (!Row->MapPath.IsEmpty())
				{
					InOutPath = Row->MapPath;
				}
			}
		};
		OverrideFromRow(TEXT("Title"), TitleMapPath);
		OverrideFromRow(TEXT("Lobby"), LobbyMapPath);
		OverrideFromRow(TEXT("Tutorial"), TutorialMapPath);
	}

	BindGameInstanceEvents();

	// MoviePlayer 로딩 화면(hard travel 보강, 2장·4장-9). 두 델리게이트 모두 GameInstance 서브시스템
	// 수명(Initialize~Deinitialize) 동안만 구독한다 — 코어 델리게이트라 해제하지 않으면 이 서브시스템
	// 인스턴스가 파괴돼도 콜백이 계속 걸려 있어 댕글링 호출로 이어진다.
	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UTCSessionFlow::HandlePreLoadMap);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UTCSessionFlow::HandlePostLoadMap);

	UE_LOG(LogTCNet, Log, TEXT("UTCSessionFlow Initialized. (Title=%s Lobby=%s)"), *TitleMapPath, *LobbyMapPath);
}

void UTCSessionFlow::Deinitialize()
{
	FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadMapHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);

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

TArray<FSaveSlotInfo> UTCSessionFlow::GetAllSaveSlotInfos() const
{
	TArray<FSaveSlotInfo> Result;
	Result.Reserve(NumSaveSlots);

	for (int32 Index = 0; Index < NumSaveSlots; ++Index)
	{
		FSaveSlotInfo Info;
		Info.SlotIndex = Index;
		Info.SlotName = MakeSaveSlotName(Index);
		Info.bHasSaveData = UGameplayStatics::DoesSaveGameExist(Info.SlotName, 0);

		if (Info.bHasSaveData)
		{
			if (const UTCSaveGame* SaveData = Cast<UTCSaveGame>(UGameplayStatics::LoadGameFromSlot(Info.SlotName, 0)))
			{
				Info.LastPlayedStage = SaveData->LastPlayedStage;
			}
		}

		Result.Add(Info);
	}

	return Result;
}

void UTCSessionFlow::DeleteSaveSlot(const FString& SlotName)
{
	const bool bDeleted = UGameplayStatics::DeleteGameInSlot(SlotName, 0);
	UE_LOG(LogTCNet, Log, TEXT("[SessionFlow] 세이브 슬롯 삭제: '%s' (성공=%d)"), *SlotName, bDeleted);
}

// ── 스테이지 선택 ──
void UTCSessionFlow::SetStageSelection(int32 InStageId)
{
	SelectedStageId = InStageId;
	UE_LOG(LogTCNet, Log, TEXT("[SessionFlow] StageSelection: StageId=%d"), InStageId);

	// O_StageSelect 는 방장에게만 노출되므로, 이 호출은 곧 서버 권위 프로세스에서 일어난다.
	// 참가자도 로비에서 확인 가능하도록 GameState 에 복제(명세 5장).
	if (UWorld* World = GetWorld())
	{
		if (ATCLobbyGameState* LobbyGS = World->GetGameState<ATCLobbyGameState>())
		{
			LobbyGS->SetSelectedStageIdAuthoritative(InStageId);
		}
	}

	// 방장이 스테이지를 바꿀 때마다 새로 선택된 맵도 백그라운드 프리로드 대상으로 삼는다
	// (기본 선택 스테이지는 InitGameState()에서 이미 1회 트리거됨).
	PreloadSelectedStageMapAsync();
}

void UTCSessionFlow::PreloadSelectedStageMapAsync()
{
	const FString MapPath = GetSelectedStageMapPath();
	if (MapPath.IsEmpty())
	{
		return;
	}

	UE_LOG(LogTCNet, Log, TEXT("[SessionFlow] 스테이지 맵 백그라운드 프리로드 시작: %s"), *MapPath);

	// Best-effort — 실패/지연돼도 실제 트래블 시점엔 어차피 정상적으로 (다시) 로드되므로
	// 기능적으로 문제없다. 목적은 로비 대기 시간을 이용해 그 레벨의 머티리얼/텍스처 패키지를
	// 미리 메모리에 올려, 실제 진입 시점의 로드 부담을 조금이라도 줄이는 것뿐이다.
	LoadPackageAsync(MapPath, FLoadPackageAsyncDelegate::CreateLambda(
		[MapPath](const FName& PackageName, UPackage* LoadedPackage, EAsyncLoadingResult::Type Result)
		{
			UE_LOG(LogTCNet, Log, TEXT("[SessionFlow] 스테이지 맵 백그라운드 프리로드 완료: %s (결과=%d)"),
				*MapPath, static_cast<int32>(Result));
		}));
}

int32 UTCSessionFlow::GetSelectedStageId() const
{
	// 미선택(0 이하)이면 기본 1스테이지로 폴백.
	return SelectedStageId > 0 ? SelectedStageId : 1;
}

FString UTCSessionFlow::GetSelectedStageMapPath() const
{
	FStageInfo Info;
	if (FindStageInfo(GetSelectedStageId(), Info) && !Info.MapPath.IsEmpty())
	{
		return Info.MapPath;
	}
	// DT_Stages 미설정/일치하는 행 없음 시 단일 폴백 맵을 반환한다.
	return DefaultStageMapPath;
}

TArray<FStageInfo> UTCSessionFlow::GetAllStageInfos() const
{
	TArray<FStageInfo> Result;
	if (const UDataTable* Table = StageDataTable.LoadSynchronous())
	{
		TArray<FStageInfo*> Rows;
		Table->GetAllRows<FStageInfo>(TEXT("GetAllStageInfos"), Rows);
		for (const FStageInfo* Row : Rows)
		{
			// StageId<=0 은 맵 경로 예약행(Title/Lobby/Tutorial) — 스테이지 목록에서 제외
			if (Row && Row->StageId > 0)
			{
				Result.Add(*Row);
			}
		}
	}
	return Result;
}

bool UTCSessionFlow::FindStageInfo(int32 StageId, FStageInfo& OutInfo) const
{
	if (const UDataTable* Table = StageDataTable.LoadSynchronous())
	{
		TArray<FStageInfo*> Rows;
		Table->GetAllRows<FStageInfo>(TEXT("FindStageInfo"), Rows);
		for (const FStageInfo* Row : Rows)
		{
			if (Row && Row->StageId == StageId)
			{
				OutInfo = *Row;
				return true;
			}
		}
	}
	return false;
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
	// [임시 2026-07-09] 튜토리얼 비활성 — 새 게임/이어하기 모두 선택된 스테이지로 직행.
	// 튜토리얼(S_Tutorial·스킵 UI 포함)을 다시 켜려면 아래 원본 분기로 복원할 것:
	//   const FString& NextMap = bContinueMode ? GetSelectedStageMapPath() : TutorialMapPath;
	const FString NextMap = GetSelectedStageMapPath();
	HostServerTravel(NextMap);
}

void UTCSessionFlow::HostReturnToLobby()
{
	if (!IsHost())
	{
		return;
	}

	// [스테이지 진행] 클리어(게임 종료) 상태로 로비에 복귀하면 다음 스테이지를 기본 선택 —
	// 로비에서 준비 후 재시작하면 다음 레벨로 이어진다. 마지막 스테이지면 유지,
	// 일시정지 메뉴의 중도 포기 복귀(미종료)는 진행하지 않는다.
	if (const UWorld* World = GetWorld())
	{
		const ATeamCarryGameState* GS = World->GetGameState<ATeamCarryGameState>();
		if (GS && GS->bIsGameFinished)
		{
			FStageInfo NextInfo;
			const int32 NextId = GetSelectedStageId() + 1;
			if (FindStageInfo(NextId, NextInfo))
			{
				SetStageSelection(NextId);
				UE_LOG(LogTCNet, Log, TEXT("[SessionFlow] 스테이지 클리어 — 다음 스테이지(%d) 자동 선택"), NextId);
			}
		}
	}

	HostServerTravel(LobbyMapPath);
}

void UTCSessionFlow::RestartStage()
{
	if (!IsHost())
	{
		UE_LOG(LogTCNet, Warning, TEXT("[SessionFlow] RestartStage: 호스트 아님 — 무시"));
		return;
	}
	HostServerTravel(GetSelectedStageMapPath());
}

void UTCSessionFlow::CompleteTutorial()
{
	if (!IsHost())
	{
		UE_LOG(LogTCNet, Warning, TEXT("[SessionFlow] CompleteTutorial: 호스트 아님 — 무시"));
		return;
	}

	// 세이브에 튜토리얼 완료 플래그 기록(명세 5장). 이 세션이 선택한 슬롯(SelectedSlotName)에 기록해,
	// ATeamCarryGameMode::SaveGame()/LoadGame() 이 같은 슬롯을 계속 사용하도록 한다. 슬롯 미선택
	// (구버전 호환) 시에만 "TCGameSave" 로 폴백한다.
	const FString SlotName = SelectedSlotName.IsEmpty() ? TEXT("TCGameSave") : SelectedSlotName;
	UTCSaveGame* SaveData = Cast<UTCSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	if (!SaveData)
	{
		SaveData = Cast<UTCSaveGame>(UGameplayStatics::CreateSaveGameObject(UTCSaveGame::StaticClass()));
	}
	SaveData->bTutorialCompleted = true;
	UGameplayStatics::SaveGameToSlot(SaveData, SlotName, 0);

	// 같은 방의 다음 HostStartGame() 이 이어하기(스테이지 직행) 경로를 타도록 전환.
	bContinueMode = true;

	UE_LOG(LogTCNet, Log, TEXT("[SessionFlow] 튜토리얼 완료 기록. 이후 게임 시작은 스테이지로 직행합니다."));

	HostReturnToLobby();
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
	OnTravelStarted.Broadcast(MapPath);

	// (2026-07-19 되돌림) seamless travel 도중 StartMovieLoadingScreen()을 직접 호출해 보았으나,
	// 실측 결과 게임 스레드가 완전히 멈추는 행(hang)이 발생해 되돌렸다 — SeamlessTravel 도중에는
	// 엔진이 계속 게임 스레드로 패키지 로딩/월드 전환 작업을 진행해야 하는데, MoviePlayer의
	// bWaitForManualStop 로딩 화면이 렌더 스레드를 통째로 점유하는 방식과 충돌해 데드락으로
	// 보인다(로그가 SeamlessTravel 시작 직후 수십 초간 완전히 끊김, 강제 종료 전까지 회복 안 됨).
	// MoviePlayer는 hard travel(?listen, PreLoadMap이 실제로 발화하는 경우)에서만 안전하다 —
	// StartMovieLoadingScreen()은 그쪽(HandlePreLoadMap) 경로로만 호출한다. seamless travel의
	// "레벨 첫 방문 시 셰이더 컴파일 스톨" 문제는 별도 접근(예: PSO 사전 워밍업)이 필요하다.

	// 알려진 문제 수정(명세 2장): OnTravelStarted는 이 프로세스(호스트) 로컬에서만 발화되어,
	// 다른 클라이언트는 로딩 화면을 못 보고 곧장 목적지 화면으로 순간이동한 것처럼 보였다.
	// 서버 트래블 직전, 접속 중인 모든 PC에 Client RPC로 로딩 화면 표시를 명시적으로 지시한다.
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (ATCPlayerController* PC = Cast<ATCPlayerController>(It->Get()))
		{
			PC->ClientShowLoadingScreen();
		}
	}

	// 이미 리슨서버이므로 ?listen 재지정 불필요. 클라는 자동 추종.
	World->ServerTravel(MapPath);
}

// ── MoviePlayer 로딩 화면(hard travel 보강, 2장·4장-9) ──
bool UTCSessionFlow::IsKnownNonStageMapPath(const FString& MapName) const
{
	return MapName.Contains(TitleMapPath) || MapName.Contains(LobbyMapPath) || MapName.Contains(TutorialMapPath);
}

void UTCSessionFlow::HandlePreLoadMap(const FString& MapName)
{
	// hard travel(?listen) 전용 발화 경로 — 실제 로직은 StartMovieLoadingScreen()이 담당한다.
	// (HostServerTravel의 seamless travel 호출과 중복될 일이 없다: PreLoadMap은 seamless에서
	// 아예 발화되지 않으므로 — StartMovieLoadingScreen() 내부의 "이미 재생 중" 가드는 순수 방어용.)
	StartMovieLoadingScreen(MapName);
}

void UTCSessionFlow::StartMovieLoadingScreen(const FString& MapName)
{
	if (!MovieLoadingWidgetClass)
	{
		UE_LOG(LogTCNet, Warning, TEXT("[SessionFlow] MovieLoadingWidgetClass 없음 — MoviePlayer 로딩 화면 생략 (%s)"), *MapName);
		return;
	}

	// 이미 재생 중이면 재시작하지 않는다(중복 호출 방어 — 예: 짧은 시간 안에 연속 트래블).
	if (IGameMoviePlayer* ExistingPlayer = GetMoviePlayer())
	{
		if (ExistingPlayer->IsMovieCurrentlyPlaying())
		{
			UE_LOG(LogTCNet, Verbose, TEXT("[SessionFlow] MoviePlayer 이미 재생 중 — 재시작 생략 (%s)"), *MapName);
			return;
		}
	}

	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}

	// GameInstance 소유로 생성한다 — 이 시점(PreLoadMap)엔 아직 새 World가 없고, 기존 World도
	// 곧 파괴될 예정이라 World 컨텍스트에 의존할 수 없다(MockUIController::PersistentLoadingWidget과
	// 동일한 이유).
	ActiveMovieLoadingWidget = CreateWidget<US_Loading>(GI, MovieLoadingWidgetClass);
	if (!ActiveMovieLoadingWidget)
	{
		return;
	}

	const bool bIsStageMap = !IsKnownNonStageMapPath(MapName);

	FLoadingScreenAttributes Attr;
	Attr.WidgetLoadingScreen = ActiveMovieLoadingWidget->TakeWidget();
	Attr.bAutoCompleteWhenLoadingCompletes = !bIsStageMap;
	// 스테이지 맵(S_InGame) 진입만 "전원 대기 게이트" 대상이다(로비는 참가자가 서로 다른 시점에
	// 합류하므로 "전원 대기" 개념 자체가 적용되지 않는다 — UI_Technical_Spec.md 2장). 대기 해제는
	// StopMovieLoadingScreen()(ATCPlayerController::ClientNotifyAllPlayersLoaded_Implementation()이
	// "전원 로딩 완료"를 확정하는 시점에 호출)이 담당한다.
	Attr.bWaitForManualStop = bIsStageMap;
	Attr.MinimumLoadingScreenDisplayTime = 0.25f;

	// GetMoviePlayer()는 TSharedPtr가 아니라 IGameMoviePlayer* 원시 포인터를 반환하며(MoviePlayer.h),
	// 무비 플레이어가 이 빌드 설정에서 활성화되지 않았으면(예: 데디케이티드 서버) nullptr일 수 있다.
	if (IGameMoviePlayer* MoviePlayer = GetMoviePlayer())
	{
		MoviePlayer->SetupLoadingScreen(Attr);
		MoviePlayer->PlayMovie();
		UE_LOG(LogTCNet, Log, TEXT("[SessionFlow] MoviePlayer 로딩 화면 시작 → %s (스테이지 맵=%d)"), *MapName, bIsStageMap);
	}
	else
	{
		UE_LOG(LogTCNet, Warning, TEXT("[SessionFlow] GetMoviePlayer() == nullptr — MoviePlayer 로딩 화면 생략 (%s)"), *MapName);
	}
}

void UTCSessionFlow::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (!ActiveMovieLoadingWidget || !LoadedWorld)
	{
		return;
	}

	const FString MapName = LoadedWorld->GetOutermost()->GetName();
	if (!IsKnownNonStageMapPath(MapName))
	{
		// 이 클라이언트의 로컬 로딩은 끝났지만, MoviePlayer는 아직 내려가지 않는다
		// (SetupLoadingScreen()에서 bWaitForManualStop=true로 설정됨) — WBP_S_Loading 자신의
		// 상태 머신(Traveling → LocalComplete)을 그대로 진행시켜 "다른 플레이어를 기다리는 중..."
		// 문구와 99% 표시로 자연스럽게 전환한다(지속형 인스턴스가 쓰는 것과 동일한 공개 API).
		ActiveMovieLoadingWidget->NotifyLocalLoadComplete();
	}
}

void UTCSessionFlow::StopMovieLoadingScreen()
{
	if (IGameMoviePlayer* MoviePlayer = GetMoviePlayer())
	{
		if (MoviePlayer->IsMovieCurrentlyPlaying())
		{
			MoviePlayer->StopMovie();
		}
	}
	ActiveMovieLoadingWidget = nullptr;
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
		// (2026-07-19) OnTravelStarted를 여기서 Broadcast하지 않는다 — OpenLevel()은 hard travel이라
		// 엔진의 PreLoadMap 델리게이트가 자체적으로 발화되고, HandlePreLoadMap()이 MoviePlayer로
		// 이 구간을 이미 전부 덮는다(WBP_S_Loading 재사용, 위 주석 참고). 예전처럼 여기서도
		// 같이 Broadcast하면 지속형 인스턴스가 별도로 한 번 더 뜨면서 "같은 로딩 화면이 두 번
		// 나온다"는 체감 버그가 생긴다(실측 확인) — hard travel 구간은 MoviePlayer 단독 담당,
		// seamless travel(HostServerTravel)만 지속형 인스턴스가 담당하도록 역할을 분리한다.
		UGameplayStatics::OpenLevel(this, FName(*TitleMapPath));
	}
}

// ── UTCGameInstance 콜백 ──
void UTCSessionFlow::HandleCreateSessionComplete(bool bSuccess)
{
	if (bSuccess)
	{
		// 실제 트래블(HostSteamSession 콜백의 hard ServerTravel(?listen))은 이 Broadcast를 거치지
		// 않는다 — hard travel은 엔진 PreLoadMap이 자체 발화되어 MoviePlayer가 전담한다(2026-07-19,
		// LeaveToTitle() 주석 참고). 여기서 같이 Broadcast하면 로딩 화면이 두 번 뜬다.
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
		// ClientTravel(TRAVEL_Absolute) 도 hard travel이라 이 클라이언트 프로세스에서 엔진
		// PreLoadMap이 자체 발화되어 MoviePlayer가 전담한다(2026-07-19, LeaveToTitle() 주석 참고).
		// 여기서 같이 Broadcast하면 로딩 화면이 두 번 뜬다.
		SetPhase(ETCSessionPhase::Joined, TEXT("방 접속 완료"));
	}
	else
	{
		SetPhase(ETCSessionPhase::Failed, TEXT("방 접속 실패"));
	}
}
