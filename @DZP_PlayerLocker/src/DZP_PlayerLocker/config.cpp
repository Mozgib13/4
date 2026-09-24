// ============================================================================
// DZP_PlayerLocker — config.cpp
// Полная перезапись под DayZ 1.29 (build 1.29.163047).
//
// CfgPatches  — идентичность аддона и зависимости.
// CfgMods     — подключение скриптовых модулей 3_Game / 4_World / 5_Mission.
// CfgVehicles — DZP_LlockerPoint (невидимая точка) и DZP_StorageContainer
//               (личный ящик игрока, размерные классы 10..100 слотов).
//
// Методы Open/Close/m_IsOpened у Container_Base в 1.29 НЕ существует —
// инвентарь открывается только штатным Mission.ShowInventory().
//
// Зависимости requiredAddons:
//   DZ_Data           — базовые скрипты предметов (Inventory_Base и т.д.);
//   DZ_Gear_Camping   — класс WoodenCrate, на который наследуются все
//                       объекты мода (деревянный ящик: cargo, модель,
//                       allowOwnedCargoManipulation).
//
// CfgMods.defs.files[] — полные виртуальные пути в архиве: префикс PBO
// (свойство prefix записи «Vers») + путь записи внутри PBO. Селекторы
// действий не требуют отдельного модуля: они компилируются в 4_World и
// регистрируются через modded class ActionConstructor (DZP_ActionRegistration.c).
//
// Габариты личного ящика нельзя задать полем экземпляра — itemsCargoSize[]
// читается только из конфига класса, поэтому объявлены классы-контейнеры:
//
//   класс            itemsCargoSize   слотов   скрипт-класс
//   ----------------------------------------------------------------
//   DZP_LlockerBox_10   {5, 2}          10     DZP_StorageContainer
//   DZP_LlockerBox_20   {5, 4}          20     DZP_StorageContainer
//   DZP_LlockerBox_30   {6, 5}          30     DZP_StorageContainer
//   DZP_LlockerBox_40   {8, 5}          40     DZP_StorageContainer
//   DZP_LlockerBox_50   {10, 5}         50     DZP_StorageContainer
//   DZP_LlockerBox_60   {10, 6}         60     DZP_StorageContainer
//   DZP_LlockerBox_70   {10, 7}         70     DZP_StorageContainer
//   DZP_LlockerBox_80   {10, 8}         80     DZP_StorageContainer
//   DZP_LlockerBox_90   {10, 9}         90     DZP_StorageContainer
//   DZP_LlockerBox_100  {10, 10}       100     DZP_StorageContainer
//
// Скрипт-класс наследуется по цепочке конфигов — файл DZP_StorageContainer.c
// описывает поведение (невидимость, отсутствие действий), а не каждый класс.
// У точки cargo = {0, 0}: у неё нет полезного объёма, она только цель курсора.
// ============================================================================

class CfgPatches
{
	class DZP_PlayerLocker
	{
		units[] = {};
		weapons[] = {};
		requiredVersion = 0.1;
		requiredAddons[] = {"DZ_Data", "DZ_Gear_Camping"};
	};
};

class CfgMods
{
	class DZP_PlayerLocker
	{
		dir = "DZP_PlayerLocker";
		picture = "";
		action = "";
		hideName = 0;
		hidePicture = 1;
		name = "DZP Player Locker";
		credits = "";
		version = "1.1.0";
		author = "Mozgib13";
		extra = 0;
		type = "mod";
		dependencies[] = {"Game", "World", "Mission"};

		class defs
		{
			// Конфиг, общий конфиг-класс и логирование
			class gameScriptModule
			{
				value = "";
				files[] = {"DZP_PlayerLocker/scripts/3_game"};
			};

			// Точка, ящик, действия и серверный менеджер
			class worldScriptModule
			{
				value = "";
				files[] = {"DZP_PlayerLocker/scripts/4_world"};
			};

			// Хуки миссии: сервер (PlayerDisconnected) и клиент (ShowInventory)
			class missionScriptModule
			{
				value = "";
				files[] = {"DZP_PlayerLocker/scripts/5_mission"};
			};
		};
	};
};

class CfgVehicles
{
	// Штатный деревянный ящик (DZ_Gear_Camping) как база
	class WoodenCrate;

