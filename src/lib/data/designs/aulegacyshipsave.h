#ifndef AuLegacyShipSave_H
#define AuLegacyShipSave_H

#include "aulegacyship.h"
#include <QSet>

namespace AuLegacyShip
{
// AuSpaceShipModules' 2016 constructor defaults (also used by version 0).
QVector<quint16> DefaultModuleSlots();
quint16 ScalarModule(const Design& design, int slot);

void ModuleSystemKeys(const Design& design, QSet<quint16>& bays, QSet<quint16>& transporters);
void TrimMissingModuleSystems(Design& design);

// DnDetailArray::Add orders records by signed deck, room ID, then detail ID.
void SortDetails(QVector<Detail>& details);

// Reproduce the original designer's load/analysis preparation on a copy:
// equipment ordering and weapon-bay/transporter module reconciliation.
// Identity metadata is retained. Failure leaves output untouched.
bool PrepareForSave(const Design& design, Design& output, QString& error);

// The 2016 writer emits version 2, in existing record order. See FORMAT.txt
// for load-time normalization and ellipse compression. Failure leaves output
// untouched. An optional design name replaces only an empty header name.
bool Write(const Design& design, QByteArray& output, QString& error,
	const QString& fallbackName = QString());

// Prepare and serialize before opening the destination; replace it atomically.
// Failure leaves the existing file and the working design untouched.
bool Save(const QString& fileName, const Design& design, QString& error,
	const QString& fallbackName = QString());
}

#endif // AuLegacyShipSave_H
