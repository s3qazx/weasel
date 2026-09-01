#include "stdafx.h"
#include "FloatingToolbar.h"

#include <ShellScalingApi.h>
#include <algorithm>
#include <cstdlib>
#include <cwchar>

#pragma comment(lib, "Shcore.lib")

namespace {

constexpr wchar_t kRegistryPath[] = L"Software\\Rime\\Weasel";
constexpr wchar_t kPositionValue[] = L"FloatingToolbarPosition";

COLORREF BlendColor(COLORREF foreground,
                    COLORREF background,
                    BYTE foreground_alpha) {
  const int inverse_alpha = 255 - foreground_alpha;
  return RGB((GetRValue(foreground) * foreground_alpha +
              GetRValue(background) * inverse_alpha) /
                 255,
             (GetGValue(foreground) * foreground_alpha +
              GetGValue(background) * inverse_alpha) /
                 255,
             (GetBValue(foreground) * foreground_alpha +
              GetBValue(background) * inverse_alpha) /
                 255);
}

COLORREF ResolveColor(int color, COLORREF fallback) {
  const BYTE alpha = static_cast<BYTE>((color >> 24) & 0xff);
  if (!alpha)
    return fallback;
  const COLORREF value = static_cast<COLORREF>(color & 0x00ffffff);
  return alpha == 0xff ? value : BlendColor(value, fallback, alpha);
}

}  // namespace

FloatingToolbar::FloatingToolbar(weasel::UI& ui) : ui_(ui) {}

FloatingToolbar::~FloatingToolbar() {
  if (font_)
    DeleteObject(font_);
}

int FloatingToolbar::Scale(int value) const {
  return MulDiv(value, dpi_, 96);
}

int FloatingToolbar::ScaleSystemMetric(int index) const {
  POINT origin = {0, 0};
  HMONITOR primary = MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY);
  UINT primary_dpi_x = 96;
  UINT primary_dpi_y = 96;
  if (primary)
    GetDpiForMonitor(primary, MDT_EFFECTIVE_DPI, &primary_dpi_x,
                     &primary_dpi_y);
  const UINT primary_dpi = index == SM_CYDRAG ? primary_dpi_y : primary_dpi_x;
  return (std::max)(1, MulDiv(GetSystemMetrics(index), static_cast<int>(dpi_),
                              static_cast<int>(primary_dpi)));
}

void FloatingToolbar::UpdateDpi() {
  HMONITOR monitor = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
  UINT dpi_x = 96;
  UINT dpi_y = 96;
  if (monitor)
    GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y);
  dpi_ = dpi_x;
}

void FloatingToolbar::UpdateMetrics() {
  const int button_width = Scale(button_width_);
  width_ = button_width * kButtonCount;
  height_ = Scale(toolbar_height_);
  for (int i = 0; i < kButtonCount; ++i) {
    const int left = button_width * i;
    button_rects_[i].SetRect(left, 0, left + button_width, height_);
  }

  HRGN region = corner_radius_ > 0
                    ? CreateRoundRectRgn(0, 0, width_ + 1, height_ + 1,
                                         Scale(corner_radius_ * 2),
                                         Scale(corner_radius_ * 2))
                    : CreateRectRgn(0, 0, width_ + 1, height_ + 1);
  if (region && !SetWindowRgn(region, TRUE))
    DeleteObject(region);
}

void FloatingToolbar::RecreateFonts() {
  if (font_) {
    DeleteObject(font_);
    font_ = nullptr;
  }

  NONCLIENTMETRICSW metrics = {sizeof(metrics)};
  LOGFONTW log_font = {};
  if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics,
                            0)) {
    log_font = metrics.lfMessageFont;
  } else {
    wcscpy_s(log_font.lfFaceName, L"Microsoft YaHei UI");
  }
  if (!font_face_.empty())
    wcsncpy_s(log_font.lfFaceName, LF_FACESIZE, font_face_.c_str(), _TRUNCATE);
  log_font.lfHeight = -MulDiv(font_point_, static_cast<int>(dpi_), 72);
  log_font.lfWeight = FW_NORMAL;
  font_ = CreateFontIndirectW(&log_font);
}

void FloatingToolbar::ClampPosition(int& x, int& y) const {
  POINT point = {x + width_ / 2, y + height_ / 2};
  HMONITOR monitor = MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
  MONITORINFO info = {sizeof(info)};
  if (!monitor || !GetMonitorInfoW(monitor, &info))
    return;
  x = (std::max)(static_cast<int>(info.rcWork.left),
                 (std::min)(x, static_cast<int>(info.rcWork.right) - width_));
  y = (std::max)(static_cast<int>(info.rcWork.top),
                 (std::min)(y, static_cast<int>(info.rcWork.bottom) - height_));
}

