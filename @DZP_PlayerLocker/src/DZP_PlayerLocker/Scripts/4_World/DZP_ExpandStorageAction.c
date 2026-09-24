// ============================================================================
// DZP_PlayerLocker — 4_World/DZP_ExpandStorageAction.c
//
// Действие «Расширить хранилище на N» (п. 1.3, 2.1):
//   * выбор колесом мыши в штатном селекторе действий DayZ 1.29
//     (кастомных UI-меню в 1.29 не существует — п. 2.4);
//   * наследник ActionInteractBase, CCINone + CCTCursor;
//   * m_Text обновляется в ActionCondition из netsync-поля точки,
//     поэтому в HUD всегда отражается шаг из config.json;
//   * покупка выполняется только на сервере (OnExecuteServer),
//     результат приходит игроку через ChatMP.
//
// Wheel-селектор: оба действия точки участвуют в стандартном селекторе —
// основное действие привязано к F (CMD_ACTIONMOD_INTERACTONCE), второе
// доступно прокруткой колеса, без отрисовки собственных меню (п. 2.4).
// Отсутствие vanilla-сетки у точки (пустой SetActions) гарантирует, что
// в колесе будут ровно эти пункты мода и ничего лишнего (не «Взять»,
// не «Выбросить» — см. DZP_LlockerPoint.c).
//
// Отказы (максимум/валюта) НЕ блокируют показ действия — сервер отправляет
// ChatMP с причиной, что быстрее и понятнее, чем прятать пункт в меню
// (критерии 6.4: отказ через ChatMP с причиной).
// ============================================================================

class DZP_ExpandStorageAction : ActionInteractBase
{
	void DZP_ExpandStorageAction()
	{
		m_Text = "Расширить хранилище на 10";
		m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
		m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
		m_HUDCursorIcon = CursorIcons.None;
	}

	override void CreateConditionComponents()
	{
		m_ConditionItem = new CCINone;
		m_ConditionTarget = new CCTCursor;
	}

	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		DZP_LlockerPoint point = DZP_LlockerPoint.Cast(target.GetObject());
		if (!point || !point.IsLockerEnabled())
			return false;

		// Шаг синхронизируется сервером (RegisterNetSyncVariableInt)
		m_Text = string.Format("Расширить хранилище на %1", point.GetExpandStep());
		return true;
	}

	// Сервер авторитетен: проверка максимума, списание валюты, запись файла.
	override void OnExecuteServer(ActionData data)
	{
#ifdef SERVER
		DZP_StorageManager manager = DZP_StorageManager.Get();
		if (manager)
			manager.ExpandStorage(data.m_Player);
#endif
	}
};
