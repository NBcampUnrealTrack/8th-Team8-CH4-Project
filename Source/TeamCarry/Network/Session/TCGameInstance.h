// TCGameInstance.h

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "TCGameInstance.generated.h"

// 세션 비동기 결과를 UI(BP)로 전달하는 이벤트.
// 호스트/검색/접속 버튼이 이 이벤트를 구독해 결과 화면을 갱신한다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTCSessionBoolEvent, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTCFindSessionsEvent, bool, bSuccess, int32, NumFound);

// 세션 골격 — LAN/직접 IP(레거시) + OnlineSubsystem(Steam) 매치메이킹.
// Steam 경로는 IOnlineSession 인터페이스 기반 비동기. 콜백에서 BP 이벤트로 결과 통지.
UCLASS()
class TEAMCARRY_API UTCGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	// ── 레거시: LAN / 직접 IP (OSS 미초기화 환경 폴백용으로 유지) ──

	// 리슨 서버로 호스트 — 지정 맵을 listen 모드로 오픈(호스트도 플레이)
	UFUNCTION(BlueprintCallable, Category = "TeamCarry|Session")
	void HostListenServer(const FString& MapName);

	// 직접 IP/주소로 접속(127.0.0.1, LAN IP 등)
	UFUNCTION(BlueprintCallable, Category = "TeamCarry|Session")
	void JoinByAddress(const FString& Address);

	// ── Steam OSS 세션 ──

	// 세션 생성 → 성공 시 지정 맵을 리슨 서버로 ServerTravel.
	// bLAN=true면 Steam 대신 NULL 서브시스템(같은 랜) 매치.
	UFUNCTION(BlueprintCallable, Category = "TeamCarry|Session|Steam")
	void HostSteamSession(const FString& MapName, int32 MaxPlayers = 4, bool bLAN = false);

	// 주변 TeamCarry 세션 검색 — 결과는 OnFindSessionsComplete 로 통지.
	UFUNCTION(BlueprintCallable, Category = "TeamCarry|Session|Steam")
	void FindSteamSessions(bool bLAN = false, int32 MaxSearchResults = 20);

	// 직전 검색 결과의 index 세션에 접속(접속 후 자동 ClientTravel).
	UFUNCTION(BlueprintCallable, Category = "TeamCarry|Session|Steam")
	void JoinFoundSession(int32 SearchResultIndex);

	// 현재 세션 파기(나가기/호스트 종료).
	UFUNCTION(BlueprintCallable, Category = "TeamCarry|Session|Steam")
	void DestroySteamSession();

	// ── 콘솔 테스트 트리거 (패키징 빌드에서 ` 콘솔로 호출) ──
	// 호스트: 테스트 맵을 Steam 세션으로 열고 리슨 서버로 이동
	UFUNCTION(Exec)
	void Steam_Host();
	// 주변 Steam 세션 검색
	UFUNCTION(Exec)
	void Steam_Find();
	// 검색 결과 0번 세션에 접속
	UFUNCTION(Exec)
	void Steam_Join();
	// 현재 세션 나가기/파기
	UFUNCTION(Exec)
	void Steam_Leave();

	// 검색된 세션 수(UI에서 목록 길이 조회용).
	UFUNCTION(BlueprintPure, Category = "TeamCarry|Session|Steam")
	int32 GetFoundSessionCount() const;

	// 검색 결과의 호스트 표시명(UI 목록 표기용).
	UFUNCTION(BlueprintPure, Category = "TeamCarry|Session|Steam")
	FString GetFoundSessionName(int32 SearchResultIndex) const;

	// ── 방 코드(Room Code) ──

	// 호스트가 광고 중인 방 코드. 생성 후 UI 표시·공유용. 호스트가 아니면 빈 문자열.
	UFUNCTION(BlueprintPure, Category = "TeamCarry|Session|Steam")
	FString GetHostRoomCode() const { return HostRoomCode; }

	// 검색 결과 세션에 광고된 방 코드(클라이언트 코드 매칭용). 없으면 빈 문자열.
	UFUNCTION(BlueprintPure, Category = "TeamCarry|Session|Steam")
	FString GetFoundSessionCode(int32 SearchResultIndex) const;

	// ── BP 바인딩용 결과 이벤트 ──
	UPROPERTY(BlueprintAssignable, Category = "TeamCarry|Session|Steam")
	FTCSessionBoolEvent OnCreateSessionComplete;

	UPROPERTY(BlueprintAssignable, Category = "TeamCarry|Session|Steam")
	FTCFindSessionsEvent OnFindSessionsComplete;

	UPROPERTY(BlueprintAssignable, Category = "TeamCarry|Session|Steam")
	FTCSessionBoolEvent OnJoinSessionComplete;

private:
	// 세션 인터페이스 핸들(없으면 유효하지 않은 포인터)
	IOnlineSessionPtr GetSessionInterface() const;

	// CreateSession 완료 후 이동할 맵
	FString PendingTravelMap;

	// 호스트가 이번 세션에 광고한 방 코드(6자리 A-Z0-9). 호스트 전용.
	FString HostRoomCode;

	// 6자리 방 코드 생성(혼동 문자 제외).
	static FString GenerateRoomCode();

	// 직전 검색 결과 보관(Join 시 참조)
	TSharedPtr<FOnlineSessionSearch> SessionSearch;

	// OSS 세션 콜백
	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);

	// 델리게이트 핸들(콜백 1회 후 해제)
	FDelegateHandle CreateSessionCompleteHandle;
	FDelegateHandle FindSessionsCompleteHandle;
	FDelegateHandle JoinSessionCompleteHandle;
	FDelegateHandle DestroySessionCompleteHandle;
};
