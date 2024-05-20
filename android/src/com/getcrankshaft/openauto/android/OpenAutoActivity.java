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

package com.getcrankshaft.openauto.android;

import android.os.Build;
import android.os.Bundle;
import android.system.ErrnoException;
import android.system.Os;
import android.util.Log;
import java.lang.System;
import java.lang.UnsatisfiedLinkError;
import org.freedesktop.gstreamer.GStreamer;
import org.qtproject.qt5.android.bindings.QtActivity;

public class OpenAutoActivity extends QtActivity
{
    @Override
    public void onCreate(Bundle bundle)
    {
        /* Due to how Gstreamer on Android works, Gstreamer has to be
         * initialized from the Android side for many of the functionalities
         * to work.
         * Now, because Qt's CMake code suffixes our library name, we have to
         * try the device's supported architecture one at a time, assumption
         * being that if e.g. arm64 gstreamer lib exists, arm64 app and Qt
         * should exist as well. */

        for (String abi : Build.SUPPORTED_ABIS) {
            try {
                System.loadLibrary("gstreamer_android_" + abi);
                break;
            } catch (UnsatisfiedLinkError e) {
                continue;
            }
        }

        try {
            GStreamer.init(this);
        } catch (Exception e) {
            Log.e("OpenAutoActivity", "Huh? Gstreamer.init() throws: " + e);
            Log.e("OpenAutoActivity", "Cannot continue, exitting now!");
            finish();
            return;
        }

        /* Continue with Qt loading process. */
        super.onCreate(bundle);
    }
}
