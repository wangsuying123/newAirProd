#ifndef MACHINELOCKAUTHORIZER_H
#define MACHINELOCKAUTHORIZER_H

#include <QMainWindow>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QDateTimeEdit>
#include <QGroupBox>
#include "machinelockmanager.h"

class MachineLockAuthorizer : public QMainWindow
{
    Q_OBJECT

public:
    explicit MachineLockAuthorizer(QWidget *parent = nullptr);
    ~MachineLockAuthorizer();

private slots:
    void onGetFingerprintClicked();
    void onGenerateLicenseClicked();
    void onVerifyLicenseClicked();

private:
    void setupUI();
    void centerWindow();
    void appendLog(const QString &message, const QString &type = "INFO");

    QTextEdit     *m_logTextEdit;
    QTextEdit     *m_fingerprintTextEdit;
    QPushButton   *m_getFingerprintButton;
    QPushButton   *m_generateLicenseButton;
    QPushButton   *m_verifyLicenseButton;
    QLineEdit     *m_outputPathEdit;
    QPushButton   *m_browseButton;
    QCheckBox     *m_permanentCheckBox;
    QDateTimeEdit *m_startDateTimeEdit;
    QDateTimeEdit *m_endDateTimeEdit;
    QGroupBox     *m_timeGroupBox;

    MachineLockManager *m_lockManager;
};

#endif // MACHINELOCKAUTHORIZER_H
