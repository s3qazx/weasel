#pragma once

#include <WeaselUI.h>

#include <mutex>

typedef CWinTraits<WS_POPUP | WS_CLIPSIBLINGS,
                   WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE>
    CFloatingToolbarTraits;

class FloatingToolbar
    : public CWindowImpl<FloatingToolbar, CWindow, CFloatingToolbarTraits>,
      public CDoubleBufferImpl<FloatingToolbar> {
 public:
  DECLARE_WND_CLASS_EX(L"WeaselFloatingToolbar",
                       CS_HREDRAW | CS_VREDRAW | CS_DROPSHADOW,
                       COLOR_WINDOW)

  BEGIN_MSG_MAP(FloatingToolbar)
  MESSAGE_HANDLER(WM_CREATE, OnCreate)
  MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
  MESSAGE_HANDLER(WM_MOUSEACTIVATE, OnMouseActivate)
  MESSAGE_HANDLER(WM_DPICHANGED, OnDpiChanged)
  MESSAGE_HANDLER(WM_DISPLAYCHANGE, OnDisplayChanged)
  MESSAGE_HANDLER(WM_SETTINGCHANGE, OnSettingChanged)
  MESSAGE_HANDLER(WM_SYSCOLORCHANGE, OnSettingChanged)
  MESSAGE_HANDLER(WM_THEMECHANGED, OnSettingChanged)
  MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLeftButtonDown)
  MESSAGE_HANDLER(WM_LBUTTONUP, OnLeftButtonUp)
  MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
  MESSAGE_HANDLER(WM_MOUSELEAVE, OnMouseLeave)
  MESSAGE_HANDLER(WM_CAPTURECHANGED, OnCaptureChanged)
  MESSAGE_HANDLER(WM_SETCURSOR, OnSetCursor)
  MESSAGE_HANDLER(kRefreshMessage, OnRefresh)
  CHAIN_MSG_MAP(CDoubleBufferImpl<FloatingToolbar>)
  END_MSG_MAP()

  explicit FloatingToolbar(weasel::UI& ui);
  ~FloatingToolbar();

  void Refresh();
  void Show();
  void DoPaint(CDCHandle dc);

 private:
  struct State {
    bool enabled = false;
    bool ascii_mode = false;
    bool full_shape = false;
    bool ascii_punct = false;
    bool simplified = false;
    int back_color = 0;
    int text_color = 0;
    int border_color = 0;
    int hilited_back_color = 0;
    int hilited_text_color = 0;
  };

  static constexpr UINT kRefreshMessage = WM_APP + 0x31;
  static constexpr int kButtonCount = 5;

  LRESULT OnCreate(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnMouseActivate(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnDpiChanged(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnDisplayChanged(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnSettingChanged(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnLeftButtonDown(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnLeftButtonUp(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnMouseMove(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnMouseLeave(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnCaptureChanged(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnSetCursor(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnRefresh(UINT, WPARAM, LPARAM, BOOL&);

  int Scale(int value) const;
  void UpdateDpi();
  void UpdateMetrics();
  void RecreateFonts();
  void RestorePosition();
  void SavePosition() const;
  void FinishDrag();
  void ClampPosition(int& x, int& y) const;
  int HitTestButton(const CPoint& point) const;
  bool IsGrip(const CPoint& point) const;
  void InvokeAction(int index, const State& state);
  State GetState() const;

  weasel::UI& ui_;
  mutable std::mutex state_mutex_;
  State state_;
  UINT dpi_ = 96;
  int width_ = 0;
  int height_ = 0;
  CRect grip_rect_;
  CRect button_rects_[kButtonCount];
  int hover_ = -1;
  int pressed_ = -1;
  bool tracking_mouse_ = false;
  bool dragging_ = false;
  CPoint drag_start_cursor_;
  CPoint drag_start_window_;
  HFONT font_ = nullptr;
  HFONT symbol_font_ = nullptr;
};
