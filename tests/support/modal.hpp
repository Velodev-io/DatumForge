#pragma once
#include <QAbstractEventDispatcher>
#include <QApplication>
#include <QDebug>
#include <QDialog>
#include <QScopeGuard>
#include <QThread>
#include <QTimer>
#include <QWindow>
#include <functional>
#include <memory>
namespace cad::test {
// A native widget constructor can process events before exec() starts. Poll
// readiness instead of consuming a one-shot callback before the modal exists.
inline std::unique_ptr<QTimer> whenModal(QObject *owner, std::function<void(QDialog *)> action,
                                         QString objectName = {}) {
    auto timer = std::make_unique<QTimer>();
    auto *poll = timer.get();
    poll->setInterval(5);
    poll->setTimerType(Qt::PreciseTimer);
    int beforeLoop = QThread::currentThread()->loopLevel();
    QObject::connect(
        poll, &QTimer::timeout, owner,
        [poll, action = std::move(action), objectName, beforeLoop, ticks = 0]() mutable {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (qEnvironmentVariableIsSet("CAD_MODAL_TRACE") && (++ticks == 1 || ticks == 100))
                qInfo() << "Modal readiness" << beforeLoop << QThread::currentThread()->loopLevel()
                        << (dialog ? dialog->objectName() : QString("none"))
                        << (dialog && dialog->isVisible())
                        << (dialog && dialog->windowHandle() &&
                            dialog->windowHandle()->isExposed());
            if (QThread::currentThread()->loopLevel() <= beforeLoop || !dialog ||
                !dialog->isVisible() || !dialog->windowHandle() ||
                !dialog->windowHandle()->isExposed() ||
                (!objectName.isEmpty() && dialog->objectName() != objectName))
                return;
            poll->stop();
            if (qEnvironmentVariableIsSet("CAD_MODAL_TRACE"))
                qInfo() << "Modal action entered" << dialog->objectName();
            auto close = qScopeGuard([&] {
                if (dialog->isVisible())
                    dialog->reject();
            });
            action(dialog);
            // Nested test waits can consume Cocoa's close interrupt. Recheck the
            // enclosing event loop after the callback; its exit state remains owned
            // by QDialog, so this cannot accept or close a still-visible editor.
            if (!dialog->isVisible())
                QAbstractEventDispatcher::instance()->interrupt();
            if (qEnvironmentVariableIsSet("CAD_MODAL_TRACE"))
                qInfo() << "Modal action returned" << dialog->objectName() << dialog->isVisible();
        });
    poll->start();
    return timer;
}
} // namespace cad::test
