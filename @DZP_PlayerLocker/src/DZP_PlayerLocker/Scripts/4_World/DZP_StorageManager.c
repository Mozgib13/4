// ============================================================================
// DZP_PlayerLocker — 4_World/DZP_StorageManager.c
//
// Серверный менеджер личных хранилищ:
//   * спавн невидимой точки из config.json (на старте миссии);
//   * сеансы игроков (uid -> данные + открытый ящик);
//   * открытие: личный ящик на 1.5 м перед игроком + ChatMP (п. 4.2);
//   * покупка: максимум -> списание ровно pricePerStep единиц валюты ->
//     расширение ящика на step -> сохранение -> ChatMP успех/отказ (п. 4.3);
//   * автосейв раз в 5 минут (CallLater, CALL_CATEGORY_SYSTEM);
//   * автозакрытие ящика на дистанции >= 15 м (проверка раз в 2 секунды);
//   * сохранение при выходе игрока и при остановке миссии.
//
// Валюта (п. 4.3): ванильный SmallStone сложен в 1 штуку (нет varQuantity),
// поэтому количество считается как (HasQuantity() ? GetQuantity() : 1)
// на предмет, а списание — SetQuantity() для сложенных и ObjectDelete()
// поштучно для несложенных (критерий 6.4: списывает ровно N камней).
//
// Все вызовы игрока выполняются только на сервере (#ifdef SERVER — п. 2.16).
// ============================================================================

// Открытый сеанс одного игрока.
class DZP_LlockerSession
{
	string uid;
	PlayerBase player;
	ref DZP_PlayerStorageData data;
	DZP_StorageContainer box;
};

class DZP_StorageManager
{
	static const int AUTOSAVE_INTERVAL_MS = 300000; // 5 минут (п. 1.7)
	static const int WATCH_INTERVAL_MS = 2000;       // период проверки дистанции
	static const float AUTO_CLOSE_DISTANCE = 15.0;   // метров до автозакрытия (п. 4.4)

	protected static ref DZP_StorageManager s_Instance;

	protected DZP_LlockerConfig m_Config;
	protected DZP_LlockerPoint m_Point;
	protected ref array<ref DZP_LlockerSession> m_Sessions;

	static DZP_StorageManager Get()
	{
		return s_Instance;
	}

	void DZP_StorageManager()
	{
		s_Instance = this;
		m_Sessions = new array<ref DZP_LlockerSession>;
	}

	void ~DZP_StorageManager()
	{
		if (s_Instance == this)
			s_Instance = null;
	}

	// -------------------------------------------------------------- lifecycle
	// Вызывается из MissionServer.OnMissionStart (#ifdef SERVER — п. 2.16).
	// Порядок:
	//   1. чтение config.json (или автосоздание с умолчаниями — п. 1.6);
	//   2. при enabled=false точка не спавнится, таймеры не ставятся —
	//      действия мода физически недоступны игрокам;
	//   3. спавн DZP_LlockerPoint по координатам с ECE_PLACE_ON_SURFACE —
	//      объект опускается на рельеф (п. 4.1);
	//   4. запуск автосейва (5 мин) и наблюдения за дистанцией (2 с).
	void Init()
	{
#ifdef SERVER
		m_Config = DZP_PlayerLocker.LoadConfig();

		if (!m_Config.enabled)
		{
			DZP_PlayerLocker.Log("disabled in config.json, locker point will not spawn");
			return;
		}

		SpawnLockerPoint();
		StartTimers();
		DZP_PlayerLocker.Log("manager initialized");
#endif
	}

	protected void SpawnLockerPoint()
	{
		vector position = Vector(m_Config.pointX, m_Config.pointY, m_Config.pointZ);
		Object obj = GetGame().CreateObjectEx("DZP_LlockerPoint", position, ECE_PLACE_ON_SURFACE); // п. 2.7

		m_Point = DZP_LlockerPoint.Cast(obj);
		if (!m_Point)
		{
			DZP_PlayerLocker.Log("ERROR: failed to spawn DZP_LlockerPoint");
			return;
		}

		m_Point.SetAllowDamage(false); // п. 2.10
		m_Point.ApplyConfig(m_Config);  // netsync шага и флага enabled
		DZP_PlayerLocker.Log("locker point spawned");
	}

	protected void StartTimers()
	{
		// п. 2.9: CallLater(func, delay) — две сигнатуры без параметров
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(AutoSaveTick, AUTOSAVE_INTERVAL_MS);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(WatchTick, WATCH_INTERVAL_MS);
	}

	protected void AutoSaveTick()
	{
#ifdef SERVER
		// После рестарта миссии менеджер уже новый — старый таймер гаснет
		if (s_Instance != this)
			return;

		SaveAllSessions("autosave");
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(AutoSaveTick, AUTOSAVE_INTERVAL_MS);
#endif
	}

