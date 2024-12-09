git ls-files --recurse-submodules | grep -E '\.(hpp|cpp|h)$' | grep -v '^lib/esp32-mcp2515' | xargs wc -l
