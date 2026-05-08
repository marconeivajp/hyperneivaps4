import os

def clean_file(path, replacements):
    if not os.path.exists(path):
        print(f"Skipping {path}: Not found.")
        return
    
    with open(path, 'r', encoding='latin-1') as f:
        content = f.read()
    
    for r in replacements:
        content = content.replace(r, '')
        
    with open(path, 'w', encoding='latin-1') as f:
        f.write(content)

if __name__ == '__main__':
    # Fix bloca_de_notas.cpp manually to be sure
    bn_path = 'bloco_de_notas.cpp'
    if os.path.exists(bn_path):
        with open(bn_path, 'r', encoding='latin-1') as f:
            lines = f.readlines()
        
        new_lines = []
        for line in lines:
            # Fix line 10 corruption
            if 'int estadoNotepad = 0; scrollHorizontalNotepad = 0;' in line:
                new_lines.append('int estadoNotepad = 0;\n')
            # Fix line 14 redefinition
            elif 'int totalLinhasNotepad = 1; int scrollHorizontalNotepad = 0;' in line:
                 new_lines.append('int totalLinhasNotepad = 1; int scrollHorizontalNotepad = 0;\n')
            elif 'estadoNotepad = 0; scrollHorizontalNotepad = 0;' in line:
                 new_lines.append('    estadoNotepad = 0;\n    scrollHorizontalNotepad = 0;\n')
            else:
                new_lines.append(line)
        
        with open(bn_path, 'w', encoding='latin-1') as f:
            f.writelines(new_lines)
            
    # Fix IME files
    clean_file('controle_virtual.cpp', ['    imeSetting->enterButtonAssignment = 1; // For\u00e7a CROSS (X) como Enter\n', '    imeSetting->enterButtonAssignment = 1; // For\u00a7a CROSS (X) como Enter\n'])
    clean_file('controle_editar.cpp', [' imeConfig.enterButtonAssignment = 1;'])
    clean_file('ftp.cpp', ['    param.enterButtonAssignment = 1; // For\u00e7a CROSS (X) como Enter\n', '    param.enterButtonAssignment = 1; // For\u00a7a CROSS (X) como Enter\n'])
    
    print("Cleanup complete.")
