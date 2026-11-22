// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

// Core pins a few actions under a surface's content for a decision that
// must not scroll away, and only for a shell that says it keeps them in
// view (vauchi/private#1304, contract 0.99.0). The surface draws the group
// under its scroll area and above the primary button, in Core's order; a
// bar without the group, which is every bar of an older Core, draws none.

#include "coreui/presentationsurface.h"

#include <QApplication>
#include <QBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QPushButton>
#include <QScrollArea>
#include <QStringList>
#include <cassert>

namespace {

QJsonObject action(const QString &id, const QString &label) {
    return {
        {"interaction_id", id},
        {"label", label},
        {"accessibility_label", label},
        {"icon_token", QJsonValue::Null},
        {"enabled", true},
        {"shortcut", QJsonValue::Null},
    };
}

QJsonObject scrollingSurface() {
    return {
        {"surface_id", "exchange"},
        {"revision", 1},
        {"title", "Exchange"},
        {"subtitle", QJsonValue::Null},
        {"accessibility_label", "Exchange"},
        {"layout", "scroll"},
        {"tokens", QJsonObject{}},
        {"nodes", QJsonArray{}},
    };
}

QJsonObject barPinning(const QJsonArray &pinned) {
    return {
        {"primary", action("primary", "Done")},
        {"pinned", pinned},
    };
}

QWidget *pinnedGroup(const PresentationSurface &surface) {
    return surface.findChild<QWidget *>("context-pinned");
}

QStringList pinnedLabels(const PresentationSurface &surface) {
    QStringList labels;
    const QWidget *group = pinnedGroup(surface);
    if (group == nullptr) return labels;
    for (int index = 0; index < group->layout()->count(); ++index) {
        const auto *button = qobject_cast<QPushButton *>(
            group->layout()->itemAt(index)->widget());
        assert(button != nullptr);
        labels.append(button->text());
    }
    return labels;
}

} // namespace

// @internal
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    const QJsonArray answers{action("pinned.0", "Yes, reconnect Bob"),
                             action("pinned.1", "No, stop the exchange")};

    {
        // In Core's order, outside the scroll area and above Primary.
        PresentationSurface surface(scrollingSurface(), barPinning(answers),
                                    /*sidebarShown=*/false);
        assert((pinnedLabels(surface)
                == QStringList{"Yes, reconnect Bob", "No, stop the exchange"}));

        QWidget *group = pinnedGroup(surface);
        auto *scroll = surface.findChild<QScrollArea *>();
        assert(scroll != nullptr);
        assert(!scroll->isAncestorOf(group));
        auto *outer = qobject_cast<QBoxLayout *>(surface.layout());
        auto *primary = surface.findChild<QPushButton *>("context-primary");
        assert(primary != nullptr);
        assert(outer->indexOf(scroll) < outer->indexOf(group));
        assert(outer->indexOf(group) < outer->indexOf(primary));

        QString reported;
        QObject::connect(&surface, &PresentationSurface::interactionReady,
                         [&reported](const QString &, const QString &id) {
                             reported = id;
                         });
        auto *no = surface.findChild<QPushButton *>("context-pinned-1");
        assert(no != nullptr);
        assert(no->accessibleName() == QStringLiteral("No, stop the exchange"));
        no->click();
        assert(reported == QStringLiteral("pinned.1"));
    }

    {
        // No group, or an empty one: nothing is drawn.
        PresentationSurface older(scrollingSurface(),
                                  QJsonObject{{"primary", action("primary", "Done")}},
                                  /*sidebarShown=*/false);
        assert(pinnedGroup(older) == nullptr);
        PresentationSurface empty(scrollingSurface(), barPinning(QJsonArray{}),
                                  /*sidebarShown=*/false);
        assert(pinnedGroup(empty) == nullptr);
    }

    {
        // A new bar on the same surface: the group goes when Core takes
        // it away and comes back above Primary when Core sends it again.
        PresentationSurface surface(scrollingSurface(), barPinning(answers),
                                    /*sidebarShown=*/false);
        assert(surface.update(
            scrollingSurface(),
            QJsonObject{{"primary", action("primary", "Done")}},
            /*sidebarShown=*/false));
        assert(pinnedGroup(surface) == nullptr);

        assert(surface.update(
            scrollingSurface(),
            barPinning(QJsonArray{action("pinned.0", "No, stop the exchange")}),
            /*sidebarShown=*/false));
        assert(pinnedLabels(surface) == QStringList{"No, stop the exchange"});
        auto *outer = qobject_cast<QBoxLayout *>(surface.layout());
        assert(outer->indexOf(pinnedGroup(surface))
               < outer->indexOf(
                   surface.findChild<QPushButton *>("context-primary")));
    }

    return 0;
}
