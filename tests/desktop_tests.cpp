#include "chat_controller.hpp"
#include "server_fixture.hpp"

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QtTest/qtest.h>

class DesktopTests : public QObject {
    Q_OBJECT
  private slots:
    void model() {
        MessageModel model;
        const QAbstractItemModelTester tester(
            &model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        qchat::rpc::MessagePage page;
        qchat::rpc::Message message;
        message.__set_id(1);
        message.__set_displayName("世界");
        message.__set_text("<b>plain text</b>");
        page.messages.push_back(message);
        model.apply(page);
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.data(model.index(0), MessageModel::NameRole).toString(),
                 QStringLiteral("世界"));
        QCOMPARE(model.data(model.index(0), MessageModel::TextRole).toString(),
                 QStringLiteral("<b>plain text</b>"));
        QVERIFY(!model.data(model.index(0), MessageModel::TimeRole).toString().isEmpty());
        QCOMPARE(model.roleNames().value(MessageModel::TextRole), QByteArray("body"));
        QVERIFY(!model.data({}, MessageModel::NameRole).isValid());
        QVERIFY(!model.data(model.index(0), -1).isValid());
        QCOMPARE(model.rowCount(model.index(0)), 0);
        model.apply(page);
        QCOMPARE(model.rowCount(), 1);
        for (int i = 2; i <= 1100; ++i) {
            message.__set_id(i);
            page.messages = {message};
            model.apply(page);
        }
        QCOMPARE(model.rowCount(), 1000);
        page.reset = true;
        page.messages.clear();
        model.apply(page);
        QCOMPARE(model.rowCount(), 0);
    }

    void invalid_connection() {
        ChatController controller;
        controller.connectToServer(QStringLiteral("hostname"), 9090, QStringLiteral("A"));
        QVERIFY(!controller.busy());
        QVERIFY(!controller.error().isEmpty());
        controller.connectToServer(QStringLiteral("127.0.0.1"), 0, QStringLiteral("A"));
        QVERIFY(!controller.busy());
        controller.connectToServer(QStringLiteral("127.0.0.1"), 9090, QStringLiteral(" "));
        QVERIFY(!controller.busy());
        controller.sendMessage(QStringLiteral("Offline"));
        QVERIFY(!controller.sending());
    }

    void two_clients() {
        ServerFixture server;
        ChatController alice;
        ChatController bob;
        const QSignalSpy sent(&alice, &ChatController::messageSent);
        alice.connectToServer(QStringLiteral("127.0.0.1"), server.port(), QStringLiteral("Alice"));
        // A second connect and disconnect during the initial request are ignored.
        alice.connectToServer(QStringLiteral("127.0.0.1"), server.port(), QStringLiteral("Other"));
        alice.disconnectFromServer();
        bob.connectToServer(QStringLiteral("127.0.0.1"), server.port(), QStringLiteral("Bob"));
        QTRY_VERIFY_WITH_TIMEOUT(alice.connected() && bob.connected(), 5000);
        alice.sendMessage(QStringLiteral(" \n "));
        QVERIFY(!alice.sending());
        QVERIFY(!alice.error().isEmpty());
        alice.sendMessage(QStringLiteral("Hello from Qt 👋"));
        alice.sendMessage(QStringLiteral("Duplicate click"));
        alice.disconnectFromServer();
        QTRY_COMPARE_WITH_TIMEOUT(sent.count(), 1, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(bob.messages()->rowCount(), 1, 5000);
        QCOMPARE(bob.messages()->data(bob.messages()->index(0), MessageModel::TextRole).toString(),
                 QStringLiteral("Hello from Qt 👋"));
        alice.disconnectFromServer();
        QVERIFY(!alice.connected());
        alice.connectToServer(QStringLiteral("127.0.0.1"), server.port(), QStringLiteral("Alice"));
        QTRY_VERIFY_WITH_TIMEOUT(alice.connected(), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(alice.messages()->rowCount(), 1, 5000);
    }

    void worker_failures() {
        NetworkWorker worker;
        const QSignalSpy failed(&worker, &NetworkWorker::failed);
        const QSignalSpy rejected(&worker, &NetworkWorker::rejected);
        const QSignalSpy opened(&worker, &NetworkWorker::opened);
        worker.poll();
        worker.send(QStringLiteral("A"), QStringLiteral("Offline"));
        QCOMPARE(failed.count(), 1);
        int closed_port = 0;
        {
            ServerFixture server;
            closed_port = server.port();
            worker.open(QStringLiteral("127.0.0.1"), closed_port);
            QCOMPARE(opened.count(), 1);
            worker.send(QString(), QStringLiteral("Rejected name"));
            QCOMPARE(rejected.count(), 1);
        }
        worker.send(QStringLiteral("A"), QStringLiteral("Delivery cannot be confirmed"));
        QCOMPARE(failed.count(), 2);
        worker.open(QStringLiteral("127.0.0.1"), closed_port);
        QCOMPARE(failed.count(), 3);
        worker.close();
    }

    void server_loss() {
        ChatController controller;
        {
            ServerFixture server;
            controller.connectToServer(QStringLiteral("127.0.0.1"), server.port(),
                                       QStringLiteral("A"));
            QTRY_VERIFY_WITH_TIMEOUT(controller.connected(), 5000);
        }
        QTRY_VERIFY_WITH_TIMEOUT(!controller.connected(), 5000);
        QVERIFY(!controller.error().isEmpty());
        QVERIFY(!controller.sending());
    }
};

QTEST_GUILESS_MAIN(DesktopTests)
#include "desktop_tests.moc"
