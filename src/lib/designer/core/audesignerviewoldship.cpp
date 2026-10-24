#include "audesignerview.h"
//
#include "auapp.h"
#include "auatomrenderinfo.h"
#include "aubddesign.h"
#include "aubdreport.h"
#include "audesignerapp.h"
#include "audesignerframe.h"
#include "audesigneroldship.h"
#include "auframe.h"
#include "augleffects.h"
#include "auglobal.h"
#include "auscdesign.h"
#include "auscparts.h"
#include "auscreport.h"
#include "dadesigner.h"
#include "daperson.h"
#include "dgdata.h"
#include "oldbuildingconvert.h"
#include "oldbuildingedit.h"
#include "oldshipconvert.h"
#include "oldshipedit.h"
#include "oldshipview.h"
#include "psenginetrail.h"
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QScopedPointer>
#include <QSharedPointer>
#include <QTimer>
#include <cmath>

namespace
{

const int ModelDelayMs = 400;
const int ReportDelayMs = 700;
const int ReportDelayMaxMs = 15000;

void DrawPoints(AuGLEffects& re, const std::vector<OldShip::Vec3>& points, GLenum mode)
{
	if(points.empty())
		return;
	AuPointArray verts;
	verts.reserve((int)points.size());
	for(size_t i = 0; i < points.size(); i++)
		verts.append(AuPoint(points[i].x, points[i].y, points[i].z));
	AuGLVAO vao;
	vao.CreateForLines(re, verts, false, GL_STREAM_DRAW);
	if(vao.VAO())
	{
		vao.SelectGL();
		vao.DrawGL(mode);
		vao.DeselectGL();
		vao.DeleteGL();
	}
}

std::vector<OldShip::Vec3> ContrailPlaces(const OldShip::Design& d)
{
	std::vector<OldShip::Vec3> places;
	if(d.rooms.empty())
		return places;
	double x0 = 1e30, x1 = -1e30, y0 = 1e30, y1 = -1e30;
	int deck0 = d.rooms[0].deck, deck1 = d.rooms[0].deck;
	for(size_t i = 0; i < d.rooms.size(); i++)
	{
		deck0 = std::min(deck0, d.rooms[i].deck);
		deck1 = std::max(deck1, d.rooms[i].deck);
		for(size_t k = 0; k < d.rooms[i].points.size(); k++)
		{
			const double x = d.rooms[i].points[k].x * OldShip::GridUnit, y = d.rooms[i].points[k].y * OldShip::GridUnit;
			x0 = std::min(x0, x);
			x1 = std::max(x1, x);
			y0 = std::min(y0, y);
			y1 = std::max(y1, y);
		}
	}
	const double width = x1 - x0, length = y1 - y0;
	for(size_t i = 0; i < d.details.size() && places.size() < 10; i++)
		if(d.details[i].type == 98)
			places.push_back(OldShip::Vec3(d.details[i].x * OldShip::GridUnit, d.details[i].y * OldShip::GridUnit - width / 10,
					d.details[i].deck * OldShip::DeckHeight + 4));
	if(places.empty())
	{
		const double y = (y0 + y1) / 2 - length / 2 - width / 10;
		const double z = (deck0 + deck1) * OldShip::DeckHeight / 2;
		const double offsets[3] = { 0.0, -width / 3, width / 3 };
		for(int i = 0; i < 3; i++)
			places.push_back(OldShip::Vec3((x0 + x1) / 2 + offsets[i], y, z));
	}
	return places;
}

void DrawIcons(AuGLEffects& re, const std::vector<OldShip::IconQuad>& icons)
{
	if(icons.empty())
		return;
	QList<AuTriTex> tris;
	for(size_t i = 0; i < icons.size(); i++)
	{
		const OldShip::IconQuad& q = icons[i];
		const AuPointTex ll(q.corner[0].x, q.corner[0].y, q.corner[0].z, q.u0, 0.f);
		const AuPointTex lr(q.corner[1].x, q.corner[1].y, q.corner[1].z, q.u1, 0.f);
		const AuPointTex ur(q.corner[2].x, q.corner[2].y, q.corner[2].z, q.u1, 1.f);
		const AuPointTex ul(q.corner[3].x, q.corner[3].y, q.corner[3].z, q.u0, 1.f);
		tris << AuTriTex(ll, ul, ur) << AuTriTex(ur, lr, ll);
	}
	AuGLVAO vao;
	vao.CreateForTexture(re, tris, GL_STREAM_DRAW);
	if(vao.VAO())
	{
		vao.SelectGL();
		vao.DrawGL();
		vao.DeselectGL();
		vao.DeleteGL();
	}
}

}

AuOldShipPanel* AuDesignerView::OldShipPanel()
{
	AuDesignerFrame* frame = auDesignerApp ? auDesignerApp->DesignerFrame() : 0;
	if(!frame || !frame->OldShipMode() || !frame->OldShipDock())
		return 0;
	if(SceneContext() != SCENE_Designer || !IsInNewDesignEnvironment() || IsDesignPreview())
		return 0;
	return frame->OldShipDock()->Panel();
}

bool AuDesignerView::OldShipPick(DaPerson* sceneAtom, const QPoint& pxLoc, double& gx, double& gy)
{
	AuOldShipPanel* panel = OldShipPanel();
	if(!panel || !sceneAtom)
		return false;
	DaDesigner* pde = sceneAtom->FindParentDesignerInEnv();
	if(!pde)
		return false;

	const OldShip::Vec3& origin = panel->Origin();
	AuMatrix4x4 floorPlane;
	floorPlane.Translate(-origin.x, -origin.y, panel->Deck() * OldShip::DeckHeight - origin.z);
	AuMatrix4x4 tperson;
	sceneAtom->GetTransformRelativeToParent(tperson, pde);
	AuMatrix4x4 twp = tperson.Inverse() * floorPlane;

	AuMatrix4x4 tDevicePersp, tDeviceOrtho;
	GetPixelTransforms(tDevicePersp, tDeviceOrtho);
	AuPoint hit;
	if(!tDevicePersp.UnProjectIntoPlane(hit, pxLoc, twp))
		return false;
	const AuPoint studio = tperson.Project(hit);
	gx = (studio.x + origin.x) / OldShip::GridUnit;
	gy = (studio.y + origin.y) / OldShip::GridUnit;
	return true;
}

