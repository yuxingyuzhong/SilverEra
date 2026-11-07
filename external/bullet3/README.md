# Bullet3（裁剪副本）

- **来源**：`bulletphysics/bullet3`；本机基准副本为 `D:\rely\bullet3-master`（上游 `VERSION` 为 3.27）。
- **实际代码版本**：`BT_BULLET_VERSION = 326`（3.26），以 `src/LinearMath/btScalar.h` 为准。
- **许可**：zlib，见 `LICENSE.txt`；版权与作者信息见 `AUTHORS.txt`（两份文件自上游基准副本复制，许可文本与版本无关）。
- **裁剪说明**（zlib 许可要求“修改过的源码版本必须明确标注”，以下为标注内容）：
  - 仅保留 `src/LinearMath`、`src/BulletCollision`（含 `Gimpact`）；
  - 已删除 `BulletDynamics`、`BulletSoftBody`、`Bullet3*`、`Extras`、`examples`、`docs`、`data` 等与碰撞检测无关的部分；
  - **未修改任何保留文件的源码内容**，仅删除文件；
  - 不包含上游顶层 `CMakeLists.txt`，构建由 `Engine/EngineSystem/CMakeLists.txt` 以源清单方式直接控制。
- **用途**：EngineSystem 碰撞检测的内部实现，直接编入 `EngineSystem.lib`，不单独产出或发布库。
