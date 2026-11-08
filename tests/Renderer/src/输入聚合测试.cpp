/*
输入聚合测试（L1 无 GPU）
覆盖：键位按住 / 按下沿 / 抬起沿 / 重复沿、帧边界清空、指针位移与按键边沿、
      滚轮增量、修饰键、文本提交与预编辑、窗口尺寸与焦点、帧序号推进。
做法：直接构造 SDL 事件喂入聚合器，不创建窗口或 Vulkan 资源。
*/
#include <gtest/gtest.h>

//获取帧输入聚合器（公共扩展头，含 SDL3 类型）
#include "Engine/EngineRenderer/src/render/Input/输入聚合.h"

//游戏引擎命名空间
using namespace engine;

namespace
{
	//构造键盘按下事件
	SDL_Event key_down(SDL_Scancode scancode, bool repeat = false, SDL_Keymod mod = SDL_KMOD_NONE)
	{
		SDL_Event event{};
		event.type = SDL_EVENT_KEY_DOWN;
		event.key.scancode = scancode;
		event.key.down = true;
		event.key.repeat = repeat;
		event.key.mod = mod;
		return event;
	}

	//构造键盘抬起事件
	SDL_Event key_up(SDL_Scancode scancode, SDL_Keymod mod = SDL_KMOD_NONE)
	{
		SDL_Event event{};
		event.type = SDL_EVENT_KEY_UP;
		event.key.scancode = scancode;
		event.key.down = false;
		event.key.mod = mod;
		return event;
	}

	//构造指针移动事件
	SDL_Event mouse_motion(float x, float y, float dx, float dy)
	{
		SDL_Event event{};
		event.type = SDL_EVENT_MOUSE_MOTION;
		event.motion.x = x;
		event.motion.y = y;
		event.motion.xrel = dx;
		event.motion.yrel = dy;
		return event;
	}
}

//键位状态跨帧保持，边沿只在按下 / 抬起的帧出现
TEST(Input_Aggregator_Test, 键位边沿与按住状态)
{
	Input_Aggregator aggregator;
	//按下 W
	SDL_Event down = key_down(SDL_SCANCODE_W);
	aggregator.event_feed(&down);
	aggregator.frame_advance(0.016, 320, 180, 1.0f);

	const Input_Frame& pressed_frame = aggregator.frame_get();
	EXPECT_EQ(pressed_frame.frame, 1u);
	EXPECT_TRUE(pressed_frame.key_held(SDL_SCANCODE_W));
	EXPECT_TRUE(pressed_frame.key_pressed_check(SDL_SCANCODE_W));
	EXPECT_FALSE(pressed_frame.key_released_check(SDL_SCANCODE_W));

	//无新事件推进：按住保持，边沿清空
	aggregator.frame_advance(0.016, 320, 180, 1.0f);
	const Input_Frame& held_frame = aggregator.frame_get();
	EXPECT_TRUE(held_frame.key_held(SDL_SCANCODE_W));
	EXPECT_FALSE(held_frame.key_pressed_check(SDL_SCANCODE_W));

	//抬起 W
	SDL_Event up = key_up(SDL_SCANCODE_W);
	aggregator.event_feed(&up);
	aggregator.frame_advance(0.016, 320, 180, 1.0f);
	const Input_Frame& released_frame = aggregator.frame_get();
	EXPECT_FALSE(released_frame.key_held(SDL_SCANCODE_W));
	EXPECT_TRUE(released_frame.key_released_check(SDL_SCANCODE_W));
}

//重复按下只记入重复沿，不重复计入按下沿
TEST(Input_Aggregator_Test, 重复按下沿)
{
	Input_Aggregator aggregator;
	//首次按下
	SDL_Event first = key_down(SDL_SCANCODE_A);
	aggregator.event_feed(&first);
	aggregator.frame_advance(0.016, 320, 180, 1.0f);
	//重复按下
	SDL_Event repeat = key_down(SDL_SCANCODE_A, true);
	aggregator.event_feed(&repeat);
	aggregator.frame_advance(0.016, 320, 180, 1.0f);

	const Input_Frame& frame = aggregator.frame_get();
	EXPECT_TRUE(frame.key_held(SDL_SCANCODE_A));
	EXPECT_FALSE(frame.key_pressed_check(SDL_SCANCODE_A));
	//重复沿登记
	EXPECT_EQ(frame.key_repeat.size(), std::size_t{1});
	EXPECT_EQ(frame.key_repeat.front(), static_cast<Input_Key_Code>(SDL_SCANCODE_A));
}

//指针位置 / 位移增量 / 鼠标按键边沿
TEST(Input_Aggregator_Test, 指针与鼠标按键)
{
	Input_Aggregator aggregator;
	//移动与左键按下
	SDL_Event motion = mouse_motion(100.0f, 50.0f, 4.0f, -2.0f);
	aggregator.event_feed(&motion);
	SDL_Event button{};
	button.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
	button.button.button = SDL_BUTTON_LEFT;
	button.button.down = true;
	button.button.x = 100.0f;
	button.button.y = 50.0f;
	aggregator.event_feed(&button);
	aggregator.frame_advance(0.016, 320, 180, 1.0f);

	const Input_Frame& pressed_frame = aggregator.frame_get();
	EXPECT_FLOAT_EQ(pressed_frame.pointer.x, 100.0f);
	EXPECT_FLOAT_EQ(pressed_frame.pointer.y, 50.0f);
	EXPECT_FLOAT_EQ(pressed_frame.pointer.dx, 4.0f);
	EXPECT_FLOAT_EQ(pressed_frame.pointer.dy, -2.0f);
	EXPECT_TRUE(pressed_frame.pointer.button_down[0]);
	EXPECT_TRUE(pressed_frame.pointer.button_pressed[0]);

	//无新事件推进：按住保持，位移增量为零，边沿清空
	aggregator.frame_advance(0.016, 320, 180, 1.0f);
	const Input_Frame& held_frame = aggregator.frame_get();
	EXPECT_TRUE(held_frame.pointer.button_down[0]);
	EXPECT_FALSE(held_frame.pointer.button_pressed[0]);
	EXPECT_FLOAT_EQ(held_frame.pointer.dx, 0.0f);

	//左键抬起
	SDL_Event release{};
	release.type = SDL_EVENT_MOUSE_BUTTON_UP;
	release.button.button = SDL_BUTTON_LEFT;
	release.button.down = false;
	release.button.x = 100.0f;
	release.button.y = 50.0f;
	aggregator.event_feed(&release);
	aggregator.frame_advance(0.016, 320, 180, 1.0f);
	const Input_Frame& released_frame = aggregator.frame_get();
	EXPECT_FALSE(released_frame.pointer.button_down[0]);
	EXPECT_TRUE(released_frame.pointer.button_released[0]);
}

