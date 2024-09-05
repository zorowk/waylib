// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "layersurfacecontainer.h"
#include "surfacewrapper.h"
#include "output.h"

#include <wlayersurface.h>
#include <woutputitem.h>

WAYLIB_SERVER_USE_NAMESPACE

OutputLayerSurfaceContainer::OutputLayerSurfaceContainer(Output *output, LayerSurfaceContainer *parent)
    : SurfaceContainer(parent)
    , m_output(output)
{
    connect(output->outputItem(), &WOutputItem::geometryChanged, this, &OutputLayerSurfaceContainer::onOutputGeometryChanged);
    setClip(true);
}

Output *OutputLayerSurfaceContainer::output() const
{
    return m_output;
}

void OutputLayerSurfaceContainer::addSurface(SurfaceWrapper *surface)
{
    SurfaceContainer::addSurface(surface);
}

void OutputLayerSurfaceContainer::removeSurface(SurfaceWrapper *surface)
{
    SurfaceContainer::removeSurface(surface);
}

void OutputLayerSurfaceContainer::onOutputGeometryChanged()
{
    setPosition(m_output->outputItem()->position());
    setSize(m_output->outputItem()->size());
}

LayerSurfaceContainer::LayerSurfaceContainer(SurfaceContainer *parent)
    : SurfaceContainer(parent)
{

}

void LayerSurfaceContainer::addOutput(Output *output)
{
    Q_ASSERT(!getSurfaceContainer(output));
    auto container = new OutputLayerSurfaceContainer(output, this);
    m_surfaceContainers.append(container);
}

void LayerSurfaceContainer::removeOutput(Output *output)
{
    OutputLayerSurfaceContainer *container = getSurfaceContainer(output);
    Q_ASSERT(container);
    m_surfaceContainers.removeOne(container);
    container->deleteLater();
}

OutputLayerSurfaceContainer *LayerSurfaceContainer::getSurfaceContainer(const Output *output) const
{
    for (OutputLayerSurfaceContainer *container : std::as_const(m_surfaceContainers)) {
        if (container->output() == output)
            return container;
    }
    return nullptr;
}

OutputLayerSurfaceContainer *LayerSurfaceContainer::getSurfaceContainer(const WOutput *output) const
{
    for (OutputLayerSurfaceContainer *container : std::as_const(m_surfaceContainers)) {
        if (container->output()->output() == output)
            return container;
    }
    return nullptr;
}

void LayerSurfaceContainer::addSurface(SurfaceWrapper *surface)
{
    Q_ASSERT(surface->type() == SurfaceWrapper::Type::Layer);
    if (!SurfaceContainer::doAddSurface(surface, false))
        return;
    auto shell = qobject_cast<WLayerSurface*>(surface->shellSurface());
    auto output = shell->output();
    auto container = getSurfaceContainer(output);
    Q_ASSERT(container);
    Q_ASSERT(!container->surfaces().contains(surface));
    container->addSurface(surface);
}

void LayerSurfaceContainer::removeSurface(SurfaceWrapper *surface)
{
    if (!SurfaceContainer::doRemoveSurface(surface, false))
        return;
    auto shell = qobject_cast<WLayerSurface*>(surface->shellSurface());
    auto output = shell->output();
    auto container = getSurfaceContainer(output);
    Q_ASSERT(container);
    Q_ASSERT(container->surfaces().contains(surface));
    container->removeSurface(surface);
}