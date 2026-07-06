"""
PlatformIO Pre-Build Script for RTLola Specifications

This script runs before each build to:
1. Find all .lola files in the specs/ directory
2. Compile them to C using rtlola2c
3. Adapt the generated code for Arduino/ESP32
4. Place output in lib/rtlola_monitors/

Usage: This script is automatically called by PlatformIO via extra_scripts
"""

import os
import sys
import subprocess
import shutil
import tempfile
import re
from pathlib import Path

# PlatformIO imports
Import("env")

def get_project_dir():
    """Get the PlatformIO project directory."""
    return Path(env.get("PROJECT_DIR", os.getcwd()))

def get_rtlola_compiler():
    """Find the rtlola2c compiler."""
    project_dir = get_project_dir()
    
    # Look for compiler in parent rtlola directory
    possible_paths = [
        project_dir.parent / "target" / "release" / "rtlola2c",
        project_dir.parent.parent / "rtlola" / "target" / "release" / "rtlola2c",
        Path.home() / ".cargo" / "bin" / "rtlola2c",
    ]
    
    for path in possible_paths:
        if path.exists():
            return path
    
    # Try to find it in PATH
    result = shutil.which("rtlola2c")
    if result:
        return Path(result)
    
    return None

def adapt_c_code(content, is_header=False, string_constants=None):
    """Adapt C code content for Arduino/ESP32."""
    lines = content.split('\n')
    adapted_lines = []
    i = 0
    
    added_arduino_h = False
    stdio_included = False
    
    while i < len(lines):
        line = lines[i]
        
        # Fix case-sensitive header includes
        if re.search(r'#include\s*"Monitor\.h"', line):
            line = line.replace('"Monitor.h"', '"monitor.h"')
        
        # For header files: Fix string constant definitions
        if is_header and re.match(r'^\s*char\*\s+STR_CONSTANT_\d+\s*=', line):
            match = re.match(r'^\s*(char\*\s+STR_CONSTANT_\d+)\s*=\s*([^;]+);', line)
            if match:
                var_decl = match.group(1)
                value = match.group(2).strip()
                adapted_lines.append(f'extern {var_decl};')
                if string_constants is not None:
                    string_constants.append((var_decl, value))
                i += 1
                continue
        
        # For header files: Add extern "C" wrapper
        if is_header and not added_arduino_h:
            if re.match(r'^\s*#include', line):
                adapted_lines.append(line)
                i += 1
                continue
            elif line.strip() and not line.strip().startswith('#'):
                adapted_lines.append('#ifdef __cplusplus')
                adapted_lines.append('extern "C" {')
                adapted_lines.append('#endif')
                adapted_lines.append('')
                adapted_lines.append(line)
                added_arduino_h = True
                i += 1
                continue
        
        # Handle includes
        if re.match(r'^\s*#include\s*["<]stdio\.h[">]', line, re.IGNORECASE):
            stdio_included = True
            if not added_arduino_h:
                adapted_lines.append('#include <Arduino.h>')
                added_arduino_h = True
            i += 1
            continue
        
        # Keep stdlib.h and string.h
        if re.match(r'^\s*#include\s*["<](stdlib|string)\.h[">]', line):
            adapted_lines.append(line)
            i += 1
            continue
        
        # Replace printf with Serial.printf
        if 'printf' in line and re.search(r'printf\s*\([^)]+\)\s*;', line):
            simple_print = re.search(r'printf\s*\(\s*"([^"]+)"\s*\)\s*;', line)
            if simple_print:
                string_content = simple_print.group(1)
                if string_content.endswith('\\n'):
                    string_content = string_content[:-2]
                    adapted_lines.append(re.sub(
                        r'printf\s*\([^)]+\)\s*;',
                        f'Serial.println("{string_content}");',
                        line
                    ))
                else:
                    adapted_lines.append(re.sub(
                        r'printf\s*\([^)]+\)\s*;',
                        f'Serial.print("{string_content}");',
                        line
                    ))
                i += 1
                continue
            
            if '%' in line:
                adapted_lines.append(re.sub(r'printf\s*\(', 'Serial.printf(', line))
                i += 1
                continue
            
            adapted_lines.append(re.sub(r'printf\s*\(', 'Serial.printf(', line))
            i += 1
            continue
        
        # Add Arduino.h at the beginning if needed
        if not added_arduino_h and not is_header:
            if line.strip() and not line.strip().startswith('#'):
                adapted_lines.insert(0, '#include <Arduino.h>')
                added_arduino_h = True
        
        adapted_lines.append(line)
        i += 1
    
    # For .c files: Add string constant definitions at the top
    if not is_header and string_constants:
        insert_pos = 0
        for idx, line in enumerate(adapted_lines):
            if line.strip().startswith('#include'):
                insert_pos = idx + 1
            elif line.strip() and insert_pos > 0 and not line.strip().startswith('#'):
                break
        for var_decl, value in string_constants:
            adapted_lines.insert(insert_pos, f'{var_decl} = {value};')
            insert_pos += 1
    
    # Add Arduino.h if we removed stdio.h
    if not added_arduino_h and stdio_included and not is_header:
        insert_pos = 0
        for i, line in enumerate(adapted_lines):
            if line.strip().startswith('#include'):
                insert_pos = i + 1
            elif line.strip() and insert_pos > 0:
                break
        adapted_lines.insert(insert_pos, '#include <Arduino.h>')
    
    # For headers, close extern "C" guard
    if is_header and any('#ifdef __cplusplus' in line for line in adapted_lines):
        adapted_lines.append('')
        adapted_lines.append('#ifdef __cplusplus')
        adapted_lines.append('}')
        adapted_lines.append('#endif')
    
    return '\n'.join(adapted_lines)

