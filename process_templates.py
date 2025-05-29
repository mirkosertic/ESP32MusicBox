import os
from pathlib import Path

def process_templates(templates_dir, output_file):
    with open(output_file, 'w') as out:
        out.write("#pragma once\n\n")
        out.write("#include <Arduino.h>\n\n")
        
        for filename in os.listdir(templates_dir):
            if filename.endswith(".html"):
                var_name = os.path.splitext(filename)[0].upper() + "_TEMPLATE"
                with open(os.path.join(templates_dir, filename), 'r') as f:
                    content = f.read().replace('"', '\\"').replace('\n', '\\n')
                    out.write(f'const char* {var_name} PROGMEM = "{content}";\n\n')

Import("env")

templates_dir = "data"

OUTPUT_PATH = (
    Path(env.subst("$BUILD_DIR")) / "generated"
)

output_file = os.path.join(OUTPUT_PATH, "generated_html_templates.h")

def before_build():
    if not os.path.exists(OUTPUT_PATH):
        os.makedirs(OUTPUT_PATH)
    process_templates(templates_dir, output_file)

    # Add the directory containing the generated file to include path
    env.Append(CPPPATH=[OUTPUT_PATH])    

before_build()
