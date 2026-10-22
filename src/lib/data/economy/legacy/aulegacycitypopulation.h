#ifndef AuLegacyCityPopulation_H
#define AuLegacyCityPopulation_H

#include "audata.h"
#include <QByteArray>
#include <QString>
#include <array>
#include <cstddef>
#include <vector>

class AuLegacyCityCatalogue;

namespace AuLegacyCity
{
struct State;
enum class CitizenExpense : quint8 { Commission, Confiscated, Emigrate, Loans, Purchases, Rewards, Salary, Wholesale, Withdrawn, Count };
enum class CitizenIncome : quint8 { Immigrate, Loans, Mint, Salary, Sales, Wholesale, Count };
enum class GovernmentExpense : quint8 { Rewards, Research, Tribute, Withdrawal, Works, Count };
enum class GovernmentIncome : quint8 { Confiscated, Deposit, IncomeTax, Research, SalesTax, SubsidyTax, TaxLevy, Count };

struct TaxRates
{
	float income = 0, sales = 0, subsidy = 0;
	bool hasCapital = false;
};

// Original city-wide citizen bank and treasury. Counters cover the current
// report period; changing a balance always records the corresponding category.
struct AUDATA_DLL Accounts
{
	double citizens = 0, government = 0;
	std::array<double, std::size_t(CitizenExpense::Count)> citizenExpenses = {};
	std::array<double, std::size_t(CitizenIncome::Count)> citizenIncome = {};
	std::array<double, std::size_t(GovernmentExpense::Count)> governmentExpenses = {};
	std::array<double, std::size_t(GovernmentIncome::Count)> governmentIncome = {};
	void Credit(CitizenIncome category, double amount, const TaxRates& taxes);
	void Credit(GovernmentIncome category, double amount, bool hasCapital);
	double Debit(CitizenExpense category, double amount);
	double Debit(GovernmentExpense category, double amount);
	double BeginningCitizenBalance() const;
	double BeginningGovernmentBalance() const;
	double LoanFundsAvailable(int population) const;
	void ResetPeriod();
	void CirculateSalaries(int population, int jobs, const TaxRates& taxes);
};

// Original 0x271de0/0x2d0680 flag guard, flag/control/allegiance transfer and
// bank run using loyalty before the population cycle changes it. The caller
// selects native pirate allegiance first. Returns whether control changed;
// notifications and world entities are handled only after committing state.
AUDATA_DLL bool ApplyCitySurrender(State& state, Accounts& accounts, quint32 oldEmpire, quint32 newEmpire);

struct AccountingState
{
	Accounts accounts;
	TaxRates taxes; // Original city-cached percentages, refreshed at report time.
	quint32 powerRequested = 0;
	quint32 unemployed = 0;
	quint16 foodPerMeal = 0;
	qint64 lastPopulationCycle = 0;
	qint64 lastEconomyCycle = 0;
	double lastTribute = 0;
	bool tributeSentToday = false;
};

AUDATA_DLL bool EncodeAccounting(const AccountingState& state, QByteArray& output, QString& error);
AUDATA_DLL bool DecodeAccounting(const QByteArray& bytes, AccountingState& output, QString& error);

struct PopulationContext
{
	std::array<quint32, 256> levels = {}; // Historical GD industry IDs.
	int population = 0, unemployed = 0;
	int satisfaction = 1;
	int homes = 0, apartmentHomes = 0;
	bool needsEnclosure = false, hasPower = false, hasAir = false;
	quint32 airConsumed = 0;
	quint8 foodVariety = 0, ecoRole = 0;
	bool ateFullMeal = false;
	bool occupied = false, underAttack = false, isolated = false;
	bool indigenousWorld = false, differentNativeRace = false;
	bool destroyedBuildings = false;
	float loyalty = 1;
	int environmentCapacity = 0, environmentPopulation = 0;
	int decayedLevels = 0;
	int daysSinceSystemVisit = 0;
	int daysSinceCityVisit = 0;
	bool exemptFromVisitPenalty = false;
	bool reportExemptFromVisitPenalty = false;
	int townSquareTroops = 0, militaryFlagTroops = 0;
};

enum class MoraleFactorType : quint8
{
	HarshEnvironmentWithoutPower, HarshEnvironment, Suffocation, Gasping, Starvation, Hunger,
	FoodVariety, Loyalty, Rebellious, IncreasinglyDisloyal, Disloyalty, AlienInvader,
	ResourceZoneOverpopulation, Homelessness, Decay, Abandoned, Abandonment, DestroyedBuildings,
	CrampedLivingConditions, Unemployment, Overworked, Arena, Garrison, Cantina,
	Casino, Church, Grocery, Hospital, Park, PoliceStation, RetailStore, University, Zoo,
	NoCitizens, Count
};

struct MoraleFactor
{
	MoraleFactorType type = MoraleFactorType::NoCitizens;
	int modifier = 0, detail = 0;
};

struct PopulationCycleResult
{
	int population = 0, unemployed = 0, satisfaction = 1;
	int moraleChange = 0;
	int suffocated = 0, starved = 0, births = 0, immigrants = 0, emigrants = 0;
	int hospitalShortage = 0;
	std::vector<MoraleFactor> moraleFactors;
};

AUDATA_DLL int Homes(const AuLegacyCityCatalogue& catalogue, const std::array<quint32, 256>& levels,
	int* apartmentHomes = nullptr);
AUDATA_DLL PopulationCycleResult PopulationCycle(const PopulationContext& context);
// The original report calculated its own result. Its loyalty, abandonment and
// population rules differ from CityCycle; cantinas report the actual modifier.
AUDATA_DLL PopulationCycleResult PopulationReport(const PopulationContext& context);

struct WealthLevyContext
{
	int population = 0;
	quint32 loyalPopulation = 0;
	float fraction = 0;
	bool hasCapital = false, hasBank = false, occupied = false;
	bool underAttack = false, inArena = false, empireMatches = false;
	// Runtime Surrender performs a bank run between the bounty and final
	// withdrawal. Pure callers retain the original complete calculation.
	bool deferFinalWithdrawal = false;
	bool hasGovernors = true;
};

struct WealthLevyResult
{
	bool applied = false, revolution = false;
	quint32 loyalPopulation = 0;
	double confiscated = 0, withdrawn = 0, bounty = 0;
};

// The caller performs Surrender, pirate-allegiance selection, bounty posting
// and notifications when revolution is returned, before applying allegiance.
AUDATA_DLL WealthLevyResult LevyCitizenWealth(Accounts& accounts, const WealthLevyContext& context);
}

#endif
