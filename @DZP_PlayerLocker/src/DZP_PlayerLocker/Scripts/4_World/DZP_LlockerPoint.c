// ============================================================================
// DZP_PlayerLocker — 4_World/DZP_LlockerPoint.c
//
// Невидимая точка-куб 1x1 в конфигурируемых координатах.
// Единственный объект, на котором висят действия мода:
//   * DZP_OpenStorageAction   — «Открыть хранилище» (F);
//   * DZP_ExpandStorageAction — «Расширить хранилище на N» (колесо мыши).
//
// Сетевая синхронизация (п. 2.12):
//   RegisterNetSyncVariableInt("field", min, max) + SetSynchDirty() —
//   шаг расширения и флаг enabled попадают к клиенту, чтобы GetText()
//   действия показывал актуальное значение из server-конфига.
//   Причина netsync: config.json читается ТОЛЬКО на сервере, клиенту же
//   нужно знать число в подписи wheel-пункта меню. Без синхронизации клиент
//   показал бы дефолтные «10» при любом конфиге.
//
// Конструктор регистрирует сетевые переменные ДО создания объекта
// (классический Enforce-ctor), ApplyConfig выставляет значения и вызывает
// SetSynchDirty — ванидный механизм рассылки изменения всем клиентам.
//
// Невидимость/бессмертность — см. EEInit (п. 2.10 и п. 1.1). Точка
// дополнительно не имеет inventorySlot[] (нельзя вставить в слот транспорта)
// и canBeDigged=0 (нельзя закопать — бурить/закапывать ей нечем).
// ============================================================================

class DZP_LlockerPoint : WoodenCrate
{
	protected int m_ExpandStep; // шаг расширения из config.json (слоты), netsync
	protected bool m_Enabled;    // config.enabled, netsync

	void DZP_LlockerPoint()
	{
		RegisterNetSyncVariableInt("m_ExpandStep", 0, 1000);
		RegisterNetSyncVariableBool("m_Enabled");
	}

	// Действия вешаются только здесь, через AddAction (п. 2.3), без super:
	// точку нельзя взять, выложить, разобрать или закопать.
	override void SetActions()
	{
		AddAction(DZP_OpenStorageAction);
		AddAction(DZP_ExpandStorageAction);
	}

	override void EEInit()
	{
		super.EEInit();

		SetAllowDamage(false);
		SetInvisible(true);
	}

	// Сервер: передать состояние конфига всем клиентам.
	void ApplyConfig(DZP_LlockerConfig config)
	{
		if (!GetGame().IsServer())
			return;

		m_ExpandStep = config.slots.step;
		m_Enabled = config.enabled;
		SetSynchDirty();
	}

	int GetExpandStep()
	{
		return m_ExpandStep;
	}

	bool IsLockerEnabled()
	{
		return m_Enabled;
	}
};