	// Закрыть ящик, если игрок отошёл на 15+ метров (п. 4.4).
	// Метод перевооружается сам: в конце тела CallLater ставит его в очередь
	// снова через WATCH_INTERVAL_MS — классический 2-аргументный CallLater
	// из п. 2.9 (без repeat=true, т.к. после OnMissionFinish очередь чистится).
	// Расстояние меряется до позиции ящика, а не до точки: ящик следует за
	// игроком на 1.5 м только при открытии, дальше он неподвижен.
	protected void WatchTick()
	{
#ifdef SERVER
		// После рестарта миссии менеджер уже новый — старый таймер гаснет
		if (s_Instance != this)
			return;

		for (int i = 0; i < m_Sessions.Count(); i++)
		{
			DZP_LlockerSession session = m_Sessions.Get(i);
			if (!session || !session.box || !session.player)
				continue;

			float distance = vector.Distance(session.player.GetPosition(), session.box.GetPosition());
			if (distance >= AUTO_CLOSE_DISTANCE)
			{
				CloseSessionBox(session, "too far");
				NotifyPlayer(session.player, "Хранилище закрыто: вы отошли слишком далеко.");
			}
		}

		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(WatchTick, WATCH_INTERVAL_MS);
#endif
	}

	// ---------------------------------------------------------------- open
	// П. 4.2: сервер создаёт личный ящик на 1.5 м перед игроком и шлёт ChatMP.
	// Штатный TAB-инвентарь открывает клиент (DZP_OpenStorageAction, ~800 мс).
	void OpenStorage(PlayerBase player)
	{
#ifdef SERVER
		if (!m_Config || !m_Config.enabled)
		{
			NotifyPlayer(player, "Хранилище отключено на сервере.");
			return;
		}

		DZP_LlockerSession session = GetOrCreateSession(player);
		if (!session)
			return;

		if (!session.box)
			CreateBoxAtPlayer(session, player);
		else
			session.box.SetPosition(BoxSpawnPosition(player));

		DZP_PlayerLocker.Log("storage opened for " + session.uid);
		NotifyPlayer(player, "Хранилище открыто. Ящик — в секции «вокруг» (TAB).");
#endif
	}

	// --------------------------------------------------------------- expand
	// П. 4.3: максимум -> валюта -> новый размер -> сохранение -> ChatMP.
	// Порядок проверок важен: сначала лимит (не списываем деньги зря),
	// затем валюта (точно pricePerStep единиц), только потом изменение слотов.
	// Если игрок закрыл ящик, покупка всё равно валидна — увеличатся слоты
	// в data.items и при следующем открытии создастся ящик нового габарита.
	void ExpandStorage(PlayerBase player)
	{
#ifdef SERVER
		if (!m_Config || !m_Config.enabled)
		{
			NotifyPlayer(player, "Хранилище отключено на сервере.");
			return;
		}

		DZP_LlockerSession session = GetOrCreateSession(player);
		if (!session)
			return;

		int currentSlots = session.data.slots;
		if (currentSlots <= 0)
			currentSlots = m_Config.slots.initial;

		if (currentSlots >= m_Config.slots.max)
		{
			NotifyPlayer(player, string.Format("Достигнут максимум хранилища: %1 слотов.", m_Config.slots.max));
			DZP_PlayerLocker.Log("expand refused (max) for " + session.uid);
			return;
		}

		int price = m_Config.pricePerStep;
		string currency = m_Config.currencyItem;

		string chargeError;
		if (!ChargeCurrency(player, price, currency, chargeError))
		{
			NotifyPlayer(player, "Недостаточно валюты: " + chargeError);
			DZP_PlayerLocker.Log("expand refused (currency) for " + session.uid + ": " + chargeError);
			return;
		}

		int newSlots = currentSlots + m_Config.slots.step;
		if (newSlots > m_Config.slots.max)
			newSlots = m_Config.slots.max;

		session.data.slots = newSlots;

		// Ящик открыт — пересоздать под больший габарит, сохранив содержимое.
		// Габарит cargo задаётся конфигом класса, поэтому нужен другой класс.
		if (session.box)
		{
			vector boxPosition = session.box.GetPosition();
			CaptureBoxContents(session);
			GetGame().ObjectDelete(session.box); // п. 2.7
			session.box = DZP_StorageContainer.Cast(GetGame().CreateObjectEx(BoxClassName(newSlots), boxPosition, ECE_PLACE_ON_SURFACE));

			if (session.box)
			{
				session.box.SetAllowDamage(false);
				RestoreItems(session);
			}
			else
			{
				// Предметы остаются в data.items — вернутся при следующем открытии
				DZP_PlayerLocker.Log("ERROR: failed to recreate box for " + session.uid);
			}
		}

		SaveSession(session, "purchase");
		NotifyPlayer(player, string.Format("Хранилище расширено: %1/%2 слотов. Списано: %3 x %4.",
			newSlots, m_Config.slots.max, price, currency));
		DZP_PlayerLocker.Log("expanded " + session.uid + ": " + currentSlots + " -> " + newSlots
			+ ", paid " + price + " " + currency);
#endif
	}