def compile_lola_spec(spec_path, output_dir, compiler_path, optimize=False):
    """Compile a single .lola specification to C code."""
    spec_path = Path(spec_path)
    output_dir = Path(output_dir)
    
    print(f"  Compiling: {spec_path.name}")
    
    # Create temp directory for rtlola2c output
    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        
        # Build compiler command
        cmd = [
            str(compiler_path),
            str(spec_path),
            "--output-dir", str(temp_path),
            "--overwrite"
        ]
        
        if optimize:
            cmd.append("--optimize")
        
        # Run rtlola2c
        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                cwd=str(compiler_path.parent.parent.parent)  # rtlola directory
            )
            
            if result.returncode != 0:
                print(f"    ERROR: rtlola2c failed for {spec_path.name}")
                print(f"    stderr: {result.stderr}")
                return False
                
        except Exception as e:
            print(f"    ERROR: Failed to run rtlola2c: {e}")
            return False
        
        # Find generated files (could be monitor.c/h or Monitor.c/h)
        c_file = None
        h_file = None
        
        for name in ["monitor.c", "Monitor.c"]:
            if (temp_path / name).exists():
                c_file = temp_path / name
                break
        
        for name in ["monitor.h", "Monitor.h"]:
            if (temp_path / name).exists():
                h_file = temp_path / name
                break
        
        if not c_file or not h_file:
            print(f"    ERROR: Generated files not found in {temp_path}")
            print(f"    Contents: {list(temp_path.iterdir())}")
            return False
        
        # Read and adapt the files
        with open(h_file, 'r') as f:
            h_content = f.read()
        
        with open(c_file, 'r') as f:
            c_content = f.read()
        
        # Extract string constants from header
        string_constants = []
        for line in h_content.split('\n'):
            if re.match(r'^\s*char\*\s+STR_CONSTANT_\d+\s*=', line):
                match = re.match(r'^\s*(char\*\s+STR_CONSTANT_\d+)\s*=\s*([^;]+);', line)
                if match:
                    string_constants.append((match.group(1), match.group(2).strip()))
        
        # Adapt code for Arduino/ESP32
        adapted_h = adapt_c_code(h_content, is_header=True, string_constants=None)
        adapted_c = adapt_c_code(c_content, is_header=False, string_constants=string_constants)
        
        # Ensure output directory exists
        output_dir.mkdir(parents=True, exist_ok=True)
        
        # Write adapted files
        # Note: We use .cpp extension because the adapted code uses Serial (C++ object)
        with open(output_dir / "monitor.h", 'w') as f:
            f.write(adapted_h)
        
        with open(output_dir / "monitor.cpp", 'w') as f:
            f.write(adapted_c)
        
        print(f"    ✓ Generated: {output_dir}/monitor.cpp, monitor.h")
        return True

