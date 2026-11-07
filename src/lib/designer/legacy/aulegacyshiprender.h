#ifndef AuLegacyShipRender_H
#define AuLegacyShipRender_H

#include "audesignerview.h"
#include <QIcon>
#include <QPointF>
#include <QRectF>
#include <QStringList>
#include <functional>

namespace AuLegacyShip { struct Design; }
class QKeyEvent;
class QMouseEvent;
class QWheelEvent;
class QFocusEvent;
class QHideEvent;
class QShowEvent;
class QMatrix4x4;

struct AuLegacyShipEditTool
{
	bool active = false;
	bool erasing = false;
	bool wholeRoom = false;
	bool equipment = false;
	int detailType = 0;
	int radius = 0;
	int rotation = 0;
	QIcon icon;
	QRectF footprint;
};

class AuLegacyShipRender : public AuDesignerView
{
public:
	enum class InteriorLight { Off = 0, On = 1, Dim = 2 };
	explicit AuLegacyShipRender(QWidget* parent = nullptr);
	~AuLegacyShipRender() override;

	void SetDesign(const AuLegacyShip::Design& design);
	void SetDeck(int deck);
	void SetGravity(bool enabled);
	void ResetCharacter();
	void SetRenderSolid(bool solid);
	void CycleInteriorLight();
	void SetGrid(bool visible, quint8 mask);
	void SetEditTool(const AuLegacyShipEditTool& tool);
	void ResetEditStroke();
	void FocusDesignPlane();
	void UseHandCursor();
	bool SkyboxNeedsPreparation() const;
	void PrepareSkybox();
	void SetLoadingScreen(bool loading);
	void PrepareDesign();
	void SetInterfaceVisible(bool visible);
	void ControlsChanged();
	QStringList Warnings() const;
	std::function<void(const QString&)> tipsChanged;
	std::function<void(const QPointF&)> cellEntered;
	std::function<void()> editStrokeFinished;
	std::function<void(int)> rotateEquipment;
	std::function<void(InteriorLight)> interiorLightChanged;

protected:
	void initializeGL() override;
	void resizeGL(int width, int height) override;
	void resizeEvent(QResizeEvent* event) override;
	bool event(QEvent* event) override;
	void paintGL() override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void mouseDoubleClickEvent(QMouseEvent* event) override;
	void wheelEvent(QWheelEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;
	void keyReleaseEvent(QKeyEvent* event) override;
	bool eventFilter(QObject* watched, QEvent* event) override;
	void focusInEvent(QFocusEvent* event) override;
	void focusOutEvent(QFocusEvent* event) override;
	void showEvent(QShowEvent* event) override;
	void hideEvent(QHideEvent* event) override;

private:
	void CleanupGL();
	void PrepareSky();
	void RenderSky();
	void PrepareWireframe();
	void Tick();
	float FieldOfView() const;
	void UpdateTarget(qint64 now);
	void UpdateMouseCapture();
	void UpdateToolCursor();
	void ReleaseMouseCapture();
	void CenterMouse();
	void UseDoor(const QPoint& screenPosition);
	bool UseConsole(const QPoint& screenPosition);
	void OperateDoor(quint32 id, bool open);
	void CompleteSystemInitialization();
	void OperateSystem(int action);
	bool DesignPlanePoint(const QPoint& pixel, QPointF& point);
	void UpdateEditCursor(const QPoint& pixel);
	void EditCursor();
	void RenderWireframe(const QMatrix4x4& viewProjection);
	class Impl;
	Impl* m_Impl;
};

#endif // AuLegacyShipRender_H
