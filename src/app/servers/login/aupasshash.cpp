#include "aupasshash.h"
//
#include <QCryptographicHash>
#include <QList>
#include <QPasswordDigestor>
#include <QRandomGenerator>
#include <QtGlobal>

static const char* s_Prefix = "$pbkdf2-sha256$";
static const int s_SaltBytes = 16;
static const int s_HashBytes = 32;
static const int s_DefaultIterations = 310000;
static const int s_MaxIterations = 10000000;

int AuPassHash::Iterations()
{
	static const int iterations = qBound(10000,
			qEnvironmentVariableIsSet("AU_PASSWORD_ITERATIONS") ? qEnvironmentVariableIntValue("AU_PASSWORD_ITERATIONS")
			: s_DefaultIterations,
			s_MaxIterations);
	return iterations;
}

bool AuPassHash::IsProtected(const QByteArray& stored)
{
	return stored.startsWith(s_Prefix);
}

bool AuPassHash::ConstantTimeEquals(const QByteArray& a, const QByteArray& b)
{
	if(a.size() != b.size())
		return false;
	unsigned char diff = 0;
	for(int i=0; i<a.size(); i++)
		diff |= (unsigned char)(a[i] ^ b[i]);
	return diff == 0;
}

QByteArray AuPassHash::Protect(const QByteArray& passCrypt)
{
	if(passCrypt.isEmpty())
		return QByteArray();

	QByteArray salt(s_SaltBytes, 0);
	for(int i=0; i<salt.size(); i++)
		salt[i] = (char)QRandomGenerator::system()->bounded(256);

	const int iterations = Iterations();
	QByteArray hash = QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256, passCrypt, salt, iterations, s_HashBytes);

	QByteArray stored(s_Prefix);
	stored += QByteArray::number(iterations);
	stored += '$';
	stored += salt.toBase64();
	stored += '$';
	stored += hash.toBase64();
	return stored;
}

bool AuPassHash::Verify(const QByteArray& stored, const QByteArray& passCrypt)
{
	if(stored.isEmpty() || passCrypt.isEmpty())
		return false;

	if(!IsProtected(stored))
		return ConstantTimeEquals(stored, passCrypt);

	QList<QByteArray> parts = stored.split('$');
	if(parts.count() != 5)
		return false;
	bool ok = false;
	int iterations = parts[2].toInt(&ok);
	if(!ok || iterations < 1 || iterations > s_MaxIterations)
		return false;
	QByteArray salt = QByteArray::fromBase64(parts[3]);
	QByteArray want = QByteArray::fromBase64(parts[4]);
	if(salt.size() < s_SaltBytes || want.size() != s_HashBytes)
		return false;

	QByteArray got = QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256, passCrypt, salt, iterations, want.size());
	return ConstantTimeEquals(got, want);
}

bool AuPassHash::NeedsUpgrade(const QByteArray& stored)
{
	if(!IsProtected(stored))
		return true;
	QList<QByteArray> parts = stored.split('$');
	if(parts.count() != 5)
		return true;
	return parts[2].toInt() < Iterations();
}
