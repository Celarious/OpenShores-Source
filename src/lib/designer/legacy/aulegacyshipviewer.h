#ifndef AuLegacyShipViewer_H
#define AuLegacyShipViewer_H

#include <QMainWindow>
#include <QString>
#include <QVector>
#include <functional>

class QCloseEvent;
class AuCommPacket;
namespace AuLegacyShip { struct Design; }
namespace AuLegacyStudioProtocol { struct Request; struct LibraryEntry; }

// Editor for the recovered 2016 authoring format. Shared studios apply server
// snapshots while standalone editing keeps the original local file workflow.
class AuLegacyShipViewer : public QMainWindow
{
public:
	explicit AuLegacyShipViewer(QWidget* parent = nullptr);
	~AuLegacyShipViewer() override;

	bool OpenFile(const QString& fileName);
	void NewDesign();
	void ChooseFile();
	void SetExitHandler(const std::function<void()>& handler);
	bool ConfirmDiscardChanges();
	void ControlsChanged();
	void ToggleTools();
	void SetStudioCommandHandler(const std::function<void(AuLegacyStudioProtocol::Request)>& handler);
	void ApplyStudioState(const AuLegacyShip::Design& design, bool owner, bool modified,
		quint32 generation, quint64 revision);
	void SetStudioConnected(bool connected);
	void SetStudioBusy(bool busy);
	void SetStudioBurnAvailable(bool available);
	void SetStudioLibraryDesignId(quint32 designId);
	void ApplyStudioLibrary(const QVector<AuLegacyStudioProtocol::LibraryEntry>& entries, bool truncated);
	void ShowStudioMessage(const QString& message);
	void ChatReceived(AuCommPacket* message);

protected:
	void showEvent(QShowEvent* event) override;
	void closeEvent(QCloseEvent* event) override;

private:
	class Impl;
	Impl* m_Impl;
};

#endif // AuLegacyShipViewer_H
