# VS Code Configs

## C/C++

For file `.vscode/c_cpp_properties.json`, change the kernel version to yours.

```JSON
{
    "configurations": [
        {
            "name": "Linux",
            "includePath": [
                "${workspaceFolder}/**",
                "${workspaceFolder}/src",
                "${workspaceFolder}/src/microbench",
                "${workspaceFolder}/src/lib",
                "/usr/src/linux-headers-4.15.0-166-generic/include",
                "/usr/src/linux-headers-4.15.0-166-generic/include/asm-generic",
                "/usr/src/linux-headers-4.15.0-166-generic/arch/x86/include"
            ],
            "defines": [
                "__GNUC__",
                "__KERNEL__"
            ],
            "compilerPath": "/usr/bin/gcc",
            "cStandard": "gnu89",
            "cppStandard": "gnu++14",
            "intelliSenseMode": "linux-gcc-x64"
        }
    ],
    "version": 4
}
```
