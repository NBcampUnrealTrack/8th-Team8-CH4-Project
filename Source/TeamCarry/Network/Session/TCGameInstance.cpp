// TCGameInstance.cpp

#include "Network/Session/TCGameInstance.h"
#include "Network/Net/TCNetStatics.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Online/OnlineSessionNames.h"   // UE5: SETTING_MAPNAME / SEARCH_PRESENCE 등 표준 키

namespace
{
	// TeamCarry 세션 식별 키 — 검색 시 우리 게임 세션만 필터링.
	const FName TC_SESSION_KEY = TEXT("TCGameName");
	const FString TC_SESSION_VALUE = TEXT("TeamCarry");
	
	// 방 코드 광고 키 — 클라이언트가 입력한 코드와 매칭.
	const FName TC_ROOMCODE_KEY = TEXT("TCRoomCode");
}

// ─────────────────────────────────────────────────────────────
// 레거시: LAN / 직접 IP
// ─────────────────────────────────────────────────────────────

// 지정 맵을 listen 옵션으로 ServerTravel — 호스트가 서버 겸 클라이언트가 된다.
void UTCGameInstance::HostListenServer(const FString& MapName)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTCNet, Warning, TEXT("HostListenServer: World 없음"));
		return;
	}

	// 리슨 시작은 hard travel 필수 (seamless 는 ?listen 무시 — HandleCreateSessionComplete 주석 참고)
	if (AGameModeBase* GM = World->GetAuthGameMode())
	{
		GM->bUseSeamlessTravel = false;
	}

	// 맵 경로 뒤에 ?listen 을 붙여 리슨 서버로 오픈
	const FString TravelURL = FString::Printf(TEXT("%s?listen"), *MapName);
	UE_LOG(LogTCNet, Log, TEXT("HostListenServer: %s"), *TravelURL);
	World->ServerTravel(TravelURL);
}

// 로컬 플레이어 컨트롤러를 통해 지정 주소로 ClientTravel
void UTCGameInstance::JoinByAddress(const FString& Address)
{
	APlayerController* PC = GetFirstLocalPlayerController();
	if (!PC)
	{
		UE_LOG(LogTCNet, Warning, TEXT("JoinByAddress: 로컬 PlayerController 없음"));
		return;
	}

	UE_LOG(LogTCNet, Log, TEXT("JoinByAddress: %s"), *Address);
	PC->ClientTravel(Address, ETravelType::TRAVEL_Absolute);
}

// ─────────────────────────────────────────────────────────────
// Steam OSS 세션
// ─────────────────────────────────────────────────────────────

// 6자리 방 코드 생성 — 혼동 문자(0/O, 1/I) 제외한 32자 집합.
FString UTCGameInstance::GenerateRoomCode()
{
	static const TCHAR Alphabet[] = TEXT("ABCDEFGHJKLMNPQRSTUVWXYZ23456789");
	const int32 AlphabetLen = UE_ARRAY_COUNT(Alphabet) - 1;	// 널 종단 제외
	FString Code;
	Code.Reserve(6);
	for (int32 i = 0; i < 6; ++i)
	{
		Code.AppendChar(Alphabet[FMath::RandRange(0, AlphabetLen - 1)]);
	}
	return Code;
}

IOnlineSessionPtr UTCGameInstance::GetSessionInterface() const
{
	IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
	if (!OSS)
	{
		return nullptr;
	}
	return OSS->GetSessionInterface();
}

void UTCGameInstance::SetAllowJoinInProgress(bool bAllow)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		return;
	}

	FOnlineSessionSettings* Settings = Sessions->GetSessionSettings(NAME_GameSession);
	if (!Settings)
	{
		return;
	}

	Settings->bAllowJoinInProgress = bAllow;
	Sessions->UpdateSession(NAME_GameSession, *Settings, true);

	UE_LOG(LogTCNet, Log, TEXT("SetAllowJoinInProgress: %s"), bAllow ? TEXT("true") : TEXT("false"));
}

