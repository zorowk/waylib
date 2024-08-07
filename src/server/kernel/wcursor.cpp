// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#define private public
#include <QCursor>
#undef private

#include "wcursor.h"
#include "private/wcursor_p.h"
#include "winputdevice.h"
#include "wimagebuffer.h"
#include "wseat.h"
#include "woutput.h"
#include "woutputlayout.h"

#include <qwbuffer.h>
#include <qwcompositor.h>
#include <qwcursor.h>
#include <qwoutput.h>
#include <qwxcursormanager.h>
#include <qwoutputlayout.h>
#include <qwinputdevice.h>
#include <qwpointer.h>
#include <qwtouch.h>
#include <qwseat.h>

#include <QPixmap>
#include <QCoreApplication>
#include <QQuickWindow>
#include <QLoggingCategory>
#include <private/qcursor_p.h>

extern "C" {
#define static
#include <wlr/types/wlr_cursor.h>
#undef static
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_touch.h>
}

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(qLcWlrCursor, "waylib.server.cursor", QtWarningMsg)

inline QWBuffer* createImageBuffer(QImage image)
{
    auto *bufferImpl = new WImageBufferImpl(image);
    return QWBuffer::create(bufferImpl, image.width(), image.height());
}

WCursorPrivate::WCursorPrivate(WCursor *qq)
    : WWrapObjectPrivate(qq)
{
    initHandle(new QWCursor());
    handle()->setData(this, qq);
}

WCursorPrivate::~WCursorPrivate()
{
    delete handle();
}

void WCursorPrivate::instantRelease()
{
    handle()->setData(nullptr, nullptr);
    if (seat)
        seat->setCursor(nullptr);

    if (outputLayout) {
        for (auto o : outputLayout->outputs())
            o->removeCursor(q_func());
    }
}

const char *WCursorPrivate::checkTypeAndFallback(const char *name)
{
    if (!xcursor_manager)
        return nullptr;

    if (xcursor_manager->getXCursor(name, 1.0)) {
        return name;
    }

    static const std::vector<std::vector<const char*>> typeLists = {
        {"ibeam", "text", "xterm"},
        {"openhand", "grab"},
        {"crosshair", "cross", "all-scroll"},
        {"closedhand", "dnd-move", "move", "dnd-none", "grabbing"},
        {"dnd-copy", "copy"},
        {"dnd-link", "link"},
        {"row-resize", "size_ver", "ns-resize", "split_v", "n-resize", "s-resize"},
        {"col-resize", "size_hor", "ew-resize", "split_h", "e-resize", "w-resize"},
        {"nwse-resize", "nw-resize", "se-resize", "size_fdiag"},
        {"progress", "wait", "watch"},
        {"hand1", "hand2", "pointer", "pointing_hand"},
    };

    for (const auto &typeList : std::as_const(typeLists)) {
        bool hasType = std::find_if(typeList.begin(), typeList.end(), [name](const char *type) {
            return std::strcmp(type, name) == 0;
        }) != typeList.end();

        if(!hasType)
            continue;

        auto it = std::find_if(typeList.begin(), typeList.end(), [this](const char *type) {
            return xcursor_manager->getXCursor(type, 1.0) != nullptr;
        });

        if (it == typeList.end())
            return nullptr;

        qCDebug(qLcWlrCursor) << "Can't load cursor `" << name
                              <<"`, use `" << *it << "` as fallback";
        return *it;
    }

    return nullptr;
}

void WCursorPrivate::setType(const char *name)
{
    W_Q(WCursor);

    if (!xcursor_manager)
        return;

    // FIXME: Prevent the cursor from being black in some situations，but why need unsetImage manually?
    handle()->unsetImage();
    handle()->setXCursor(xcursor_manager, name);
}