//滚轮增量累积到本帧，下一帧清零
TEST(Input_Aggregator_Test, 滚轮增量)
{
	Input_Aggregator aggregator;
	//两次滚轮事件在同一帧累积
	SDL_Event wheel{};
	wheel.type = SDL_EVENT_MOUSE_WHEEL;
	wheel.wheel.x = 1.0f;
	wheel.wheel.y = -2.0f;
	aggregator.event_feed(&wheel);
	wheel.wheel.x = 0.0f;
	wheel.wheel.y = -3.0f;
	aggregator.event_feed(&wheel);
	aggregator.frame_advance(0.016, 320, 180, 1.0f);

	const Input_Frame& frame = aggregator.frame_get();
	EXPECT_FLOAT_EQ(frame.pointer.wheel_x, 1.0f);
	EXPECT_FLOAT_EQ(frame.pointer.wheel_y, -5.0f);
	//滚轮齿数按方向累计（两次均为向下）
	EXPECT_EQ(frame.pointer.wheel_up_ticks, 0u);
	EXPECT_EQ(frame.pointer.wheel_down_ticks, 2u);

	//下一帧清零
	aggregator.frame_advance(0.016, 320, 180, 1.0f);
	EXPECT_FLOAT_EQ(aggregator.frame_get().pointer.wheel_y, 0.0f);
	EXPECT_EQ(aggregator.frame_get().pointer.wheel_down_ticks, 0u);
}

//修饰键按最近一次键盘事件的 mod 更新
TEST(Input_Aggregator_Test, 修饰键状态)
{
	Input_Aggregator aggregator;
	//带 Ctrl 与 Shift 的按键事件
	SDL_Event down = key_down(SDL_SCANCODE_B, false, static_cast<SDL_Keymod>(SDL_KMOD_LCTRL | SDL_KMOD_LSHIFT));
	aggregator.event_feed(&down);
	aggregator.frame_advance(0.016, 320, 180, 1.0f);

	const Input_Frame& frame = aggregator.frame_get();
	EXPECT_TRUE(frame.modifiers.ctrl);
	EXPECT_TRUE(frame.modifiers.shift);
	EXPECT_FALSE(frame.modifiers.alt);
}

//文本提交按帧锁存，预编辑串持续保留
TEST(Input_Aggregator_Test, 文本提交与预编辑)
{
	Input_Aggregator aggregator;
	//预编辑串
	SDL_Event editing{};
	editing.type = SDL_EVENT_TEXT_EDITING;
	editing.edit.text = "ni";
	editing.edit.start = 0;
	editing.edit.length = 2;
	aggregator.event_feed(&editing);
	aggregator.frame_advance(0.016, 320, 180, 1.0f);
	EXPECT_EQ(aggregator.frame_get().text.preedit, "ni");

	//文本提交（两次累积）
	SDL_Event text{};
	text.type = SDL_EVENT_TEXT_INPUT;
	text.text.text = "你";
	aggregator.event_feed(&text);
	text.text.text = "好";
	aggregator.event_feed(&text);
	aggregator.frame_advance(0.016, 320, 180, 1.0f);

	const Input_Frame& frame = aggregator.frame_get();
	EXPECT_EQ(frame.text.committed, "你好");
	//预编辑串持续保留
	EXPECT_EQ(frame.text.preedit, "ni");

	//下一帧提交清空，预编辑保留
	aggregator.frame_advance(0.016, 320, 180, 1.0f);
	EXPECT_TRUE(aggregator.frame_get().text.committed.empty());
	EXPECT_EQ(aggregator.frame_get().text.preedit, "ni");
}

//窗口尺寸与焦点由事件更新，帧推进参数仅作为缺省来源
TEST(Input_Aggregator_Test, 窗口尺寸与焦点)
{
	Input_Aggregator aggregator;
	//尺寸变化事件
	SDL_Event resize{};
	resize.type = SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED;
	resize.window.data1 = 640;
	resize.window.data2 = 360;
	aggregator.event_feed(&resize);
	//失去焦点
	SDL_Event lost{};
	lost.type = SDL_EVENT_WINDOW_FOCUS_LOST;
	aggregator.event_feed(&lost);
	aggregator.frame_advance(0.016, 320, 180, 2.0f);

	const Input_Frame& frame = aggregator.frame_get();
	EXPECT_EQ(frame.window_width, 640u);
	EXPECT_EQ(frame.window_height, 360u);
	EXPECT_FALSE(frame.window_focused);
	EXPECT_FLOAT_EQ(frame.dpi_scale, 2.0f);
}
