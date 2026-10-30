#include "aulegacycityprotocol.h"
#include <QDataStream>
#include <QIODevice>
#include <QSet>
#include <cmath>

namespace
{
using namespace AuLegacyCityProtocol;
bool Valid(const Request& r)
{
	// TEMPORARY ARCHITECT MANUFACTURING TEST: action 30 uses the existing economy envelope.
	if(!r.token || r.action > TestingFinishIndustryProcess || (IsStudio(r.action) && (r.studioIndex >= 16
		|| r.studioPassword.size() > 128))) return false;
	if(r.action != List && r.action != Spawn && r.action != ConstructionList && r.action != Build
		&& r.action != EconomyList && r.action != BankState
		&& !(r.action == ClearLand && r.explicitLocation)
		&& (!r.cityId || !r.developmentId)) return false;
	if(IsEconomy(r.action) && (r.processSlot >= 32 || r.minimumTechnology > 31
		|| (r.action == SetIndustryProcess && !r.manufacturingRecipe))) return false;
	if(!std::isfinite(r.money) || r.money < 0
		|| (IsBank(r.action) && r.action != BankState && r.action != SetEmpireCapital
			&& r.action != BankClaimReward && r.money <= 0)) return false;
	if(IsGameplay(r.action) && (!r.levels || r.levels > 32 || r.cityName.size() > 128
		|| !std::isfinite(r.roadLengthMeters) || r.roadLengthMeters < MinimumRoadLengthMeters || r.roadLengthMeters > MaximumRoadLengthMeters
		|| !std::isfinite(r.roadWidthMeters) || r.roadWidthMeters < 1 || r.roadWidthMeters > 80)) return false;
	if(IsGameplay(r.action) && (!std::isfinite(r.biodomeRadiusMeters)
		|| r.biodomeRadiusMeters < MinimumBiodomeRadiusMeters || r.biodomeRadiusMeters > MaximumBiodomeRadiusMeters)) return false;
	if(r.explicitLocation && (!IsGameplay(r.action) || !r.worldId
		|| !std::isfinite(r.latitudeRadians) || std::abs(r.latitudeRadians) > 1.5707964f
		|| !std::isfinite(r.longitudeRadians) || std::abs(r.longitudeRadians) > 3.1415927f)) return false;
	if(r.action == Spawn || r.action == Update)
	{
		if(!r.designId || r.designId > 74 || r.minimumDeck < -127 || r.maximumDeck > 126
			|| r.minimumDeck > r.maximumDeck || int(r.maximumDeck) - int(r.minimumDeck) > 127) return false;
	}
	if(r.action == Build || r.action == Spawn || r.action == Update)
		if(r.material != 86 && r.material != 48 && r.material != 76
			&& r.material != 51 && r.material != 146 && r.material != 145) return false;
	return std::isfinite(r.distanceMeters) && r.distanceMeters >= (IsGameplay(r.action) ? 0 : 5) && r.distanceMeters <= 500
		&& std::isfinite(r.headingDegrees) && r.headingDegrees >= -180 && r.headingDegrees <= 180
		&& std::isfinite(r.elevationMeters) && std::abs(r.elevationMeters) <= 100
		&& std::isfinite(r.targetX) && std::abs(r.targetX) <= 1000
		&& std::isfinite(r.targetY) && std::abs(r.targetY) <= 1000
		&& std::isfinite(r.targetZ) && std::abs(r.targetZ) <= 1100;
}
bool Text(QDataStream& out, const QString& text, int maximum)
{
	const QByteArray utf8 = text.toUtf8();
	if(utf8.size() > maximum) return false;
	out << quint16(utf8.size());
	return out.writeRawData(utf8.constData(), utf8.size()) == utf8.size();
}
bool ReadText(QDataStream& in, QString& text, int maximum)
{
	quint16 size = 0; in >> size;
	if(in.status() != QDataStream::Ok || size > maximum || size > in.device()->bytesAvailable()) return false;
	QByteArray utf8(int(size), '\0');
	if(in.readRawData(utf8.data(), size) != size) return false;
	text = QString::fromUtf8(utf8);
	return text.toUtf8() == utf8;
}
}