void AuDesignerView::OldShipMousePress(QMouseEvent* e)
{
	AuOldShipPanel* panel = OldShipPanel();
	if(!panel || e->button() != Qt::LeftButton)
		return;
	double gx = 0, gy = 0;
	DaPerson* sceneAtom;
	LockSceneAtom(&sceneAtom);
	const bool picked = OldShipPick(sceneAtom, e->pos(), gx, gy);
	UnlockSceneAtom();
	if(picked)
		panel->ViewStroke(gx, gy, true);
}

void AuDesignerView::OldShipMouseMove(QMouseEvent* e)
{
	AuOldShipPanel* panel = OldShipPanel();
	if(!panel)
		return;
	double gx = 0, gy = 0;
	DaPerson* sceneAtom;
	LockSceneAtom(&sceneAtom);
	const bool picked = OldShipPick(sceneAtom, e->pos(), gx, gy);
	UnlockSceneAtom();
	if(!picked)
		return;
	panel->ViewHover(gx, gy);
	if(e->buttons() & Qt::LeftButton)
		panel->ViewStroke(gx, gy, false);
}

bool AuDesignerView::OldShipKeyPress(QKeyEvent* e)
{
	AuOldShipPanel* panel = OldShipPanel();
	if(!panel)
		return false;
	if(e->key() == Qt::Key_Left)
		return panel->ViewTurn(1);
	if(e->key() == Qt::Key_Right)
		return panel->ViewTurn(-1);
	if(e->key() == Qt::Key_Escape)
		return panel->ViewCancel();
	return false;
}

bool AuDesignerView::OldShipSetCursor()
{
	AuOldShipPanel* panel = IsUIInputMode() ? OldShipPanel() : 0;
	QString icon;
	if(panel && (panel->Tool() == AuOldShipPanel::TOOL_Room || panel->Tool() == AuOldShipPanel::TOOL_EraseCells
				|| panel->Tool() == AuOldShipPanel::TOOL_EraseRoom || panel->Tool() == AuOldShipPanel::TOOL_Roof))
		icon = panel->ToolIcon();
	if(icon.isEmpty())
	{
		m_OldShipCursor = QString();
		return false;
	}
	if(icon != m_OldShipCursor)
	{
		QPixmap pixmap(icon);
		if(pixmap.isNull())
		{
			m_OldShipCursor = QString();
			return false;
		}
		setCursor(QCursor(pixmap, panel->Tool() == AuOldShipPanel::TOOL_Room || panel->Tool() == AuOldShipPanel::TOOL_Roof ? 7 : 2, 7));
		m_OldShipCursor = icon;
		m_Cursor = CURSOR_Unknown;
	}
	return true;
}

