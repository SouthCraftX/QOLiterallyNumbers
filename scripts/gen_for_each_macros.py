def generate_for_each_macros(max_num=127):
    macros = []
    
    # 处理 0 的情况
    macros.append("#define __QOBD_FOR_EACH_0(ctx, action, ...)")
    
    # 生成 1-127 的宏
    for i in range(1, max_num + 1):
        macro = f"#define __QOBD_FOR_EACH_{i}(ctx, action"
        
        # 添加参数 _1 到 _i
        params = [f"_{j+1}" for j in range(i)]
        macro += ", " + ", ".join(params) + ") "
        
        # 添加宏体
        if i == 1:
            macro += "action(ctx, _1);"
        else:
            macro += f"action(ctx, _1); __QOBD_FOR_EACH_{i-1}(ctx, action, " + ", ".join(params[1:]) + ")"
        
        macros.append(macro)
    
    return "\n".join(macros)

# 生成并打印宏定义
print(generate_for_each_macros(127))