QByteArray AuLegacyCityProtocol::EncodeRequest(const Request& r)
{
	if(!Valid(r)) return QByteArray();
	QByteArray bytes; QDataStream out(&bytes, QIODevice::WriteOnly); out.setVersion(QDataStream::Qt_4_1);
	const quint8 version = IsStudio(r.action) ? quint8(6) : quint8(4);
	out << version << quint8(r.action) << r.token << r.cityId << r.developmentId << r.designId
		// Preserve the retired door boolean's version-1 byte position.
		<< r.material << r.minimumDeck << r.maximumDeck << r.constructed << false
		<< r.distanceMeters << r.headingDegrees << r.elevationMeters << r.targetX << r.targetY << r.targetZ;
	out << r.industryId << r.processId << r.levels << r.roadLengthMeters << r.roadWidthMeters << r.citizenEnabled;
	if(!Text(out, r.cityName, 512)) return QByteArray();
	out << r.explicitLocation << r.worldId << r.latitudeRadians << r.longitudeRadians << r.roadSlab << r.biodomeRadiusMeters;
	out << r.manufacturingRecipe << r.processSlot << r.minimumTechnology << r.money;
	if(IsStudio(r.action))
	{
		out << r.studioIndex;
		if(!Text(out, r.studioPassword, 512)) return QByteArray();
	}
	return out.status() == QDataStream::Ok && bytes.size() <= MaximumRequestBytes ? bytes : QByteArray();
}
bool AuLegacyCityProtocol::DecodeRequest(const QByteArray& bytes, Request& result)
{
	if(bytes.size() > MaximumRequestBytes || bytes.size() < 6) return false;
	Request r; quint8 version = 0, action = 0; bool retiredDoorsOpen = false;
	QDataStream in(bytes); in.setVersion(QDataStream::Qt_4_1);
	in >> version >> action >> r.token >> r.cityId >> r.developmentId >> r.designId
		>> r.material >> r.minimumDeck >> r.maximumDeck >> r.constructed >> retiredDoorsOpen
		>> r.distanceMeters >> r.headingDegrees >> r.elevationMeters >> r.targetX >> r.targetY >> r.targetZ;
	if(version >= 2 && version <= 6)
	{
		in >> r.industryId >> r.processId >> r.levels >> r.roadLengthMeters >> r.roadWidthMeters >> r.citizenEnabled;
		if(!ReadText(in, r.cityName, 512)) return false;
		in >> r.explicitLocation >> r.worldId >> r.latitudeRadians >> r.longitudeRadians >> r.roadSlab >> r.biodomeRadiusMeters;
	}
	if(version >= 3) in >> r.manufacturingRecipe >> r.processSlot >> r.minimumTechnology;
	if(version >= 4) in >> r.money;
	if(version >= 5) in >> r.studioIndex;
	if(version >= 6 && !ReadText(in, r.studioPassword, 512)) return false;
	// TEMPORARY ARCHITECT MANUFACTURING TEST: only the version-4 request carries action 30.
	if((version < 1 || version > 6) || action > TestingFinishIndustryProcess || (version == 1 && action > Route)
		|| (version == 2 && action > ChangeRoad)
		|| (version == 3 && action > RunIndustryProcess)
		|| (version == 4 && action > BankClaimReward && action != TestingFinishIndustryProcess)
		|| (version >= 5 && !IsStudio(static_cast<Action>(action)))
		|| in.status() != QDataStream::Ok || !in.atEnd()) return false;
	r.action = static_cast<Action>(action);
	if(!Valid(r)) return false;
	result = r; return true;
}
QByteArray AuLegacyCityProtocol::EncodeReply(const Reply& r)
{
	// TEMPORARY ARCHITECT MANUFACTURING TEST: the normal economy reply also reports action 30.
	if(!r.token || r.action > TestingFinishIndustryProcess || r.entries.size() > MaximumEntries
		|| r.production.size() > MaximumReplyBytes / 2
		|| r.recipeChoices.size() > MaximumRecipeChoices
		|| (!r.hasRecipeChoices && !r.recipeChoices.isEmpty())
		|| (r.hasRecipeChoices && (!IsEconomy(r.action) || !r.hasEconomy))
		|| r.bankAccounting.size() > 1024 || ((!IsBank(r.action) || !r.hasBank) && !r.bankAccounting.isEmpty())
		|| (!r.bankGovernmentAuthorized && !r.bankAccounting.isEmpty()) || (r.hasBank && !IsBank(r.action))
		|| ((!IsEconomy(r.action) || !r.hasEconomy) && !r.production.isEmpty())
		|| (r.hasStudio && !IsStudio(r.action)) || r.studioCount > 16
		|| (!r.hasStudio && (r.studioCount || r.occupiedStudios))
		|| (quint32(r.occupiedStudios) >> r.studioCount)) return QByteArray();
	for(double amount : {r.bankCitizenBalance, r.bankGovernmentBalance, r.bankLoanFunds,
		r.bankPersonalBalance, r.bankPersonalLoan, r.bankCreditAvailable, r.bankPersonalReward}) if(!std::isfinite(amount)) return QByteArray();
	QSet<quint16> recipes;
	for(const quint16 id : r.recipeChoices)
	{
		if(!id || recipes.contains(id)) return QByteArray();
		recipes.insert(id);
	}
	QByteArray bytes; QDataStream out(&bytes, QIODevice::WriteOnly); out.setVersion(QDataStream::Qt_4_1);
	const quint8 version = IsStudio(r.action) ? 6 : r.hasRecipeChoices ? 5 : 4;
	out << version << quint8(r.action) << r.token << r.selectedCity << r.selectedDevelopment << r.hasList << r.truncated;
	if(!Text(out, r.message, 4096)) return QByteArray();
	out << quint16(r.entries.size());
	for(const Entry& e : r.entries)
	{
		out << e.cityId << e.developmentId << e.designId << e.material << e.minimumDeck << e.maximumDeck
			<< e.constructed << quint16(0) << e.elevationMeters << e.headingDegrees;
		if(!Text(out, e.name, 512)) return QByteArray();
		out << e.industryId << e.processId << e.completedLevels << e.targetLevels << e.halted << e.citizenEnabled
			<< e.siteLaborRemaining << e.levelLaborRemaining;
		if(!Text(out, e.materials, 1024)) return QByteArray();
		if(e.components.size() > 128) return QByteArray();
		out << quint16(e.components.size());
		for(const auto& component : e.components)
			out << component.commodityId << component.effect << component.quality << component.quantity << component.applied;
	}
	out << r.accepted;
	out << r.hasEconomy << r.economyIndustry << r.economyTechnology << r.economyLevels
		<< r.economyEmployees << quint32(r.production.size());
	if(out.writeRawData(r.production.constData(), r.production.size()) != r.production.size()) return QByteArray();
	out << r.hasBank << r.bankCitizenBalance << r.bankGovernmentBalance << r.bankLoanFunds
		<< r.bankPersonalBalance << r.bankPersonalLoan << r.bankCreditAvailable << r.bankPersonalReward
		<< r.bankGovernmentAuthorized << r.bankCapitalAuthorized << r.bankHasCapital << r.bankIsCapital
		<< quint16(r.bankAccounting.size());
	if(out.writeRawData(r.bankAccounting.constData(), r.bankAccounting.size()) != r.bankAccounting.size()) return QByteArray();
	if(r.hasRecipeChoices)
	{
		out << quint16(r.recipeChoices.size());
		for(const quint16 id : r.recipeChoices) out << id;
	}
	if(version == 6) out << r.hasStudio << r.studioCount << r.occupiedStudios;
	return out.status() == QDataStream::Ok && bytes.size() <= MaximumReplyBytes ? bytes : QByteArray();
}
bool AuLegacyCityProtocol::DecodeReply(const QByteArray& bytes, Reply& result)
{
	if(bytes.size() > MaximumReplyBytes || bytes.size() < 6) return false;
	Reply r; quint8 version = 0, action = 0; quint16 count = 0;
	QDataStream in(bytes); in.setVersion(QDataStream::Qt_4_1);
	in >> version >> action >> r.token >> r.selectedCity >> r.selectedDevelopment >> r.hasList >> r.truncated;
	// TEMPORARY ARCHITECT MANUFACTURING TEST: preserve ordinary version-4/5 economy replies for action 30.
	if((version < 1 || version > 6) || action > TestingFinishIndustryProcess || (version == 1 && action > Route)
		|| (version == 2 && action > ChangeRoad)
		|| (version == 3 && action > RunIndustryProcess)
		|| (version < 6 && action > BankClaimReward && action != TestingFinishIndustryProcess)
		|| (version == 6 && !IsStudio(static_cast<Action>(action)))
		|| !r.token || !ReadText(in, r.message, 4096)) return false;
	in >> count;
	if(in.status() != QDataStream::Ok || count > MaximumEntries || (!r.hasList && count)
		|| count * 29 > in.device()->bytesAvailable()) return false;
	for(int i = 0; i < count; ++i)
	{
		Entry e; quint16 retiredDoorCount = 0;
		in >> e.cityId >> e.developmentId >> e.designId >> e.material >> e.minimumDeck >> e.maximumDeck
			>> e.constructed >> retiredDoorCount >> e.elevationMeters >> e.headingDegrees;
		if(!e.cityId || !e.developmentId || (version == 1 && !e.designId)
			|| !std::isfinite(e.elevationMeters) || !std::isfinite(e.headingDegrees)
			|| !ReadText(in, e.name, 512)) return false;
		if(version >= 2)
		{
			in >> e.industryId >> e.processId >> e.completedLevels >> e.targetLevels >> e.halted >> e.citizenEnabled
				>> e.siteLaborRemaining >> e.levelLaborRemaining;
			if(!ReadText(in, e.materials, 1024)) return false;
			quint16 components = 0; in >> components;
			if(in.status() != QDataStream::Ok || components > 128 || quint64(components) * 12 > quint64(in.device()->bytesAvailable())) return false;
			for(int componentIndex = 0; componentIndex < components; ++componentIndex)
			{
				AuLegacyCity::ConstructionComponent component;
				in >> component.commodityId >> component.effect >> component.quality >> component.quantity >> component.applied;
				if(!component.commodityId || component.effect < 1 || component.effect > 6 || component.quantity > 65535
					|| component.applied > qMax(quint32(1), component.quantity)) return false;
				e.components.append(component);
			}
		}
		r.entries.append(e);
	}
	if(version >= 2) in >> r.accepted;
	if(version >= 3)
	{
		quint32 size = 0;
		in >> r.hasEconomy >> r.economyIndustry >> r.economyTechnology >> r.economyLevels >> r.economyEmployees >> size;
		if(in.status() != QDataStream::Ok || size > MaximumReplyBytes / 2
			|| size > quint64(in.device()->bytesAvailable()) || r.economyTechnology > 31 || r.economyLevels > 32
			|| ((!IsEconomy(static_cast<Action>(action)) || !r.hasEconomy) && size)) return false;
		r.production.resize(int(size));
		if(in.readRawData(r.production.data(), int(size)) != int(size)) return false;
	}
	if(version >= 4)
	{
		quint16 size = 0;
		in >> r.hasBank >> r.bankCitizenBalance >> r.bankGovernmentBalance >> r.bankLoanFunds
			>> r.bankPersonalBalance >> r.bankPersonalLoan >> r.bankCreditAvailable >> r.bankPersonalReward
			>> r.bankGovernmentAuthorized >> r.bankCapitalAuthorized >> r.bankHasCapital >> r.bankIsCapital >> size;
		if(in.status() != QDataStream::Ok || size > 1024 || size > in.device()->bytesAvailable()
			|| (r.hasBank && (action < BankState || action > BankClaimReward))
			|| ((!r.hasBank || !r.bankGovernmentAuthorized) && size)) return false;
		for(double amount : {r.bankCitizenBalance, r.bankGovernmentBalance, r.bankLoanFunds,
			r.bankPersonalBalance, r.bankPersonalLoan, r.bankCreditAvailable, r.bankPersonalReward}) if(!std::isfinite(amount)) return false;
		r.bankAccounting.resize(size);
		if(in.readRawData(r.bankAccounting.data(), size) != size) return false;
	}
	if(version == 5)
	{
		quint16 choices = 0; in >> choices;
		if(in.status() != QDataStream::Ok || !r.hasEconomy || !IsEconomy(static_cast<Action>(action))
			|| choices > MaximumRecipeChoices || quint64(choices) * 2 > quint64(in.device()->bytesAvailable())) return false;
		QSet<quint16> recipes;
		for(quint16 index = 0; index < choices; ++index)
		{
			quint16 id = 0; in >> id;
			if(in.status() != QDataStream::Ok || !id || recipes.contains(id)) return false;
			recipes.insert(id); r.recipeChoices.append(id);
		}
		r.hasRecipeChoices = true;
	}
	if(version == 6)
	{
		in >> r.hasStudio >> r.studioCount >> r.occupiedStudios;
		if(r.hasEconomy || r.hasBank || r.studioCount > 16
			|| (!r.hasStudio && (r.studioCount || r.occupiedStudios))
			|| (quint32(r.occupiedStudios) >> r.studioCount)) return false;
	}
	if(in.status() != QDataStream::Ok || !in.atEnd()) return false;
	r.action = static_cast<Action>(action); result = r; return true;
}
