// ============================================================================
// DZP_PlayerLocker — 4_World/DZP_PlayerStorageData.c
//
// Данные одного игрока: $profile:PlayerLocker/data/<playerUID>.json
//   * slots — текущее число слотов (0 = ещё не выдано, берётся из конфига);
//   * items — содержимое личного ящика: classname, quantity (float), health.
//
// Формат JSON: JsonFileLoader<T> (проверен по dayzexplorer 1.29.163047,
// 3_game/tools/jsonfileloader.c). Сохранение: покупка, выход игрока,
// автосейв раз в 5 минут, закрытие ящика и остановка миссии.
// ============================================================================

// Один сохранённый предмет личного ящика.
class DZP_StoredItem
{
	string classname; // класс предмета, например "BandageDressing"
	float quantity;    // количество (GetQuantity() -> float); 0 — у предмета нет quantity
	float health;      // здоровье: GetHealth("", ""); -1 = неизвестно (не восстанавливать)
};

// Персональный файл игрока.
class DZP_PlayerStorageData
{
	int slots;                        // текущее число слотов игрока
	ref array<ref DZP_StoredItem> items; // содержимое ящика (плоский cargo)

	void DZP_PlayerStorageData()
	{
		items = new array<ref DZP_StoredItem>;
	}

	// Загрузка файла игрока; отсутствие файла — данные по умолчанию.
	static DZP_PlayerStorageData Load(string uid)
	{
		DZP_PlayerStorageData data = new DZP_PlayerStorageData;
		string path = DZP_PlayerLocker.GetPlayerDataPath(uid);

		if (FileExist(path))
		{
			JsonFileLoader<DZP_PlayerStorageData>.JsonLoadFile(path, data);
			DZP_PlayerLocker.Log("player data loaded: " + path);
		}

		if (!data.items)
			data.items = new array<ref DZP_StoredItem>;

		return data;
	}

	// Запись файла игрока (каталог data/ создаётся заранее).
	void Save(string uid)
	{
		DZP_PlayerLocker.EnsureDirectories();

		string path = DZP_PlayerLocker.GetPlayerDataPath(uid);
		JsonFileLoader<DZP_PlayerStorageData>.JsonSaveFile(path, this);
		DZP_PlayerLocker.Log("player data saved: " + path);
	}
};
