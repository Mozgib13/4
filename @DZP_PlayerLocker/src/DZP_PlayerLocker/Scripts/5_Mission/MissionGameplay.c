// ============================================================================
// DZP_PlayerLocker — 5_Mission/MissionGameplay.c
//
// Клиентская половина мода (компилируется на сервере как пустая — #ifndef SERVER, п. 2.16).
//
// Открытие штатного инвентаря TAB в 1.29 выполняется через
//   GetGame().GetMission().ShowInventory()
// из DZP_OpenStorageAction.OnExecuteClient (через CallLater ~800 мс).
// Здесь мод НЕ рисует и НЕ подменяет UI: кастомных меню в 1.29 нет (п. 2.4),
// второй пункт селектора («Расширить хранилище на N») — стандартное wheel-меню.
// ============================================================================

#ifndef SERVER
modded class MissionGameplay
{
	// Точка присутствия клиентского мода. Вызовы идут в DZP_OpenStorageAction:
	// сервер переместил личный ящик -> клиент открывает ShowInventory().
	override void ShowInventory()
	{
		super.ShowInventory();
	}
}
#endif