void AuDesignerView::OldShipRender(DaPerson* sceneAtom)
{
	AuOldShipPanel* panel = OldShipPanel();
	if(!panel || !sceneAtom || !m_RenderInfo->re)
		return;
	const DaDesigner* pde = sceneAtom->FindParentDesignerInEnv();
	if(!pde)
		return;
	AuGLEffects& re = *m_RenderInfo->re;

	AuMatrix4x4 ts;
	sceneAtom->GetTransformRelativeToParent(ts, pde);
	ts = m_tCamera.Inverse() * ts.Inverse();
	const OldShip::Vec3& origin = panel->Origin();
	const int deck = panel->Deck();
	AuMatrix4x4 shipM(ts);
	shipM.Translate(-origin.x, -origin.y, -origin.z);
	AuMatrix4x4 planeM(shipM);
	planeM.Translate(0, 0, deck * OldShip::DeckHeight);

	if(!m_OldShipDrawing)
		m_OldShipDrawing = new OldShip::Drawing;
	if(m_OldShipDrawnPanel != panel || m_OldShipDrawnRevision != panel->Revision() || m_OldShipDrawnDeck != deck)
	{
		OldShip::BuildDrawing(panel->Design(), deck, *m_OldShipDrawing, panel->IsBuilding());
		m_OldShipDrawnPanel = panel;
		m_OldShipDrawnRevision = panel->Revision();
		m_OldShipDrawnDeck = deck;
	}
	const OldShip::Drawing& drawing = *m_OldShipDrawing;

	if(!m_texOldShipIcons)
		m_texOldShipIcons = re.CreateTextureFromBitmap(STATIC_TEXTURE_ID, "OldShipDesign", ":data/txSpacecraftDesign.png",
				GL_LINEAR, GL_LINEAR, false, 3);

	const QFont& font = g_Au.FontDesigner();
	const int textFlags = Qt::AlignLeft | Qt::AlignBottom | Qt::TextSingleLine;

	re.glEnable(GL_DEPTH_TEST);
	re.glDepthMask(true);
	re.glDisable(GL_BLEND);
	re.glEnable(GL_CULL_FACE);

	if(!panel->IsSolid())
	{
		re.GLSL().LineShader().Select();
		re.GLSL().LineShader().SetProjectionModelView(m_tProjection * shipM);

		for(size_t i = 0; i < drawing.lines.size(); i++)
		{
			const OldShip::Batch& b = drawing.lines[i];
			re.GLSL().LineShader().SetColor(b.r, b.g, b.b, 1);
			DrawPoints(re, b.points, GL_LINES);
		}

		re.glDisable(GL_CULL_FACE);
		re.glEnable(GL_POLYGON_OFFSET_FILL);
		re.glPolygonOffset(1.f, 1.f);
		for(size_t i = 0; i < drawing.floorsBelow.size(); i++)
		{
			const OldShip::Batch& b = drawing.floorsBelow[i];
			re.GLSL().LineShader().SetColor(b.r, b.g, b.b, 1);
			DrawPoints(re, b.points, GL_TRIANGLES);
		}
		re.glDisable(GL_POLYGON_OFFSET_FILL);
		re.glEnable(GL_CULL_FACE);
		re.GLSL().LineShader().Deselect();

		re.glEnable(GL_BLEND);
		re.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		re.glDisable(GL_CULL_FACE);
		if(m_texOldShipIcons && !drawing.icons.empty())
		{
			re.GLSL().TextureShader().Select();
			re.GLSL().TextureShader().SetProjectionModelView(m_tProjection * shipM);
			re.GLSL().TextureShader().SetTexture(m_texOldShipIcons);
			re.GLSL().TextureShader().SetColor(1, 1, 1, 1);
			DrawIcons(re, drawing.icons);
			re.GLSL().TextureShader().Deselect();
		}

		re.GLSL().LineShader().Select();
		re.GLSL().LineShader().SetProjectionModelView(m_tProjection * shipM);
		re.GLSL().LineShader().SetColor(.5f, .5f, .5f, 1);
		DrawPoints(re, drawing.stairs, GL_LINES);
		re.glEnable(GL_POLYGON_OFFSET_FILL);
		re.glPolygonOffset(1.f, 1.f);
		for(size_t i = 0; i < drawing.floors.size(); i++)
		{
			const OldShip::Batch& b = drawing.floors[i];
			re.GLSL().LineShader().SetColor(b.r, b.g, b.b, b.a);
			DrawPoints(re, b.points, GL_TRIANGLES);
		}
		re.glDisable(GL_POLYGON_OFFSET_FILL);
		re.GLSL().LineShader().Deselect();
		re.glEnable(GL_CULL_FACE);

		re.glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		for(size_t i = 0; i < drawing.labels.size(); i++)
		{
			const OldShip::Label& label = drawing.labels[i];
			const AuPoint at(label.at.x, label.at.y, label.at.z);
			if(shipM.Project(at).z > -1.0)
				continue;
			QColor color;
			color.setRgbF(label.color.r, label.color.g, label.color.b);
			re.RenderText(m_tProjection, shipM, at, QString::fromUtf8(label.text.c_str()), font, color, size().height(), textFlags);
		}
		re.glDisable(GL_BLEND);
	}

	if(!panel->IsBuilding())
	{
		if(m_OldShipContrailsPanel != panel || m_OldShipContrailsRevision != panel->Revision())
		{
			qDeleteAll(m_OldShipContrails);
			m_OldShipContrails.clear();
			m_OldShipContrailsPanel = panel;
			m_OldShipContrailsRevision = panel->Revision();
			const std::vector<OldShip::Vec3> places = ContrailPlaces(panel->Design());
			double width = 0;
			if(!panel->Design().rooms.empty())
			{
				double x0 = 1e30, x1 = -1e30;
				for(size_t i = 0; i < panel->Design().rooms.size(); i++)
					for(size_t p = 0; p < panel->Design().rooms[i].points.size(); p++)
					{
						x0 = std::min(x0, panel->Design().rooms[i].points[p].x * OldShip::GridUnit);
						x1 = std::max(x1, panel->Design().rooms[i].points[p].x * OldShip::GridUnit);
					}
				width = x1 - x0;
			}
			const DgData& gd = g_Au.LockDataForRead();
			const QColor color = gd.EmpireContrailColor(0);
			g_Au.UnlockData();
			for(size_t i = 0; i < places.size(); i++)
			{
				PsEngineTrail* trail = new PsEngineTrail;
				trail->SetMaxParticles(35);
				trail->SetLifeCycle(1.f);
				trail->SetReleaseInterval(0.f);
				trail->SetNumToRelease(1);
				trail->SetColor(color);
				trail->SetPosition(AuPoint(places[i].x, places[i].y, places[i].z));
				trail->SetSize((float)(width * 100));
				trail->SetVelocity(AuPoint(0, -500, 0));
				m_OldShipContrails.append(trail);
			}
		}
		if(!m_OldShipContrails.isEmpty())
		{
			const GLuint streak = re.CreateTextureFromBitmap(STATIC_TEXTURE_ID, "EngineStreak", ":data/EngineStreak.png");
			const GLuint glow = re.CreateTextureFromBitmap(STATIC_TEXTURE_ID, "tBCM", ":data/particle.bmp");
			re.glDepthMask(GL_FALSE);
			re.glEnable(GL_BLEND);
			re.glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			for(int i = 0; i < m_OldShipContrails.size(); i++)
			{
				PsEngineTrail* trail = m_OldShipContrails[i];
				trail->SetTextureID(streak);
				trail->SetTextureID2(glow);
				trail->SetParent(pde->Id(), AuMatrix4x4());
				if(trail->Update() && trail->GetParticleCount())
					trail->Render(m_tProjection, shipM, re);
			}
			re.glDisable(GL_BLEND);
			re.glDepthMask(GL_TRUE);
		}
	}

	const float k = m_RenderInfo->textBrightness > 0 ? m_RenderInfo->textBrightness : 1.f;
	re.glDisable(GL_DEPTH_TEST);
	re.GLSL().LineShader().Select();
	re.GLSL().LineShader().SetProjectionModelView(m_tProjection * planeM);
	const bool building = panel->IsBuilding();
	const double siteRadius = building ? OldShip::BuildingSiteRadius(panel->Industry(), deck) : 0.0;
	const bool landSide = building && OldShip::BuildingIsCoastal(panel->Industry());
	{
		std::vector<OldShip::Vec3> minor, major;
		if(building)
		{
			OldShip::BuildingSiteLines(siteRadius, landSide, deck * OldShip::DeckHeight, OldShip::BuildingMinDeck(panel->Industry()),
					OldShip::BuildingMaxDeck(panel->Industry()), minor);
			OldShip::BuildingGridLines(panel->GridLines(), siteRadius, landSide, minor, major);
		}
		else
		{
			OldShip::CageLines(deck * OldShip::DeckHeight, minor);
			OldShip::GridLines(panel->GridLines(), minor, major);
		}
		re.GLSL().LineShader().SetColor(.1f * k, .2f * k, .3f * k, 1);
		DrawPoints(re, minor, GL_LINES);
		re.GLSL().LineShader().SetColor(.2f * k, .3f * k, .4f * k, 1);
		DrawPoints(re, major, GL_LINES);
	}
	re.GLSL().LineShader().Deselect();

	re.glEnable(GL_BLEND);
	re.glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	{
		QColor color;
		color.setRgbF(qMin(1.f, .3f * k), qMin(1.f, .4f * k), qMin(1.f, .5f * k));
		int count = 0;
		const OldShip::CompassLabel* labels = OldShip::CompassLabels(count);
		OldShip::CompassLabel siteLabels[8];
		if(building)
		{
			count = OldShip::BuildingCompassLabels(siteRadius, landSide, siteLabels);
			labels = siteLabels;
		}
		for(int i = 0; i < count; i++)
		{
			const AuPoint at(labels[i].x, labels[i].y, 6.0);
			if(planeM.Project(at).z > -1.0)
				continue;
			re.RenderText(m_tProjection, planeM, at, QString::fromLatin1(labels[i].text), font, color, size().height(),
					Qt::AlignHCenter | Qt::AlignVCenter | Qt::TextSingleLine);
		}
	}

	if(!panel->IsSolid() && panel->IsReportCurrent() && !panel->Problems().isEmpty())
	{
		const QList<OldShip::Problem>& problems = panel->Problems();
		std::vector<OldShip::Vec3> red;
		for(int i = 0; i < problems.size(); i++)
		{
			const OldShip::Problem& p = problems[i];
			const int corners = p.outline.size();
			if(corners >= 3)
			{
				const double low = 0.08, high = OldShip::DeckHeight - 0.08;
				for(int k = 0; k < corners; k++)
				{
					const AuPoint& a = p.outline[k];
					const AuPoint& b = p.outline[(k + 1) % corners];
					red.push_back(OldShip::Vec3(a.x, a.y, a.z + low));
					red.push_back(OldShip::Vec3(b.x, b.y, b.z + low));
					red.push_back(OldShip::Vec3(a.x, a.y, a.z + high));
					red.push_back(OldShip::Vec3(b.x, b.y, b.z + high));
					red.push_back(OldShip::Vec3(a.x, a.y, a.z + low));
					red.push_back(OldShip::Vec3(a.x, a.y, a.z + high));
				}
			}
			else
			{
				const double r = 1.6, z = p.at.z + 0.12;
				const OldShip::Vec3 e(p.at.x + r, p.at.y, z), n(p.at.x, p.at.y + r, z), w(p.at.x - r, p.at.y, z), s(p.at.x, p.at.y - r, z);
				const OldShip::Vec3 ring[10] = { e, n, n, w, w, s, s, e, e, w };
				for(int k = 0; k < 10; k++)
					red.push_back(ring[k]);
				red.push_back(n);
				red.push_back(s);
				red.push_back(OldShip::Vec3(p.at.x, p.at.y, z));
				red.push_back(OldShip::Vec3(p.at.x, p.at.y, p.at.z + 5.0));
			}
		}
		re.glDisable(GL_BLEND);
		re.GLSL().LineShader().Select();
		re.GLSL().LineShader().SetProjectionModelView(m_tProjection * shipM);
		re.GLSL().LineShader().SetColor(1, .13f, .13f, 1);
		DrawPoints(re, red, GL_LINES);
		re.GLSL().LineShader().Deselect();
		re.glEnable(GL_BLEND);
		for(int i = 0; i < problems.size(); i++)
		{
			const OldShip::Problem& p = problems[i];
			if(p.deck != deck || p.text.isEmpty())
				continue;
			const AuPoint at(p.at.x, p.at.y, p.at.z + (p.outline.isEmpty() ? 5.4 : 4.0));
			if(shipM.Project(at).z > -1.0)
				continue;
			re.RenderText(m_tProjection, shipM, at, p.text, font, QColor(255, 72, 72), size().height(),
					Qt::AlignHCenter | Qt::AlignVCenter | Qt::TextSingleLine);
		}
	}

	double gx = 0, gy = 0;
	const AuOldShipPanel::TOOL tool = panel->Tool();
	if(IsUIInputMode() && !Command() && underMouse() && tool == AuOldShipPanel::TOOL_Roof && OldShipPick(sceneAtom, m_MouseLast, gx, gy))
	{
		std::vector<OldShip::Vec3> ghost;
		OldShip::RoofGhost(panel->ToolRoofType(), deck, panel->RoofClicks(), panel->RoofPoint(0), panel->RoofPoint(1), gx, gy, ghost);
		re.glDisable(GL_BLEND);
		re.GLSL().LineShader().Select();
		re.GLSL().LineShader().SetProjectionModelView(m_tProjection * shipM);
		re.GLSL().LineShader().SetColor(.7f, 1, .7f, 1);
		DrawPoints(re, ghost, GL_LINES);
		re.GLSL().LineShader().Deselect();
		re.glEnable(GL_BLEND);
	}
	if(IsUIInputMode() && !Command() && underMouse() && (tool == AuOldShipPanel::TOOL_Detail
				|| tool == AuOldShipPanel::TOOL_EraseDetail || tool == AuOldShipPanel::TOOL_EraseDetails)
			&& OldShipPick(sceneAtom, m_MouseLast, gx, gy))
	{
		const int detailType = panel->ToolDetailType();
		const bool area = tool == AuOldShipPanel::TOOL_EraseDetails || (detailType >= 0 && OldShip::UsesBrush(detailType));
		std::vector<OldShip::Vec3> outline;
		const OldShip::GHOST ghost = OldShip::ToolGhost(detailType, area, panel->Brush(), outline, panel->ToolCommodity());
		double atX = gx, atY = gy;
		if(detailType >= 101 && detailType <= 140)
		{
			int tileX = 0, tileY = 0;
			OldShip::StairSnap(detailType, gx, gy, tileX, tileY);
			atX = tileX;
			atY = tileY;
		}
		AuMatrix4x4 mouseM(planeM);
		mouseM.Translate(atX * OldShip::GridUnit, atY * OldShip::GridUnit, OldShip::IconLift);

		OldShip::IconQuad quad;
		if(m_texOldShipIcons && detailType >= 0
				&& OldShip::DetailIconQuad(detailType, OldShip::Vec3(0, 0, 0), quad, panel->ToolCommodity()))
		{
			re.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			re.glDisable(GL_CULL_FACE);
			re.GLSL().TextureShader().Select();
			re.GLSL().TextureShader().SetProjectionModelView(m_tProjection * mouseM);
			re.GLSL().TextureShader().SetTexture(m_texOldShipIcons);
			re.GLSL().TextureShader().SetColor(1, 1, 1, .85f);
			DrawIcons(re, std::vector<OldShip::IconQuad>(1, quad));
			re.GLSL().TextureShader().Deselect();
			re.glEnable(GL_CULL_FACE);
		}

		re.glDisable(GL_BLEND);
		re.GLSL().LineShader().Select();
		re.GLSL().LineShader().SetProjectionModelView(m_tProjection * mouseM);
		if(detailType >= 0)
		{
			std::vector<OldShip::Vec3> figure;
			OldShip::StairFigure(detailType, figure);
			re.GLSL().LineShader().SetColor(.5f, .5f, .5f, 1);
			DrawPoints(re, figure, GL_LINES);
		}
		if(ghost == OldShip::GHOST_Footprint)
			re.GLSL().LineShader().SetColor(.7f, 1, .7f, 1);
		else
			re.GLSL().LineShader().SetColor(1, .8235f, 0, 1);
		DrawPoints(re, outline, GL_LINES);
		re.GLSL().LineShader().Deselect();
	}

	re.glDisable(GL_BLEND);
	re.glEnable(GL_CULL_FACE);
	re.glEnable(GL_DEPTH_TEST);
	re.glDepthMask(true);
}

