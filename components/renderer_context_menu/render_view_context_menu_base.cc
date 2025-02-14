// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/renderer_context_menu/render_view_context_menu_base.h"

#include <algorithm>
#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/menu_item.h"
#include "third_party/WebKit/public/web/WebContextMenuData.h"

using blink::WebContextMenuData;
using blink::WebString;
using blink::WebURL;
using content::BrowserContext;
using content::OpenURLParams;
using content::RenderFrameHost;
using content::RenderViewHost;
using content::WebContents;

namespace {

// The (inclusive) range of command IDs reserved for content's custom menus.
int content_context_custom_first = -1;
int content_context_custom_last = -1;

bool IsCustomItemEnabledInternal(const std::vector<content::MenuItem>& items,
                                 int id) {
  DCHECK(RenderViewContextMenuBase::IsContentCustomCommandId(id));
  for (size_t i = 0; i < items.size(); ++i) {
    int action_id = RenderViewContextMenuBase::ConvertToContentCustomCommandId(
        items[i].action);
    if (action_id == id)
      return items[i].enabled;
    if (items[i].type == content::MenuItem::SUBMENU) {
      if (IsCustomItemEnabledInternal(items[i].submenu, id))
        return true;
    }
  }
  return false;
}

bool IsCustomItemCheckedInternal(const std::vector<content::MenuItem>& items,
                                 int id) {
  DCHECK(RenderViewContextMenuBase::IsContentCustomCommandId(id));
  for (size_t i = 0; i < items.size(); ++i) {
    int action_id = RenderViewContextMenuBase::ConvertToContentCustomCommandId(
        items[i].action);
    if (action_id == id)
      return items[i].checked;
    if (items[i].type == content::MenuItem::SUBMENU) {
      if (IsCustomItemCheckedInternal(items[i].submenu, id))
        return true;
    }
  }
  return false;
}

const size_t kMaxCustomMenuDepth = 5;
const size_t kMaxCustomMenuTotalItems = 1000;

void AddCustomItemsToMenu(const std::vector<content::MenuItem>& items,
                          size_t depth,
                          size_t* total_items,
                          ScopedVector<ui::SimpleMenuModel>* submenus,
                          ui::SimpleMenuModel::Delegate* delegate,
                          ui::SimpleMenuModel* menu_model) {
  if (depth > kMaxCustomMenuDepth) {
    LOG(ERROR) << "Custom menu too deeply nested.";
    return;
  }
  for (size_t i = 0; i < items.size(); ++i) {
    int command_id = RenderViewContextMenuBase::ConvertToContentCustomCommandId(
        items[i].action);
    if (!RenderViewContextMenuBase::IsContentCustomCommandId(command_id)) {
      LOG(ERROR) << "Custom menu action value out of range.";
      return;
    }
    if (*total_items >= kMaxCustomMenuTotalItems) {
      LOG(ERROR) << "Custom menu too large (too many items).";
      return;
    }
    (*total_items)++;
    switch (items[i].type) {
      case content::MenuItem::OPTION:
        menu_model->AddItem(
            RenderViewContextMenuBase::ConvertToContentCustomCommandId(
                items[i].action),
            items[i].label);
        break;
      case content::MenuItem::CHECKABLE_OPTION:
        menu_model->AddCheckItem(
            RenderViewContextMenuBase::ConvertToContentCustomCommandId(
                items[i].action),
            items[i].label);
        break;
      case content::MenuItem::GROUP:
        // TODO(viettrungluu): I don't know what this is supposed to do.
        NOTREACHED();
        break;
      case content::MenuItem::SEPARATOR:
        menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
        break;
      case content::MenuItem::SUBMENU: {
        ui::SimpleMenuModel* submenu = new ui::SimpleMenuModel(delegate);
        submenus->push_back(submenu);
        AddCustomItemsToMenu(items[i].submenu, depth + 1, total_items, submenus,
                             delegate, submenu);
        menu_model->AddSubMenu(
            RenderViewContextMenuBase::ConvertToContentCustomCommandId(
                items[i].action),
            items[i].label,
            submenu);
        break;
      }
      default:
        NOTREACHED();
        break;
    }
  }
}

}  // namespace

