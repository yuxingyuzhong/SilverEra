/*
原生 UI 最小闭环测试（L2 隐藏窗口 + 像素回读）
覆盖：面板 / 按钮绘制、悬停命中、点击回调与单击标记、文本框焦点与文本编辑、光标绘制。
做法：隐藏窗口初始化渲染器，创建控件；合成输入经 Input_Aggregator 直接喂入；
      断言一律格式无关（以「是否近似清屏色」判定控件像素，alpha 取第 4 字节）。
字体取自系统 TTF（`C:\\Windows\\Fonts\\consola.ttf`，不存在时字符类断言跳过）。
*/
#include <gtest/gtest.h>

//获取渲染器门面（公共头，Vulkan / SDL-free）
#include "Engine/EngineRenderer/src/render/Renderer/渲染器.h"
//获取原生 UI 控件树
#include "Engine/EngineRenderer/src/render/UI/用户界面.h"
//获取帧输入聚合器（公共扩展头，含 SDL3）
#include "Engine/EngineRenderer/src/render/Input/输入聚合.h"

//游戏引擎命名空间
using namespace engine;

namespace
{
	//窗口尺寸
	constexpr uint32_t WINDOW_WIDTH = 320;
	constexpr uint32_t WINDOW_HEIGHT = 180;
	//字体像素高度
	constexpr uint32_t FONT_PIXEL_HEIGHT = 16;
	//系统字体路径（本机存在才跑字符类断言）
	const char* const FONT_PATH = "C:\\Windows\\Fonts\\consola.ttf";

	//清屏色候选（0.05 / 0.05 / 0.10：UNORM ≈ 13/13/26，sRGB ≈ 63/63/89）
	constexpr std::array<std::array<uint8_t, 3>, 2> CLEAR_CANDIDATES = { {
		{ { 13, 13, 26 } },
		{ { 63, 63, 89 } },
	} };
	//清屏色通道容差
	constexpr int CLEAR_TOLERANCE = 8;

	//构造隐藏窗口配置
	Renderer_Config hidden_config(void)
	{
		Renderer_Config config;
		config.width = WINDOW_WIDTH;
		config.height = WINDOW_HEIGHT;
		config.hidden = true;
		return config;
	}

	//本帧像素字节数
	uint32_t pixel_bytes(void)
	{
		return WINDOW_WIDTH * WINDOW_HEIGHT * 4;
	}

	//系统字体是否可用
	bool font_available(void)
	{
		std::error_code code;
		return std::filesystem::exists(std::filesystem::path(FONT_PATH), code);
	}

	//取指定像素的四个字节
	std::array<uint8_t, 4> pixel_get(const std::vector<uint8_t>& pixels, uint32_t x, uint32_t y)
	{
		std::size_t offset = (static_cast<std::size_t>(y) * WINDOW_WIDTH + x) * 4;
		std::array<uint8_t, 4> value{};
		for (std::size_t index = 0; index < 4; ++index)
			value[index] = pixels[offset + index];
		return value;
	}

	//判断像素是否近似等于清屏色（通道排序后比对，兼容 RGBA / BGRA 读回顺序）
	bool pixel_is_clear(std::array<uint8_t, 4> value)
	{
		//排序后的四通道
		std::array<uint8_t, 4> sorted = value;
		std::sort(sorted.begin(), sorted.end());
		//逐候选比对前三个通道
		for (const std::array<uint8_t, 3>& candidate : CLEAR_CANDIDATES)
		{
			bool matched = true;
			for (std::size_t index = 0; index < 3; ++index)
			{
				int delta = static_cast<int>(sorted[index]) - static_cast<int>(candidate[index]);
				if (delta < -CLEAR_TOLERANCE || delta > CLEAR_TOLERANCE)
					matched = false;
			}
			if (matched)
				return true;
		}
		return false;
	}

	//构造指针移动事件
	SDL_Event mouse_motion(float x, float y)
	{
		SDL_Event event{};
		event.type = SDL_EVENT_MOUSE_MOTION;
		event.motion.x = x;
		event.motion.y = y;
		return event;
	}

	//构造鼠标左键事件
	SDL_Event mouse_button(bool down, float x, float y)
	{
		SDL_Event event{};
		event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
		event.button.button = SDL_BUTTON_LEFT;
		event.button.down = down;
		event.button.x = x;
		event.button.y = y;
		return event;
	}

