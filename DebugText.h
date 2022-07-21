#pragma once
#ifndef IKD_OX_OXDEBUGFONT_H
#define IKD_OX_OXDEBUGFONT_H

namespace OX {
	class DebugFont {
		DebugFont() {}

	public:
		// 表示オプション
		struct Option {
			enum Align {
				LEFT, RIGHT, CENTER
			};
			bool clip;
			int clipWidth;
			Align align;
			Option() : clip(), clipWidth(), align(LEFT) {}
		};

		~DebugFont();

		// 初期化
		static bool initialize(void* device, unsigned maxStrNum, unsigned maxWorkingBufferSize);

		// デバッグ文字列を追加
		static bool print(int x, int y, unsigned color, const char* format, ...);

		// デバッグ文字列をオプション付きで追加
		static bool print(int x, int y, unsigned color, Option& option, const char* format, ...);

		// 描画
		static bool draw(void* device);

		// バッファをクリア
		static void clear();

		// 終了時処理
		static void terminate();
	};
}

#endif