static inline const char *qcursorShapeToType(std::underlying_type_t<WCursor::CursorShape> shape) {
    switch (shape) {
    case Qt::ArrowCursor:
        return "left_ptr";
    case Qt::UpArrowCursor:
        return "up_arrow";
    case Qt::CrossCursor:
        return "cross";
    case Qt::WaitCursor:
        return "wait";
    case Qt::IBeamCursor:
        return "ibeam";
    case Qt::SizeAllCursor:
        return "size_all";
    case Qt::BlankCursor:
        return "blank";
    case Qt::PointingHandCursor:
        return "pointing_hand";
    case Qt::SizeBDiagCursor:
        return "size_bdiag";
    case Qt::SizeFDiagCursor:
        return "size_fdiag";
    case Qt::SizeVerCursor:
        return "size_ver";
    case Qt::SplitVCursor:
        return "split_v";
    case Qt::SizeHorCursor:
        return "size_hor";
    case Qt::SplitHCursor:
        return "split_h";
    case Qt::WhatsThisCursor:
        return "whats_this";
    case Qt::ForbiddenCursor:
        return "forbidden";
    case Qt::BusyCursor:
        return "left_ptr_watch";
    case Qt::OpenHandCursor:
        return "openhand";
    case Qt::ClosedHandCursor:
        return "closedhand";
    case Qt::DragCopyCursor:
        return "dnd-copy";
    case Qt::DragMoveCursor:
        return "dnd-move";
    case Qt::DragLinkCursor:
        return "dnd-link";
    case WCursor::Default:
        return "default";
    case WCursor::BottomLeftCorner:
        return "bottom_left_corner";
    case WCursor::BottomRightCorner:
        return "bottom_right_corner";
    case WCursor::TopLeftCorner:
        return "top_left_corner";
    case WCursor::TopRightCorner:
        return "top_right_corner";
    case WCursor::BottomSide:
        return "bottom_side";
    case WCursor::LeftSide:
        return "left_side";
    case WCursor::RightSide:
        return "right_side";
    case WCursor::TopSide:
        return "top_side";
    case WCursor::Grabbing:
        return "grabbing";
    case WCursor::Xterm:
        return "xterm";
    case WCursor::Hand1:
        return "hand1";
    case WCursor::Watch:
        return "watch";
    case WCursor::SWResize:
        return "sw-resize";
    case WCursor::SEResize:
        return "se-resize";
    case WCursor::SResize:
        return "s-resize";
    case WCursor::WResize:
        return "w-resize";
    case WCursor::EResize:
        return "e-resize";
    case WCursor::EWResize:
        return "ew-resize";
    case WCursor::NWResize:
        return "nw-resize";
    case WCursor::NWSEResize:
        return "nwse-resize";
    case WCursor::NEResize:
        return "ne-resize";
    case WCursor::NESWResize:
        return "nesw-resize";
    case WCursor::NSResize:
        return "ns-resize";
    case WCursor::NResize:
        return "n-resize";
    case WCursor::AllScroll:
        return "all-scroll";
    case WCursor::Text:
        return "text";
    case WCursor::Pointer:
        return "pointer";
    case WCursor::Wait:
        return "wait";
    case WCursor::ContextMenu:
        return "context-menu";
    case WCursor::Help:
        return "help";
    case WCursor::Progress:
        return "progress";
    case WCursor::Cell:
        return "cell";
    case WCursor::Crosshair:
        return "crosshair";
    case WCursor::VerticalText:
        return "vertical-text";
    case WCursor::Alias:
        return "alias";
    case WCursor::Copy:
        return "copy";
    case WCursor::Move:
        return "move";
    case WCursor::NoDrop:
        return "no-drop";
    case WCursor::NotAllowed:
        return "not-allowed";
    case WCursor::Grab:
        return "grab";
    case WCursor::ColResize:
        return "col-resize";
    case WCursor::RowResize:
        return "row-resize";
    case WCursor::ZoomIn:
        return "zoom-in";
    case WCursor::ZoomOut:
        return "zoom-out";
    default:
        break;
    }

    return nullptr;
}