void AuDesignerView::OldBuildingUpdateReport(AuOldShipPanel* panel)
{
	QElapsedTimer watch;
	watch.start();
	OldBuilding::Design named(panel->Building());
	if(named.name.empty())
	{
		QString fileName = QFileInfo(panel->FileName()).completeBaseName();
		if(fileName.isEmpty())
			fileName = "Old Building";
		named.name = fileName.toUtf8().constData();
	}
	const int revision = panel->Revision();
	QStringList log, msgs;
	OldShip::BuildingInfo info;
	info.industry = named.industry;
	info.material = panel->Material();
	bool hasRooms = false;
	for(size_t i = 0; i < named.rooms.size() && !hasRooms; i++)
		hasRooms = !named.rooms[i].IsRoof();
	if(!hasRooms)
	{
		panel->SetReport(OldShip::BuildingReportHtml(named, info, log, false, 0, msgs), revision);
		return;
	}
	AuBdDesign design;
	AuBdReport report;
	OldShip::BuildingOptions options;
	options.material = panel->Material();
	const bool converted = OldShip::ConvertBuilding(named, design, log, &info, &options);
	if(converted)
		OldShip::AnalyzeConvertedBuilding(design, report, msgs);
	const QList<OldShip::Problem> problems = OldShip::BuildingProblems(named, info);
	panel->SetReport(OldShip::BuildingReportHtml(named, info, log, converted, converted ? &report : 0, msgs, false, &problems), revision,
			problems);
	m_OldShipReportMs = (int)watch.elapsed();
}

