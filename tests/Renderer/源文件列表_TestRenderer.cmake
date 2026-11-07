#
# =============================================================================
# Application/Test/tests/Renderer 源文件列表（生成物，勿手改）
# -----------------------------------------------------------------------------
# 生成方式
#     由 Engine/EngineFoundation/cmake/生成源文件列表.cmake 在每轮配置时自动维护：把
#     file(GLOB_RECURSE) 的扫描结果与本文件比对（按路径集合），仅在集合有差异时
#     整体写回。
#
# 为什么要这个文件
#     CMakeLists 用 GLOB 收集源文件时，构建脚本里没有任何「具体文件名」。
#     Visual Studio 的文件视图改名 / 迁移操作，靠 File API 的 backtrace 找
#     「可改写的引用」；backtrace 只落在「把源文件直接加入 target 的那条命令」
#     上。本文件用 target_sources 直接列出文件名的字面量，于是 VS 的改写就会落到
#     本文件上；而本文件被 CMakeLists include，属配置依赖，被改写即触发重新配置。
#
# 维护
#     - 不要手工编辑。新增 / 删除 / 改名文件后，下一轮配置自动刷新。
#     - 路径一律相对本层根目录（Application/Test/tests/Renderer），与 VS 改写后的写法一致。
#     - 这里是这些文件的「唯一」加入点：CMakeLists 的 add_library 不再传这些源文件，
#       否则 backtrace 会归到 add_library 上，VS 就又无法改写了。
# =============================================================================
#
target_sources(TestRenderer PRIVATE
    "src/注册锚.cpp"
    "src/渲染Alpha覆盖测试.cpp"
    "src/渲染Cluster测试.cpp"
    "src/渲染GPU耗时测试.cpp"
    "src/渲染GPU驱动测试.cpp"
    "src/渲染HiZ测试.cpp"
    "src/渲染HiZ遮挡测试.cpp"
    "src/渲染MSAA测试.cpp"
    "src/渲染PCSS软阴影测试.cpp"
    "src/渲染中文字形测试.cpp"
    "src/渲染交换链重建测试.cpp"
    "src/渲染像素回读测试.cpp"
    "src/渲染光照查询测试.cpp"
    "src/渲染光照测试.cpp"
    "src/渲染光照选择测试.cpp"
    "src/渲染关卡测试.cpp"
    "src/渲染初始化测试.cpp"
    "src/渲染动态网格测试.cpp"
    "src/渲染可见窗口测试.cpp"
    "src/渲染合批测试.cpp"
    "src/渲染同帧后处理测试.cpp"
    "src/渲染后处理伽马测试.cpp"
    "src/渲染后处理体积光测试.cpp"
    "src/渲染后处理体积雾测试.cpp"
    "src/渲染后处理分级调色测试.cpp"
    "src/渲染后处理反射测试.cpp"
    "src/渲染后处理扩展预设测试.cpp"
    "src/渲染后处理接触阴影测试.cpp"
    "src/渲染后处理景深测试.cpp"
    "src/渲染后处理查找表测试.cpp"
    "src/渲染后处理模糊测试.cpp"
    "src/渲染后处理法线描边测试.cpp"
    "src/渲染后处理泛光测试.cpp"
    "src/渲染后处理色彩校正测试.cpp"
    "src/渲染后处理色相曝光测试.cpp"
    "src/渲染后处理调色测试.cpp"
    "src/渲染后处理遮蔽测试.cpp"
    "src/渲染后处理链测试.cpp"
    "src/渲染后处理颗粒测试.cpp"
    "src/渲染场景效果测试.cpp"
    "src/渲染场景数据测试.cpp"
    "src/渲染外扩描边测试.cpp"
    "src/渲染多管线测试.cpp"
    "src/渲染多线程测试.cpp"
    "src/渲染多线程阈值测试.cpp"
    "src/渲染天空盒测试.cpp"
    "src/渲染实例化测试.cpp"
    "src/渲染帧测试.cpp"
    "src/渲染帧统计测试.cpp"
    "src/渲染文字布局测试.cpp"
    "src/渲染文字测试.cpp"
    "src/渲染曝光补偿测试.cpp"
    "src/渲染材质测试.cpp"
    "src/渲染材质贴图测试.cpp"
    "src/渲染水平面测试.cpp"
    "src/渲染深度描边测试.cpp"
    "src/渲染混合测试.cpp"
    "src/渲染物体多材质测试.cpp"
    "src/渲染相机测试.cpp"
    "src/渲染着色器校验测试.cpp"
    "src/渲染离屏目标测试.cpp"
    "src/渲染窗口属性测试.cpp"
    "src/渲染粒子测试.cpp"
    "src/渲染级联淡化数据测试.cpp"
    "src/渲染级联配置数据测试.cpp"
    "src/渲染级联阴影测试.cpp"
    "src/渲染纹理分级测试.cpp"
    "src/渲染缓存与阴影范围测试.cpp"
    "src/渲染网格纹理测试.cpp"
    "src/渲染聚光偏置缩放测试.cpp"
    "src/渲染聚光灯测试.cpp"
    "src/渲染聚光逐盏阴影测试.cpp"
    "src/渲染聚光阴影测试.cpp"
    "src/渲染计算测试.cpp"
    "src/渲染资产加载测试.cpp"
    "src/渲染资源与链查询测试.cpp"
    "src/渲染超采样测试.cpp"
    "src/渲染逐物体卡通测试.cpp"
    "src/渲染逐绘制项纹理测试.cpp"
    "src/渲染采样器各向异性测试.cpp"
    "src/渲染阴影口径一致性测试.cpp"
    "src/渲染阴影测试.cpp"
    "src/渲染阴影稳定性测试.cpp"
    "src/渲染阴影纹素钳制测试.cpp"
    "src/渲染阴影诊断测试.cpp"
    "src/渲染阴影预算测试.cpp"
    "src/渲染雾测试.cpp"
    "src/渲染风格化测试.cpp"
    "src/粒子数据测试.cpp"
    "src/粒子系统测试.cpp"
    "src/纹理Mipmap测试.cpp"
    "src/纹理解码测试.cpp"
)

# 头文件不参与编译，只为让 IDE 建立文件索引；同样经 target_sources 列出，
# 使头文件改名 / 迁移时 VS 也能改写引用
target_sources(TestRenderer PRIVATE
    "src/渲染后处理链测试辅助.h"
    "src/渲染场景测试辅助.h"
)
