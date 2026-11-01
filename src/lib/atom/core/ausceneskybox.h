#ifndef AUSCENESKYBOX_H
#define AUSCENESKYBOX_H

#include "auatom.h"
#include "auid.h"
#include "aupoint.h"
#include <memory>

class AuGLEffects;
class DaGalaxy;
class auNebula;
class AuSceneSkyBox;

// A view owns completed textures. Sectors borrow them through retained handles;
// pending work never keeps a live scene atom, widget or data lock.
class AUATOM_DLL AuSceneSkyBoxCache
{
public:
	explicit AuSceneSkyBoxCache(bool diskCache);
	~AuSceneSkyBoxCache();
	bool Prepare(AuGLEffects& re, const DaGalaxy& galaxy, const AuId& systemId,
		const AuPoint& viewerInSectors, auNebula& sky, std::shared_ptr<AuSceneSkyBox>& handle);
	void Clear(AuGLEffects* re);

private:
	struct State;
	std::unique_ptr<State> m_State;
};

#endif
