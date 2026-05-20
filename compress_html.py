import gzip
import re

with open('include/web_dashboard.h', 'r', encoding='utf-8') as f:
    content = f.read()

# Extract the raw HTML content
match = re.search(r'const char index_html\[\] = R"rawliteral\((.*?)\)rawliteral";', content, re.DOTALL)
if not match:
    print('Failed to extract HTML')
    exit(1)

html_content = match.group(1).strip()

# Compress the HTML
compressed = gzip.compress(html_content.encode('utf-8'))

# Generate C header content
header_content = '#pragma once\n\n'
header_content += f'const uint32_t index_html_gz_len = {len(compressed)};\n'
header_content += 'const uint8_t index_html_gz[] PROGMEM = {\n'

for i, byte in enumerate(compressed):
    header_content += f'0x{byte:02X}, '
    if (i + 1) % 16 == 0:
        header_content += '\n'

header_content += '\n};\n'

with open('include/web_dashboard_gz.h', 'w') as f:
    f.write(header_content)

print(f'Original size: {len(html_content)}')
print(f'Compressed size: {len(compressed)}')
