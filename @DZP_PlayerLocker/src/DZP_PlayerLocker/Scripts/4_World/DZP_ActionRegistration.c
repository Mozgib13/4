// ============================================================================
// DZP_PlayerLocker — 4_World/DZP_ActionRegistration.c
//
// ОБЯЗАТЕЛЬНАЯ регистрация действий (п. 2.2): без modded ActionConstructor
// AddAction молча ничего не добавляет и действия не появятся в селекторе.
// Формат проверен по dayzexplorer 1.29 (actionconstructor.c:
//   void RegisterActions(TTypenameArray actions)) и по официальным примерам
//   BI (feedback.bistudio.com).
//
// Почему это отдельный файл-модификатор:
//   * ActionConstructor — ванидный класс, собирающий typename-реестр всех
//     действий мира на старте миссии;
//   * modded class + override RegisterActions расширяет реестр, не ломая
//     ванидные действия (super.RegisterActions обязателен);
//   * порядок внутри реестра не важен — ActionManagerBase ищет действие
//     по typename, а привязка к объекту идёт в SetActions() точки;
//   * без строк actions.Insert(...) вызов AddAction(DZP_...) в SetActions
//     найдёт пустой реестр и напечатает "Action ... doesn't exist!" в RPT.
// ============================================================================

modded class ActionConstructor
{
	override void RegisterActions(TTypenameArray actions)
	{
		super.RegisterActions(actions);
		actions.Insert(DZP_OpenStorageAction);
		actions.Insert(DZP_ExpandStorageAction);
	}
};
