#pragma once
#include "Windows.h"

class WindowsApp {
public:

	//�E�B���h�E�T�C�Y
	const int window_width = 1280;	//����
	const int window_height = 720;	//�c��

	//�E�B���h�E�N���X�̐ݒ�
	WNDCLASSEX w{};

	static LRESULT WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

};