	// -------------------------------------------------------------- sessions
	protected DZP_LlockerSession GetOrCreateSession(PlayerBase player)
	{
		if (!player)
			return null;

		PlayerIdentity identity = player.GetIdentity();
		if (!identity)
			return null;

		// П. 2.13: UID игрока
		string uid = identity.GetId();
		if (uid == "")
			return null;

		DZP_LlockerSession session = FindSession(uid);
		if (session)
		{
			session.player = player;
			return session;
		}

		session = new DZP_LlockerSession;
		session.uid = uid;
		session.player = player;
		session.data = DZP_PlayerStorageData.Load(uid);

		// Первое открытие: выдать начальное число слотов из конфига (п. 1.4)
		if (session.data.slots <= 0)
			session.data.slots = m_Config.slots.initial;

		// Привести к кратности 10 — иначе BoxClassName() укажет на
		// несуществующий класс DZP_LlockerBox_N (см. config.cpp)
		session.data.slots = DZP_PlayerLocker.SnapSlots(session.data.slots);

		if (session.data.slots > m_Config.slots.max)
			session.data.slots = m_Config.slots.max;

		m_Sessions.Insert(session);
		DZP_PlayerLocker.Log("session created for " + uid + ", slots=" + session.data.slots);
		return session;
	}

	protected DZP_LlockerSession FindSession(string uid)
	{
		for (int i = 0; i < m_Sessions.Count(); i++)
		{
			if (m_Sessions.Get(i).uid == uid)
				return m_Sessions.Get(i);
		}

		return null;
	}

	// П. 4.4 / 1.7: сохранение файла игрока (содержимое берётся из открытого ящика).
	void SaveAllSessions(string reason)
	{
#ifdef SERVER
		for (int i = 0; i < m_Sessions.Count(); i++)
			SaveSession(m_Sessions.Get(i), reason);
#endif
	}

	// Вызывается из MissionServer.PlayerDisconnected до super (п. 2.13).
	void OnPlayerDisconnected(PlayerBase player, string uid)
	{
#ifdef SERVER
		DZP_LlockerSession session = FindSession(uid);
		if (!session)
			return;

		SaveSession(session, "logout");

		if (session.box)
		{
			GetGame().ObjectDelete(session.box);
			session.box = null;
		}

		m_Sessions.RemoveItem(session);
		DZP_PlayerLocker.Log("session closed (logout): " + uid);
#endif
	}

	protected void SaveSession(DZP_LlockerSession session, string reason)
	{
		if (!session || !session.data)
			return;

		CaptureBoxContents(session);
		session.data.Save(session.uid);
		DZP_PlayerLocker.Log("saved " + session.uid + " (" + reason + "), slots=" + session.data.slots
			+ ", items=" + session.data.items.Count());
	}

	protected void CloseSessionBox(DZP_LlockerSession session, string reason)
	{
		if (!session || !session.box)
			return;

		SaveSession(session, reason);
		GetGame().ObjectDelete(session.box);
		session.box = null;
		DZP_PlayerLocker.Log("locker box closed for " + session.uid + " (" + reason + ")");
	}

	// ------------------------------------------------------------ box helpers
	// Позиция ящика: ровно 1.5 м перед лицом игрока (п. 4.2).
	protected vector BoxSpawnPosition(PlayerBase player)
	{
		return player.GetPosition() + player.GetDirection() * 1.5;
	}

	// Класс ящика соответствует числу слотов (DZP_LlockerBox_10 .. _100).
	protected string BoxClassName(int slots)
	{
		return string.Format("DZP_LlockerBox_%1", slots);
	}

	protected void CreateBoxAtPlayer(DZP_LlockerSession session, PlayerBase player)
	{
		session.box = DZP_StorageContainer.Cast(
			GetGame().CreateObjectEx(BoxClassName(session.data.slots), BoxSpawnPosition(player), ECE_PLACE_ON_SURFACE));

		if (!session.box)
		{
			DZP_PlayerLocker.Log("ERROR: failed to create locker box for " + session.uid);
			return;
		}

		session.box.SetAllowDamage(false);
		RestoreItems(session);
	}