void AuDesignerView::OldShipModeEntered()
{
	AuOldShipPanel* panel = OldShipPanel();
	if(panel && !panel->IsReportCurrent())
		OldShipScheduleReport();
}

void AuDesignerView::OldShipLookAt(double x, double y, int deck)
{
	AuOldShipPanel* panel = OldShipPanel();
	if(!panel || !IsInNewDesignEnvironment())
		return;
	const OldShip::Vec3& origin = panel->Origin();
	const AuPoint place(x - origin.x, y - origin.y, deck * OldShip::DeckHeight - origin.z);
	AuPose pose(AuPose::Stand);
	float headTilt = 0;
	AuMatrix4x4 stand;
	bool found = false;
	DaPerson* sceneAtom;
	LockSceneAtom(&sceneAtom);
	if(sceneAtom)
	{
		DaDesigner* pde = sceneAtom->FindParentDesignerInEnv();
		if(pde)
		{
			headTilt = sceneAtom->CurrentHeadTilt();
			pose = sceneAtom->CurrentGeneralPose();
			AuMatrix4x4 tperson;
			sceneAtom->GetTransformRelativeToParent(tperson, pde);
			const AuPoint here = tperson.Project(AuPoint(0, 0, 0));
			AuPoint forward = tperson.Project(AuPoint(0, 1, 0)) - here;
			forward.z = 0;
			const double length = sqrt(forward.x * forward.x + forward.y * forward.y);
			if(length > 1e-6)
				forward = AuPoint(forward.x / length, forward.y / length, 0);
			else
				forward = AuPoint(0, 1, 0);
			const AuPoint to(place.x - forward.x * 10.0, place.y - forward.y * 10.0, place.z);
			AuMatrix4x4 shift;
			shift.Translate(to.x - here.x, to.y - here.y, to.z - here.z);
			stand = shift * tperson;
			found = true;
		}
	}
	UnlockSceneAtom();
	if(!found)
		return;
	auApp->DesignerViewPositionGrid(SceneAtomId(), AuDesignPosition(stand, headTilt, pose));
}

void AuDesignerView::OldShipScheduleReport(int delayMs)
{
	if(!OldShipPanel())
		return;
	if(!m_OldShipReportTimer)
	{
		m_OldShipReportTimer = new QTimer(this);
		m_OldShipReportTimer->setSingleShot(true);
		connect(m_OldShipReportTimer, SIGNAL(timeout()), SLOT(OldShipUpdateReport()));
	}
	m_OldShipReportTimer->start(qMax(delayMs, qBound(ReportDelayMs, m_OldShipReportMs * 4, ReportDelayMaxMs)));
}

void AuDesignerView::OldShipUpdateReport()
{
	AuOldShipPanel* panel = OldShipPanel();
	if(!panel)
		return;
	if((QApplication::mouseButtons() & Qt::LeftButton) || QApplication::activeModalWidget())
	{
		OldShipScheduleReport();
		return;
	}
	if(panel->IsBuilding())
	{
		OldBuildingUpdateReport(panel);
		return;
	}
	QElapsedTimer watch;
	watch.start();
	OldShip::Design named(panel->Design());
	if(named.name.empty())
	{
		QString fileName = QFileInfo(panel->FileName()).completeBaseName();
		if(fileName.isEmpty())
			fileName = "Old Ship";
		named.name = fileName.toUtf8().constData();
	}
	const int revision = panel->Revision();
	QStringList log, msgs;
	OldShip::ConvertInfo info;
	if(named.rooms.empty())
	{
		panel->SetReport(OldShip::ReportHtml(named, info, log, false, 0, msgs), revision);
		return;
	}
	AuScDesign design;
	AuScReport report;
	const bool converted = OldShip::Convert(named, design, log, &info);
	if(converted)
		OldShip::AnalyzeConverted(design, report, msgs);
	const QList<OldShip::Problem> problems = OldShip::Problems(named, info);
	panel->SetReport(OldShip::ReportHtml(named, info, log, converted, converted ? &report : 0, msgs, false, &problems), revision,
			problems);
	m_OldShipReportMs = (int)watch.elapsed();
}

