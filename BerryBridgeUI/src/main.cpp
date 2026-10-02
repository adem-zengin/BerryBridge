/*
 * Copyright (c) 2013-2015 BlackBerry Limited.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "applicationui.hpp"
#include "Database.hpp"
#include <bb/cascades/Application>
#include "LocationSession.hpp"

#include <QLocale>
#include <QTranslator>

#include <Qt/qdeclarativedebug.h>
#include <bb/cascades/pickers/FilePicker>
#include <bb/cascades/pickers/FilePickerMode>
#include <bb/cascades/pickers/FilePickerSortFlag>
#include <bb/cascades/pickers/FilePickerSortOrder>
#include <bb/cascades/pickers/FilePickerViewMode>
#include <bb/cascades/pickers/FileType>

using namespace bb::cascades;

Q_DECL_EXPORT int main(int argc, char **argv)
{
    qmlRegisterType<pickers::FilePicker>("bb.cascades.pickers", 1, 0, "FilePicker");
    qmlRegisterUncreatableType<pickers::FilePickerMode>("bb.cascades.pickers", 1, 0, "FilePickerMode", "");
    qmlRegisterUncreatableType<pickers::FilePickerSortFlag>("bb.cascades.pickers", 1, 0, "FilePickerSortFlag", "");
    qmlRegisterUncreatableType<pickers::FilePickerSortOrder>("bb.cascades.pickers", 1, 0, "FilePickerSortOrder", "");
    qmlRegisterUncreatableType<pickers::FileType>("bb.cascades.pickers", 1, 0, "FileType", "");
    qmlRegisterUncreatableType<pickers::FilePickerViewMode>("bb.cascades.pickers", 1, 0, "FilePickerViewMode", "");

    Application app(argc, argv);

    // is loaded and the application scene is set.
    qmlRegisterType<Database>("com.database.web", 1, 0, "Database");
    qmlRegisterType<LocationSession>("my.location", 1, 0, "LocationSession");
    ApplicationUI appui;

    // Enter the application main event loop.
    return Application::exec();
}