	// ------------------------------------------------------- cargo <-> file
	// Загрузка предметов из data.items в пустой ящик (CreateInInventory, п. 2.11).
	protected void RestoreItems(DZP_LlockerSession session)
	{
		if (!session.box || !session.data)
			return;

		for (int i = 0; i < session.data.items.Count(); i++)
		{
			DZP_StoredItem stored = session.data.items.Get(i);
			if (!stored || stored.classname == "")
				continue;

			EntityAI created = session.box.GetInventory().CreateInInventory(stored.classname);
			if (!created)
			{
				DZP_PlayerLocker.Log("WARNING: no cargo space for " + stored.classname + " (uid " + session.uid + ")");
				continue;
			}

			ItemBase item = ItemBase.Cast(created);
			if (!item)
				continue;

			// quantity — float (п. 2.11); восстанавливаем только у сложенных
			if (stored.quantity > 0 && item.HasQuantity())
				item.SetQuantity(stored.quantity);

			// health = -1 — значение неизвестно (старый/рукописный файл)
			if (stored.health >= 0)
				item.SetHealth("", "", stored.health);
		}
	}

	// Снять содержимое ящика в data.items (только прямой cargo ящика, п. 2.11).
	protected void CaptureBoxContents(DZP_LlockerSession session)
	{
		if (!session.box || !session.data)
			return;

		session.data.items.Clear();

		CargoBase cargo = session.box.GetInventory().GetCargo();
		if (!cargo)
			return;

		for (int i = 0; i < cargo.GetItemCount(); i++)
		{
			ItemBase item = ItemBase.Cast(cargo.GetItem(i));
			if (!item)
				continue;

			DZP_StoredItem stored = new DZP_StoredItem;
			stored.classname = item.GetType();
			stored.quantity = item.GetQuantity();
			stored.health = item.GetHealth("", "");
			session.data.items.Insert(stored);
		}
	}

	// -------------------------------------------------------------- currency
	// Списать ровно price единиц currency у игрока.
	// Подсчёт: HasQuantity() ? GetQuantity() : 1 — покрывает и сложенные
	// предметы, и штучные SmallStone (п. 2.11, критерий 6.4).
	//
	// Важно про ванильный SmallStone (gear_consumables config.cpp, класс
	// Inventory_Base): varQuantity* у него НЕ заданы, т.е. GetQuantity()==0,
	// HasQuantity()==false — один камень это одна единица валюты. Поэтому
	// списание для несложенных идёт ObjectDelete() по одному камню, а для
	// предметов с quantity (если admin сменит currencyItem в конфиге) —
	// SetQuantity(остаток), и ObjectDelete() только когда остаток <= 0.
	//
	// EnumerateInventory(PREORDER) обходит весь инвентарь: руки, одежда,
	// контейнеры вложенные — камни из рюкзака списываются до карманов,
	// порядок обхода детерминирован (п. 2.11).
	protected bool ChargeCurrency(PlayerBase player, int price, string currency, out string error)
	{
		error = "";

		if (price <= 0)
			return true;

		array<EntityAI> found = new array<EntityAI>;
		player.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, found); // п. 2.11

		float available = 0;
		for (int i = 0; i < found.Count(); i++)
		{
			ItemBase item = ItemBase.Cast(found.Get(i));
			if (!item || item.GetType() != currency)
				continue;

			if (item.HasQuantity())
				available += item.GetQuantity();
			else
				available += 1;
		}

		if (available < price)
		{
			error = string.Format("нужно %1 x %2, найдено %3.", price, currency, Math.Round(available));
			return false;
		}

		// Списание ровно price единиц: сложенные уменьшаются SetQuantity(),
		// штучные удаляются поштучно ObjectDelete() (п. 2.7).
		float need = price;
		for (int j = 0; j < found.Count() && need > 0; j++)
		{
			ItemBase currencyItem = ItemBase.Cast(found.Get(j));
			if (!currencyItem || currencyItem.GetType() != currency)
				continue;

			if (currencyItem.HasQuantity())
			{
				float quantity = currencyItem.GetQuantity();
				if (quantity <= 0)
					continue;

				float take = quantity;
				if (need < take)
					take = need;

				float newQuantity = quantity - take;
				need = need - take;

				if (newQuantity <= 0)
					GetGame().ObjectDelete(currencyItem);
				else
					currencyItem.SetQuantity(newQuantity);
			}
			else
			{
				need = need - 1;
				GetGame().ObjectDelete(currencyItem);
			}
		}

		return true;
	}

	// П. 1.9: уведомления в чат через ChatMP(..., "colorAction") (п. 2.8).
	protected void NotifyPlayer(PlayerBase player, string text)
	{
		if (player)
			GetGame().ChatMP(player, text, "colorAction");
	}
};