void AuDesignerView::OldShipDesignEdited()
{
	OldShipScheduleReport();
	AuOldShipPanel* panel = OldShipPanel();
	if(!panel || !panel->IsSolid())
		return;
	if(!m_OldShipTimer)
	{
		m_OldShipTimer = new QTimer(this);
		m_OldShipTimer->setSingleShot(true);
		connect(m_OldShipTimer, SIGNAL(timeout()), SLOT(OldShipRebuildModel()));
	}
	m_OldShipTimer->start(ModelDelayMs);
}

void AuDesignerView::OldShipRebuildModel()
{
	AuOldShipPanel* panel = OldShipPanel();
	if(!panel || !panel->IsSolid() || !panel->IsModelStale())
		return;
	if(QApplication::mouseButtons() & Qt::LeftButton)
	{
		m_OldShipTimer->start(ModelDelayMs);
		return;
	}
	OldShipMakeModel();
}

void AuDesignerView::OldShipRenderModeChanged(bool solid)
{
	AuOldShipPanel* panel = OldShipPanel();
	if(!panel)
		return;
	if(solid && panel->IsModelStale() && !OldShipMakeModel())
	{
		panel->ShowLines();
		g_Au.DesignerSetOldShipView(AuGlobal::OLDSHIP_Lines);
		return;
	}
	g_Au.DesignerSetOldShipView(solid ? AuGlobal::OLDSHIP_Solid : AuGlobal::OLDSHIP_Lines);
}

void AuDesignerView::OldShipModeLeft()
{
	if(m_OldShipTimer)
		m_OldShipTimer->stop();
	if(m_OldShipReportTimer)
		m_OldShipReportTimer->stop();
	m_OldShipCursor = QString();
	m_Cursor = CURSOR_Unknown;
}

bool AuDesignerView::OldShipAskToFinish()
{
	if(!OldShipPanel())
		return false;
	QMessageBox mb(g_Au.ApplicationName(), (OldShipPanel()->IsBuilding()
				? tr("<html>The old building designer is on. The design in the studio is only the test model of the old "
					"design.<br><br>Press <b>Finalize</b> in the Old Building Design panel to make it a blueprint. File, Save "
					"saves the old design as a building design file.</html>")
				: tr("<html>The old spacecraft designer is on. The design in the studio is only the test model of the old "
					"design.<br><br>Press <b>Finalize</b> in the Old Spacecraft Design panel to make it a blueprint. File, Save "
					"saves the old design as a .ship file.</html>")),
			QMessageBox::Information, QMessageBox::Ok, QMessageBox::NoButton, QMessageBox::NoButton, auApp->Frame());
	auApp->Frame()->ExecMessageBox(mb);
	return true;
}

bool AuDesignerView::OldShipMakeModel(bool blueprint, AuDesign** made)
{
	if(SceneContext() != SCENE_Designer)
		return false;
	AuDesignerFrame* frame = auDesignerApp->DesignerFrame();
	AuOldShipDock* oldShipDock = frame ? frame->OldShipDock() : 0;
	if(!oldShipDock)
		return false;
	AuOldShipPanel* panel = oldShipDock->Panel();
	OldShipScheduleReport();

	const bool firstModel = !panel->HasModel();
	if(firstModel && !DesignerConfirmLoseChanges())
		return false;

	const bool building = panel->IsBuilding();
	AuScDesign scDesign;
	AuBdDesign bdDesign;
	AuDesign& design = building ? static_cast<AuDesign&>(bdDesign) : static_cast<AuDesign&>(scDesign);
	QStringList log;
	OldShip::BuildingInfo info;
	bool converted;
	bool noRooms = true;
	for(size_t i = 0; i < panel->Design().rooms.size() && noRooms; i++)
		noRooms = building && OldShip::IsRoofType(panel->Design().rooms[i].type);
	if(noRooms && blueprint)
	{
		QMessageBox mb(g_Au.ApplicationName(), tr("The old design has no rooms yet: there is nothing to finalize."),
				QMessageBox::Information, QMessageBox::Ok, QMessageBox::NoButton, QMessageBox::NoButton, auApp->Frame());
		auApp->Frame()->ExecMessageBox(mb);
		return false;
	}
	if(noRooms)
	{
		if(building)
			bdDesign.SetStructure(BD_Building);
		else
			scDesign.SetStructure(SC_Spaceship);
		log.append(tr("The old design has no rooms yet."));
		converted = true;
	}
	else if(building)
	{
		OldBuilding::Design named(panel->Building());
		if(named.name.empty())
		{
			QString fileName = QFileInfo(panel->FileName()).completeBaseName();
			if(fileName.isEmpty())
				fileName = "Old Building";
			named.name = fileName.toUtf8().constData();
		}
		OldShip::BuildingOptions options;
		options.material = panel->Material();
		options.keepOrigin = !blueprint;
		QApplication::setOverrideCursor(Qt::WaitCursor);
		converted = OldShip::ConvertBuilding(named, bdDesign, log, &info, &options);
		QApplication::restoreOverrideCursor();
	}
	else
	{
		OldShip::Design named(panel->Design());
		if(named.name.empty())
		{
			QString fileName = QFileInfo(panel->FileName()).completeBaseName();
			if(fileName.isEmpty())
				fileName = "Old Ship";
			named.name = fileName.toUtf8().constData();
		}
		OldShip::ConvertOptions options;
		options.keepOrigin = !blueprint;
		QApplication::setOverrideCursor(Qt::WaitCursor);
		converted = OldShip::Convert(named, scDesign, log, &info, &options);
		QApplication::restoreOverrideCursor();
	}
	const OldShip::Vec3 origin(info.offset.x, info.offset.y, info.offset.z);

	if(!converted)
	{
		panel->SetConvertLog(log, false, origin);

		QString headline = blueprint ? tr("The old design could not be made into a blueprint.")
				: tr("The solid model of the old design could not be made. The view stays a line drawing.");
		bool tooBig = false;
		QString sizeLine;
		for(int i = 0; i < log.size(); i++)
		{
			if(log[i].startsWith("TOO BIG"))
				tooBig = true;
			if(log[i].startsWith("Size:"))
				sizeLine = log[i];
		}
		if(tooBig)
			headline = blueprint ? tr("The blueprint made of the old design is too big for the game. %1").arg(sizeLine)
					: tr("The solid model of the old design is too big for the game. %1 The view stays a line drawing.")
					.arg(sizeLine);
		AuOldShipLogDlg dlg(headline, log, auApp->Frame(), building);
		auApp->Frame()->ExecDialog(dlg);
		return false;
	}

	EndCommands();

	if(!auApp->DesignerLoadedFromFile(design))
	{
		panel->SetConvertLog(log, false, origin);

		QMessageBox mb(g_Au.ApplicationName(), blueprint ? tr("The blueprint data is too large. The file cannot be sent to the server.")
				: tr("The solid model of the old design is too large. The view stays a line drawing."),
				QMessageBox::Warning, QMessageBox::Ok, QMessageBox::NoButton, QMessageBox::NoButton, auApp->Frame());
		auApp->Frame()->ExecMessageBox(mb);
		return false;
	}
	panel->SetConvertLog(log, true, origin);

	m_DesignerFileName = "";
	if(blueprint)
		auApp->Frame()->setWindowTitle(QString("%1 %2 %3 (not saved)")
				.arg(g_Au.ApplicationName())
				.arg(g_Au.MiddleDotSymbol())
				.arg(design.Name()));
	else
		panel->UpdateTitle();
	if(made)
		*made = design.Clone();

	m_RenderInfo->DesignerClearPartSelection();
	frame->DesignerSetObjectMode(false);
	emit DesignerSelectionChanged();

	bool noteworthy = false;
	for(int i = 0; i < log.size() && !noteworthy; i++)
		noteworthy = AuOldShipPanel::IsLogLineNoteworthy(log[i]);
	if((firstModel || blueprint) && noteworthy)
	{
		AuOldShipLogDlg dlg(blueprint ? tr("The old design was made into a blueprint. Some of it needs your attention:")
				: tr("The solid model of the old design was made. Some of it needs your attention:"), log, auApp->Frame(), building);
		auApp->Frame()->ExecDialog(dlg);
	}
	return true;
}

