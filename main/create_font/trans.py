def convert_u_to_0x(input_file, output_file):
    """
    将文本文件中的所有 \u 转换为 0x
    使用 unicode_escape 编码来避免转义错误
    """
    
    try:
        # 使用 unicode_escape 编码读取文件，避免 \u 被当作转义序列
        with open(input_file, 'r', encoding='unicode_escape') as file:
            content = file.read()
        
        # 将所有的 \u 替换为 0x
        converted_content = content.replace('\\u', '0x')
        
        # 以普通 UTF-8 编码写入新文件
        with open(output_file, 'w', encoding='utf-8') as file:
            file.write(converted_content)
        
        print(f"转换完成！已将 '{input_file}' 中的所有 \\u 转换为 0x")
        print(f"结果已保存到 '{output_file}'")
        
    except FileNotFoundError:
        print(f"错误：找不到文件 '{input_file}'")
    except Exception as e:
        print(f"发生错误：{e}")

# 使用示例
if __name__ == "__main__":
    # 请修改为你的实际文件路径
    input_filename = "example.txt"  # 输入文件名
    output_filename = "converted_example.txt"  # 输出文件名
    
    convert_u_to_0x(input_filename, output_filename)