void UTCGameInstance::HostSteamSession(const FString& MapName, int32 MaxPlayers, bool bLAN)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		// OSS 미초기화(Steam 미실행 등) → 레거시 리슨 서버로 폴백
		UE_LOG(LogTCNet, Warning, TEXT("HostSteamSession: SessionInterface 없음 — LAN 리슨서버로 폴백"));
		HostListenServer(MapName);
		return;
	}

	// 같은 이름 세션이 남아 있으면 먼저 파기(재호스트 안전화)
	if (Sessions->GetNamedSession(NAME_GameSession) != nullptr)
	{
		UE_LOG(LogTCNet, Log, TEXT("HostSteamSession: 기존 세션 정리 후 재생성"));
		Sessions->DestroySession(NAME_GameSession);
	}

	PendingTravelMap = MapName;

	// 이번 세션의 방 코드 생성 — 호스트 UI 표시 + 클라 매칭용.
	HostRoomCode = GenerateRoomCode();
	UE_LOG(LogTCNet, Log, TEXT("HostSteamSession: 방 코드 = %s"), *HostRoomCode);

	FOnlineSessionSettings Settings;
	Settings.bIsLANMatch = bLAN;
	Settings.NumPublicConnections = FMath::Max(1, MaxPlayers);
	Settings.NumPrivateConnections = 0;
	Settings.bShouldAdvertise = true;			 // 검색 목록에 노출
	Settings.bAllowJoinInProgress = true;        // 로비에서는 참여 허용 (스테이지 시작 시 false로 변경)
	Settings.bAllowJoinViaPresence = !bLAN;		 // Steam presence 기반 합류
	Settings.bUsesPresence = !bLAN;				 // 친구 합류/초대
	Settings.bUseLobbiesIfAvailable = !bLAN;	 // Steam 로비 API
	Settings.bAllowInvites = true;

	// 식별 키 + 맵 이름 + 방 코드 광고(검색 측에서 필터·표시·매칭)
	Settings.Set(TC_SESSION_KEY, TC_SESSION_VALUE, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(SETTING_MAPNAME, MapName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(TC_ROOMCODE_KEY, HostRoomCode, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	CreateSessionCompleteHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UTCGameInstance::HandleCreateSessionComplete));

	if (!Sessions->CreateSession(0, NAME_GameSession, Settings))
	{
		UE_LOG(LogTCNet, Warning, TEXT("HostSteamSession: CreateSession 호출 실패"));
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
		OnCreateSessionComplete.Broadcast(false);
	}
}

void UTCGameInstance::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
	}

	UE_LOG(LogTCNet, Log, TEXT("CreateSession 완료: %s (성공=%d)"), *SessionName.ToString(), bWasSuccessful);
	OnCreateSessionComplete.Broadcast(bWasSuccessful);

	if (bWasSuccessful)
	{
		UWorld* World = GetWorld();
		if (World && !PendingTravelMap.IsEmpty())
		{
			// 리슨 시작(?listen)은 반드시 hard travel 이어야 한다.
			// Seamless travel 은 기존 넷드라이버를 유지하는 방식이라 ?listen 옵션을
			// 무시하며, 비리슨(타이틀) 상태에서 seamless 로 가면 서버가 열리지 않는다.
			// 현재 맵의 GameMode(예: 타이틀에 로비 GM 지정된 경우 seamless=true)를
			// 이 한 번의 트래블에 한해 hard 로 강제한다. (이후 로비→게임 트래블은
			// 새 GameMode 의 seamless=true 로 진행되어 SteamSockets vport 재바인딩 회피)
			if (AGameModeBase* GM = World->GetAuthGameMode())
			{
				GM->bUseSeamlessTravel = false;
			}

			const FString TravelURL = FString::Printf(TEXT("%s?listen"), *PendingTravelMap);
			UE_LOG(LogTCNet, Log, TEXT("CreateSession → ServerTravel(hard): %s"), *TravelURL);
			World->ServerTravel(TravelURL);
		}
	}
	PendingTravelMap.Reset();
}

void UTCGameInstance::FindSteamSessions(bool bLAN, int32 MaxSearchResults)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		UE_LOG(LogTCNet, Warning, TEXT("FindSteamSessions: SessionInterface 없음"));
		OnFindSessionsComplete.Broadcast(false, 0);
		return;
	}

	SessionSearch = MakeShared<FOnlineSessionSearch>();
	SessionSearch->bIsLanQuery = bLAN;
	SessionSearch->MaxSearchResults = FMath::Max(1, MaxSearchResults);
	// Steam 로비 세션 검색(호스트의 bUseLobbiesIfAvailable과 짝). LAN 검색에선 비적용.
	if (!bLAN)
	{
		SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	}
	// TeamCarry 세션만 필터
	SessionSearch->QuerySettings.Set(TC_SESSION_KEY, TC_SESSION_VALUE, EOnlineComparisonOp::Equals);

	FindSessionsCompleteHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UTCGameInstance::HandleFindSessionsComplete));

	if (!Sessions->FindSessions(0, SessionSearch.ToSharedRef()))
	{
		UE_LOG(LogTCNet, Warning, TEXT("FindSteamSessions: FindSessions 호출 실패"));
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
		OnFindSessionsComplete.Broadcast(false, 0);
	}
}

void UTCGameInstance::HandleFindSessionsComplete(bool bWasSuccessful)
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
	}

	const int32 Num = SessionSearch.IsValid() ? SessionSearch->SearchResults.Num() : 0;
	UE_LOG(LogTCNet, Log, TEXT("FindSessions 완료: 성공=%d, %d개"), bWasSuccessful, Num);
	OnFindSessionsComplete.Broadcast(bWasSuccessful, Num);
}