void AuDesignerView::DesignerOldShipFinalize()
{
	AuOldShipPanel* panel = OldShipPanel();
	if(!panel)
		return;
	QMessageBox mb(g_Au.ApplicationName(), panel->IsBuilding()
				? tr("Do you want to finalize as is or do you want to move the building to the new editor?")
				: tr("Do you want to finalize as is or do you want to move the ship to the new editor?"),
			QMessageBox::Question, QMessageBox::NoButton, QMessageBox::NoButton, QMessageBox::NoButton, auApp->Frame());
	QAbstractButton* asIs = mb.addButton(tr("Finalize As Is"), QMessageBox::AcceptRole);
	QAbstractButton* move = mb.addButton(tr("Move to New Editor"), QMessageBox::AcceptRole);
	mb.addButton(QMessageBox::Cancel);
	mb.setDefaultButton(qobject_cast<QPushButton*>(asIs));
	auApp->Frame()->ExecMessageBox(mb);
	const bool quick = mb.clickedButton() == asIs;
	if(!quick && mb.clickedButton() != move)
		return;
	if(!panel->ConfirmPathsForFinalize() || !panel->ConfirmLoseChanges())
		return;

	AuDesign* blueprint = 0;
	if(!OldShipMakeModel(true, &blueprint))
		return;
	QSharedPointer<AuDesign> made(blueprint);
	auDesignerApp->DesignerFrame()->SetOldShipMode(false);
	if(!quick || !made)
		return;

	AuPoint madeMin, madeMax;
	made->Parts().ModelExtents(madeMin, madeMax);
	const DaDesignBase::DESIGNMODE mode = made->Type() == AuDesign::BLUEPRINT_Building ? DaDesignBase::DESIGN_Building
			: DaDesignBase::DESIGN_Spacecraft;
	OldShipFinalizeWhen([this, madeMin, made, mode](const AuDesign& studio)
		{
			if(studio.Type() != made->Type() || m_DesignMode != mode)
				return false;
			AuPoint min, max;
			studio.Parts().ModelExtents(min, max);
			return (min - madeMin).Length() < 0.01;
		},
		[this, made]()
		{
			if(!DesignerProperties(made.data()))
				return;
			OldShipFinalizeWhen([made](const AuDesign& studio)
				{
					QScopedPointer<AuDesign> probe(studio.Clone());
					if(!probe)
						return false;
					probe->SetProperties(*made);
					return probe->Pack() == studio.Pack();
				},
				[this]() { DesignerFinalize(); }, 50);
		}, 50);
}

void AuDesignerView::OldShipFinalizeWhen(const std::function<bool(const AuDesign&)>& ready, const std::function<void()>& then,
		int tries)
{
	if(OldShipStudioDesignIs(ready))
	{
		then();
		return;
	}
	if(tries <= 0)
	{
		QMessageBox mb(g_Au.ApplicationName(), tr("The blueprint is open in the editor, but the studio did not take it in time. "
					"Use Blueprint Properties and Finalize from the Design menu once it shows."),
				QMessageBox::Information, QMessageBox::Ok, QMessageBox::NoButton, QMessageBox::NoButton, auApp->Frame());
		auApp->Frame()->ExecMessageBox(mb);
		return;
	}
	QTimer::singleShot(200, this, [this, ready, then, tries]() { OldShipFinalizeWhen(ready, then, tries - 1); });
}

bool AuDesignerView::OldShipStudioDesignIs(const std::function<bool(const AuDesign&)>& test)
{
	bool is = false;
	DaPerson* person;
	LockSceneAtom(&person);
	if(person)
	{
		DaDesigner* env = person->FindParentDesignerInEnv();
		if(env)
		{
			const AuDesign* studio = env->LockDesignForRead();
			if(studio)
				is = test(*studio);
			env->UnlockDesign();
		}
	}
	UnlockSceneAtom();
	return is;
}