void WCursorPrivate::updateCursorImage()
{
    if (!outputLayout)
        return;

    if (seat && seat->pointerFocusSurface()) {
        auto typeName = qcursorShapeToType(shape);
        if (typeName) {
            if (auto checkedName = checkTypeAndFallback(typeName))
                setType(checkedName);
            else {
                qCWarning(qLcWlrCursor) << "Can't load cursor `" << typeName
                                        << "`, Use `default` as fallback";
                setType("default");
            }
        }
        return; // Using the wl_client's cursor resource if shape is Invalid
    }

    surfaceOfCursor.clear();

    if (!visible)
        return;

    if (auto typeName = checkTypeAndFallback(qcursorShapeToType(cursor.shape()))) {
        setType(typeName);
    } else {
        const QImage &img = cursor.pixmap().toImage();
        if (img.isNull())
            return;

        auto *imageBuffer = createImageBuffer(img);
        handle()->setBuffer(imageBuffer, cursor.hotSpot(), img.devicePixelRatio());
    }
}

const QPointingDevice *getDevice(const QString &seatName) {
    const auto devices = QInputDevice::devices();
    for (auto i : devices) {
        if (i->seatName() == seatName && (i->type() == QInputDevice::DeviceType::Mouse
                                          || i->type() == QInputDevice::DeviceType::TouchPad))
            return static_cast<const QPointingDevice*>(i);
    }

    return nullptr;
}

void WCursorPrivate::sendEnterEvent() {
    if (enterWindowEventHasSend)
        return;

    W_Q(WCursor);

    auto device = getDevice(seat->name());
    Q_ASSERT(device && WInputDevice::from(device));

    if (WInputDevice::from(device)->seat()) {
        enterWindowEventHasSend = true;
        const QPointF global = q->position();
        const QPointF local = global - eventWindow->position();
        QEnterEvent event(local, local, global, device);
        QCoreApplication::sendEvent(eventWindow, &event);
    }
}

void WCursorPrivate::sendLeaveEvent()
{
    if (leaveWindowEventHasSend)
        return;

    auto device = getDevice(seat->name());
    if (!device)
        return;
    if (!WInputDevice::from(device))
        return;
    if (WInputDevice::from(device)->seat()) {
        leaveWindowEventHasSend = true;
        QInputEvent event(QEvent::Leave, device);
        QCoreApplication::sendEvent(eventWindow, &event);
    }
}

void WCursorPrivate::on_motion(wlr_pointer_motion_event *event)
{
    auto device = QWPointer::from(event->pointer);
    q_func()->move(device, QPointF(event->delta_x, event->delta_y));
    processCursorMotion(device, event->time_msec);
}

void WCursorPrivate::on_motion_absolute(wlr_pointer_motion_absolute_event *event)
{
    auto device = QWPointer::from(event->pointer);
    q_func()->setScalePosition(device, QPointF(event->x, event->y));
    processCursorMotion(device, event->time_msec);
}

void WCursorPrivate::on_button(wlr_pointer_button_event *event)
{
    auto device = QWPointer::from(event->pointer);
    button = WCursor::fromNativeButton(event->button);

    if (event->state == WLR_BUTTON_RELEASED) {
        state &= ~button;
    } else {
        state |= button;
        lastPressedOrTouchDownPosition = q_func()->position();
    }

    if (Q_LIKELY(seat)) {
        seat->notifyButton(q_func(), WInputDevice::fromHandle(device),
                           button, event->state, event->time_msec);
    }
}

void WCursorPrivate::on_axis(wlr_pointer_axis_event *event)
{
    auto device = QWPointer::from(event->pointer);

    if (Q_LIKELY(seat)) {
        seat->notifyAxis(q_func(), WInputDevice::fromHandle(device), event->source,
                         event->orientation == WLR_AXIS_ORIENTATION_HORIZONTAL
                         ? Qt::Horizontal : Qt::Vertical, event->delta, event->delta_discrete,
                         event->time_msec);
    }
}