def create_library_json(output_dir, spec_name):
    """Create a library.json for PlatformIO library discovery."""
    library_json = {
        "name": f"rtlola_{spec_name}",
        "version": "1.0.0",
        "description": f"RTLola monitor generated from {spec_name}.lola",
        "frameworks": ["arduino"],
        "platforms": ["espressif32"]
    }
    
    import json
    with open(output_dir / "library.json", 'w') as f:
        json.dump(library_json, f, indent=2)

def prebuild_rtlola(source, target, env):
    """Main pre-build function called by PlatformIO."""
    project_dir = get_project_dir()
    specs_dir = project_dir / "specs"
    lib_dir = project_dir / "lib"
    
    print("\n" + "="*60)
    print("RTLola Pre-Build: Compiling .lola specifications")
    print("="*60)
    
    # Find rtlola2c compiler
    compiler = get_rtlola_compiler()
    if not compiler:
        print("ERROR: rtlola2c compiler not found!")
        print("Please build the compiler first:")
        print("  cd rtlola && cargo build --release")
        print("")
        print("Or ensure rtlola2c is in your PATH.")
        sys.exit(1)
    
    print(f"Using compiler: {compiler}")
    
    # Find all .lola files
    if not specs_dir.exists():
        print(f"No specs/ directory found at {specs_dir}")
        print("Create specs/ and add your .lola files there.")
        print("="*60 + "\n")
        return
    
    lola_files = list(specs_dir.glob("*.lola"))
    
    if not lola_files:
        print("No .lola files found in specs/")
        print("Add your RTLola specifications to the specs/ directory.")
        print("="*60 + "\n")
        return
    
    print(f"Found {len(lola_files)} specification(s):\n")
    
    # Check for RTLOLA_OPTIMIZE environment variable
    optimize = os.environ.get("RTLOLA_OPTIMIZE", "0") == "1"
    if optimize:
        print("Optimization: ENABLED (RTLOLA_OPTIMIZE=1)\n")
    
    # Compile each specification
    success_count = 0
    for spec_file in lola_files:
        spec_name = spec_file.stem
        output_dir = lib_dir / f"rtlola_{spec_name}"
        
        if compile_lola_spec(spec_file, output_dir, compiler, optimize):
            create_library_json(output_dir, spec_name)
            success_count += 1
    
    print("")
    print(f"Compiled {success_count}/{len(lola_files)} specifications successfully")
    print("="*60 + "\n")
    
    if success_count < len(lola_files):
        print("WARNING: Some specifications failed to compile!")
        print("Check the errors above and fix your .lola files.")

# Register the pre-build action
# Run before building the program
env.AddPreAction("buildprog", prebuild_rtlola)

# Also run before compiling any source file in src/
# This ensures the monitor is generated before compilation starts
env.AddPreAction("$BUILD_DIR/src/main.cpp.o", prebuild_rtlola)
env.AddPreAction("$BUILD_DIR/src/main_dual_core.cpp.o", prebuild_rtlola)
env.AddPreAction("$BUILD_DIR/src/main_pachinko.cpp.o", prebuild_rtlola)

