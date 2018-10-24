// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_EXTENSIONS_API_TABS_TABS_API_H_
#define CHROMECAST_BROWSER_EXTENSIONS_API_TABS_TABS_API_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "chromecast/common/extensions_api/tabs.h"
#include "components/zoom/zoom_controller.h"
#include "extensions/browser/api/execute_code_function.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/extension_resource.h"
#include "url/gurl.h"

namespace chromecast {
struct ActiveWebview;
}

namespace content {
class WebContents;
}

namespace ui {
class ListSelectionModel;
}

namespace extensions {
namespace cast {
namespace api {

// Converts a ZoomMode to its ZoomSettings representation.
void ZoomModeToZoomSettings(zoom::ZoomController::ZoomMode zoom_mode,
                            api::tabs::ZoomSettings* zoom_settings);

// Windows
class WindowsGetFunction : public UIThreadExtensionFunction {
  ~WindowsGetFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("windows.get", WINDOWS_GET)
};
class WindowsGetCurrentFunction : public UIThreadExtensionFunction {
  ~WindowsGetCurrentFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("windows.getCurrent", WINDOWS_GETCURRENT)
};
class WindowsGetLastFocusedFunction : public UIThreadExtensionFunction {
  ~WindowsGetLastFocusedFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("windows.getLastFocused", WINDOWS_GETLASTFOCUSED)
};
class WindowsGetAllFunction : public UIThreadExtensionFunction {
  ~WindowsGetAllFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("windows.getAll", WINDOWS_GETALL)
};
class WindowsCreateFunction : public UIThreadExtensionFunction {
  ~WindowsCreateFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("windows.create", WINDOWS_CREATE)
};
class WindowsUpdateFunction : public UIThreadExtensionFunction {
  ~WindowsUpdateFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("windows.update", WINDOWS_UPDATE)
};
class WindowsRemoveFunction : public UIThreadExtensionFunction {
  ~WindowsRemoveFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("windows.remove", WINDOWS_REMOVE)
};

// Tabs
class TabsGetFunction : public UIThreadExtensionFunction {
  ~TabsGetFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.get", TABS_GET)
};
class TabsGetCurrentFunction : public UIThreadExtensionFunction {
  ~TabsGetCurrentFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.getCurrent", TABS_GETCURRENT)
};
class TabsGetSelectedFunction : public UIThreadExtensionFunction {
  ~TabsGetSelectedFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.getSelected", TABS_GETSELECTED)
};
class TabsGetAllInWindowFunction : public UIThreadExtensionFunction {
  ~TabsGetAllInWindowFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.getAllInWindow", TABS_GETALLINWINDOW)
};
class TabsQueryFunction : public UIThreadExtensionFunction {
  ~TabsQueryFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.query", TABS_QUERY)
};
class TabsCreateFunction : public UIThreadExtensionFunction {
  ~TabsCreateFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.create", TABS_CREATE)
};
class TabsDuplicateFunction : public UIThreadExtensionFunction {
  ~TabsDuplicateFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.duplicate", TABS_DUPLICATE)
};
class TabsHighlightFunction : public UIThreadExtensionFunction {
  ~TabsHighlightFunction() override {}
  ResponseAction Run() override;
  bool HighlightTab(const std::vector<chromecast::ActiveWebview>& tabs,
                    ui::ListSelectionModel* selection,
                    int* active_index,
                    int index,
                    std::string* error);
  DECLARE_EXTENSION_FUNCTION("tabs.highlight", TABS_HIGHLIGHT)
};
class TabsUpdateFunction : public UIThreadExtensionFunction {
 public:
  TabsUpdateFunction();

 protected:
  ~TabsUpdateFunction() override {}
  bool UpdateURL(const std::string& url,
                 int tab_id,
                 bool* is_async,
                 std::string* error);

  content::WebContents* web_contents_;

 private:
  ResponseAction Run() override;
  void OnExecuteCodeFinished(const std::string& error,
                             const GURL& on_url,
                             const base::ListValue& script_result);

