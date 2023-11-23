// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wquickwaylandserver.h>
#include <qwglobal.h>

#include <QObject>
#include <QQmlEngine>

Q_MOC_INCLUDE("wsurface.h")

QW_BEGIN_NAMESPACE
class QWForeignToplevelManagerV1;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WXdgSurface;
class WOutput;
class WQuickForeignToplevelManagerV1Private;
class WQuickForeignToplevelManagerV1 : public WQuickWaylandServerInterface, public WObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ForeignToplevelManagerV1)
    W_DECLARE_PRIVATE(WQuickForeignToplevelManagerV1)

public:
    explicit WQuickForeignToplevelManagerV1(QObject *parent = nullptr);

    Q_INVOKABLE void add(WXdgSurface *surface);
    Q_INVOKABLE void remove(WXdgSurface *surface);

private:
    void create() override;
};

WAYLIB_SERVER_END_NAMESPACE