// ============================================================================
// DZP_PlayerLocker — 4_World/DZP_OpenStorageAction.c
//
// Действие «Открыть хранилище» (п. 1.2, 2.1):
//   * наследник ActionInteractBase;
//   * CreateConditionComponents: CCINone + CCTCursor (п. 2.1);
//   * конструктор задаёт m_Text, m_CommandUID = CMD_ACTIONMOD_INTERACTONCE,
//     m_StanceMask = STANCEMASK_ALL, m_HUDCursorIcon = CursorIcons.None
//     (CursorIcons.Default в 1.29 не существует — п. 2.1);
//   * OnExecuteServer — сервер спавнит/телепортирует личный ящик и шлёт ChatMP;
//   * OnExecuteClient — через ~800 мс (CallLater, 2 аргумента) открывает
//     штатный инвентарь DayZ: GetGame().GetMission().ShowInventory() (п. 2.5).
//
// Почему именно так:
//   * CCINone — предмет в руках не требуется, действие всегда доступно;
//   * CCTCursor — дистанция курсора UAMaxDistances.DEFAULT, работает и на
//     невидимой цели: луч курсора попадает в коллайдер точки;
//   * CMD_ACTIONMOD_INTERACTONCE — мгновенный интеракт без цикла удержания;
//   * задержка 800 мс — компромисс: сервер должен успеть разместить ящик и
//     клиент — получить netsync позиции, иначе секция «вокруг» откроется
//     пустой; ShowInventory() — публичный метод Mission (проверен в
//     missiongameplay.c:1153), единственный способ открыть штатный TAB;
//   * вызов ShowInventory идёт С клиентской стороны (OnExecuteClient):
//     инвентарь — UI-меню, оно существует только на клиенте (п. 4.4 ТЗ: TAB).
// ============================================================================

class DZP_OpenStorageAction : ActionInteractBase
{
	void DZP_OpenStorageAction()
	{
		m_Text = "Открыть хранилище";
		m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
		m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
		m_HUDCursorIcon = CursorIcons.None;
	}

	override void CreateConditionComponents()
	{
		m_ConditionItem = new CCINone;
		m_ConditionTarget = new CCTCursor;
	}

	// Действие видно только на точке мода и только при включённом конфиге.
	// Точка сетевая (netsync), состояние читается с клиента.
	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		DZP_LlockerPoint point = DZP_LlockerPoint.Cast(target.GetObject());
		if (!point)
			return false;

		return point.IsLockerEnabled();
	}

	// Сервер: найти/создать личный ящик, телепортировать его на 1.5 м
	// перед игроком, сообщить в чат (п. 4.2, 1.8 — сервер авторитетен).
	override void OnExecuteServer(ActionData data)
	{
#ifdef SERVER
		DZP_StorageManager manager = DZP_StorageManager.Get();
		if (manager)
			manager.OpenStorage(data.m_Player);
#endif
	}

	// Клиент: сервер уже разместил ящик — открываем штатный TAB-инвентарь.
	override void OnExecuteClient(ActionData data)
	{
		// CallLater(method, ms) — ровно два аргумента, биндится к this (п. 2.9)
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ShowLockerInventory, 800);
	}

	protected void ShowLockerInventory()
	{
		// Ящик появится в секции «вокруг», клик по нему открывает cargo слева
		GetGame().GetMission().ShowInventory();
	}
};
