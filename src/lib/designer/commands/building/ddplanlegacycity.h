#ifndef DdPlanLegacyCity_H
#define DdPlanLegacyCity_H

#include "ddcommand.h"
#include "aulegacycityprotocol.h"
#include "auglobell.h"
#include "auglsl.h"
#include <QPointer>

class AuLegacyCityConstruction;
class DaWorldLL;

// Reuses only the viewport's input/projection mechanism. Historical city
// placement and requests are independent of DdPlanBdBuilding/modern roads.
class DdPlanLegacyCity : public DdCommand
{
public:
	DdPlanLegacyCity(AuDesignerView* view, AuLegacyCityConstruction* controls,
		const AuLegacyCityProtocol::Request& request);
	~DdPlanLegacyCity() override;
	bool UsesControls(const AuLegacyCityConstruction* controls) const;
	void UpdateSelection(const AuLegacyCityProtocol::Request& request);
	bool OnKeyDown(QKeyEvent* event) override;
	void OnButtonDown(QMouseEvent* event) override;
	void OnMouseWheel(QWheelEvent* event) override;
	void DeleteDeviceObjects(AuGLEffects& effects) override;
	void SetCursor(const AuMatrix4x4& camera, DaPerson* person, const QPoint& pixel) override;
	void RenderCursor(const AuMatrix4x4& projection, const AuMatrix4x4& camera, DaPerson* person,
		const QPoint& pixel, AuAtomRenderInfo& info, bool snapped) override;
private:
	QPointer<AuLegacyCityConstruction> m_Interface;
	AuLegacyCityProtocol::Request m_Request;
	bool m_HasStart = false;
	bool m_SiteBlocked = false;
	int m_SiteStatus = 0;
	bool m_AlignmentAvailable = false;
	bool m_HeadingRelativeToNorth = true;
	double m_AlignmentFacing = 0;
	quint32 m_ConflictWorld = 0;
	QList<AuPointAlpha> m_ConflictingPerimeters;
	int m_MaximumLevels = -1;
	int m_ExistingLevels = 0;
	quint16 m_SiteMaterial = 0;
	quint32 m_SiteDesignId = 0;
	AuPoint m_SiteCenter;
	double m_SiteFacing = 0;
	quint32 m_PreviousSiteWorld = 0;
	AuGlobeLLF m_PreviousSiteLocation;
	QVector<quint16> m_AvailableProducts;
	QString m_SiteMessage;
	quint32 m_StartWorld = 0;
	AuPoint m_Start;
	double m_Angle = 90;
	bool m_PreviewValid = false;
	QList<AuPointAlpha> m_Preview;
	AuGLVAO m_PreviewVAO;
	void ChangeMaterial(int direction);
	void ChangeLevels(int direction);
	void ChangeRotation(int direction, bool fine);
	void ChangeProduct(int direction);
	bool AlignToObstruction();
	void UpdateSite(const DaWorldLL& world, const AuPoint& point, const DaPerson& person);
	void RenderSiteCursor(const AuMatrix4x4& projection, const AuMatrix4x4& camera, DaPerson* person,
		const QPoint& pixel, AuAtomRenderInfo& info);
	void RenderPlacementMessage(AuAtomRenderInfo& info) const;
	void ConstrainRoad(const DaWorldLL& world, AuPoint& point, bool constrain) const;
};

#endif