void WCursorPrivate::on_frame()
{
    if (Q_LIKELY(seat)) {
        seat->notifyFrame(q_func());
    }
}

void WCursorPrivate::on_swipe_begin(wlr_pointer_swipe_begin_event *event)
{
    auto device = QWPointer::from(event->pointer);
    if (Q_LIKELY(seat)) {
        seat->notifyGestureBegin(q_func(), WInputDevice::fromHandle(device),
                               event->time_msec, event->fingers, WGestureEvent::SwipeGesture);
    }
}

void WCursorPrivate::on_swipe_update(wlr_pointer_swipe_update_event *event)
{
    auto device = QWPointer::from(event->pointer);
    if (Q_LIKELY(seat)) {
        QPointF delta = QPointF(event->dx, event->dy);
        seat->notifyGestureUpdate(q_func(), WInputDevice::fromHandle(device),
                                event->time_msec, delta, 0, 0, WGestureEvent::SwipeGesture);
    }
}

void WCursorPrivate::on_swipe_end(wlr_pointer_swipe_end_event *event)
{
    auto device = QWPointer::from(event->pointer);
    if (Q_LIKELY(seat)) {
        seat->notifyGestureEnd(q_func(), WInputDevice::fromHandle(device),
                             event->time_msec, event->cancelled, WGestureEvent::SwipeGesture);
    }
}

void WCursorPrivate::on_pinch_begin(wlr_pointer_pinch_begin_event *event)
{
    auto device = QWPointer::from(event->pointer);
    if (Q_LIKELY(seat)) {
        seat->notifyGestureBegin(q_func(), WInputDevice::fromHandle(device),
                              event->time_msec, event->fingers, WGestureEvent::PinchGesture);
    }
}

void WCursorPrivate::on_pinch_update(wlr_pointer_pinch_update_event *event)
{
    auto device = QWPointer::from(event->pointer);
    if (Q_LIKELY(seat)) {
        QPointF delta = QPointF(event->dx, event->dy);
        seat->notifyGestureUpdate(q_func(), WInputDevice::fromHandle(device),
                                event->time_msec, delta, event->scale, event->rotation,
                                WGestureEvent::PinchGesture);
    }
}

void WCursorPrivate::on_pinch_end(wlr_pointer_pinch_end_event *event)
{
    auto device = QWPointer::from(event->pointer);
    if (Q_LIKELY(seat)) {
        seat->notifyGestureEnd(q_func(), WInputDevice::fromHandle(device),
                             event->time_msec, event->cancelled,
                             WGestureEvent::PinchGesture);
    }
}

void WCursorPrivate::on_hold_begin(wlr_pointer_hold_begin_event *event)
{
    auto device = QWPointer::from(event->pointer);
    if (Q_LIKELY(seat)) {
        seat->notifyHoldBegin(q_func(), WInputDevice::fromHandle(device),
                              event->time_msec, event->fingers);
    }
}

void WCursorPrivate::on_hold_end(wlr_pointer_hold_end_event *event)
{
    auto device = QWPointer::from(event->pointer);
    if (Q_LIKELY(seat)) {
        seat->notifyHoldEnd(q_func(), WInputDevice::fromHandle(device),
                            event->time_msec, event->cancelled);
    }
}

void WCursorPrivate::on_touch_down(wlr_touch_down_event *event)
{
    auto device = QWTouch::from(event->touch);

    q_func()->setScalePosition(device, QPointF(event->x, event->y));
    lastPressedOrTouchDownPosition = q_func()->position();

    if (Q_LIKELY(seat)) {
        seat->notifyTouchDown(q_func(), WInputDevice::fromHandle(device),
                              event->touch_id, event->time_msec);
    }

}

void WCursorPrivate::on_touch_motion(wlr_touch_motion_event *event)
{
    auto device = QWTouch::from(event->touch);

    q_func()->setScalePosition(device, QPointF(event->x, event->y));

    if (Q_LIKELY(seat)) {
        seat->notifyTouchMotion(q_func(), WInputDevice::fromHandle(device),
                                event->touch_id, event->time_msec);
    }
}

