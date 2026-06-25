// TCGameInstance.cpp

#include "Network/Session/TCGameInstance.h"
#include "Network/Net/TCNetStatics.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

// 지정 맵을 listen 옵션으로 ServerTravel — 호스트가 서버 겸 클라이언트가 된다.
void UTCGameInstance::HostListenServer(const FString& MapName)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTCNet, Warning, TEXT("HostListenServer: World 없음"));
		return;
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
