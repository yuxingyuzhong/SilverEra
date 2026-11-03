#include "src/scene/Level/局部命名空间使用.h"
//三角函数与常量
#include <cmath>

namespace engine
{
	/*
	构造关卡对象的变换矩阵（第四百三十一轮迁入系统层）
	M = T · R · S（列主序写出）；R 为欧拉角（度）按 X → Y → Z 合成，即 R = Rz · Ry · Rx。
	旋转全零时 R 退化为单位阵，输出即「对角缩放 ＋ 末列平移」，与既有手工构造口径一致。
	*/
	void level_transform_build(const float position[3], const float rotation_deg[3],
		const float scale[3], float out[16])
	{
		//角度（弧度）
		const float degree_to_radian = 3.14159265f / 180.0f;
		const float angle_x = rotation_deg[0] * degree_to_radian;
		const float angle_y = rotation_deg[1] * degree_to_radian;
		const float angle_z = rotation_deg[2] * degree_to_radian;
		//三轴余弦 / 正弦
		const float cx = std::cos(angle_x);
		const float sx = std::sin(angle_x);
		const float cy = std::cos(angle_y);
		const float sy = std::sin(angle_y);
		const float cz = std::cos(angle_z);
		const float sz = std::sin(angle_z);

		/*
		R = Rz · Ry · Rx（先行后列记法）
		r[row][column]；顺序 X → Y → Z 表示先绕 X，再绕 Y，最后绕 Z。
		*/
		const float r00 = cz * cy;
		const float r01 = cz * sy * sx - sz * cx;
		const float r02 = cz * sy * cx + sz * sx;
		const float r10 = sz * cy;
		const float r11 = sz * sy * sx + cz * cx;
		const float r12 = sz * sy * cx - cz * sx;
		const float r20 = -sy;
		const float r21 = cy * sx;
		const float r22 = cy * cx;

		//第 0 列：R 的第 0 列 × 缩放 x
		out[0] = r00 * scale[0];
		out[1] = r10 * scale[0];
		out[2] = r20 * scale[0];
		out[3] = 0.0f;
		//第 1 列：R 的第 1 列 × 缩放 y
		out[4] = r01 * scale[1];
		out[5] = r11 * scale[1];
		out[6] = r21 * scale[1];
		out[7] = 0.0f;
		//第 2 列：R 的第 2 列 × 缩放 z
		out[8] = r02 * scale[2];
		out[9] = r12 * scale[2];
		out[10] = r22 * scale[2];
		out[11] = 0.0f;
		//第 3 列：平移
		out[12] = position[0];
		out[13] = position[1];
		out[14] = position[2];
		out[15] = 1.0f;
	}
}