void WCursorPrivate::on_touch_frame()
{
    if (Q_LIKELY(seat)) {
        seat->notifyTouchFrame(q_func());
    }
}

void WCursorPrivate::on_touch_cancel(wlr_touch_cancel_event *event)
{
    auto device = QWTouch::from(event->touch);

    if (Q_LIKELY(seat)) {
        seat->notifyTouchCancel(q_func(), WInputDevice::fromHandle(device),
                                event->touch_id, event->time_msec);
    }
}

void WCursorPrivate::on_touch_up(wlr_touch_up_event *event)
{
    auto device = QWTouch::from(event->touch);

    if (Q_LIKELY(seat)) {
        seat->notifyTouchUp(q_func(), WInputDevice::fromHandle(device),
                            event->touch_id, event->time_msec);
    }
}

void WCursorPrivate::connect()
{
    W_Q(WCursor);
    Q_ASSERT(seat);

    QObject::connect(handle(), &QWCursor::motion, q, [this] (wlr_pointer_motion_event *event) {
        on_motion(event);
    });
    QObject::connect(handle(), &QWCursor::motionAbsolute, q, [this] (wlr_pointer_motion_absolute_event *event) {
        on_motion_absolute(event);
    });
    QObject::connect(handle(), &QWCursor::button, q, [this] (wlr_pointer_button_event *event) {
        on_button(event);
    });
    QObject::connect(handle(), &QWCursor::axis, q, [this] (wlr_pointer_axis_event *event) {
        on_axis(event);
    });
    QObject::connect(handle(), &QWCursor::frame, q, [this] () {
        on_frame();
    });

    QObject::connect(handle(), SIGNAL(swipeBegin(wlr_pointer_swipe_begin_event*)),
                     q, SLOT(on_swipe_begin(wlr_pointer_swipe_begin_event*)));
    QObject::connect(handle(), SIGNAL(swipeUpdate(wlr_pointer_swipe_update_event*)),
                     q, SLOT(on_swipe_update(wlr_pointer_swipe_update_event*)));
    QObject::connect(handle(), SIGNAL(swipeEnd(wlr_pointer_swipe_end_event*)),
                     q, SLOT(on_swipe_end(wlr_pointer_swipe_end_event*)));
    QObject::connect(handle(), SIGNAL(pinchBegin(wlr_pointer_pinch_begin_event*)),
                     q, SLOT(on_pinch_begin(wlr_pointer_pinch_begin_event*)));
    QObject::connect(handle(), SIGNAL(pinchUpdate(wlr_pointer_pinch_update_event*)),
                     q, SLOT(on_pinch_update(wlr_pointer_pinch_update_event*)));
    QObject::connect(handle(), SIGNAL(pinchEnd(wlr_pointer_pinch_end_event*)),
                     q, SLOT(on_pinch_end(wlr_pointer_pinch_end_event*)));
    QObject::connect(handle(), SIGNAL(holdBegin(wlr_pointer_hold_begin_event*)),
                     q, SLOT(on_hold_begin(wlr_pointer_hold_begin_event*)));
    QObject::connect(handle(), SIGNAL(holdEnd(wlr_pointer_hold_end_event*)),
                     q, SLOT(on_hold_end(wlr_pointer_hold_end_event*)));

    // Handle touch device related signals
    QObject::connect(handle(), &QWCursor::touchDown, q, [this] (wlr_touch_down_event *event) {
        on_touch_down(event);
    });
    QObject::connect(handle(), &QWCursor::touchMotion, q, [this] (wlr_touch_motion_event *event) {
        on_touch_motion(event);
    });
    QObject::connect(handle(), &QWCursor::touchFrame, q, [this] () {
        on_touch_frame();
    });
    QObject::connect(handle(), &QWCursor::touchCancel, q, [this] (wlr_touch_cancel_event *event) {
        on_touch_cancel(event);
    });
    QObject::connect(handle(), &QWCursor::touchUp, q, [this] (wlr_touch_up_event *event) {
        on_touch_up(event);
    });
}

