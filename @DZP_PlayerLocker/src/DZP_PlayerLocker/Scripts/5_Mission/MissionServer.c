// ============================================================================
// DZP_PlayerLocker — 5_Mission/MissionServer.c
//
// Серверные хуки миссии (модифицируем только на сервере — #ifdef SERVER, п. 2.16):
//   * OnMissionStart     — создание менеджера, чтение/создание config.json,
//                          спавн невидимой точки (п. 1.6, 4.1);
//   * OnMissionFinish    — аварийное сохранение всех открытых файлов игроков;
//   * PlayerDisconnected — сохранение и закрытие ящика ДО super.PlayerDisconnected
//                          (сигнатура 1.29: PlayerBase, PlayerIdentity, string uid,
//                          uid = identity.GetId() — п. 2.13).
//
// Особенности порядка вызовов:
//   * PlayerDisconnected идёт до super, потому что super завершает сессию
//     игрока (логаут/таймаут) — после него PlayerBase может быть уже
//     недоступен для чтения инвентаря, а для автосейва нужен открытый ящик;
//   * OnMissionFinish вызывается у MissionBase на любом завершении миссии
//     (штатный рестарт миссии и graceful-stop сервера) — туда же попадают
//     незакрытые сеансы (игрок не вышел, сервер остановили);
//   * OnInit мод не использует: на нём мир ещё не готов к CreateObjectEx,
//     спавн точки идёт строго в OnMissionStart.
//
// Хуки объявлены как modded class — расширение без замены ванидного
// MissionServer; тело методов полностью помещено в #ifdef SERVER, чтобы
// на сервере мод не компилировать в клиентскую логику (п. 2.16).
// ============================================================================

#ifdef SERVER
modded class MissionServer
{
	override void OnMissionStart()
	{
		super.OnMissionStart();

		DZP_StorageManager manager = new DZP_StorageManager();
		manager.Init();
	}

	override void OnMissionFinish()
	{
		DZP_StorageManager manager = DZP_StorageManager.Get();
		if (manager)
			manager.SaveAllSessions("shutdown");

		super.OnMissionFinish();
	}

	override void PlayerDisconnected(PlayerBase player, PlayerIdentity identity, string uid)
	{
		DZP_StorageManager manager = DZP_StorageManager.Get();
		if (manager)
			manager.OnPlayerDisconnected(player, uid);

		super.PlayerDisconnected(player, identity, uid);
	}
}
#endif