	//构造键盘事件
	SDL_Event key_event(bool down, SDL_Scancode scancode, SDL_Keymod mod = SDL_KMOD_NONE)
	{
		SDL_Event event{};
		event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
		event.key.scancode = scancode;
		event.key.down = down;
		event.key.mod = mod;
		return event;
	}

	//点击回调计数
	struct Click_Counter
	{
		//触发次数
		uint32_t count = 0;
	};

	//按钮点击回调
	void on_click(Ui_Handle, void* user)
	{
		static_cast<Click_Counter*>(user)->count += 1;
	}

	//复选框切换计数
	struct Toggle_Counter
	{
		//触发次数
		uint32_t count = 0;
		//最近一次状态
		bool last = false;
	};

	//复选框切换回调
	void on_toggle(Ui_Handle, bool checked, void* user)
	{
		Toggle_Counter* counter = static_cast<Toggle_Counter*>(user);
		counter->count += 1;
		counter->last = checked;
	}

	//滑动条取值变化计数
	struct Value_Counter
	{
		//变化次数
		uint32_t count = 0;
		//最近一次取值
		float last = 0.0f;
	};

	//滑动条取值回调
	void on_value(Ui_Handle, float value, void* user)
	{
		Value_Counter* counter = static_cast<Value_Counter*>(user);
		counter->count += 1;
		counter->last = value;
	}
}

//面板与按钮绘制：控件矩形内像素不再是清屏色
TEST(User_Interface_Test, 面板与按钮绘制)
{
	Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config())) << "隐藏窗口初始化失败";
	if (!renderer.readback_supported())
		GTEST_SKIP() << "设备不支持像素回读";

	//取原生 UI 上下文
	Ui_Context* ui = renderer.ui_context();
	ASSERT_NE(ui, nullptr);

	//面板铺满窗口
	Ui_Handle panel = ui->panel_create(ui->root_get(), Ui_Rect{ 0.0f, 0.0f,
		static_cast<float>(WINDOW_WIDTH), static_cast<float>(WINDOW_HEIGHT) },
		Ui_Color{ 0.25f, 0.08f, 0.08f, 1.0f });
	ASSERT_NE(panel, 0u);
	//按钮
	Ui_Handle button = ui->button_create(ui->root_get(), Ui_Rect{ 20.0f, 20.0f, 100.0f, 30.0f }, "OK");
	ASSERT_NE(button, 0u);

	//渲染一帧并回读
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));

	//面板区域（左上角）
	EXPECT_FALSE(pixel_is_clear(pixel_get(pixels, 5, 5))) << "面板未绘制";
	//按钮区域（中心）
	EXPECT_FALSE(pixel_is_clear(pixel_get(pixels, 60, 35))) << "按钮未绘制";
}

//悬停命中与点击回调
TEST(User_Interface_Test, 悬停与点击)
{
	Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	Ui_Context* ui = renderer.ui_context();
	ASSERT_NE(ui, nullptr);
	Input_Aggregator* input = renderer.input_aggregator();
	ASSERT_NE(input, nullptr);

	//按钮与点击计数
	Click_Counter counter{};
	Ui_Handle button = ui->button_create(ui->root_get(), Ui_Rect{ 20.0f, 20.0f, 100.0f, 30.0f }, "OK", on_click, &counter);
	ASSERT_NE(button, 0u);

	//指针移到按钮中心并推进一帧
	SDL_Event motion = mouse_motion(60.0f, 35.0f);
	input->event_feed(&motion);
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_TRUE(ui->hovered_get(button)) << "指针应命中按钮";

	//按下与抬起（同一帧完成一次单击）
	SDL_Event down = mouse_button(true, 60.0f, 35.0f);
	input->event_feed(&down);
	ASSERT_TRUE(renderer.frame_render());
	SDL_Event up = mouse_button(false, 60.0f, 35.0f);
	input->event_feed(&up);
	ASSERT_TRUE(renderer.frame_render());

	EXPECT_EQ(counter.count, 1u) << "点击回调应触发一次";
	EXPECT_TRUE(ui->clicked_consume(button)) << "单击标记应在同帧可消费";
	//再次消费应为假
	EXPECT_FALSE(ui->clicked_consume(button));
}

