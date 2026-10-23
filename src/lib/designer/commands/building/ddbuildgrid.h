#ifndef DdBuildGrid_H
#define DdBuildGrid_H

#include "auid.h"
#include "aumatrix4x4.h"
#include "aupoint.h"
#include <QColor>
#include <QList>

class AuGLEffects;
class DaWorldLL;

namespace BuildGrid
{

bool Enabled();

bool Snap(const DaWorldLL* world, AuPoint& loc);

double NorthRotation(const DaWorldLL* world, const AuPoint& loc);

void Render(AuGLEffects& re, const AuMatrix4x4& projectionModel, const DaWorldLL* world, const AuPoint& loc);

}

#endif
