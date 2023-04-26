import re
import pprint

def parse_pages():
    with open('libwolfssl.S', 'r') as f:
        asm_code = f.read()

    pages = ["fp_sqr", "fp_montgomery_reduce", "_fp_exptmod_base_2"]
    results = {}
    for page in pages:
        pattern = f'([\da-f]+)(?:\s*<{page}>:)'
        addr = re.findall(pattern, asm_code)
        assert len(addr) == 1
        addr = addr[0]
        addr = int(addr, 16)

        results[page] = {}
        results[page]['addr'] = addr
        results[page]['pageno'] = addr // 4096
        results[page]['offset'] = addr % 4096
    
    pp = pprint.PrettyPrinter(indent=4)
    pp.pprint(results)

if __name__ == '__main__':
    parse_pages()
