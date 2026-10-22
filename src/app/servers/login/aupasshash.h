#ifndef AuPassHash_H
#define AuPassHash_H

#include <QByteArray>

class AuPassHash
{
public:
	static QByteArray Protect(const QByteArray& passCrypt);
	static bool Verify(const QByteArray& stored, const QByteArray& passCrypt);
	static bool NeedsUpgrade(const QByteArray& stored);
	static int Iterations();

private:
	static bool IsProtected(const QByteArray& stored);
	static bool ConstantTimeEquals(const QByteArray& a, const QByteArray& b);
};

#endif // AuPassHash_H
