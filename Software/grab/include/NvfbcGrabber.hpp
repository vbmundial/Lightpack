/*
* NvfbcGrabber.hpp
*
*  Created on: 06.03.17
*     Project: Lightpack
*
*  Copyright (c) 2017
*
*  Lightpack a USB content-driving ambient lighting system
*
*  Lightpack is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 2 of the License, or
*  (at your option) any later version.
*
*  Lightpack is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*/

#pragma once

#include "GrabberBase.hpp"
#ifdef NVFBC_GRAB_SUPPORT

#include "enums.hpp"
#include <QAbstractNativeEventFilter>

class NvfbcGrabber : public GrabberBase {
    Q_OBJECT

    void* m_nvfbc; //NvFBCLibrary 
    void* m_pNvfbcToSys; //NvFBCCuda*
    unsigned char *m_frameBuffer;

public:
    NvfbcGrabber(QObject* parent, GrabberContext* context);
    virtual ~NvfbcGrabber();

    DECLARE_GRABBER_NAME("NvfbcGrabber")

protected slots:
    virtual GrabResult grabScreens() override;
    virtual bool reallocate(const QList<ScreenInfo> &grabScreens) override;
    virtual QList<ScreenInfo>* screensWithWidgets(QList<ScreenInfo> * result, const QList<GrabWidget *> &grabWidgets) override;
    virtual bool isReallocationNeeded(const QList< ScreenInfo > &screens) const override;
};


#endif //NVFBC_GRAB_SUPPORT