void UTCGameInstance::JoinFoundSession(int32 SearchResultIndex)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid() || !SessionSearch.IsValid() ||
		!SessionSearch->SearchResults.IsValidIndex(SearchResultIndex))
	{
		UE_LOG(LogTCNet, Warning, TEXT("JoinFoundSession: 유효하지 않은 인덱스/검색결과(%d)"), SearchResultIndex);
		OnJoinSessionComplete.Broadcast(false);
		return;
	}

	JoinSessionCompleteHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &UTCGameInstance::HandleJoinSessionComplete));

	if (!Sessions->JoinSession(0, NAME_GameSession, SessionSearch->SearchResults[SearchResultIndex]))
	{
		UE_LOG(LogTCNet, Warning, TEXT("JoinFoundSession: JoinSession 호출 실패"));
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
		OnJoinSessionComplete.Broadcast(false);
	}
}

void UTCGameInstance::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid())
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
	}

	const bool bOk = (Result == EOnJoinSessionCompleteResult::Success);
	OnJoinSessionComplete.Broadcast(bOk);

	if (!bOk || !Sessions.IsValid())
	{
		UE_LOG(LogTCNet, Warning, TEXT("JoinSession 실패: 결과=%d"), static_cast<int32>(Result));
		return;
	}

	// 세션 → 실제 접속 주소 해석 후 ClientTravel
	FString ConnectString;
	if (Sessions->GetResolvedConnectString(NAME_GameSession, ConnectString))
	{
		if (APlayerController* PC = GetFirstLocalPlayerController())
		{
			UE_LOG(LogTCNet, Log, TEXT("JoinSession 접속: %s"), *ConnectString);
			PC->ClientTravel(ConnectString, ETravelType::TRAVEL_Absolute);
		}
		else
		{
			UE_LOG(LogTCNet, Warning, TEXT("JoinSession: 로컬 PlayerController 없음"));
		}
	}
	else
	{
		UE_LOG(LogTCNet, Warning, TEXT("JoinSession: ConnectString 해석 실패"));
	}
}

void UTCGameInstance::DestroySteamSession()
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		return;
	}

	DestroySessionCompleteHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &UTCGameInstance::HandleDestroySessionComplete));

	Sessions->DestroySession(NAME_GameSession);
}

void UTCGameInstance::HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteHandle);
	}
	UE_LOG(LogTCNet, Log, TEXT("DestroySession 완료: %s (성공=%d)"), *SessionName.ToString(), bWasSuccessful);
	HostRoomCode.Reset();
}

// ── 콘솔 테스트 트리거 ──
static const TCHAR* TC_PROTO_MAP = TEXT("/Game/Prototype/L_FurnitureProto");

void UTCGameInstance::Steam_Host()
{
	UE_LOG(LogTCNet, Log, TEXT("[Exec] Steam_Host"));
	HostSteamSession(TC_PROTO_MAP, 4, false);
}

void UTCGameInstance::Steam_Find()
{
	UE_LOG(LogTCNet, Log, TEXT("[Exec] Steam_Find"));
	FindSteamSessions(false, 20);
}

void UTCGameInstance::Steam_Join()
{
	UE_LOG(LogTCNet, Log, TEXT("[Exec] Steam_Join (index 0)"));
	JoinFoundSession(0);
}

void UTCGameInstance::Steam_Leave()
{
	UE_LOG(LogTCNet, Log, TEXT("[Exec] Steam_Leave"));
	DestroySteamSession();
}

int32 UTCGameInstance::GetFoundSessionCount() const
{
	return SessionSearch.IsValid() ? SessionSearch->SearchResults.Num() : 0;
}

FString UTCGameInstance::GetFoundSessionName(int32 SearchResultIndex) const
{
	if (!SessionSearch.IsValid() || !SessionSearch->SearchResults.IsValidIndex(SearchResultIndex))
	{
		return FString();
	}

	const FOnlineSessionSearchResult& Result = SessionSearch->SearchResults[SearchResultIndex];

	// 광고된 맵 이름이 있으면 그것을, 없으면 세션 소유자명을 표시
	FString MapName;
	if (Result.Session.SessionSettings.Get(SETTING_MAPNAME, MapName) && !MapName.IsEmpty())
	{
		return MapName;
	}
	return Result.Session.OwningUserName;
}

FString UTCGameInstance::GetFoundSessionCode(int32 SearchResultIndex) const
{
	if (!SessionSearch.IsValid() || !SessionSearch->SearchResults.IsValidIndex(SearchResultIndex))
	{
		return FString();
	}

	FString RoomCode;
	SessionSearch->SearchResults[SearchResultIndex].Session.SessionSettings.Get(TC_ROOMCODE_KEY, RoomCode);
	return RoomCode;
}
