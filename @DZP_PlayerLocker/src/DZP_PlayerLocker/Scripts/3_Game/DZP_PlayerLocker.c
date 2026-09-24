// ============================================================================
// DZP_PlayerLocker — 3_Game/DZP_PlayerLocker.c
//
// Общие классы конфигурации мода и сервисные функции:
//   * DZP_SlotsConfig    — раздел "slots" конфига (initial / step / max);
//   * DZP_LlockerConfig  — $profile:PlayerLocker/config.json;
//   * DZP_PlayerLocker   — пути, создание каталогов, чтение/запись конфига,
//                          логирование в server RPT с префиксом [DZP_PlayerLocker].
//
// Все API проверены по DayZ Explorer (build 1.29.163047):
//   JsonFileLoader<T>.JsonLoadFile / JsonSaveFile (3_game/tools/jsonfileloader.c),
//   FileExist, MakeDirectory (proto native bool MakeDirectory(string)).
//
// Гарантии модуля 3_Game:
//   * модуль компилируется и на сервере, и на клиенте — конфиг-классы
//     должны собираться без серверных вызовов (CreateObjectEx, ChatMP здесь
//     не используются);
//   * MakeDirectory принимает только существующего родителя — порядок
//     EnsureDirectories(): сначала $profile:PlayerLocker, потом .../data;
//   * JsonFileLoader пишет pretty-JSON: файл config.json можно править руками
//     без потери структуры;
//   * санитайзер выполняется ВСЕГДА после чтения — кривые значения админа
//     (step=7, max=1000, отрицательная цена) не ломают классы ящиков и CE.
//
// ВАЖНО (правило 1.29): конкатенация string + bool НЕ компилируется —
// для булевых значений используется только тернарник (b ? "true" : "false").
// Числа в строку безопасно собирать string.Format("%1", value).
// ============================================================================

// Раздел "slots" конфига: старт, шаг покупки и максимум числа слотов.
class DZP_SlotsConfig
{
	int initial = 10; // стартовое число слотов нового игрока
	int step = 10;    // на сколько слотов расширяется одна покупка
	int max = 100;    // потолок числа слотов
};

// Конфиг мода. При первом старте сервера создаётся с этими значениями по умолчанию.
class DZP_LlockerConfig
{
	bool enabled = true;             // включить ли мод (точка и действия)
	float pointX = 7500;             // координаты невидимой точки: X
	float pointY = 500;              // координаты невидимой точки: Y (высота, объект опускается на поверхность)
	float pointZ = 7500;             // координаты невидимой точки: Z
	string currencyItem = "SmallStone"; // предмет-валюта
	int pricePerStep = 1;            // цена одного расширения (1 шаг = slots.step слотов)
	ref DZP_SlotsConfig slots;       // initial / step / max

	void DZP_LlockerConfig()
	{
		slots = new DZP_SlotsConfig;
	}
};

// Статический сервис мода: каталоги, конфиг, логи.
class DZP_PlayerLocker
{
	static const string LOG_PREFIX = "[DZP_PlayerLocker] ";

	// $profile:PlayerLocker/config.json и $profile:PlayerLocker/data/<uid>.json
	static const string CONFIG_DIR = "$profile:PlayerLocker";
	static const string DATA_DIR = "$profile:PlayerLocker/data";

	static ref DZP_LlockerConfig s_Config;

	// Лог в server RPT (Print уходит в .RPT на сервере).
	static void Log(string message)
	{
		Print(LOG_PREFIX + message);
	}

	static string GetConfigPath()
	{
		return CONFIG_DIR + "/config.json";
	}

	static string GetPlayerDataPath(string uid)
	{
		return DATA_DIR + "/" + uid + ".json";
	}

	// Создать $profile:PlayerLocker и $profile:PlayerLocker/data (порядок: родитель -> потомок).
	static void EnsureDirectories()
	{
		if (!FileExist(CONFIG_DIR))
			MakeDirectory(CONFIG_DIR);

		if (!FileExist(DATA_DIR))
			MakeDirectory(DATA_DIR);
	}

	// Чтение конфига. Файла нет — создаётся файл с умолчаниями (критерий 6.2).
	static DZP_LlockerConfig LoadConfig()
	{
		EnsureDirectories();

		DZP_LlockerConfig config = new DZP_LlockerConfig;
		string path = GetConfigPath();

		if (FileExist(path))
		{
			JsonFileLoader<DZP_LlockerConfig>.JsonLoadFile(path, config);
			Log("config.json loaded: " + path);
		}
		else
		{
			JsonFileLoader<DZP_LlockerConfig>.JsonSaveFile(path, config);
			Log("config.json not found, created with defaults: " + path);
		}

		SanitizeConfig(config);
		s_Config = config;
		return config;
	}

	// Приведение значений конфига к допустимому виду:
	// слоты кратны 10 и лежат в 10..100 (размерные классы ящика), цена >= 0.
	static void SanitizeConfig(DZP_LlockerConfig config)
	{
		if (!config.slots)
			config.slots = new DZP_SlotsConfig;

		config.slots.initial = SnapSlots(config.slots.initial);
		config.slots.step = SnapSlots(config.slots.step);
		config.slots.max = SnapSlots(config.slots.max);

		if (config.slots.initial > config.slots.max)
			config.slots.initial = config.slots.max;

		if (config.pricePerStep < 0)
			config.pricePerStep = 0;

		if (config.currencyItem == "")
			config.currencyItem = "SmallStone";

		Log("config: enabled=" + (config.enabled ? "true" : "false")
			+ ", point=[" + config.pointX + " " + config.pointY + " " + config.pointZ + "]"
			+ ", currency=" + config.currencyItem
			+ ", pricePerStep=" + config.pricePerStep
			+ ", slots initial=" + config.slots.initial
			+ " step=" + config.slots.step
			+ " max=" + config.slots.max);
	}

	// Округление до кратного 10 в диапазоне 10..100.
	static int SnapSlots(int value)
	{
		int snapped = ((value + 5) / 10) * 10;

		if (snapped < 10)
			return 10;

		if (snapped > 100)
			return 100;

		return snapped;
	}
};