void AuDesignerView::DesignerImportOldShip()
{
	if(SceneContext() != SCENE_Designer)
		return;

	if(!DesignerConfirmLoseChanges())
		return;

	AuOldShipDock* oldShipDock = auDesignerApp->DesignerFrame()->OldDock(false);
	if(!oldShipDock || !oldShipDock->Panel()->ConfirmLoseChanges())
		return;

	QString shipFileName(oldShipDock->Panel()->FileName());
	if(shipFileName.isEmpty())
		shipFileName = g_Au.Settings().readEntry("/Designer/LastShipFile", "");
	if(shipFileName.isEmpty())
		shipFileName = QDir::currentPath();
	else
	{
		QFileInfo lastFileInfo(shipFileName);
		if(!lastFileInfo.exists())
			shipFileName = lastFileInfo.absolutePath();
	}

	QString loadFile = QFileDialog::getOpenFileName(this, tr("Old Spacecraft Design File"), shipFileName,
			"Old Spacecraft Designs (*.ship);;All Files (*.*)", 0,
			g_Au.UseNativeFileDialog() ? QFileDialog::Options() : QFileDialog::DontUseNativeDialog);
	if(loadFile.isEmpty())
		return;

	g_Au.Settings().writeEntry("/Designer/LastShipFile", loadFile);
	OldShipOpenFile(loadFile);
}

void AuDesignerView::OldShipOpenFile(const QString& shipFile)
{
	if(SceneContext() != SCENE_Designer)
		return;
	AuOldShipDock* oldShipDock = auDesignerApp->DesignerFrame()->OldDock(false);
	if(!oldShipDock)
		return;

	OldShip::Design ship;
	QString why;
	if(!AuOldShipPanel::LoadShipFile(shipFile, ship, why))
	{
		QMessageBox mb(g_Au.ApplicationName(), tr("The old spacecraft design file could not be read. %1").arg(why),
				QMessageBox::Warning, QMessageBox::Ok, QMessageBox::NoButton, QMessageBox::NoButton,
				auApp->Frame());
		auApp->Frame()->ExecMessageBox(mb);
		return;
	}

	EndCommands();
	oldShipDock->Panel()->SetDesign(ship, shipFile);
	auDesignerApp->DesignerFrame()->SetOldShipMode(true);
	OldShipMakeModel();
}

bool AuDesignerView::OldShipSave(bool askName)
{
	AuOldShipPanel* panel = OldShipPanel();
	if(!panel)
		return false;
	panel->Save(askName);
	return true;
}

void AuDesignerView::DesignerNewOldShip()
{
	if(SceneContext() != SCENE_Designer)
		return;

	if(!DesignerConfirmLoseChanges())
		return;

	AuOldShipDock* oldShipDock = auDesignerApp->DesignerFrame()->OldDock(false);
	if(!oldShipDock || !oldShipDock->Panel()->ConfirmLoseChanges())
		return;

	EndCommands();

	AuScDesign designRec;
	designRec.SetStructure(SC_Spaceship);

	m_DesignerFileName = "";
	auApp->DesignerNew(designRec);

	m_RenderInfo->DesignerClearPartSelection();
	auDesignerApp->DesignerFrame()->DesignerSetObjectMode(false);
	emit DesignerSelectionChanged();

	oldShipDock->Panel()->NewDesign();
	auDesignerApp->DesignerFrame()->SetOldShipMode(true);
	OldShipScheduleReport();
}

void AuDesignerView::DesignerImportOldBuilding()
{
	if(SceneContext() != SCENE_Designer)
		return;

	if(!DesignerConfirmLoseChanges())
		return;

	AuOldShipDock* oldBuildingDock = auDesignerApp->DesignerFrame()->OldDock(true);
	if(!oldBuildingDock || !oldBuildingDock->Panel()->ConfirmLoseChanges())
		return;

	QString buildingFileName(oldBuildingDock->Panel()->FileName());
	if(buildingFileName.isEmpty())
		buildingFileName = g_Au.Settings().readEntry("/Designer/LastBuildingFile", "");
	if(buildingFileName.isEmpty())
		buildingFileName = QDir::currentPath();
	else
	{
		QFileInfo lastFileInfo(buildingFileName);
		if(!lastFileInfo.exists())
			buildingFileName = lastFileInfo.absolutePath();
	}

	QString loadFile = QFileDialog::getOpenFileName(this, tr("Old Building Design File"), buildingFileName,
			AuOldShipPanel::BuildingFileFilter() + QString(";;All Files (*.*)"), 0,
			(g_Au.UseNativeFileDialog() ? QFileDialog::Options() : QFileDialog::Options(QFileDialog::DontUseNativeDialog))
				| QFileDialog::HideNameFilterDetails);
	if(loadFile.isEmpty())
		return;

	g_Au.Settings().writeEntry("/Designer/LastBuildingFile", loadFile);
	OldBuildingOpenFile(loadFile);
}

void AuDesignerView::OldBuildingOpenFile(const QString& buildingFile)
{
	if(SceneContext() != SCENE_Designer)
		return;
	AuOldShipDock* oldBuildingDock = auDesignerApp->DesignerFrame()->OldDock(true);
	if(!oldBuildingDock)
		return;

	OldBuilding::Design building;
	QString why;
	if(!AuOldShipPanel::LoadBuildingFile(buildingFile, building, why))
	{
		QMessageBox mb(g_Au.ApplicationName(), tr("The old building design file could not be read. %1").arg(why),
				QMessageBox::Warning, QMessageBox::Ok, QMessageBox::NoButton, QMessageBox::NoButton,
				auApp->Frame());
		auApp->Frame()->ExecMessageBox(mb);
		return;
	}

	EndCommands();
	oldBuildingDock->Panel()->SetBuilding(building, buildingFile);
	auDesignerApp->DesignerFrame()->SetOldBuildingMode(true);
	OldShipMakeModel();
}

void AuDesignerView::DesignerNewOldBuilding()
{
	if(SceneContext() != SCENE_Designer)
		return;

	if(!DesignerConfirmLoseChanges())
		return;

	AuOldShipDock* oldBuildingDock = auDesignerApp->DesignerFrame()->OldDock(true);
	if(!oldBuildingDock || !oldBuildingDock->Panel()->ConfirmLoseChanges())
		return;

	EndCommands();

	AuBdDesign designRec;
	designRec.SetStructure(BD_Building);

	m_DesignerFileName = "";
	auApp->DesignerNew(designRec);

	m_RenderInfo->DesignerClearPartSelection();
	auDesignerApp->DesignerFrame()->DesignerSetObjectMode(false);
	emit DesignerSelectionChanged();

	oldBuildingDock->Panel()->NewDesign();
	auDesignerApp->DesignerFrame()->SetOldBuildingMode(true);
	OldShipScheduleReport();
}