// static
void RenderViewContextMenuBase::SetContentCustomCommandIdRange(
    int first, int last) {
  // The range is inclusive.
  content_context_custom_first = first;
  content_context_custom_last = last;
}

// static
const size_t RenderViewContextMenuBase::kMaxSelectionTextLength = 50;

// static
int RenderViewContextMenuBase::ConvertToContentCustomCommandId(int id) {
  return content_context_custom_first + id;
}

// static
bool RenderViewContextMenuBase::IsContentCustomCommandId(int id) {
  return id >= content_context_custom_first &&
         id <= content_context_custom_last;
}

RenderViewContextMenuBase::RenderViewContextMenuBase(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params)
    : params_(params),
      source_web_contents_(WebContents::FromRenderFrameHost(render_frame_host)),
      browser_context_(source_web_contents_->GetBrowserContext()),
      menu_model_(this),
      render_frame_id_(render_frame_host->GetRoutingID()),
      command_executed_(false),
      render_process_id_(render_frame_host->GetProcess()->GetID()) {
}

RenderViewContextMenuBase::~RenderViewContextMenuBase() {
}

// Menu construction functions -------------------------------------------------

void RenderViewContextMenuBase::Init() {
  // Command id range must have been already initializerd.
  DCHECK_NE(-1, content_context_custom_first);
  DCHECK_NE(-1, content_context_custom_last);

  InitMenu();
  if (toolkit_delegate_)
    toolkit_delegate_->Init(&menu_model_);
}

void RenderViewContextMenuBase::Cancel() {
  if (toolkit_delegate_)
    toolkit_delegate_->Cancel();
}

void RenderViewContextMenuBase::InitMenu() {
  if (content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_CUSTOM)) {
    AppendCustomItems();

    const bool has_selection = !params_.selection_text.empty();
    if (has_selection) {
      // We will add more items if there's a selection, so add a separator.
      // TODO(lazyboy): Clean up separator logic.
      menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
    }
  }
}

void RenderViewContextMenuBase::AddMenuItem(int command_id,
                                            const base::string16& title) {
  menu_model_.AddItem(command_id, title);
}

void RenderViewContextMenuBase::AddCheckItem(int command_id,
                                         const base::string16& title) {
  menu_model_.AddCheckItem(command_id, title);
}

void RenderViewContextMenuBase::AddSeparator() {
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
}

void RenderViewContextMenuBase::AddSubMenu(int command_id,
                                       const base::string16& label,
                                       ui::MenuModel* model) {
  menu_model_.AddSubMenu(command_id, label, model);
}

void RenderViewContextMenuBase::UpdateMenuItem(int command_id,
                                           bool enabled,
                                           bool hidden,
                                           const base::string16& label) {
  if (toolkit_delegate_) {
    toolkit_delegate_->UpdateMenuItem(command_id,
                                      enabled,
                                      hidden,
                                      label);
  }
}

RenderViewHost* RenderViewContextMenuBase::GetRenderViewHost() const {
  return source_web_contents_->GetRenderViewHost();
}

WebContents* RenderViewContextMenuBase::GetWebContents() const {
  return source_web_contents_;
}

BrowserContext* RenderViewContextMenuBase::GetBrowserContext() const {
  return browser_context_;
}

bool RenderViewContextMenuBase::AppendCustomItems() {
  size_t total_items = 0;
  AddCustomItemsToMenu(params_.custom_items, 0, &total_items, &custom_submenus_,
                       this, &menu_model_);
  return total_items > 0;
}

bool RenderViewContextMenuBase::IsCommandIdKnown(
    int id,
    bool* enabled) const {
  // If this command is added by one of our observers, we dispatch
  // it to the observer.
  base::ObserverListBase<RenderViewContextMenuObserver>::Iterator it(
      &observers_);
  RenderViewContextMenuObserver* observer;
  while ((observer = it.GetNext()) != NULL) {
    if (observer->IsCommandIdSupported(id)) {
      *enabled = observer->IsCommandIdEnabled(id);
      return true;
    }
  }

  // Custom items.
  if (IsContentCustomCommandId(id)) {
    *enabled = IsCustomItemEnabled(id);
    return true;
  }

  return false;
}

// Menu delegate functions -----------------------------------------------------