void WCursorPrivate::processCursorMotion(QWPointer *device, uint32_t time)
{
    W_Q(WCursor);

    if (Q_LIKELY(seat))
        seat->notifyMotion(q, WInputDevice::fromHandle(device), time);
}

WCursor::WCursor(WCursorPrivate &dd, QObject *parent)
    : WWrapObject(dd, parent)
{

}

void WCursor::move(QWInputDevice *device, const QPointF &delta)
{
    const QPointF oldPos = position();
    d_func()->handle()->move(device, delta);

    if (oldPos != position())
        Q_EMIT positionChanged();
}

void WCursor::setPosition(QWInputDevice *device, const QPointF &pos)
{
    const QPointF oldPos = position();
    d_func()->handle()->warpClosest(device, pos);

    if (oldPos != position())
        Q_EMIT positionChanged();
}

bool WCursor::setPositionWithChecker(QWInputDevice *device, const QPointF &pos)
{
    const QPointF oldPos = position();
    bool ok = d_func()->handle()->warp(device, pos);

    if (oldPos != position())
        Q_EMIT positionChanged();
    return ok;
}

void WCursor::setScalePosition(QWInputDevice *device, const QPointF &ratio)
{
    const QPointF oldPos = position();
    d_func()->handle()->warpAbsolute(device, ratio);

    if (oldPos != position())
        Q_EMIT positionChanged();
}

WCursor::WCursor(QObject *parent)
    : WCursor(*new WCursorPrivate(this), parent)
{

}

QWCursor *WCursor::handle() const
{
    W_DC(WCursor);
    return d->handle();
}

WCursor *WCursor::fromHandle(const QWCursor *handle)
{
    return handle->getData<WCursor>();
}

Qt::MouseButton WCursor::fromNativeButton(uint32_t code)
{
    Qt::MouseButton qt_button = Qt::NoButton;
    // translate from kernel (input.h) 'button' to corresponding Qt:MouseButton.
    // The range of mouse values is 0x110 <= mouse_button < 0x120, the first Joystick button.
    switch (code) {
    case 0x110: qt_button = Qt::LeftButton; break;    // kernel BTN_LEFT
    case 0x111: qt_button = Qt::RightButton; break;
    case 0x112: qt_button = Qt::MiddleButton; break;
    case 0x113: qt_button = Qt::ExtraButton1; break;  // AKA Qt::BackButton
    case 0x114: qt_button = Qt::ExtraButton2; break;  // AKA Qt::ForwardButton
    case 0x115: qt_button = Qt::ExtraButton3; break;  // AKA Qt::TaskButton
    case 0x116: qt_button = Qt::ExtraButton4; break;
    case 0x117: qt_button = Qt::ExtraButton5; break;
    case 0x118: qt_button = Qt::ExtraButton6; break;
    case 0x119: qt_button = Qt::ExtraButton7; break;
    case 0x11a: qt_button = Qt::ExtraButton8; break;
    case 0x11b: qt_button = Qt::ExtraButton9; break;
    case 0x11c: qt_button = Qt::ExtraButton10; break;
    case 0x11d: qt_button = Qt::ExtraButton11; break;
    case 0x11e: qt_button = Qt::ExtraButton12; break;
    case 0x11f: qt_button = Qt::ExtraButton13; break;
    default: qWarning() << "invalid button number (as far as Qt is concerned)" << code; // invalid button number (as far as Qt is concerned)
    }

    return qt_button;
}

