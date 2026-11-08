#ifndef DdRoadConstruction_H
#define DdRoadConstruction_H

#include <QCursor>
#include <QPoint>

namespace DdRoadConstruction
{
QCursor Cursor(quint8 process, bool change, bool slab, const QPoint& hotspot = QPoint(-1, -1));
}

#endif