bool RenderViewContextMenuBase::IsCommandIdChecked(int id) const {
  // If this command is is added by one of our observers, we dispatch it to the
  // observer.
  base::ObserverListBase<RenderViewContextMenuObserver>::Iterator it(
      &observers_);
  RenderViewContextMenuObserver* observer;
  while ((observer = it.GetNext()) != NULL) {
    if (observer->IsCommandIdSupported(id))
      return observer->IsCommandIdChecked(id);
  }

  // Custom items.
  if (IsContentCustomCommandId(id))
    return IsCustomItemChecked(id);

  return false;
}

void RenderViewContextMenuBase::ExecuteCommand(int id, int event_flags) {
  command_executed_ = true;
  RecordUsedItem(id);

  // If this command is is added by one of our observers, we dispatch
  // it to the observer.
  base::ObserverListBase<RenderViewContextMenuObserver>::Iterator it(
      &observers_);
  RenderViewContextMenuObserver* observer;
  while ((observer = it.GetNext()) != NULL) {
    if (observer->IsCommandIdSupported(id))
      return observer->ExecuteCommand(id);
  }

  // Process custom actions range.
  if (IsContentCustomCommandId(id)) {
    unsigned action = id - content_context_custom_first;
    const content::CustomContextMenuContext& context = params_.custom_context;
#if defined(ENABLE_PLUGINS)
    if (context.request_id && !context.is_pepper_menu)
      HandleAuthorizeAllPlugins();
#endif
    source_web_contents_->ExecuteCustomContextMenuCommand(action, context);
    return;
  }
  command_executed_ = false;
}

void RenderViewContextMenuBase::MenuWillShow(ui::SimpleMenuModel* source) {
  for (int i = 0; i < source->GetItemCount(); ++i) {
    if (source->IsVisibleAt(i) &&
        source->GetTypeAt(i) != ui::MenuModel::TYPE_SEPARATOR) {
      RecordShownItem(source->GetCommandIdAt(i));
    }
  }

  // Ignore notifications from submenus.
  if (source != &menu_model_)
    return;

  content::RenderWidgetHostView* view =
      source_web_contents_->GetRenderWidgetHostView();
  if (view)
    view->SetShowingContextMenu(true);

  NotifyMenuShown();
}

void RenderViewContextMenuBase::MenuClosed(ui::SimpleMenuModel* source) {
  // Ignore notifications from submenus.
  if (source != &menu_model_)
    return;

  content::RenderWidgetHostView* view =
      source_web_contents_->GetRenderWidgetHostView();
  if (view)
    view->SetShowingContextMenu(false);
  source_web_contents_->NotifyContextMenuClosed(params_.custom_context);

  if (!command_executed_) {
    FOR_EACH_OBSERVER(RenderViewContextMenuObserver,
                      observers_,
                      OnMenuCancel());
  }
}

RenderFrameHost* RenderViewContextMenuBase::GetRenderFrameHost() {
  return RenderFrameHost::FromID(render_process_id_, render_frame_id_);
}

// Controller functions --------------------------------------------------------

void RenderViewContextMenuBase::OpenURL(
    const GURL& url, const GURL& referring_url,
    WindowOpenDisposition disposition,
    ui::PageTransition transition) {
  OpenURLWithExtraHeaders(url, referring_url, disposition, transition, "");
}

void RenderViewContextMenuBase::OpenURLWithExtraHeaders(
    const GURL& url,
    const GURL& referring_url,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    const std::string& extra_headers) {
  content::Referrer referrer = content::Referrer::SanitizeForRequest(
      url,
      content::Referrer(referring_url.GetAsReferrer(),
                        params_.referrer_policy));

  if (params_.link_url == url && disposition != OFF_THE_RECORD)
    params_.custom_context.link_followed = url;

  OpenURLParams open_url_params(url, referrer, disposition, transition, false);
  if (!extra_headers.empty())
    open_url_params.extra_headers = extra_headers;

  WebContents* new_contents = source_web_contents_->OpenURL(open_url_params);
  if (!new_contents)
    return;

  NotifyURLOpened(url, new_contents);
}

bool RenderViewContextMenuBase::IsCustomItemChecked(int id) const {
  return IsCustomItemCheckedInternal(params_.custom_items, id);
}

bool RenderViewContextMenuBase::IsCustomItemEnabled(int id) const {
  return IsCustomItemEnabledInternal(params_.custom_items, id);
}