//文本框焦点与文本编辑
TEST(User_Interface_Test, 文本框焦点与编辑)
{
	//字体不可用时只验证焦点与文本状态
	bool font_ready = font_available();

	Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	Ui_Context* ui = renderer.ui_context();
	Input_Aggregator* input = renderer.input_aggregator();
	ASSERT_NE(ui, nullptr);
	ASSERT_NE(input, nullptr);

	//加载字体并绑定到 UI（失败时字体系断言跳过）
	uint64_t font = 0;
	if (font_ready)
		font_ready = renderer.font_load_file(font, FONT_PATH, FONT_PIXEL_HEIGHT);
	if (font_ready)
		ui->font_set(font);

	//文本框
	Ui_Handle text_box = ui->text_box_create(ui->root_get(), Ui_Rect{ 20.0f, 20.0f, 200.0f, 28.0f }, "");
	ASSERT_NE(text_box, 0u);

	//点击文本框获得焦点
	SDL_Event motion = mouse_motion(60.0f, 30.0f);
	input->event_feed(&motion);
	SDL_Event down = mouse_button(true, 60.0f, 30.0f);
	input->event_feed(&down);
	SDL_Event up = mouse_button(false, 60.0f, 30.0f);
	input->event_feed(&up);
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_TRUE(ui->focused_get(text_box)) << "点击应让文本框获焦";
	EXPECT_TRUE(ui->text_focus_active());

	//提交文本 "abc"
	SDL_Event text{};
	text.type = SDL_EVENT_TEXT_INPUT;
	text.text.text = "abc";
	input->event_feed(&text);
	ASSERT_TRUE(renderer.frame_render());
	std::string content{};
	ASSERT_TRUE(ui->text_get(text_box, content));
	EXPECT_EQ(content, "abc");

	//退格删除一个字符
	SDL_Event backspace{};
	backspace.type = SDL_EVENT_KEY_DOWN;
	backspace.key.scancode = SDL_SCANCODE_BACKSPACE;
	backspace.key.down = true;
	input->event_feed(&backspace);
	ASSERT_TRUE(renderer.frame_render());
	ASSERT_TRUE(ui->text_get(text_box, content));
	EXPECT_EQ(content, "ab");

	//有字体时校验文本框内出现墨迹
	if (font_ready && renderer.readback_supported())
	{
		std::vector<uint8_t> pixels(pixel_bytes());
		ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		bool ink = false;
		for (uint32_t y = 24; y < 45 && !ink; ++y)
			for (uint32_t x = 26; x < 90 && !ink; ++x)
				if (!pixel_is_clear(pixel_get(pixels, x, y)))
					ink = true;
		EXPECT_TRUE(ink) << "文本框内应出现文字墨迹";
	}
}

//复选框：点击切换选中并触发回调
TEST(User_Interface_Test, 复选框点击切换)
{
	Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	Ui_Context* ui = renderer.ui_context();
	Input_Aggregator* input = renderer.input_aggregator();
	ASSERT_NE(ui, nullptr);
	ASSERT_NE(input, nullptr);

	//复选框与切换计数
	Toggle_Counter counter{};
	Ui_Handle checkbox = ui->checkbox_create(ui->root_get(), Ui_Rect{ 20.0f, 20.0f, 120.0f, 24.0f },
		"Enable", false, on_toggle, &counter);
	ASSERT_NE(checkbox, 0u);

	//点击方框区域（左侧 20×20）
	SDL_Event motion = mouse_motion(28.0f, 32.0f);
	input->event_feed(&motion);
	SDL_Event down = mouse_button(true, 28.0f, 32.0f);
	input->event_feed(&down);
	SDL_Event up = mouse_button(false, 28.0f, 32.0f);
	input->event_feed(&up);
	ASSERT_TRUE(renderer.frame_render());

	EXPECT_TRUE(ui->checked_get(checkbox)) << "点击应切换为选中";
	EXPECT_EQ(counter.count, 1u);
	EXPECT_TRUE(counter.last);
}

//滑动条：按下与拖拽更新取值并触发回调
TEST(User_Interface_Test, 滑动条拖拽)
{
	Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	Ui_Context* ui = renderer.ui_context();
	Input_Aggregator* input = renderer.input_aggregator();
	ASSERT_NE(ui, nullptr);
	ASSERT_NE(input, nullptr);

	//滑动条：x = 20，宽 200，初值 0
	Value_Counter counter{};
	Ui_Handle slider = ui->slider_create(ui->root_get(), Ui_Rect{ 20.0f, 30.0f, 200.0f, 20.0f },
		0.0f, on_value, &counter);
	ASSERT_NE(slider, 0u);

	//在 25% 处按下
	SDL_Event motion = mouse_motion(70.0f, 40.0f);
	input->event_feed(&motion);
	SDL_Event down = mouse_button(true, 70.0f, 40.0f);
	input->event_feed(&down);
	ASSERT_TRUE(renderer.frame_render());
	float value = 0.0f;
	ASSERT_TRUE(ui->value_get(slider, value));
	EXPECT_NEAR(value, 0.25f, 0.02f);

	//按住拖到 75%
	SDL_Event drag = mouse_motion(170.0f, 40.0f);
	input->event_feed(&drag);
	ASSERT_TRUE(renderer.frame_render());
	ASSERT_TRUE(ui->value_get(slider, value));
	EXPECT_NEAR(value, 0.75f, 0.02f);
	EXPECT_GE(counter.count, 1u);
	EXPECT_NEAR(counter.last, value, 0.02f);

	//抬起结束拖拽
	SDL_Event up = mouse_button(false, 170.0f, 40.0f);
	input->event_feed(&up);
	ASSERT_TRUE(renderer.frame_render());
}

