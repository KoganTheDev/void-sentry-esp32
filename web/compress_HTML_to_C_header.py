import sys
from pathlib import Path
import utils


def generate_C_header_content(html_name: str, html_content: bytes, html_content_length: int) -> str:
    """
    Convert HTML file bytes into a C header string.

    Generates:
      - A size macro (<NAME>_LEN)
      - A const uint8_t array containing the file's bytes in 0xXX format.

    Args:
        html_name: Original HTML filename.
        html_content: Raw HTML file bytes.
        html_content_length: Total number of bytes.

    Returns:
        A C/C++ header string containing the size macro and byte array.
    """
    
    array_name = utils._sanitize_name(html_name)
    array_len_name = f"{array_name}_LEN"
    
    c_header_content = (
            f"#include <stdint.h>\n\n"
            f"// File: {html_name}, Size: {html_content_length} bytes\n"
            f"#define {array_len_name} {html_content_length}\n"
            f"const uint8_t {array_name}[] = {{\n"
        )
        
    # Add actual HTML content into the bytestream
    hex_data = []
    for i, byte in enumerate(html_content):
        hex_string = f"0x{byte:02X}"
        hex_data.append(hex_string)
        
        # Add line breaks for readability every 16 characters
        if ((i + 1) % 16 == 0):
            c_header_content += " " + ", ".join(hex_data[-16:]) + ",\n"
        elif (i == (html_content_length - 1)):
            c_header_content += " " + ", ".join(hex_data[-(i % 16 + 1):])
    
    c_header_content += "\n};\n"
    
    return c_header_content


def main(args):
    # Input validation
    if not args or len(args) < 1:
        print("Usage: python compress_HTML_to_C_header.py <input_html_file>")
        print("Output file will be named based on input (e.g., index.html -> index.h)")
        sys.exit(1)
    
    html_path = Path(args[0]).expanduser().resolve()
    
    output_file_name = html_path.with_suffix(".h").name
    output_path = html_path.parent / output_file_name
    
    # Check suffix
    if (html_path.suffix.lower() != ".html"):
        raise Exception(f"Given file: {html_path.name} is not an HTML file")
    
    try:
        with html_path.open(mode='rb') as f:
            html_content = f.read()
            
        
        html_content_length = len(html_content)
        
        c_header_content = generate_C_header_content(html_path.name, html_content, html_content_length)
        
        # Insert the C header content into a header file
        with output_path.open('w', encoding='utf-8') as f:
            f.write(c_header_content)
        
        print(f"Successfully created C header at: {output_path}\n"
              f"File name is: {output_file_name}")
        
    except FileNotFoundError:
        print(f"\nThe file \"{html_path}\" was not found.")
    except Exception as e:
        print(f"An error has occured during processing: {e}")


if __name__ == "__main__":
    main(sys.argv[1:])
    