void FloatingToolbar::RestorePosition() {
  int x = 0;
  int y = 0;
  std::wstring position;
  long saved_x = 0;
  long saved_y = 0;
  if (RegGetStringValue(HKEY_CURRENT_USER, kRegistryPath, kPositionValue,
                        position) == ERROR_SUCCESS &&
      swscanf_s(position.c_str(), L"%ld,%ld", &saved_x, &saved_y) == 2) {
    x = static_cast<int>(saved_x);
    y = static_cast<int>(saved_y);
  } else {
    HMONITOR monitor = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {sizeof(info)};
    if (monitor && GetMonitorInfoW(monitor, &info)) {
      x = static_cast<int>(info.rcWork.right) - width_ - Scale(24);
      y = static_cast<int>(info.rcWork.bottom) - height_ - Scale(24);
    }
  }
  ClampPosition(x, y);
  SetWindowPos(HWND_TOPMOST, x, y, width_, height_, SWP_NOACTIVATE);
}

void FloatingToolbar::SavePosition() const {
  CRect rect;
  if (!GetWindowRect(&rect))
    return;
  const std::wstring value =
      std::to_wstring(rect.left) + L"," + std::to_wstring(rect.top);
  HKEY key = nullptr;
  if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegistryPath, 0, nullptr, 0,
                      KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
    return;
  RegSetValueExW(key, kPositionValue, 0, REG_SZ,
                 reinterpret_cast<const BYTE*>(value.c_str()),
                 static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
  RegCloseKey(key);
}

void FloatingToolbar::FinishDrag() {
  CRect rect;
  GetWindowRect(&rect);
  int x = static_cast<int>(rect.left);
  int y = static_cast<int>(rect.top);
  ClampPosition(x, y);
  SetWindowPos(HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
  SavePosition();
}

void FloatingToolbar::ResetInteraction() {
  if (dragging_ && IsWindow())
    FinishDrag();
  pressed_ = -1;
  hover_ = -1;
  dragging_ = false;
  tracking_mouse_ = false;
  if (GetCapture() == m_hWnd)
    ReleaseCapture();
}

FloatingToolbar::State FloatingToolbar::GetState() const {
  std::lock_guard<std::mutex> lock(state_mutex_);
  return state_;
}

void FloatingToolbar::Refresh() {
  State state;
  const weasel::Status& status = ui_.status();
  const weasel::UIStyle& style = ui_.style();
  const weasel::FloatingToolbarConfig& config = ui_.toolbar_config();
  state.visible = config.show;
  state.enabled = ui_.ToolbarEnabled();
  state.ascii_mode = status.ascii_mode;
  state.full_shape = status.full_shape;
  state.ascii_punct = status.ascii_punct;
  state.simplified = status.simplified;
  state.font_face = config.font_face;
  state.font_point = config.font_point;
  state.button_width = config.button_width;
  state.height = config.height;
  state.corner_radius = config.corner_radius;
  state.back_color = style.back_color;
  state.text_color = style.candidate_text_color;
  state.border_color = style.border_color;
  state.hilited_back_color = style.hilited_back_color;
  state.hilited_text_color = style.hilited_candidate_text_color;
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    state_ = state;
  }
  if (IsWindow())
    PostMessage(kRefreshMessage);
}

void FloatingToolbar::Show() {
  if (!IsWindow())
    return;
  SetWindowPos(HWND_TOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

int FloatingToolbar::HitTestButton(const CPoint& point) const {
  for (int i = 0; i < kButtonCount; ++i)
    if (button_rects_[i].PtInRect(point))
      return i;
  return -1;
}

bool FloatingToolbar::IsButtonEnabled(int index, const State& state) const {
  if (index < 0 || index >= kButtonCount)
    return false;
  return state.enabled;
}

void FloatingToolbar::InvokeAction(int index, const State& state) {
  if (!IsButtonEnabled(index, state))
    return;

  weasel::ToolbarAction action;
  bool value = false;
  switch (index) {
    case 0:
      action = weasel::ToolbarAction::ASCII_MODE;
      value = !state.ascii_mode;
      break;
    case 1:
      action = weasel::ToolbarAction::FULL_SHAPE;
      value = !state.full_shape;
      break;
    case 2:
      action = weasel::ToolbarAction::ASCII_PUNCT;
      value = !state.ascii_punct;
      break;
    case 3:
      action = weasel::ToolbarAction::SIMPLIFICATION;
      value = !state.simplified;
      break;
    default:
      return;
  }
  if (ui_.toolbarCallback())
    ui_.toolbarCallback()(action, value);
}

void FloatingToolbar::DoPaint(CDCHandle dc) {
  const State state = GetState();
  CRect client;
  GetClientRect(&client);

  const COLORREF system_back = GetSysColor(COLOR_WINDOW);
  const COLORREF background = ResolveColor(state.back_color, system_back);
  const COLORREF text =
      ResolveColor(state.text_color, GetSysColor(COLOR_WINDOWTEXT));
  const COLORREF border =
      ResolveColor(state.border_color, BlendColor(text, background, 48));
  const COLORREF highlight =
      ResolveColor(state.hilited_back_color, BlendColor(text, background, 24));
  const COLORREF highlight_text = ResolveColor(state.hilited_text_color, text);
  const COLORREF disabled_text = BlendColor(text, background, 95);

  dc.FillSolidRect(client, background);
  dc.SetBkMode(TRANSPARENT);

  const wchar_t* labels[kButtonCount] = {
      state.ascii_mode ? L"\x82F1" : L"\x4E2D",
      state.full_shape ? L"\x5168" : L"\x534A",
      state.ascii_punct ? L".," : L"\x3002\xFF0C",
      state.simplified ? L"\x7B80" : L"\x7E41"};
  for (int i = 0; i < kButtonCount; ++i) {
    const bool enabled = IsButtonEnabled(i, state);
    const bool hot = i == hover_ && enabled;
    const bool down = i == pressed_ && hot;
    CRect button = button_rects_[i];
    if (hot) {
      CRect fill = button;
      fill.DeflateRect(Scale(3), Scale(4));
      dc.FillSolidRect(fill,
                       down ? BlendColor(text, highlight, 24) : highlight);
    }
    dc.SetTextColor(enabled ? (hot ? highlight_text : text) : disabled_text);
    HFONT old_font = dc.SelectFont(font_);
    dc.DrawText(labels[i], -1, &button,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    dc.SelectFont(old_font);
  }

  for (int i = 1; i < kButtonCount; ++i) {
    const int x = static_cast<int>(button_rects_[i].left);
    dc.FillSolidRect(x, Scale(9), 1, height_ - Scale(18), border);
  }
  dc.Draw3dRect(client, border, border);
}

LRESULT FloatingToolbar::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
  UpdateDpi();
  UpdateMetrics();
  RecreateFonts();
  Refresh();
  RestorePosition();
  return 0;
}

LRESULT FloatingToolbar::OnDestroy(UINT, WPARAM, LPARAM, BOOL&) {
  ResetInteraction();
  if (font_) {
    DeleteObject(font_);
    font_ = nullptr;
  }
  return 0;
}

LRESULT FloatingToolbar::OnMouseActivate(UINT, WPARAM, LPARAM, BOOL& handled) {
  handled = TRUE;
  return MA_NOACTIVATE;
}

LRESULT FloatingToolbar::OnDpiChanged(UINT,
                                      WPARAM w_param,
                                      LPARAM l_param,
                                      BOOL&) {
  dpi_ = LOWORD(w_param);
  UpdateMetrics();
  RecreateFonts();
  const RECT* suggested = reinterpret_cast<const RECT*>(l_param);
  int x = static_cast<int>(suggested->left);
  int y = static_cast<int>(suggested->top);
  ClampPosition(x, y);
  SetWindowPos(HWND_TOPMOST, x, y, width_, height_, SWP_NOACTIVATE);
  if (dragging_) {
    GetCursorPos(&drag_start_cursor_);
    drag_start_window_.SetPoint(x, y);
  }
  SavePosition();
  Invalidate();
  return 0;
}

LRESULT FloatingToolbar::OnDisplayChanged(UINT, WPARAM, LPARAM, BOOL&) {
  FinishDrag();
  return 0;
}

LRESULT FloatingToolbar::OnSettingChanged(UINT, WPARAM, LPARAM, BOOL&) {
  RecreateFonts();
  FinishDrag();
  Invalidate();
  return 0;
}

LRESULT FloatingToolbar::OnLeftButtonDown(UINT,
                                          WPARAM,
                                          LPARAM l_param,
                                          BOOL& handled) {
  const CPoint point(GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param));
  pressed_ = HitTestButton(point);
  if (pressed_ >= 0) {
    GetCursorPos(&drag_start_cursor_);
    CRect rect;
    GetWindowRect(&rect);
    drag_start_window_.SetPoint(rect.left, rect.top);
    SetCapture();
    Invalidate();
  }
  handled = TRUE;
  return 0;
}

LRESULT FloatingToolbar::OnLeftButtonUp(UINT,
                                        WPARAM,
                                        LPARAM l_param,
                                        BOOL& handled) {
  const CPoint point(GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param));
  const int pressed = pressed_;
  const bool was_dragging = dragging_;
  pressed_ = -1;
  dragging_ = false;
  if (GetCapture() == m_hWnd)
    ReleaseCapture();
  if (was_dragging) {
    FinishDrag();
  } else if (pressed >= 0 && HitTestButton(point) == pressed) {
    InvokeAction(pressed, GetState());
  }
  Invalidate();
  handled = TRUE;
  return 0;
}

LRESULT FloatingToolbar::OnMouseMove(UINT,
                                     WPARAM,
                                     LPARAM l_param,
                                     BOOL& handled) {
  if (!tracking_mouse_) {
    TRACKMOUSEEVENT event = {sizeof(event), TME_LEAVE, m_hWnd, 0};
    TrackMouseEvent(&event);
    tracking_mouse_ = true;
  }

  CPoint cursor;
  if (pressed_ >= 0 && GetCapture() == m_hWnd) {
    GetCursorPos(&cursor);
    const int delta_x = static_cast<int>(cursor.x - drag_start_cursor_.x);
    const int delta_y = static_cast<int>(cursor.y - drag_start_cursor_.y);
    if (std::abs(delta_x) >= ScaleSystemMetric(SM_CXDRAG) ||
        std::abs(delta_y) >= ScaleSystemMetric(SM_CYDRAG)) {
      pressed_ = -1;
      hover_ = -1;
      dragging_ = true;
      SetCursor(LoadCursor(nullptr, IDC_SIZEALL));
      Invalidate();
    }
  }

  if (dragging_) {
    GetCursorPos(&cursor);
    const int x = static_cast<int>(drag_start_window_.x + cursor.x -
                                   drag_start_cursor_.x);
    const int y = static_cast<int>(drag_start_window_.y + cursor.y -
                                   drag_start_cursor_.y);
    SetWindowPos(HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
  } else {
    const CPoint point(GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param));
    const int hover = HitTestButton(point);
    if (hover != hover_) {
      hover_ = hover;
      Invalidate();
    }
  }
  handled = TRUE;
  return 0;
}

LRESULT FloatingToolbar::OnMouseLeave(UINT, WPARAM, LPARAM, BOOL&) {
  tracking_mouse_ = false;
  if (!dragging_) {
    hover_ = -1;
    Invalidate();
  }
  return 0;
}

LRESULT FloatingToolbar::OnCaptureChanged(UINT, WPARAM, LPARAM, BOOL&) {
  if (dragging_)
    FinishDrag();
  dragging_ = false;
  pressed_ = -1;
  Invalidate();
  return 0;
}

LRESULT FloatingToolbar::OnSetCursor(UINT, WPARAM, LPARAM, BOOL& handled) {
  CPoint point;
  GetCursorPos(&point);
  ScreenToClient(&point);
  const int button = HitTestButton(point);
  const State state = GetState();
  LPCTSTR cursor = dragging_                        ? IDC_SIZEALL
                   : IsButtonEnabled(button, state) ? IDC_HAND
                                                    : IDC_ARROW;
  SetCursor(LoadCursor(nullptr, cursor));
  handled = TRUE;
  return TRUE;
}

LRESULT FloatingToolbar::OnRefresh(UINT, WPARAM, LPARAM, BOOL&) {
  const State state = GetState();
  const bool metrics_changed = state.button_width != button_width_ ||
                               state.height != toolbar_height_ ||
                               state.corner_radius != corner_radius_;
  const bool font_changed =
      state.font_face != font_face_ || state.font_point != font_point_;

  button_width_ = state.button_width;
  toolbar_height_ = state.height;
  corner_radius_ = state.corner_radius;
  font_face_ = state.font_face;
  font_point_ = state.font_point;

  if (metrics_changed || font_changed) {
    CRect rect;
    GetWindowRect(&rect);
    if (metrics_changed)
      UpdateMetrics();
    if (font_changed)
      RecreateFonts();
    int x = static_cast<int>(rect.left);
    int y = static_cast<int>(rect.top);
    ClampPosition(x, y);
    SetWindowPos(HWND_TOPMOST, x, y, width_, height_, SWP_NOACTIVATE);
    if (dragging_) {
      GetCursorPos(&drag_start_cursor_);
      drag_start_window_.SetPoint(x, y);
    }
    SavePosition();
  }

  if (!state.visible) {
    ResetInteraction();
    ShowWindow(SW_HIDE);
    visible_ = false;
    return 0;
  }
  if (!visible_) {
    Show();
    visible_ = true;
  }
  Invalidate();
  return 0;
}
