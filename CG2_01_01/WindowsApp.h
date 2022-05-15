#pragma once
#include "Windows.h"

class WindowsApp {
public:

	//ウィンドウサイズ
	const int window_width = 1280;	//横幅
	const int window_height = 720;	//縦幅

	//ウィンドウクラスの設定
	WNDCLASSEX w{};

	static LRESULT WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

};
