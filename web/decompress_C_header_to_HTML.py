import sys
from pathlib import Path
import re

def generate_HTML_content(header_content: str) -> str:
    """
    Extract hex byte values from a C header and reconstruct the original HTML text.

    Args:
        header_content: The full text of the C header containing 0xXX byte values.

    Returns:
        The decoded HTML content as a UTF-8 string.
    """
    # using RegEX fomd hex values
    hex_values = re.findall(r"0x([0-9A-Fa-f]{2})", header_content)
    
    # Convert all of the string elements into a byte objects
    byte_data = bytes(int(h, base=16) for h in hex_values)
    
    # Decode content to UTF-8
    return byte_data.decode("utf-8")


def main(args):
    # Input validation
    if not args or len(args) < 1:
        print("Usage: python decompress_C_header_to_HTML.py <input_html_file>")
        print("Output file will be named based on input (e.g., index.h -> index.html)")
        sys.exit(1)
    
    
    header_path = Path(args[0]).expanduser().resolve()
    
    output_file_name = header_path.with_suffix(".html").name
    output_path = header_path.parent / output_file_name
    
    # Check suffix
    if (header_path.suffix.lower() != ".h"):
        raise Exception(f"Given file: {header_path.name} is not a C header file")
    
    try:
        with header_path.open(mode='r') as f:
            header_content = f.read()
            
        # Parse content and output into an HTML file
        html_content = generate_HTML_content(header_content)
        
        # Insert the HTML content into the HTML file
        with output_path.open('w', encoding='utf-8') as f:
            f.write(html_content)
        
        print(f"Successfully created HTML file at: {output_path}\n"
              f"File name is: {output_file_name}")
        
    except FileNotFoundError:
        print(f"\nThe file \"{header_path}\" was not found.")
    except Exception as e:
        print(f"An error has occured during processing: {e}")


if __name__ == "__main__":
    main(sys.argv[1:])
    