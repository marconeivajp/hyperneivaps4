import os

def clean_file_generic(path, match_str):
    if not os.path.exists(path):
        return
    with open(path, 'r', encoding='latin-1') as f:
        lines = f.readlines()
    new_lines = [l for l in lines if match_str not in l]
    with open(path, 'w', encoding='latin-1') as f:
        f.writelines(new_lines)

def fix_bloco():
    p = 'bloco_de_notas.cpp'
    if not os.path.exists(p): return
    with open(p, 'r', encoding='latin-1') as f:
        content = f.read()
    
    # Very specific fixes for the doubled lines
    content = content.replace('int estadoNotepad = 0; scrollHorizontalNotepad = 0; scrollHorizontalNotepad = 0;', 'int estadoNotepad = 0;')
    content = content.replace('int totalLinhasNotepad = 1; int scrollHorizontalNotepad = 0; int scrollHorizontalNotepad = 0;', 'int totalLinhasNotepad = 1; int scrollHorizontalNotepad = 0;')
    content = content.replace('estadoNotepad = 0; scrollHorizontalNotepad = 0; scrollHorizontalNotepad = 0;', 'estadoNotepad = 0; scrollHorizontalNotepad = 0;')
    
    with open(p, 'w', encoding='latin-1') as f:
        f.write(content)

if __name__ == '__main__':
    fix_bloco()
    clean_file_generic('controle_virtual.cpp', 'enterButtonAssignment')
    clean_file_generic('controle_editar.cpp', 'enterButtonAssignment')
    clean_file_generic('ftp.cpp', 'enterButtonAssignment')
    print("FIX ALL DONE")
