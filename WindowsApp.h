#pragma once
#include<Windows.h>

class WindowsApp {
public:

	//ウィンドウサイズ
	static const int window_width = 1280;	//横幅
	static const int window_height = 720;	//縦幅

	//サブウィンドウサイズ
	static const int subWindow_width = 640;	 //横幅
	static const int subWindow_height = 720; //縦幅

	//ウィンドウクラスの設定
	WNDCLASSEX w{};

	//メッセージ
	MSG msg{};

	static LRESULT WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	HWND hwnd;

	HWND hwndSub;

	void createWin();

	void createSubWin();
};
