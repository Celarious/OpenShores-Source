#ifndef AuLegacyCityProtocol_H
#define AuLegacyCityProtocol_H

#include "audata.h"
#include "aulegacycitystate.h"
#include <QByteArray>
#include <QString>
#include <QVector>

// Legacy gameplay has its own interface and commands. Architect fixture actions
// retain their original numbers and their separate server authorization.
namespace AuLegacyCityProtocol
{
constexpr int MaximumRequestBytes = 1024;
constexpr int MaximumReplyBytes = 512 * 1024;
constexpr int MaximumEntries = 256;
constexpr int MaximumRecipeChoices = 526;
// Recovered online editors use the existing empty design environment for
// avatar isolation. Keep the original marker name for serialized studio IDs.
constexpr quint16 PrivateStudioIdBase = 0x8000;
constexpr float MinimumRoadLengthMeters = 1, MaximumRoadLengthMeters = 500;
constexpr float MinimumBiodomeRadiusMeters = 47.74f, MaximumBiodomeRadiusMeters = 803.74f;
// Keep the retired action's number reserved so stale clients cannot reinterpret
// a door request as removal or another mutation.
// The version-1 door boolean/count byte positions also remain reserved in the codec.
enum Action : quint8
{
	List = 0, Spawn = 1, Update = 2, RetiredDoors = 3, Remove = 4, Route = 5,
	ConstructionList = 6, Build = 7, Extend = 8, Halt = 9, Resume = 10,
	Contribute = 11, Demolish = 12, ClearLand = 13, FetchMaterials = 14, EmployCitizen = 15, ChangeRoad = 16,
	EconomyList = 17, SetIndustryProcess = 18, FetchIndustryMaterials = 19, RunIndustryProcess = 20,
	BankState = 21, GovernmentDeposit = 22, GovernmentWithdraw = 23, BankBorrow = 24, BankRepay = 25,
	SetEmpireCapital = 26, BankClaimReward = 27,
	StudioState = 28, EnterSpacecraftStudio = 29,
	// TEMPORARY ARCHITECT MANUFACTURING TEST: remove after testing; reserve 30 afterwards.
	TestingFinishIndustryProcess = 30
};
inline bool IsEconomy(Action action) { return (action >= EconomyList && action <= RunIndustryProcess)
	|| action == TestingFinishIndustryProcess; } // TEMPORARY ARCHITECT MANUFACTURING TEST
inline bool IsBank(Action action) { return action >= BankState && action <= BankClaimReward; }
inline bool IsStudio(Action action) { return action >= StudioState && action <= EnterSpacecraftStudio; }
inline bool IsGameplay(Action action) { return (action >= ConstructionList && action <= EnterSpacecraftStudio)
	|| action == TestingFinishIndustryProcess; } // TEMPORARY ARCHITECT MANUFACTURING TEST
struct Request
{
	Action action = List;
	quint32 token = 0, cityId = 0, developmentId = 0, designId = 0;
	quint16 material = 76;
	qint8 minimumDeck = 0, maximumDeck = 0;
	bool constructed = true;
	float distanceMeters = 40, headingDegrees = 0, elevationMeters = 0;
	float targetX = 0, targetY = 0, targetZ = 4; // Building-local feet.
	quint8 industryId = 0, processId = 0, levels = 1;
	float roadLengthMeters = 10, roadWidthMeters = 10;
	bool roadSlab = false;
	float biodomeRadiusMeters = 100;
	bool citizenEnabled = true;
	QString cityName;
	bool explicitLocation = false;
	quint32 worldId = 0;
	float latitudeRadians = 0, longitudeRadians = 0;
	// Manufacturing IDs are independent of the byte-sized construction IDs.
	quint16 manufacturingRecipe = 0, processSlot = 0;
	quint8 minimumTechnology = 0;
	double money = 0;
	quint8 studioIndex = 0;
	QString studioPassword;
};
struct Entry
{
	quint32 cityId = 0, developmentId = 0, designId = 0;
	quint16 material = 76;
	qint8 minimumDeck = 0, maximumDeck = 0;
	bool constructed = true;
	float elevationMeters = 0, headingDegrees = 0;
	QString name;
	quint8 industryId = 0, processId = 0, completedLevels = 0, targetLevels = 0;
	bool halted = false, citizenEnabled = false;
	quint32 siteLaborRemaining = 0, levelLaborRemaining = 0;
	QString materials;
	QVector<AuLegacyCity::ConstructionComponent> components;
};
struct Reply
{
	Action action = List;
	quint32 token = 0, selectedCity = 0, selectedDevelopment = 0;
	QString message;
	QVector<Entry> entries;
	bool hasList = false;
	bool truncated = false;
	// A valid labor command can be accepted while waiting for materials.
	bool accepted = false;
	// Only an authorized, occupied city/development economy response supplies
	// these fields. Never append city inventory/accounts to public site lists.
	bool hasEconomy = false;
	quint8 economyIndustry = 0, economyTechnology = 0, economyLevels = 0;
	quint32 economyEmployees = 0;
	QByteArray production;
	bool hasRecipeChoices = false; // Version-5 selected-site reply; older replies leave the list unrestricted.
	QVector<quint16> recipeChoices; // Historical process IDs, in GD order.
	bool hasBank = false;
	double bankCitizenBalance = 0, bankGovernmentBalance = 0, bankLoanFunds = 0;
	double bankPersonalBalance = 0, bankPersonalLoan = 0, bankCreditAvailable = 0, bankPersonalReward = 0;
	bool bankGovernmentAuthorized = false, bankCapitalAuthorized = false, bankHasCapital = false, bankIsCapital = false;
	QByteArray bankAccounting;
	bool hasStudio = false;
	quint8 studioCount = 0;
	quint16 occupiedStudios = 0;
};
AUDATA_DLL QByteArray EncodeRequest(const Request& request);
AUDATA_DLL bool DecodeRequest(const QByteArray& bytes, Request& request);
AUDATA_DLL QByteArray EncodeReply(const Reply& reply);
AUDATA_DLL bool DecodeReply(const QByteArray& bytes, Reply& reply);
}
#endif