uint32_t WCursor::toNativeButton(Qt::MouseButton button)
{
    switch (button) {
    case Qt::LeftButton: return 0x110;    // kernel BTN_LEFT
    case Qt::RightButton: return 0x111;
    case Qt::MiddleButton: return 0x112;
    case Qt::ExtraButton1: return 0x113;
    case Qt::ExtraButton2: return 0x114;
    case Qt::ExtraButton3: return 0x115;
    case Qt::ExtraButton4: return 0x116;
    case Qt::ExtraButton5: return 0x117;
    case Qt::ExtraButton6: return 0x118;
    case Qt::ExtraButton7: return 0x119;
    case Qt::ExtraButton8: return 0x11a;
    case Qt::ExtraButton9: return 0x11b;
    case Qt::ExtraButton10: return 0x11c;
    case Qt::ExtraButton11: return 0x11d;
    case Qt::ExtraButton12: return 0x11e;
    case Qt::ExtraButton13: return 0x11f;
    default: qWarning() << "invalid Qt's button" << button;
    }

    return 0;
}

QCursor WCursor::toQCursor(CursorShape shape)
{
    static QBitmap tmp(1, 1);
    // Ensure alloc a new QCursorData
    QCursor cursor(tmp, tmp);

    Q_ASSERT(cursor.d->ref == 1);
    Q_ASSERT(cursor.d->bm);
    Q_ASSERT(cursor.d->bmm);
    delete cursor.d->bm;
    delete cursor.d->bmm;
    cursor.d->bm = nullptr;
    cursor.d->bmm = nullptr;
    cursor.d->cshape = static_cast<Qt::CursorShape>(shape);

    return cursor;
}

Qt::MouseButtons WCursor::state() const
{
    W_DC(WCursor);
    return d->state;
}

Qt::MouseButton WCursor::button() const
{
    W_DC(WCursor);
    return d->button;
}

void WCursor::setSeat(WSeat *seat)
{
    W_D(WCursor);

    if (d->seat) {
        // reconnect signals
        d->handle()->disconnect(d->seat);
    }
    d->seat = seat;

    if (d->seat) {
        d->connect();

        if (d->eventWindow) {
            d->sendEnterEvent();
        }
    }
}

WSeat *WCursor::seat() const
{
    W_DC(WCursor);
    return d->seat;
}

QWindow *WCursor::eventWindow() const
{
    W_DC(WCursor);
    return d->eventWindow.get();
}

void WCursor::setEventWindow(QWindow *window)
{
    W_D(WCursor);
    if (d->eventWindow == window)
        return;

    if (d->eventWindow && d->seat) {
        d->sendLeaveEvent();
    }

    d->eventWindow = window;
    d->enterWindowEventHasSend = false;
    d->leaveWindowEventHasSend = false;

    if (d->eventWindow && d->seat && !d->deviceList.isEmpty()) {
        d->sendEnterEvent();
    }
}

Qt::CursorShape WCursor::defaultCursor()
{
    return Qt::ArrowCursor;
}

void WCursor::setXCursorManager(QWXCursorManager *manager)
{
    W_D(WCursor);

    if (d->xcursor_manager == manager)
        return;

    d->xcursor_manager = manager;

    // Make sure the theme with a scaling factor of 1.0 was loaded
    // Used to check whether the theme support the type of cursor in `setType`
    d->xcursor_manager->load(1.0);

    d->updateCursorImage();
}

QCursor WCursor::cursor() const
{
    W_DC(WCursor);
    return d->cursor;
}

void WCursor::setCursor(const QCursor &cursor)
{
    W_D(WCursor);

    d->cursor = cursor;
    d->updateCursorImage();
}

void WCursor::setSurface(QWSurface *surface, const QPoint &hotspot)
{
    W_D(WCursor);
    if (d->surfaceOfCursor) // don't update cursor image before older surface destroy
        d->surfaceOfCursor->disconnect(this);
    d->surfaceOfCursor = surface;
    d->surfaceCursorHotspot = hotspot;
    d->shape = WCursor::Invalid; // clear cache
    if (d->visible) {
        d->handle()->setSurface(surface, hotspot);
        if (surface) {
            connect(surface, &QWSurface::beforeDestroy, this, [d]() {
                d->updateCursorImage();
            }, Qt::QueuedConnection);
            // Do not call updateCursorImage immediately to prevent pointerFocusSurface not cleaning up in time
        }
    }
}

