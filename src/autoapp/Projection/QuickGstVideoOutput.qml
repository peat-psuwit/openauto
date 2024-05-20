/*
*  This file is part of openauto project.
*  Copyright (C) 2024 Ratchanan Srirattanamet <peathot@hotmail.com>
*
*  openauto is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 3 of the License, or
*  (at your option) any later version.

*  openauto is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with openauto. If not, see <http://www.gnu.org/licenses/>.
*/

import QtQuick 2.15
import org.freedesktop.gstreamer.GLVideoItem 1.0

Item {
    id: root

    GstGLVideoItem {
        id: videoItem
        objectName: "videoItem"
        forceAspectRatio: false

        // TODO: implement aspect-ratio-preserving size adjustment. Idea being
        // that videoItem will overscan the video, then QuickGstVideoOutput will
        // add the margin to user's configuration.
        width: root.width
        height: root.height
        anchors.centerIn: root
    }
}