  DECLARE_EXTENSION_FUNCTION("tabs.update", TABS_UPDATE)
};
class TabsMoveFunction : public UIThreadExtensionFunction {
  ~TabsMoveFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.move", TABS_MOVE)
};
class TabsReloadFunction : public UIThreadExtensionFunction {
  ~TabsReloadFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.reload", TABS_RELOAD)
};
class TabsRemoveFunction : public UIThreadExtensionFunction {
  ~TabsRemoveFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.remove", TABS_REMOVE)
};
class TabsDetectLanguageFunction : public UIThreadExtensionFunction {
 private:
  ~TabsDetectLanguageFunction() override {}
  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("tabs.detectLanguage", TABS_DETECTLANGUAGE)
};

class TabsCaptureVisibleTabFunction : public UIThreadExtensionFunction {
 public:
  TabsCaptureVisibleTabFunction();

  // ExtensionFunction implementation.
  bool HasPermission() override;
  ResponseAction Run() override;

 protected:
  ~TabsCaptureVisibleTabFunction() override {}

 private:
  DECLARE_EXTENSION_FUNCTION("tabs.captureVisibleTab", TABS_CAPTUREVISIBLETAB)

  DISALLOW_COPY_AND_ASSIGN(TabsCaptureVisibleTabFunction);
};

// Implement API call tabs.executeScript and tabs.insertCSS.
class ExecuteCodeInTabFunction : public ExecuteCodeFunction {
 public:
  ExecuteCodeInTabFunction();

 protected:
  ~ExecuteCodeInTabFunction() override;

  // ExtensionFunction:
  bool HasPermission() override;

  // Initializes |execute_tab_id_| and |details_|.
  InitResult Init() override;
  bool CanExecuteScriptOnPage(std::string* error) override;
  ScriptExecutor* GetScriptExecutor(std::string* error) override;
  bool IsWebView() const override;
  const GURL& GetWebViewSrc() const override;

 private:
  // Id of tab which executes code.
  int execute_tab_id_;
};

class TabsExecuteScriptFunction : public ExecuteCodeInTabFunction {
 protected:
  bool ShouldInsertCSS() const override;

 private:
  ~TabsExecuteScriptFunction() override {}

  DECLARE_EXTENSION_FUNCTION("tabs.executeScript", TABS_EXECUTESCRIPT)
};

class TabsInsertCSSFunction : public ExecuteCodeInTabFunction {
 private:
  ~TabsInsertCSSFunction() override {}

  bool ShouldInsertCSS() const override;

  DECLARE_EXTENSION_FUNCTION("tabs.insertCSS", TABS_INSERTCSS)
};

class TabsSetZoomFunction : public UIThreadExtensionFunction {
 private:
  ~TabsSetZoomFunction() override {}

  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("tabs.setZoom", TABS_SETZOOM)
};

class TabsGetZoomFunction : public UIThreadExtensionFunction {
 private:
  ~TabsGetZoomFunction() override {}

  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("tabs.getZoom", TABS_GETZOOM)
};

class TabsSetZoomSettingsFunction : public UIThreadExtensionFunction {
 private:
  ~TabsSetZoomSettingsFunction() override {}

  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("tabs.setZoomSettings", TABS_SETZOOMSETTINGS)
};

class TabsGetZoomSettingsFunction : public UIThreadExtensionFunction {
 private:
  ~TabsGetZoomSettingsFunction() override {}

  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("tabs.getZoomSettings", TABS_GETZOOMSETTINGS)
};

class TabsDiscardFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabs.discard", TABS_DISCARD)

  TabsDiscardFunction();

 private:
  ~TabsDiscardFunction() override;

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(TabsDiscardFunction);
};

}  // namespace api
}  // namespace cast
}  // namespace extensions

#endif  // CHROMECAST_BROWSER_EXTENSIONS_API_TABS_TABS_API_H_
