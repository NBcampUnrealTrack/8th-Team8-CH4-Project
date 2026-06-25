// TCGameInstance.h

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "TCGameInstance.generated.h"

// 세션 골격 1차 — LAN/직접 IP 기반 리슨 서버.
// OnlineSubsystem(Steam) 매치메이킹은 후순위로 이 클래스에 확장한다.
UCLASS()
class TEAMCARRY_API UTCGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	// 리슨 서버로 호스트 — 지정 맵을 listen 모드로 오픈(호스트도 플레이)
	UFUNCTION(BlueprintCallable, Category = "TeamCarry|Session")
	void HostListenServer(const FString& MapName);

	// 직접 IP/주소로 접속(127.0.0.1, LAN IP 등)
	UFUNCTION(BlueprintCallable, Category = "TeamCarry|Session")
	void JoinByAddress(const FString& Address);
};