//Tab 焦点导航：向前 / 向后循环切换可聚焦控件
TEST(User_Interface_Test, Tab焦点导航)
{
	Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	Ui_Context* ui = renderer.ui_context();
	Input_Aggregator* input = renderer.input_aggregator();
	ASSERT_NE(ui, nullptr);
	ASSERT_NE(input, nullptr);

	//两个按钮
	Ui_Handle first = ui->button_create(ui->root_get(), Ui_Rect{ 20.0f, 20.0f, 100.0f, 24.0f }, "A");
	Ui_Handle second = ui->button_create(ui->root_get(), Ui_Rect{ 20.0f, 50.0f, 100.0f, 24.0f }, "B");
	ASSERT_NE(first, 0u);
	ASSERT_NE(second, 0u);

	//点击第一个按钮使其获焦
	SDL_Event motion = mouse_motion(60.0f, 32.0f);
	input->event_feed(&motion);
	SDL_Event down = mouse_button(true, 60.0f, 32.0f);
	input->event_feed(&down);
	SDL_Event up = mouse_button(false, 60.0f, 32.0f);
	input->event_feed(&up);
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_TRUE(ui->focused_get(first));

	//Tab 向前 → 第二个按钮
	SDL_Event tab_down = key_event(true, SDL_SCANCODE_TAB);
	input->event_feed(&tab_down);
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_TRUE(ui->focused_get(second));
	SDL_Event tab_up = key_event(false, SDL_SCANCODE_TAB);
	input->event_feed(&tab_up);
	ASSERT_TRUE(renderer.frame_render());

	//Shift+Tab 向后 → 回到第一个按钮
	SDL_Event shift_tab = key_event(true, SDL_SCANCODE_TAB, SDL_KMOD_LSHIFT);
	input->event_feed(&shift_tab);
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_TRUE(ui->focused_get(first));
}

//标签绘制与文本裁剪：超出控件矩形的内容被 scissor 裁掉
TEST(User_Interface_Test, 标签绘制与裁剪)
{
	//字体不可用时跳过
	if (!font_available())
		GTEST_SKIP() << "系统字体不存在";

	Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "设备不支持像素回读";
	Ui_Context* ui = renderer.ui_context();
	ASSERT_NE(ui, nullptr);

	//加载字体并绑定
	uint64_t font = 0;
	ASSERT_TRUE(renderer.font_load_file(font, FONT_PATH, FONT_PIXEL_HEIGHT));
	ui->font_set(font);

	//标签矩形宽 40，文本远超宽度
	Ui_Handle label = ui->label_create(ui->root_get(), Ui_Rect{ 20.0f, 20.0f, 40.0f, 20.0f },
		"AAAAAAAAAAAAAAAAAAAAAAAA");
	ASSERT_NE(label, 0u);

	//渲染并回读
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));

	//矩形内应有墨迹
	bool inside_ink = false;
	for (uint32_t y = 20; y < 40 && !inside_ink; ++y)
		for (uint32_t x = 20; x < 60 && !inside_ink; ++x)
			if (!pixel_is_clear(pixel_get(pixels, x, y)))
				inside_ink = true;
	EXPECT_TRUE(inside_ink) << "标签矩形内应出现文字墨迹";

	//矩形右侧 20 像素内应保持清屏色（scissor 生效）
	bool outside_ink = false;
	for (uint32_t y = 20; y < 40 && !outside_ink; ++y)
		for (uint32_t x = 62; x < 100 && !outside_ink; ++x)
			if (!pixel_is_clear(pixel_get(pixels, x, y)))
				outside_ink = true;
	EXPECT_FALSE(outside_ink) << "标签矩形外不应出现文字墨迹（裁剪失效）";
}
