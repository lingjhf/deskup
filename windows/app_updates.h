#pragma once

#include <flutter/binary_messenger.h>
#include <flutter/encodable_value.h>
#include <flutter/method_channel.h>
#include <windows.h>

#include <memory>

// Network/patch work runs off the UI thread; channel calls stay on the UI thread.
class AppUpdates {
 public:
  static UINT StatusMessage() {
    static const UINT message = RegisterWindowMessageW(L"deskup.statusChanged");
    return message;
  }
  static constexpr UINT_PTR kTimerId = 0xCD41;
  AppUpdates(HWND window, flutter::BinaryMessenger* messenger);
  ~AppUpdates();
  bool HandleMessage(UINT message, WPARAM wparam);

 private:
  enum class Operation { Initialize, Check, AutomaticCheck, Download, Install };
  struct State;
  void Start(Operation operation);
  flutter::EncodableValue Status();
  std::shared_ptr<State> state_;
  std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>> channel_;
  std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> install_result_;
  bool ready_ = false;
};