	// ------------------------------------------------------------------------
	// Невидимая точка-куб 1x1 в настраиваемых координатах ($profile:PlayerLocker/config.json).
	// cargo 0x0 — у точки нет полезного объёма, она служит только целью курсора
	// для действий «Открыть хранилище» и «Расширить хранилище на N».
	// Невидимость и бессмертность задаются скриптом (см. DZP_LlockerPoint.c).
	// ------------------------------------------------------------------------
	class DZP_LlockerPoint: WoodenCrate
	{
		scope = 2;
		displayName = "Locker Point";
		descriptionShort = "Invisible access point of the personal player locker.";
		model = "\DZ\gear\camping\wooden_case.p3d";
		inventorySlot[] = {};
		canBeDigged = 0;

		class Cargo
		{
			itemsCargoSize[] = {0, 0};
			openable = 0;
			allowOwnedCargoManipulation = 0;
		};
	};

	// ------------------------------------------------------------------------
	// Базовый класс личного ящика. Сам по себе не спавнится (scope = 0),
	// спавнятся только размерные классы DZP_LockerBox_<N> (кратно 10, 10..100),
	// потому что габарит cargo в DayZ задаётся конфигом класса, а не экземпляра.
	// ------------------------------------------------------------------------
	class DZP_StorageContainer: WoodenCrate
	{
		scope = 0;
		displayName = "Personal Locker";
		descriptionShort = "Personal player storage. Visible to the owner only.";
		model = "\DZ\gear\camping\wooden_case.p3d";
		inventorySlot[] = {};
		canBeDigged = 0;

		class Cargo
		{
			itemsCargoSize[] = {10, 1};
			openable = 0;
			allowOwnedCargoManipulation = 1;
		};
	};

	// Размерные классы ящика: произведение габаритов = число слотов игрока.
	// Скрипт-класс у всех — DZP_StorageContainer (наследование через конфиг).
	class DZP_LlockerBox_10: DZP_StorageContainer
	{
		scope = 2;
		displayName = "Personal Locker (10 slots)";
		class Cargo
		{
			itemsCargoSize[] = {5, 2};
			openable = 0;
			allowOwnedCargoManipulation = 1;
		};
	};

	class DZP_LlockerBox_20: DZP_StorageContainer
	{
		scope = 2;
		displayName = "Personal Locker (20 slots)";
		class Cargo
		{
			itemsCargoSize[] = {5, 4};
			openable = 0;
			allowOwnedCargoManipulation = 1;
		};
	};

	class DZP_LlockerBox_30: DZP_StorageContainer
	{
		scope = 2;
		displayName = "Personal Locker (30 slots)";
		class Cargo
		{
			itemsCargoSize[] = {6, 5};
			openable = 0;
			allowOwnedCargoManipulation = 1;
		};
	};

	class DZP_LlockerBox_40: DZP_StorageContainer
	{
		scope = 2;
		displayName = "Personal Locker (40 slots)";
		class Cargo
		{
			itemsCargoSize[] = {8, 5};
			openable = 0;
			allowOwnedCargoManipulation = 1;
		};
	};

	class DZP_LlockerBox_50: DZP_StorageContainer
	{
		scope = 2;
		displayName = "Personal Locker (50 slots)";
		class Cargo
		{
			itemsCargoSize[] = {10, 5};
			openable = 0;
			allowOwnedCargoManipulation = 1;
		};
	};

	class DZP_LlockerBox_60: DZP_StorageContainer
	{
		scope = 2;
		displayName = "Personal Locker (60 slots)";
		class Cargo
		{
			itemsCargoSize[] = {10, 6};
			openable = 0;
			allowOwnedCargoManipulation = 1;
		};
	};

	class DZP_LlockerBox_70: DZP_StorageContainer
	{
		scope = 2;
		displayName = "Personal Locker (70 slots)";
		class Cargo
		{
			itemsCargoSize[] = {10, 7};
			openable = 0;
			allowOwnedCargoManipulation = 1;
		};
	};

	class DZP_LlockerBox_80: DZP_StorageContainer
	{
		scope = 2;
		displayName = "Personal Locker (80 slots)";
		class Cargo
		{
			itemsCargoSize[] = {10, 8};
			openable = 0;
			allowOwnedCargoManipulation = 1;
		};
	};

	class DZP_LlockerBox_90: DZP_StorageContainer
	{
		scope = 2;
		displayName = "Personal Locker (90 slots)";
		class Cargo
		{
			itemsCargoSize[] = {10, 9};
			openable = 0;
			allowOwnedCargoManipulation = 1;
		};
	};

	class DZP_LlockerBox_100: DZP_StorageContainer
	{
		scope = 2;
		displayName = "Personal Locker (100 slots)";
		class Cargo
		{
			itemsCargoSize[] = {10, 10};
			openable = 0;
			allowOwnedCargoManipulation = 1;
		};
	};
};