void WCursor::setCursorShape(CursorShape shape)
{
    W_D(WCursor);
    d->shape = shape;
    d->updateCursorImage();
}

void WCursor::setDragSurface(WSurface *surface)
{
    W_D(WCursor);
    if (d->dragSurface == surface)
        return;
    if (d->dragSurface)
        d->dragSurface->safeDisconnect(this);

    d->dragSurface = surface;
    if (surface) {
        connect(surface, &WSurface::destroyed, this, [this,d]() {
            d->dragSurface = nullptr;
            Q_EMIT dragSurfaceChanged();
        });
    }
    Q_EMIT dragSurfaceChanged();
}

WSurface *WCursor::dragSurface() const
{
    W_DC(WCursor);
    return d->dragSurface;
}

bool WCursor::attachInputDevice(WInputDevice *device)
{
    if (device->type() != WInputDevice::Type::Pointer
            && device->type() != WInputDevice::Type::Touch
            && device->type() != WInputDevice::Type::Tablet) {
        return false;
    }

    W_D(WCursor);
    Q_ASSERT(!d->deviceList.contains(device));
    d->handle()->attachInputDevice(device->handle());
    d->deviceList << device;

    if (d->eventWindow && d->deviceList.count() == 1) {
        Q_ASSERT(d->seat);
        d->sendEnterEvent();
    }

    return true;
}

void WCursor::detachInputDevice(WInputDevice *device)
{
    W_D(WCursor);

    if (!d->deviceList.removeOne(device))
        return;

    d->handle()->detachInputDevice(device->handle());
    d->handle()->mapInputToOutput(device->handle(), nullptr);

    if (d->eventWindow && d->deviceList.isEmpty()) {
        Q_ASSERT(d->seat);
        d->sendLeaveEvent();
    }
}

void WCursor::setLayout(WOutputLayout *layout)
{
    W_D(WCursor);

    if (d->outputLayout == layout)
        return;

    d->outputLayout = layout;
    d->handle()->attachOutputLayout(d->outputLayout);

    if (d->outputLayout) {
        for (auto o : d->outputLayout->outputs())
            o->addCursor(this);
    }

    connect(d->outputLayout, &WOutputLayout::outputAdded, this, [this, d] (WOutput *o) {
        o->addCursor(this);
    });

    d->updateCursorImage();
}

WOutputLayout *WCursor::layout() const
{
    W_DC(WCursor);
    return d->outputLayout;
}

void WCursor::setPosition(const QPointF &pos)
{
    setPosition(nullptr, pos);
}

bool WCursor::setPositionWithChecker(const QPointF &pos)
{
    return setPositionWithChecker(nullptr, pos);
}

bool WCursor::isVisible() const
{
    W_DC(WCursor);
    return d->visible;
}

void WCursor::setVisible(bool visible)
{
    W_D(WCursor);
    if (d->visible == visible)
        return;
    d->visible = visible;

    if (visible) {
        if (d->surfaceOfCursor) {
            d->handle()->setSurface(d->surfaceOfCursor, d->surfaceCursorHotspot);
            connect(d->surfaceOfCursor, &QWSurface::beforeDestroy, this, [d]() {
                d->updateCursorImage();
            }, Qt::QueuedConnection);
            // Do not call updateCursorImage immediately to prevent pointerFocusSurface not cleaning up in time
        } else {
            d->updateCursorImage();
        }
    } else {
        if (d->surfaceOfCursor)
            d->surfaceOfCursor->disconnect(this);
        d->handle()->unsetImage();
    }
}

QPointF WCursor::position() const
{
    W_DC(WCursor);
    return d->handle()->position();
}

QPointF WCursor::lastPressedOrTouchDownPosition() const
{
    W_DC(WCursor);
    return d->lastPressedOrTouchDownPosition;
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_wcursor.cpp"
